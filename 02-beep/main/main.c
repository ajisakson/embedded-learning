#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "esp_log.h"

#define TRIG GPIO_NUM_5
#define BOARD_LED GPIO_NUM_2
#define ECHO GPIO_NUM_18
#define BUZZER GPIO_NUM_4
#define BUTTON GPIO_NUM_13 // mute toggle: button between this pin and GND

#define BEEP_RANGE_CM 400.0f  // closer than this: start beeping
#define ECHO_TIMEOUT_US 30000 // 30 ms: longest echo we wait for (~5 m round trip)
#define DEBOUNCE_US 200000    // ignore button edges within 200 ms of the last

static const char *TAG = "beep";

// Flipped by the button interrupt. When true, the loop goes quiet and the
// board LED sits solid ON. `volatile` because the ISR and main loop share it.
static volatile bool g_paused = false;

// Runs on the button's falling edge (press pulls the pin to GND). Debounces in
// software by ignoring edges that arrive too soon after the last accepted one.
static void IRAM_ATTR button_isr(void *arg)
{
    static int64_t last_press_us = 0;
    int64_t now = esp_timer_get_time();
    if (now - last_press_us < DEBOUNCE_US)
        return;
    last_press_us = now;
    g_paused = !g_paused;
}

// Send one trigger pulse, time the echo, and return distance in cm.
// Returns -1.0 if no echo arrives within ECHO_TIMEOUT_US (out of range).
static float measure_distance_cm(void)
{
    // 1. 10 µs trigger pulse
    gpio_set_level(TRIG, 1);
    esp_rom_delay_us(10);
    gpio_set_level(TRIG, 0);

    // 2. wait for ECHO to go HIGH (with timeout so we never hang)
    int64_t wait_start = esp_timer_get_time();
    while (gpio_get_level(ECHO) == 0)
    {
        if (esp_timer_get_time() - wait_start > ECHO_TIMEOUT_US)
            return -1.0f;
    }

    // 3. time how long ECHO stays HIGH (with the same timeout)
    int64_t echo_start = esp_timer_get_time();
    while (gpio_get_level(ECHO) == 1)
    {
        if (esp_timer_get_time() - echo_start > ECHO_TIMEOUT_US)
            return -1.0f;
    }
    int64_t pulse_us = esp_timer_get_time() - echo_start;

    // 4. round-trip time -> distance (see README appendix for the /58)
    return pulse_us / 58.0f;
}

void app_main(void)
{
    gpio_reset_pin(TRIG);
    gpio_set_direction(TRIG, GPIO_MODE_OUTPUT);
    gpio_set_level(TRIG, 0);

    gpio_reset_pin(ECHO);
    gpio_set_direction(ECHO, GPIO_MODE_INPUT);

    gpio_reset_pin(BUZZER);
    gpio_set_direction(BUZZER, GPIO_MODE_OUTPUT);
    gpio_set_level(BUZZER, 0);

    gpio_reset_pin(BOARD_LED);
    gpio_set_direction(BOARD_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(BOARD_LED, 0);

    // Button: input with internal pull-up, so it idles HIGH and reads LOW when
    // pressed (wired to GND). Interrupt on the falling edge = the moment of press.
    gpio_config_t button_cfg = {
        .pin_bit_mask = 1ULL << BUTTON,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&button_cfg);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON, button_isr, NULL);

    bool was_paused = false;

    while (1)
    {
        if (g_paused)
        {
            if (!was_paused)
            {
                ESP_LOGI(TAG, "paused (LED solid) — press the button to resume");
                was_paused = true;
            }
            gpio_set_level(BUZZER, 0);
            gpio_set_level(BOARD_LED, 1); // solid ON while muted
            vTaskDelay(50 / portTICK_PERIOD_MS);
            continue;
        }
        if (was_paused)
        {
            ESP_LOGI(TAG, "resumed");
            was_paused = false;
        }

        float cm = measure_distance_cm();

        if (cm < 0)
            ESP_LOGI(TAG, "distance: -- (out of range / no echo)");
        else
            ESP_LOGI(TAG, "distance: %.1f cm", cm);

        if (cm < 0 || cm > BEEP_RANGE_CM)
        {
            // farther than BEEP_RANGE_CM (or no echo): stay quiet
            gpio_set_level(BUZZER, 0);
            gpio_set_level(BOARD_LED, 0);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        else
        {
            // closer = shorter gap between beeps (40 ms up close, ~600 ms near range)
            int gap_ms = (int)((cm / BEEP_RANGE_CM) * 560.0f) + 40;

            gpio_set_level(BUZZER, 1);
            gpio_set_level(BOARD_LED, 1);
            vTaskDelay(40 / portTICK_PERIOD_MS); // 40 ms beep ON

            gpio_set_level(BUZZER, 0);
            gpio_set_level(BOARD_LED, 0);
            vTaskDelay(gap_ms / portTICK_PERIOD_MS); // silence before next beep
        }
    }
}
