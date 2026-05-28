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

// Encoder A/B debounce in microseconds. EC11 A/B lines also have 10k + 10nF RC.
#define ENCODER_DEBOUNCE_US 2000

// Active-low switch debounce. Switch release safety is handled by polling the
// physical GPIO level, so a missed edge cannot leave HID stuck pressed forever.
#define SWITCH_DEBOUNCE_US 10000

// Periodic input reconciliation interval. Keep this at least one FreeRTOS tick;
// otherwise pdMS_TO_TICKS() can round to 0 and create a busy loop.
#define INPUT_POLL_MS 10

// Long press duration for forced restart (15 seconds in microseconds)
#define FORCE_RESTART_HOLD_US (15 * 1000000)

// Internal GPIO event for ISR -> task communication
typedef struct {
    uint8_t gpio_num;
    uint8_t level;
    int64_t timestamp;
} gpio_isr_event_t;

typedef struct {
    uint8_t gpio_num;
    int stable_level;
    int candidate_level;
    int64_t candidate_since_us;
    input_event_type_t press_event;
    input_event_type_t release_event;
    const char *name;
    bool tracks_force_restart;
} active_low_switch_t;

// Module state
static QueueHandle_t s_gpio_evt_queue = NULL;
static TaskHandle_t s_input_task = NULL;
static input_event_callback_t s_callback = NULL;
static volatile int64_t s_last_activity_time = 0;
static volatile bool s_running = false;
static volatile uint32_t s_isr_queue_drops = 0;

// Encoder state tracking (for detent detection)
static uint8_t s_enc1_state = 0;
static uint8_t s_enc2_state = 0;

static active_low_switch_t s_button = {
    .gpio_num = GPIO_BUTTON,
    .stable_level = 1,
    .candidate_level = 1,
    .press_event = INPUT_EVENT_BUTTON_PRESS,
    .release_event = INPUT_EVENT_BUTTON_RELEASE,
    .name = "Button",
    .tracks_force_restart = true,
};

static active_low_switch_t s_enc1_sw = {
    .gpio_num = GPIO_ENC1_SW,
    .stable_level = 1,
    .candidate_level = 1,
    .press_event = INPUT_EVENT_ENC1_SW_PRESS,
    .release_event = INPUT_EVENT_ENC1_SW_RELEASE,
    .name = "ENC1 SW",
    .tracks_force_restart = false,
};

static active_low_switch_t s_enc2_sw = {
    .gpio_num = GPIO_ENC2_SW,
    .stable_level = 1,
    .candidate_level = 1,
    .press_event = INPUT_EVENT_ENC2_SW_PRESS,
    .release_event = INPUT_EVENT_ENC2_SW_RELEASE,
    .name = "ENC2 SW",
    .tracks_force_restart = false,
};

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
    if (xQueueSendFromISR(s_gpio_evt_queue, &evt, &xHigherPriorityTaskWoken) != pdTRUE) {
        s_isr_queue_drops++;
    }

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

static void dispatch_input_event(input_event_type_t type, int64_t now_us)
{
    if (type == INPUT_EVENT_NONE) {
        return;
    }

    s_last_activity_time = now_us;

    if (s_callback != NULL) {
        input_event_t input_evt = {
            .type = type,
            .timestamp = xTaskGetTickCount(),
        };
        s_callback(&input_evt);
    }
}

static void init_switch_state(active_low_switch_t *sw, int64_t now_us)
{
    int level = gpio_get_level(sw->gpio_num);
    sw->stable_level = level;
    sw->candidate_level = level;
    sw->candidate_since_us = now_us;
}

static void update_switch_state(active_low_switch_t *sw, int64_t now_us)
{
    int level = gpio_get_level(sw->gpio_num);

    if (level != sw->candidate_level) {
        sw->candidate_level = level;
        sw->candidate_since_us = now_us;
        return;
    }

    if (level == sw->stable_level || (now_us - sw->candidate_since_us) < SWITCH_DEBOUNCE_US) {
        return;
    }

    sw->stable_level = level;

    if (sw->tracks_force_restart) {
        s_btn_press_time = (level == 0) ? now_us : 0;
    }

    ESP_LOGD(TAG, "%s %s", sw->name, level == 0 ? "pressed" : "released");
    dispatch_input_event(level == 0 ? sw->press_event : sw->release_event, now_us);
}

static void update_all_switches(int64_t now_us)
{
    update_switch_state(&s_button, now_us);
    update_switch_state(&s_enc1_sw, now_us);
    update_switch_state(&s_enc2_sw, now_us);
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
    uint32_t logged_isr_queue_drops = 0;

    ESP_LOGI(TAG, "Input task started");

    // Initialize encoder states
    s_enc1_state = (gpio_get_level(GPIO_ENC1_A) << 1) | gpio_get_level(GPIO_ENC1_B);
    s_enc2_state = (gpio_get_level(GPIO_ENC2_A) << 1) | gpio_get_level(GPIO_ENC2_B);

    int64_t now_us = esp_timer_get_time();
    init_switch_state(&s_button, now_us);
    init_switch_state(&s_enc1_sw, now_us);
    init_switch_state(&s_enc2_sw, now_us);
    s_btn_press_time = (s_button.stable_level == 0) ? now_us : 0;

    s_running = true;
    s_last_activity_time = now_us;

    while (s_running) {
        bool has_gpio_event = xQueueReceive(s_gpio_evt_queue, &evt, pdMS_TO_TICKS(INPUT_POLL_MS)) == pdTRUE;
        int64_t now = esp_timer_get_time();

        uint32_t isr_queue_drops = s_isr_queue_drops;
        if (isr_queue_drops != logged_isr_queue_drops) {
            ESP_LOGW(TAG, "GPIO ISR queue dropped %lu events total", (unsigned long)isr_queue_drops);
            logged_isr_queue_drops = isr_queue_drops;
        }

        if (has_gpio_event && evt.gpio_num <= GPIO_MAX_USED) {
            input_event_type_t event_type = INPUT_EVENT_NONE;

            if (evt.gpio_num == GPIO_ENC1_A || evt.gpio_num == GPIO_ENC1_B) {
                if ((now - s_last_isr_time[evt.gpio_num]) < ENCODER_DEBOUNCE_US) {
                    goto reconcile_switches;
                }
                s_last_isr_time[evt.gpio_num] = now;

                uint8_t a = gpio_get_level(GPIO_ENC1_A);
                uint8_t b = gpio_get_level(GPIO_ENC1_B);
                event_type = process_encoder(a, b, &s_enc1_state, true);
                if (event_type == INPUT_EVENT_ENC1_CW) {
                    ESP_LOGD(TAG, "ENC1 CW");
                } else if (event_type == INPUT_EVENT_ENC1_CCW) {
                    ESP_LOGD(TAG, "ENC1 CCW");
                }
            }
            else if (evt.gpio_num == GPIO_ENC2_A || evt.gpio_num == GPIO_ENC2_B) {
                if ((now - s_last_isr_time[evt.gpio_num]) < ENCODER_DEBOUNCE_US) {
                    goto reconcile_switches;
                }
                s_last_isr_time[evt.gpio_num] = now;

                uint8_t a = gpio_get_level(GPIO_ENC2_A);
                uint8_t b = gpio_get_level(GPIO_ENC2_B);
                event_type = process_encoder(a, b, &s_enc2_state, false);
                if (event_type == INPUT_EVENT_ENC2_CW) {
                    ESP_LOGD(TAG, "ENC2 CW");
                } else if (event_type == INPUT_EVENT_ENC2_CCW) {
                    ESP_LOGD(TAG, "ENC2 CCW");
                }
            }

            dispatch_input_event(event_type, now);
        }

reconcile_switches:
        update_all_switches(now);

        // Check for force restart: button held for 15 seconds
        if (s_btn_press_time != 0) {
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
