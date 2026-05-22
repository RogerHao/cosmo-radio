/*
 * Input Handler Module Implementation
 * Uses GPIO interrupts + FreeRTOS queue for power-efficient input handling
 */

#include <string.h>
#include "input_handler.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "esp_system.h"  // for esp_restart()

static const char *TAG = "INPUT";

// Pin map: CLAUDE.md "GPIO Pin Assignments" is the single source of truth.
// ENC2 uses a non-obvious GPIO-to-role assignment to compensate for the PCB r1.0
// J4 mirrored pinout and to keep both knobs' perceived direction symmetric — see
// "PCB r1.0 EC11-R 接线 walkaround" in CLAUDE.md before "fixing" these numbers.
#define GPIO_BUTTON     1
#define GPIO_ENC1_A     42
#define GPIO_ENC1_B     41
#define GPIO_ENC1_SW    40
#define GPIO_ENC2_A     18
#define GPIO_ENC2_B     8
#define GPIO_ENC2_SW    17

// All input GPIOs as a mask
#define GPIO_INPUT_MASK ((1ULL << GPIO_BUTTON) | \
                         (1ULL << GPIO_ENC1_A) | (1ULL << GPIO_ENC1_B) | (1ULL << GPIO_ENC1_SW) | \
                         (1ULL << GPIO_ENC2_A) | (1ULL << GPIO_ENC2_B) | (1ULL << GPIO_ENC2_SW))

// Largest GPIO number used (for debounce table sizing)
#define GPIO_MAX_USED   43

// Event queue size (should handle burst of encoder events)
#define EVENT_QUEUE_SIZE 32

// Debounce time in microseconds
#define DEBOUNCE_US 2000

// Long press duration for forced restart (15 seconds in microseconds)
#define FORCE_RESTART_HOLD_US (15 * 1000000)

// Internal GPIO event for ISR -> task communication
typedef struct {
    uint8_t gpio_num;
    uint8_t level;
    int64_t timestamp;
} gpio_isr_event_t;

// Module state
static QueueHandle_t s_gpio_evt_queue = NULL;
static TaskHandle_t s_input_task = NULL;
static input_event_callback_t s_callback = NULL;
static volatile int64_t s_last_activity_time = 0;
static volatile bool s_running = false;

// Encoder state tracking (for detent detection)
static uint8_t s_enc1_state = 0;
static uint8_t s_enc2_state = 0;
static int s_btn_last = 1;          // Pull-up, 1 = released
static int s_enc1_sw_last = 1;
static int s_enc2_sw_last = 1;

// Button press timestamp for force restart detection
static int64_t s_btn_press_time = 0;

// Last ISR time for each GPIO (for software debounce, indexed by gpio_num).
// Sized to cover the largest GPIO number we attach an ISR to.
static int64_t s_last_isr_time[GPIO_MAX_USED + 1] = {0};

// ISR handler - minimal work, just queue the event
// Note: Keep this as simple as possible to avoid watchdog issues
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;

    // Don't call esp_timer_get_time() in ISR - can cause watchdog issues
    // Debounce will be handled in the task instead
    gpio_isr_event_t evt = {
        .gpio_num = gpio_num,
        .level = gpio_get_level(gpio_num),
        .timestamp = 0  // Will be set in task
    };

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(s_gpio_evt_queue, &evt, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

// Process encoder state change, return direction
// Only triggers at detent position (state 11 -> next state)
static input_event_type_t process_encoder(uint8_t new_clk, uint8_t new_dt,
                                          uint8_t *state, bool is_enc1)
{
    uint8_t new_state = (new_clk << 1) | new_dt;

    if (new_state == *state) {
        return INPUT_EVENT_NONE;
    }

    input_event_type_t result = INPUT_EVENT_NONE;

    // Trigger on leaving detent (state 11)
    if (*state == 0b11) {
        if (new_state == 0b10) {
            result = is_enc1 ? INPUT_EVENT_ENC1_CW : INPUT_EVENT_ENC2_CW;
        } else if (new_state == 0b01) {
            result = is_enc1 ? INPUT_EVENT_ENC1_CCW : INPUT_EVENT_ENC2_CCW;
        }
    }

    *state = new_state;
    return result;
}

// Input processing task
static void input_handler_task(void *arg)
{
    gpio_isr_event_t evt;
    input_event_t input_evt;

    ESP_LOGI(TAG, "Input task started");

    // Initialize encoder states
    s_enc1_state = (gpio_get_level(GPIO_ENC1_A) << 1) | gpio_get_level(GPIO_ENC1_B);
    s_enc2_state = (gpio_get_level(GPIO_ENC2_A) << 1) | gpio_get_level(GPIO_ENC2_B);
    s_btn_last = gpio_get_level(GPIO_BUTTON);
    s_enc1_sw_last = gpio_get_level(GPIO_ENC1_SW);
    s_enc2_sw_last = gpio_get_level(GPIO_ENC2_SW);

    s_running = true;
    s_last_activity_time = esp_timer_get_time();

    while (s_running) {
        // Wait for GPIO events with timeout (allows periodic wakeup for housekeeping)
        if (xQueueReceive(s_gpio_evt_queue, &evt, pdMS_TO_TICKS(100))) {
            int64_t now = esp_timer_get_time();

            // Software debounce: ignore if too soon after last event on this pin
            if ((now - s_last_isr_time[evt.gpio_num]) < DEBOUNCE_US) {
                continue;  // Skip this event
            }
            s_last_isr_time[evt.gpio_num] = now;

            input_evt.type = INPUT_EVENT_NONE;
            input_evt.timestamp = xTaskGetTickCount();

            // Update last activity time
            s_last_activity_time = now;

            // Process based on which GPIO triggered
            if (evt.gpio_num == GPIO_BUTTON) {
                int btn_level = gpio_get_level(GPIO_BUTTON);
                if (btn_level == 0 && s_btn_last == 1) {
                    s_btn_press_time = esp_timer_get_time();
                    input_evt.type = INPUT_EVENT_BUTTON_PRESS;
                    ESP_LOGD(TAG, "Button pressed");
                } else if (btn_level == 1 && s_btn_last == 0) {
                    s_btn_press_time = 0;
                    input_evt.type = INPUT_EVENT_BUTTON_RELEASE;
                    ESP_LOGD(TAG, "Button released");
                }
                s_btn_last = btn_level;
            }
            else if (evt.gpio_num == GPIO_ENC1_A || evt.gpio_num == GPIO_ENC1_B) {
                uint8_t a = gpio_get_level(GPIO_ENC1_A);
                uint8_t b = gpio_get_level(GPIO_ENC1_B);
                input_evt.type = process_encoder(a, b, &s_enc1_state, true);
                if (input_evt.type == INPUT_EVENT_ENC1_CW) {
                    ESP_LOGD(TAG, "ENC1 CW");
                } else if (input_evt.type == INPUT_EVENT_ENC1_CCW) {
                    ESP_LOGD(TAG, "ENC1 CCW");
                }
            }
            else if (evt.gpio_num == GPIO_ENC1_SW) {
                int sw_level = gpio_get_level(GPIO_ENC1_SW);
                if (sw_level == 0 && s_enc1_sw_last == 1) {
                    input_evt.type = INPUT_EVENT_ENC1_SW_PRESS;
                    ESP_LOGD(TAG, "ENC1 SW pressed");
                } else if (sw_level == 1 && s_enc1_sw_last == 0) {
                    input_evt.type = INPUT_EVENT_ENC1_SW_RELEASE;
                    ESP_LOGD(TAG, "ENC1 SW released");
                }
                s_enc1_sw_last = sw_level;
            }
            else if (evt.gpio_num == GPIO_ENC2_A || evt.gpio_num == GPIO_ENC2_B) {
                uint8_t a = gpio_get_level(GPIO_ENC2_A);
                uint8_t b = gpio_get_level(GPIO_ENC2_B);
                input_evt.type = process_encoder(a, b, &s_enc2_state, false);
                if (input_evt.type == INPUT_EVENT_ENC2_CW) {
                    ESP_LOGD(TAG, "ENC2 CW");
                } else if (input_evt.type == INPUT_EVENT_ENC2_CCW) {
                    ESP_LOGD(TAG, "ENC2 CCW");
                }
            }
            else if (evt.gpio_num == GPIO_ENC2_SW) {
                int sw_level = gpio_get_level(GPIO_ENC2_SW);
                if (sw_level == 0 && s_enc2_sw_last == 1) {
                    input_evt.type = INPUT_EVENT_ENC2_SW_PRESS;
                    ESP_LOGD(TAG, "ENC2 SW pressed");
                } else if (sw_level == 1 && s_enc2_sw_last == 0) {
                    input_evt.type = INPUT_EVENT_ENC2_SW_RELEASE;
                    ESP_LOGD(TAG, "ENC2 SW released");
                }
                s_enc2_sw_last = sw_level;
            }

            // Dispatch event if valid
            if (input_evt.type != INPUT_EVENT_NONE && s_callback != NULL) {
                s_callback(&input_evt);
            }
        }

        // Check for force restart: button held for 15 seconds
        if (s_btn_press_time != 0) {
            int64_t now = esp_timer_get_time();
            int64_t hold_duration = now - s_btn_press_time;
            if (hold_duration >= FORCE_RESTART_HOLD_US) {
                ESP_LOGW(TAG, "Button held for 15 seconds - forcing restart!");
                // Small delay to ensure log is flushed
                vTaskDelay(pdMS_TO_TICKS(100));
                esp_restart();
            }
        }
    }

    ESP_LOGI(TAG, "Input task stopped");
    vTaskDelete(NULL);
}

esp_err_t input_handler_init(void)
{
    if (s_gpio_evt_queue != NULL) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    // Create event queue
    s_gpio_evt_queue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(gpio_isr_event_t));
    if (s_gpio_evt_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return ESP_ERR_NO_MEM;
    }

    // Configure GPIO pins WITHOUT interrupts first
    gpio_config_t io_conf = {
        .pin_bit_mask = GPIO_INPUT_MASK,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,  // Disable interrupts during setup
    };
    gpio_config(&io_conf);

    // Install GPIO ISR service
    esp_err_t ret = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        // ESP_ERR_INVALID_STATE means already installed, which is OK
        ESP_LOGE(TAG, "Failed to install ISR service: %d", ret);
        return ret;
    }

    // Add ISR handlers for each GPIO (interrupts still disabled)
    gpio_isr_handler_add(GPIO_BUTTON,  gpio_isr_handler, (void*)GPIO_BUTTON);
    gpio_isr_handler_add(GPIO_ENC1_A,  gpio_isr_handler, (void*)GPIO_ENC1_A);
    gpio_isr_handler_add(GPIO_ENC1_B,  gpio_isr_handler, (void*)GPIO_ENC1_B);
    gpio_isr_handler_add(GPIO_ENC1_SW, gpio_isr_handler, (void*)GPIO_ENC1_SW);
    gpio_isr_handler_add(GPIO_ENC2_A,  gpio_isr_handler, (void*)GPIO_ENC2_A);
    gpio_isr_handler_add(GPIO_ENC2_B,  gpio_isr_handler, (void*)GPIO_ENC2_B);
    gpio_isr_handler_add(GPIO_ENC2_SW, gpio_isr_handler, (void*)GPIO_ENC2_SW);

    // Now enable interrupts on each GPIO
    gpio_set_intr_type(GPIO_BUTTON,  GPIO_INTR_ANYEDGE);
    gpio_set_intr_type(GPIO_ENC1_A,  GPIO_INTR_ANYEDGE);
    gpio_set_intr_type(GPIO_ENC1_B,  GPIO_INTR_ANYEDGE);
    gpio_set_intr_type(GPIO_ENC1_SW, GPIO_INTR_ANYEDGE);
    gpio_set_intr_type(GPIO_ENC2_A,  GPIO_INTR_ANYEDGE);
    gpio_set_intr_type(GPIO_ENC2_B,  GPIO_INTR_ANYEDGE);
    gpio_set_intr_type(GPIO_ENC2_SW, GPIO_INTR_ANYEDGE);

    ESP_LOGI(TAG, "Input handler initialized (V4 — DevKitC N16R8)");
    ESP_LOGI(TAG, "  Button: GPIO%d", GPIO_BUTTON);
    ESP_LOGI(TAG, "  ENC1 (left):  A=GPIO%d B=GPIO%d SW=GPIO%d",
             GPIO_ENC1_A, GPIO_ENC1_B, GPIO_ENC1_SW);
    ESP_LOGI(TAG, "  ENC2 (right): A=GPIO%d B=GPIO%d SW=GPIO%d",
             GPIO_ENC2_A, GPIO_ENC2_B, GPIO_ENC2_SW);

    return ESP_OK;
}

void input_handler_set_callback(input_event_callback_t callback)
{
    s_callback = callback;
}

void input_handler_start(void)
{
    if (s_input_task != NULL) {
        ESP_LOGW(TAG, "Already running");
        return;
    }

    xTaskCreate(input_handler_task, "input_handler", 3 * 1024, NULL,
                configMAX_PRIORITIES - 3, &s_input_task);
}

void input_handler_stop(void)
{
    if (s_input_task == NULL) {
        return;
    }

    s_running = false;
    // Task will delete itself when it exits the loop

    // Wait a bit for task to clean up
    vTaskDelay(pdMS_TO_TICKS(200));
    s_input_task = NULL;
}

bool input_handler_is_running(void)
{
    return s_running;
}

uint32_t input_handler_get_idle_time_ms(void)
{
    int64_t now = esp_timer_get_time();
    int64_t idle_us = now - s_last_activity_time;
    return (uint32_t)(idle_us / 1000);
}

void input_handler_reset_idle_timer(void)
{
    s_last_activity_time = esp_timer_get_time();
}
