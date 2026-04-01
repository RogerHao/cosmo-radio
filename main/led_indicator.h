/*
 * LED Indicator Module
 * Controls WS2812 RGB LED for visual feedback
 */

#ifndef _LED_INDICATOR_H_
#define _LED_INDICATOR_H_

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the LED indicator
 * Sets up RMT peripheral for WS2812 control on GP7
 *
 * @return ESP_OK on success
 */
esp_err_t led_indicator_init(void);

/**
 * Set LED color (RGB)
 *
 * @param red   Red component (0-255)
 * @param green Green component (0-255)
 * @param blue  Blue component (0-255)
 */
void led_indicator_set_rgb(uint8_t red, uint8_t green, uint8_t blue);

/**
 * Turn LED off
 */
void led_indicator_off(void);

/**
 * Set LED to red (for button press indication)
 */
void led_indicator_red(void);

/**
 * Set LED to green
 */
void led_indicator_green(void);

/**
 * Set LED to blue
 */
void led_indicator_blue(void);

#ifdef __cplusplus
}
#endif

#endif /* _LED_INDICATOR_H_ */
