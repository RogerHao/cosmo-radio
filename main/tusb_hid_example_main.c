/*
 * Cosmo Pager Radio - USB HID Keyboard
 *
 * Converts rotary encoder and button inputs to USB HID keyboard events.
 * Replaces the BLE HID implementation for improved reliability.
 *
 * Input mapping:
 *   - Button press/release -> Enter key press/release
 *   - Encoder 1 CW/CCW -> Up/Down arrow key pulse
 *   - Encoder 2 CW/CCW -> Right/Left arrow key pulse
 */

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "class/hid/hid_device.h"

#include "input_handler.h"
#include "led_indicator.h"
#include "nfc_handler.h"

static const char *TAG = "USB_HID";

// HID key codes
#define KEY_ENTER       0x28  // HID_KEY_ENTER
#define KEY_UP_ARROW    0x52  // HID_KEY_ARROW_UP
#define KEY_DOWN_ARROW  0x51  // HID_KEY_ARROW_DOWN
#define KEY_LEFT_ARROW  0x50  // HID_KEY_ARROW_LEFT
#define KEY_RIGHT_ARROW 0x4F  // HID_KEY_ARROW_RIGHT
#define KEY_F1          0x3A  // HID_KEY_F1 — placeholder for ENC1 SW
#define KEY_F2          0x3B  // HID_KEY_F2 — placeholder for ENC2 SW

// HID modifier byte
#define HID_MOD_LSHIFT  0x02

// Key pulse duration (ms) for rotary encoder events
#define KEY_PULSE_MS    20

// Per-character pulse duration when typing strings (NFC UID, etc.)
#define STRING_KEY_DOWN_MS  15
#define STRING_KEY_UP_MS    15

// HID IN endpoint poll interval is 10 ms in the descriptor. A release report
// must wait for readiness instead of being dropped while the endpoint is busy.
#define HID_REPORT_TIMEOUT_MS 80
// Keep at least one FreeRTOS tick; 5 ms can round to 0 on a 100 Hz tick.
#define HID_REPORT_RETRY_MS   10

/************* TinyUSB descriptors ****************/

#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_DESC_LEN)

// HID report descriptor - keyboard only (no mouse)
const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_ITF_PROTOCOL_KEYBOARD)),
};

// String descriptor
const char *hid_string_descriptor[5] = {
    (char[]){0x09, 0x04},      // 0: Supported language is English (0x0409)
    "Cosmo",                   // 1: Manufacturer
    "Pager Radio Input",       // 2: Product
    "000001",                  // 3: Serial number
    "HID Keyboard",            // 4: HID interface
};

// Configuration descriptor
static const uint8_t hid_configuration_descriptor[] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, boot protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(0, 4, false, sizeof(hid_report_descriptor), 0x81, 16, 10),
};

/********* TinyUSB HID callbacks ***************/

// Invoked when received GET HID REPORT DESCRIPTOR request
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return hid_report_descriptor;
}

// Invoked when received GET_REPORT control request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

// Invoked when received SET_REPORT control request
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}

/********* HID Key State + Reporting ***************/

// Multi-key HID state. Two FreeRTOS tasks write here concurrently:
//   - input_handler_task: button + encoder events
//   - RC522 event task:   NFC tag scans -> string injection
// All access goes through s_hid_mutex; the helpers below take/give it
// internally so callers don't need to think about locking.
//
// `keys_*` operate on the array (held-key set). `hid_*` are the public
// actions (down / up / pulse / type-char) that wrap report submission.
static SemaphoreHandle_t s_hid_mutex = NULL;
static TaskHandle_t s_hid_report_retry_task = NULL;
static uint8_t s_pressed_keys[6] = {0};
static uint8_t s_modifier = 0;
static volatile bool s_hid_report_pending = false;

// Add keycode to the pressed-set. No-op if already present.
// Returns false on rollover (>6 keys held) — caller can ignore safely.
// MUST be called with s_hid_mutex held.
static bool keys_add(uint8_t keycode)
{
    if (keycode == 0) return false;
    int free_slot = -1;
    for (int i = 0; i < 6; i++) {
        if (s_pressed_keys[i] == keycode) return true;     // already pressed
        if (s_pressed_keys[i] == 0 && free_slot < 0) free_slot = i;
    }
    if (free_slot < 0) return false;                       // 6KRO rollover
    s_pressed_keys[free_slot] = keycode;
    return true;
}

// Remove keycode from the pressed-set (no-op if absent).
// MUST be called with s_hid_mutex held.
static void keys_remove(uint8_t keycode)
{
    for (int i = 0; i < 6; i++) {
        if (s_pressed_keys[i] == keycode) {
            s_pressed_keys[i] = 0;
        }
    }
}

// Submit current modifier + pressed-set as a HID report if the endpoint is ready.
// MUST be called with s_hid_mutex held.
static bool hid_report_try_locked(void)
{
    if (!tud_hid_ready()) {
        s_hid_report_pending = true;
        return false;
    }

    bool ok = tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, s_modifier, s_pressed_keys);
    s_hid_report_pending = !ok;
    if (!ok) {
        ESP_LOGW(TAG, "HID report submit failed, pending retry");
    }
    return ok;
}

// Wait briefly for the HID IN endpoint and submit the current state. This makes
// key-up reports durable when they happen immediately after key-down reports.
// MUST be called with s_hid_mutex held.
static bool hid_report_locked(void)
{
    TickType_t start = xTaskGetTickCount();
    TickType_t timeout = pdMS_TO_TICKS(HID_REPORT_TIMEOUT_MS);
    if (timeout == 0) {
        timeout = 1;
    }

    while (!tud_hid_ready()) {
        if (!tud_mounted() || (xTaskGetTickCount() - start) >= timeout) {
            s_hid_report_pending = true;
            return false;
        }
        vTaskDelay(1);
    }

    return hid_report_try_locked();
}

static void hid_report_retry_task(void *arg)
{
    (void)arg;

    while (1) {
        if (s_hid_report_pending && s_hid_mutex != NULL) {
            xSemaphoreTake(s_hid_mutex, portMAX_DELAY);
            if (s_hid_report_pending) {
                hid_report_try_locked();
            }
            xSemaphoreGive(s_hid_mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(HID_REPORT_RETRY_MS));
    }
}

// Press a key (idempotent). Holds across subsequent reports until hid_key_up().
static void hid_key_down(uint8_t keycode)
{
    xSemaphoreTake(s_hid_mutex, portMAX_DELAY);
    keys_add(keycode);
    hid_report_locked();
    xSemaphoreGive(s_hid_mutex);
}

// Release a specific key. Other held keys remain pressed.
static void hid_key_up(uint8_t keycode)
{
    xSemaphoreTake(s_hid_mutex, portMAX_DELAY);
    keys_remove(keycode);
    hid_report_locked();
    xSemaphoreGive(s_hid_mutex);
}

// Pulse a key (down -> KEY_PULSE_MS -> up). Used for rotary encoder detents.
static void hid_key_pulse(uint8_t keycode)
{
    hid_key_down(keycode);
    vTaskDelay(pdMS_TO_TICKS(KEY_PULSE_MS));
    hid_key_up(keycode);
}

// Type one ASCII char as a press+release with optional modifier OR'd in.
// Releases the lock between press and release so user input can interleave
// between characters of an NFC string injection.
static void hid_type_char(uint8_t modifier, uint8_t keycode)
{
    xSemaphoreTake(s_hid_mutex, portMAX_DELAY);
    uint8_t prev_mod = s_modifier;
    s_modifier = prev_mod | modifier;
    keys_add(keycode);
    hid_report_locked();
    xSemaphoreGive(s_hid_mutex);

    vTaskDelay(pdMS_TO_TICKS(STRING_KEY_DOWN_MS));

    xSemaphoreTake(s_hid_mutex, portMAX_DELAY);
    keys_remove(keycode);
    s_modifier = prev_mod;
    hid_report_locked();
    xSemaphoreGive(s_hid_mutex);

    vTaskDelay(pdMS_TO_TICKS(STRING_KEY_UP_MS));
}

// Translate ASCII to HID (modifier, keycode). Returns false if char not supported.
// Covers what the NFC tag protocols need: alphanumeric, the '#' / 'NFC:' prefixes,
// '\n' line terminator, plus a small set of separators safe for ID encoding
// ('-', '_', '.', '/'). Space and other punctuation are intentionally omitted —
// the tag-content encoding contract is "ASCII identifier characters only".
static bool ascii_to_hid(char c, uint8_t *modifier, uint8_t *keycode)
{
    *modifier = 0;
    if (c >= 'a' && c <= 'z') {
        *keycode = 0x04 + (c - 'a');
        return true;
    } else if (c >= 'A' && c <= 'Z') {
        *modifier = HID_MOD_LSHIFT;
        *keycode = 0x04 + (c - 'A');
        return true;
    } else if (c == '0') {
        *keycode = 0x27;
        return true;
    } else if (c >= '1' && c <= '9') {
        *keycode = 0x1E + (c - '1');
        return true;
    } else if (c == '#') {
        // '#' shares the '3' key (HID 0x20), needs Shift
        *modifier = HID_MOD_LSHIFT;
        *keycode = 0x20;
        return true;
    } else if (c == ':') {
        // ':' shares physical key with ';' (HID 0x33), needs Shift
        *modifier = HID_MOD_LSHIFT;
        *keycode = 0x33;
        return true;
    } else if (c == '-') {
        *keycode = 0x2D;
        return true;
    } else if (c == '_') {
        // Underscore = Shift + minus
        *modifier = HID_MOD_LSHIFT;
        *keycode = 0x2D;
        return true;
    } else if (c == '.') {
        *keycode = 0x37;
        return true;
    } else if (c == '/') {
        *keycode = 0x38;
        return true;
    } else if (c == '\n') {
        *keycode = KEY_ENTER;
        return true;
    }
    return false;
}

// Type a null-terminated string via HID keyboard. Skips unsupported chars.
static void send_string(const char *str)
{
    for (const char *p = str; *p; p++) {
        uint8_t mod, kc;
        if (!ascii_to_hid(*p, &mod, &kc)) {
            continue;
        }
        hid_type_char(mod, kc);
    }
}

/********* Input Event Handling ***************/

// Callback for input events from input_handler module.
// Each event maps to a *single* keycode press or release, so combinations like
// "hold ENC1_SW (F1) and rotate ENC1 (Up)" produce the correct F1+Up combo.
static void on_input_event(const input_event_t *event)
{
    switch (event->type) {
    case INPUT_EVENT_BUTTON_PRESS:
        ESP_LOGI(TAG, "BTN -> ENTER (pressed)");
        led_indicator_red();
        hid_key_down(KEY_ENTER);
        break;

    case INPUT_EVENT_BUTTON_RELEASE:
        ESP_LOGI(TAG, "BTN -> ENTER (released)");
        led_indicator_off();
        hid_key_up(KEY_ENTER);
        break;

    case INPUT_EVENT_ENC1_CW:
        ESP_LOGI(TAG, "ENC1 CW -> UP");
        hid_key_pulse(KEY_UP_ARROW);
        break;

    case INPUT_EVENT_ENC1_CCW:
        ESP_LOGI(TAG, "ENC1 CCW -> DOWN");
        hid_key_pulse(KEY_DOWN_ARROW);
        break;

    case INPUT_EVENT_ENC1_SW_PRESS:
        ESP_LOGI(TAG, "ENC1 SW -> F1 (pressed)");
        hid_key_down(KEY_F1);
        break;

    case INPUT_EVENT_ENC1_SW_RELEASE:
        ESP_LOGI(TAG, "ENC1 SW -> F1 (released)");
        hid_key_up(KEY_F1);
        break;

    case INPUT_EVENT_ENC2_CW:
        ESP_LOGI(TAG, "ENC2 CW -> RIGHT");
        hid_key_pulse(KEY_RIGHT_ARROW);
        break;

    case INPUT_EVENT_ENC2_CCW:
        ESP_LOGI(TAG, "ENC2 CCW -> LEFT");
        hid_key_pulse(KEY_LEFT_ARROW);
        break;

    case INPUT_EVENT_ENC2_SW_PRESS:
        ESP_LOGI(TAG, "ENC2 SW -> F2 (pressed)");
        hid_key_down(KEY_F2);
        break;

    case INPUT_EVENT_ENC2_SW_RELEASE:
        ESP_LOGI(TAG, "ENC2 SW -> F2 (released)");
        hid_key_up(KEY_F2);
        break;

    default:
        break;
    }
}

// NFC tag scan callback — typed protocol depends on what's on the tag:
//   - NDEF Text payload present → "<payload>\n" (raw payload, no prefix)
//   - No NDEF / unparseable     → "NFC:<UID>\n" (UID fallback for diagnostics)
static void on_nfc_tag(const char *payload, const char *uid_hex)
{
    char buf[64];
    int n;
    if (payload != NULL) {
        n = snprintf(buf, sizeof(buf), "%s\n", payload);
    } else {
        n = snprintf(buf, sizeof(buf), "NFC:%s\n", uid_hex);
    }
    if (n <= 0 || n >= (int)sizeof(buf)) {
        ESP_LOGW(TAG, "NFC HID buffer overflow, uid=%s", uid_hex);
        return;
    }

    // Strip trailing newline for cleaner log line
    char log_buf[64];
    strncpy(log_buf, buf, sizeof(log_buf) - 1);
    log_buf[sizeof(log_buf) - 1] = '\0';
    size_t L = strlen(log_buf);
    if (L > 0 && log_buf[L - 1] == '\n') log_buf[L - 1] = '\0';
    ESP_LOGI(TAG, "NFC -> typing '%s\\n'", log_buf);

    led_indicator_blue();
    send_string(buf);
    led_indicator_off();
}

/********* Main Application ***************/

void app_main(void)
{
    ESP_LOGI(TAG, "Cosmo Pager Radio - USB HID Keyboard");

    // HID state mutex must exist before any task can submit a report.
    s_hid_mutex = xSemaphoreCreateMutex();
    if (s_hid_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create HID mutex");
        abort();
    }

    if (xTaskCreate(hid_report_retry_task, "hid_report_retry", 2 * 1024, NULL,
                    configMAX_PRIORITIES - 4, &s_hid_report_retry_task) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to create HID report retry task");
        abort();
    }

    // Initialize USB
    ESP_LOGI(TAG, "USB initialization");
    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();

    tusb_cfg.descriptor.device = NULL;
    tusb_cfg.descriptor.full_speed_config = hid_configuration_descriptor;
    tusb_cfg.descriptor.string = hid_string_descriptor;
    tusb_cfg.descriptor.string_count = sizeof(hid_string_descriptor) / sizeof(hid_string_descriptor[0]);
#if (TUD_OPT_HIGH_SPEED)
    tusb_cfg.descriptor.high_speed_config = hid_configuration_descriptor;
#endif

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "USB initialization DONE");

    // Initialize LED indicator
    ESP_ERROR_CHECK(led_indicator_init());

    // Initialize input handler
    ESP_ERROR_CHECK(input_handler_init());
    input_handler_set_callback(on_input_event);
    input_handler_start();

    // Initialize NFC handler (RC522 on SPI2). Optional peripheral — if the
    // module is disconnected or unresponsive, log and continue so input/HID
    // still work. Aborting here would leave the device in a reboot loop.
    esp_err_t nfc_err = nfc_handler_init();
    if (nfc_err == ESP_OK) {
        nfc_handler_set_callback(on_nfc_tag);
        nfc_err = nfc_handler_start();
        if (nfc_err != ESP_OK) {
            ESP_LOGW(TAG, "NFC start failed (0x%x) — retrying in background", nfc_err);
        }
    } else {
        ESP_LOGW(TAG, "NFC init failed (0x%x) — continuing without NFC", nfc_err);
    }

    // Brief startup indication
    led_indicator_green();
    vTaskDelay(pdMS_TO_TICKS(200));
    led_indicator_off();

    // Main loop - monitor USB connection status
    bool was_mounted = false;
    while (1) {
        bool is_mounted = tud_mounted();

        if (is_mounted && !was_mounted) {
            ESP_LOGI(TAG, "USB connected");
            led_indicator_blue();
            vTaskDelay(pdMS_TO_TICKS(100));
            led_indicator_off();
        } else if (!is_mounted && was_mounted) {
            ESP_LOGW(TAG, "USB disconnected");
        }

        was_mounted = is_mounted;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
