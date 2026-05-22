/*
 * NFC Handler Module Implementation
 * Wraps abobija/rc522 SPI driver behind a single-callback API.
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "nfc_handler.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "rc522.h"
#include "driver/rc522_spi.h"
#include "rc522_picc.h"
#include "picc/rc522_nxp.h"

static const char *TAG = "NFC";

// Suppress duplicate detections of the same UID within this window.
// RC522's heartbeat check can flicker when the card sits at the edge of the
// RF field, which would otherwise type the same string multiple times.
#define NFC_DEDUP_WINDOW_US (1500 * 1000)

// Pin map: see CLAUDE.md "GPIO Pin Assignments" for net names + PCB position.
// IRQ (GPIO 5) is wired but not used here — polling mode handles edge detection
// inside the rc522 driver.
#define NFC_SPI_HOST    SPI2_HOST
#define NFC_GPIO_RST    4
#define NFC_GPIO_MISO   6
#define NFC_GPIO_MOSI   7
#define NFC_GPIO_SCLK   15
#define NFC_GPIO_SDA    16

static rc522_spi_config_t s_driver_config = {
    .host_id = NFC_SPI_HOST,
    .bus_config = &(spi_bus_config_t){
        .miso_io_num = NFC_GPIO_MISO,
        .mosi_io_num = NFC_GPIO_MOSI,
        .sclk_io_num = NFC_GPIO_SCLK,
    },
    .dev_config = {
        .spics_io_num = NFC_GPIO_SDA,
        // 1 MHz — conservative for flying-wire prototype to survive RF-transmit
        // noise. Library default is 5 MHz; bump after the carrier PCB lands.
        .clock_speed_hz = 1000000,
    },
    .rst_io_num = NFC_GPIO_RST,
};

static rc522_driver_handle_t s_driver = NULL;
static rc522_handle_t s_scanner = NULL;
static nfc_tag_callback_t s_callback = NULL;

// Last fired UID + timestamp, for de-duplication.
static char s_last_uid[RC522_PICC_UID_SIZE_MAX * 2 + 1] = {0};
static int64_t s_last_uid_time_us = 0;

// Parse a single NDEF Text Record out of NTAG user-memory bytes (TLV-wrapped).
// We only support the minimum subset that matches what `NFC Tools` and similar
// writer apps produce: short-record, well-known TNF, type 'T', UTF-8.
// Returns true and writes a NUL-terminated UTF-8 payload to out_text on success.
static bool parse_ndef_text(const uint8_t *buf, size_t buf_len,
                            char *out_text, size_t max_text_len)
{
    size_t i = 0;

    while (i < buf_len) {
        uint8_t tlv_tag = buf[i++];
        if (tlv_tag == 0x00) {
            continue;  // NULL TLV padding
        }
        if (tlv_tag == 0xFE) {
            return false;  // Terminator TLV — no NDEF found before this
        }
        if (i >= buf_len) {
            return false;
        }

        // TLV length: 1 byte unless first byte is 0xFF, then it's 3 bytes (0xFF + uint16 BE).
        size_t tlv_len;
        if (buf[i] == 0xFF) {
            if (i + 3 > buf_len) return false;
            tlv_len = (buf[i + 1] << 8) | buf[i + 2];
            i += 3;
        } else {
            tlv_len = buf[i];
            i += 1;
        }

        if (tlv_tag != 0x03) {
            // Lock-control / Memory-control TLVs etc — skip past their payload.
            i += tlv_len;
            continue;
        }

        // Found NDEF Message TLV. Parse its first record.
        if (i + tlv_len > buf_len) return false;
        size_t msg_end = i + tlv_len;

        if (i >= msg_end) return false;
        uint8_t hdr = buf[i++];
        bool sr = (hdr & 0x10) != 0;          // Short Record flag
        bool il = (hdr & 0x08) != 0;          // ID Length present
        uint8_t tnf = hdr & 0x07;
        if (tnf != 0x01) return false;        // Only Well-known type supported

        if (i >= msg_end) return false;
        uint8_t type_len = buf[i++];

        size_t payload_len;
        if (sr) {
            if (i >= msg_end) return false;
            payload_len = buf[i++];
        } else {
            if (i + 4 > msg_end) return false;
            payload_len = ((size_t)buf[i] << 24) | ((size_t)buf[i + 1] << 16)
                        | ((size_t)buf[i + 2] << 8) | buf[i + 3];
            i += 4;
        }

        size_t id_len = 0;
        if (il) {
            if (i >= msg_end) return false;
            id_len = buf[i++];
        }

        if (type_len != 1 || i >= msg_end) return false;
        if (buf[i++] != 'T') return false;    // Not a Text record

        i += id_len;  // skip optional ID

        if (i + payload_len > msg_end || payload_len < 1) return false;

        // Text payload: [status][lang_code][text]
        // status bit 7 = UTF-16 if set, bits 0..5 = lang_code length.
        uint8_t status = buf[i];
        if (status & 0x80) return false;       // UTF-16 not supported
        size_t lang_len = status & 0x3F;
        if (1 + lang_len > payload_len) return false;

        size_t text_off = i + 1 + lang_len;
        size_t text_len = payload_len - 1 - lang_len;
        if (text_len > max_text_len) text_len = max_text_len;

        memcpy(out_text, &buf[text_off], text_len);
        out_text[text_len] = '\0';
        return text_len > 0;
    }

    return false;
}

// Read enough NTAG user pages to (usually) cover an NDEF Text payload of
// length up to NFC_PAYLOAD_MAX_LEN, then parse. Returns true on success.
static bool try_read_ndef_text(rc522_picc_t *picc, char *out_text, size_t max_text_len)
{
    // 12 pages = 48 bytes — fits an NDEF Text record with up to ~38 chars
    // after TLV + header overhead. Three sequential 16-byte reads from page 4.
    enum { NDEF_BUF_LEN = 48 };
    uint8_t buf[NDEF_BUF_LEN];

    for (size_t off = 0; off < NDEF_BUF_LEN; off += RC522_NXP_READ_SIZE) {
        uint8_t page_addr = 4 + (uint8_t)(off / RC522_NXP_PAGE_SIZE);
        esp_err_t ret = rc522_nxp_read(s_scanner, picc, page_addr, &buf[off]);
        if (ret != ESP_OK) {
            ESP_LOGD(TAG, "NDEF read failed at page %u: %s", page_addr, esp_err_to_name(ret));
            return false;
        }
    }

    return parse_ndef_text(buf, NDEF_BUF_LEN, out_text, max_text_len);
}

static void on_picc_state_changed(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    rc522_picc_state_changed_event_t *event = (rc522_picc_state_changed_event_t *)data;
    rc522_picc_t *picc = event->picc;

    if (picc->state == RC522_PICC_STATE_ACTIVE) {
        // Continuous uppercase hex, no separators. Buffer fits worst case (10 bytes -> 20 hex + NUL).
        char uid_hex[RC522_PICC_UID_SIZE_MAX * 2 + 1];
        char *p = uid_hex;
        for (uint8_t i = 0; i < picc->uid.length; i++) {
            p += sprintf(p, "%02X", picc->uid.value[i]);
        }
        *p = '\0';

        // De-duplicate: same UID within the dedup window is a heartbeat flicker, not a fresh scan.
        int64_t now_us = esp_timer_get_time();
        if (strcmp(uid_hex, s_last_uid) == 0 && (now_us - s_last_uid_time_us) < NFC_DEDUP_WINDOW_US) {
            ESP_LOGD(TAG, "Tag re-detected within dedup window: UID=%s, suppressed", uid_hex);
            s_last_uid_time_us = now_us;  // refresh window so steady contact stays suppressed
            return;
        }
        strncpy(s_last_uid, uid_hex, sizeof(s_last_uid) - 1);
        s_last_uid[sizeof(s_last_uid) - 1] = '\0';
        s_last_uid_time_us = now_us;

        // Try to read an NDEF Text payload from the tag. Falls back to NULL on
        // any parse/read failure — main app then types the UID as a debug aid.
        char payload[NFC_PAYLOAD_MAX_LEN + 1];
        const char *payload_arg = NULL;
        if (try_read_ndef_text(picc, payload, NFC_PAYLOAD_MAX_LEN)) {
            payload_arg = payload;
            ESP_LOGI(TAG, "Tag detected: UID=%s payload=\"%s\"", uid_hex, payload);
        } else {
            ESP_LOGI(TAG, "Tag detected: UID=%s (no NDEF Text record, falling back to UID)", uid_hex);
        }

        if (s_callback) {
            s_callback(payload_arg, uid_hex);
        }
    } else if (picc->state == RC522_PICC_STATE_IDLE && event->old_state >= RC522_PICC_STATE_ACTIVE) {
        ESP_LOGD(TAG, "Tag removed");
    }
}

esp_err_t nfc_handler_init(void)
{
    if (s_scanner != NULL) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    esp_err_t ret = rc522_spi_create(&s_driver_config, &s_driver);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "rc522_spi_create failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = rc522_driver_install(s_driver);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "rc522_driver_install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    rc522_config_t scanner_config = { .driver = s_driver };
    ret = rc522_create(&scanner_config, &s_scanner);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "rc522_create failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = rc522_register_events(s_scanner, RC522_EVENT_PICC_STATE_CHANGED, on_picc_state_changed, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "rc522_register_events failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "NFC handler initialized");
    ESP_LOGI(TAG, "  SPI2 (FSPI): SCK=GPIO%d MISO=GPIO%d MOSI=GPIO%d CS=GPIO%d RST=GPIO%d",
             NFC_GPIO_SCLK, NFC_GPIO_MISO, NFC_GPIO_MOSI, NFC_GPIO_SDA, NFC_GPIO_RST);
    return ESP_OK;
}

void nfc_handler_set_callback(nfc_tag_callback_t cb)
{
    s_callback = cb;
}

esp_err_t nfc_handler_start(void)
{
    if (s_scanner == NULL) {
        ESP_LOGE(TAG, "nfc_handler_init must be called first");
        return ESP_ERR_INVALID_STATE;
    }
    return rc522_start(s_scanner);
}
