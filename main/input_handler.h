/*
 * Input Handler Module
 * GPIO interrupt-based input handling for rotary encoders and button
 * Enables power management through event-driven architecture
 */

#ifndef _INPUT_HANDLER_H_
#define _INPUT_HANDLER_H_

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Input event types
typedef enum {
    INPUT_EVENT_NONE = 0,
    INPUT_EVENT_BUTTON_PRESS,
    INPUT_EVENT_BUTTON_RELEASE,
    INPUT_EVENT_ENC1_CW,        // Encoder 1 clockwise
    INPUT_EVENT_ENC1_CCW,       // Encoder 1 counter-clockwise
    INPUT_EVENT_ENC2_CW,        // Encoder 2 clockwise
    INPUT_EVENT_ENC2_CCW,       // Encoder 2 counter-clockwise
} input_event_type_t;

// Input event structure
typedef struct {
    input_event_type_t type;
    uint32_t timestamp;         // Tick count when event occurred
} input_event_t;

// Callback function type for input events
typedef void (*input_event_callback_t)(const input_event_t *event);

/**
 * Initialize the input handler
 * Sets up GPIO interrupts and event queue
 *
 * @return ESP_OK on success
 */
esp_err_t input_handler_init(void);

/**
 * Register callback for input events
 * Callback is invoked from the input processing task
 *
 * @param callback Function to call on input events
 */
void input_handler_set_callback(input_event_callback_t callback);

/**
 * Start the input processing task
 * Begins monitoring GPIO interrupts and dispatching events
 */
void input_handler_start(void);

/**
 * Stop the input processing task
 */
void input_handler_stop(void);

/**
 * Check if input handler is running
 *
 * @return true if running
 */
bool input_handler_is_running(void);

/**
 * Get time since last input activity (for power management)
 *
 * @return Milliseconds since last input
 */
uint32_t input_handler_get_idle_time_ms(void);

/**
 * Reset the idle timer (call on any activity)
 */
void input_handler_reset_idle_timer(void);

#ifdef __cplusplus
}
#endif

#endif /* _INPUT_HANDLER_H_ */
