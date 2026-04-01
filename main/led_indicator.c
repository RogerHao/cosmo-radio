/*
 * LED Indicator Module Implementation
 * Uses ESP-IDF led_strip component for WS2812 control
 */

#include "led_indicator.h"
#include "led_strip.h"
#include "esp_log.h"

static const char *TAG = "LED";

// WS2812 LED on GP7 (ESP32-S3 SuperMini)
#define LED_GPIO    7

// Number of LEDs in strip (single LED)
#define LED_COUNT   1

// LED strip handle
static led_strip_handle_t s_led_strip = NULL;

esp_err_t led_indicator_init(void)
{
    if (s_led_strip != NULL) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    // LED strip configuration
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = LED_COUNT,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags = {
            .invert_out = false,
        },
    };

    // RMT configuration for LED strip
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,  // 10MHz
        .mem_block_symbols = 64,
        .flags = {
            .with_dma = false,
        },
    };

    // Create LED strip object
    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED strip: %s", esp_err_to_name(ret));
        return ret;
    }

    // Start with LED off
    led_indicator_off();

    ESP_LOGI(TAG, "LED indicator initialized on GPIO%d", LED_GPIO);
    return ESP_OK;
}

void led_indicator_set_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
    if (s_led_strip == NULL) {
        return;
    }

    led_strip_set_pixel(s_led_strip, 0, red, green, blue);
    led_strip_refresh(s_led_strip);
}

void led_indicator_off(void)
{
    if (s_led_strip == NULL) {
        return;
    }

    led_strip_clear(s_led_strip);
}

void led_indicator_red(void)
{
    led_indicator_set_rgb(255, 0, 0);
}

void led_indicator_green(void)
{
    led_indicator_set_rgb(0, 255, 0);
}

void led_indicator_blue(void)
{
    led_indicator_set_rgb(0, 0, 255);
}
