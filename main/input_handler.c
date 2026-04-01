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

// GPIO pin definitions (ESP32-S3 SuperMini)
#define GPIO_BUTTON_A   1   // GP1 - Button (redundant pair)
#define GPIO_BUTTON_B   2   // GP2 - Button (redundant pair)
#define GPIO_ENC1_CLK   4   // GP4
#define GPIO_ENC1_DT    3   // GP3
#define GPIO_ENC2_CLK   5   // GP5
#define GPIO_ENC2_DT    6   // GP6

// All input GPIOs as a mask
#define GPIO_INPUT_MASK ((1ULL << GPIO_BUTTON_A) | (1ULL << GPIO_BUTTON_B) | \
                         (1ULL << GPIO_ENC1_CLK) | (1ULL << GPIO_ENC1_DT) | \
                         (1ULL << GPIO_ENC2_CLK) | (1ULL << GPIO_ENC2_DT))

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
static int s_btn_last = 1;  // Pull-up, 1 = released

// Button press timestamp for force restart detection
static int64_t s_btn_press_time = 0;

// Last ISR time for each GPIO (for software debounce, indexed by gpio_num)
static int64_t s_last_isr_time[8] = {0};

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
    s_enc1_state = (gpio_get_level(GPIO_ENC1_CLK) << 1) | gpio_get_level(GPIO_ENC1_DT);
    s_enc2_state = (gpio_get_level(GPIO_ENC2_CLK) << 1) | gpio_get_level(GPIO_ENC2_DT);
    // Redundant button: pressed if either pin is low
    s_btn_last = gpio_get_level(GPIO_BUTTON_A) & gpio_get_level(GPIO_BUTTON_B);

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
            if (evt.gpio_num == GPIO_BUTTON_A || evt.gpio_num == GPIO_BUTTON_B) {
                // Redundant button: read both pins, pressed if either is low
                int btn_level = gpio_get_level(GPIO_BUTTON_A) & gpio_get_level(GPIO_BUTTON_B);
                if (btn_level == 0 && s_btn_last == 1) {
                    // Button pressed - record timestamp
                    s_btn_press_time = esp_timer_get_time();
                    input_evt.type = INPUT_EVENT_BUTTON_PRESS;
                    ESP_LOGD(TAG, "Button pressed (GPIO%d)", evt.gpio_num);
                } else if (btn_level == 1 && s_btn_last == 0) {
                    // Button released - clear timestamp
                    s_btn_press_time = 0;
                    input_evt.type = INPUT_EVENT_BUTTON_RELEASE;
                    ESP_LOGD(TAG, "Button released (GPIO%d)", evt.gpio_num);
                }
                s_btn_last = btn_level;
            }
            else if (evt.gpio_num == GPIO_ENC1_CLK || evt.gpio_num == GPIO_ENC1_DT) {
                // Read both pins for encoder 1
                uint8_t clk = gpio_get_level(GPIO_ENC1_CLK);
                uint8_t dt = gpio_get_level(GPIO_ENC1_DT);
                input_evt.type = process_encoder(clk, dt, &s_enc1_state, true);
                if (input_evt.type == INPUT_EVENT_ENC1_CW) {
                    ESP_LOGD(TAG, "ENC1 CW");
                } else if (input_evt.type == INPUT_EVENT_ENC1_CCW) {
                    ESP_LOGD(TAG, "ENC1 CCW");
                }
            }
            else if (evt.gpio_num == GPIO_ENC2_CLK || evt.gpio_num == GPIO_ENC2_DT) {
                // Read both pins for encoder 2
                uint8_t clk = gpio_get_level(GPIO_ENC2_CLK);
                uint8_t dt = gpio_get_level(GPIO_ENC2_DT);
                input_evt.type = process_encoder(clk, dt, &s_enc2_state, false);
                if (input_evt.type == INPUT_EVENT_ENC2_CW) {
                    ESP_LOGD(TAG, "ENC2 CW");
                } else if (input_evt.type == INPUT_EVENT_ENC2_CCW) {
                    ESP_LOGD(TAG, "ENC2 CCW");
                }
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
    gpio_isr_handler_add(GPIO_BUTTON_A, gpio_isr_handler, (void*)GPIO_BUTTON_A);
    gpio_isr_handler_add(GPIO_BUTTON_B, gpio_isr_handler, (void*)GPIO_BUTTON_B);
    gpio_isr_handler_add(GPIO_ENC1_CLK, gpio_isr_handler, (void*)GPIO_ENC1_CLK);
    gpio_isr_handler_add(GPIO_ENC1_DT, gpio_isr_handler, (void*)GPIO_ENC1_DT);
    gpio_isr_handler_add(GPIO_ENC2_CLK, gpio_isr_handler, (void*)GPIO_ENC2_CLK);
    gpio_isr_handler_add(GPIO_ENC2_DT, gpio_isr_handler, (void*)GPIO_ENC2_DT);

    // Now enable interrupts on each GPIO
    gpio_set_intr_type(GPIO_BUTTON_A, GPIO_INTR_ANYEDGE);
    gpio_set_intr_type(GPIO_BUTTON_B, GPIO_INTR_ANYEDGE);
    gpio_set_intr_type(GPIO_ENC1_CLK, GPIO_INTR_ANYEDGE);
    gpio_set_intr_type(GPIO_ENC1_DT, GPIO_INTR_ANYEDGE);
    gpio_set_intr_type(GPIO_ENC2_CLK, GPIO_INTR_ANYEDGE);
    gpio_set_intr_type(GPIO_ENC2_DT, GPIO_INTR_ANYEDGE);

    ESP_LOGI(TAG, "Input handler initialized (GPIO interrupts)");
    ESP_LOGI(TAG, "  Button: GPIO%d+%d (redundant), ENC1: GPIO%d/%d, ENC2: GPIO%d/%d",
             GPIO_BUTTON_A, GPIO_BUTTON_B, GPIO_ENC1_CLK, GPIO_ENC1_DT, GPIO_ENC2_CLK, GPIO_ENC2_DT);

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
