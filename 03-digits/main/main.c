#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

void app_main(void)
{

#define seg_a GPIO_NUM_13
#define seg_b GPIO_NUM_14
#define seg_c GPIO_NUM_27
#define seg_d GPIO_NUM_26
#define seg_e GPIO_NUM_25
#define seg_f GPIO_NUM_33
#define seg_g GPIO_NUM_32
#define seg_dp GPIO_NUM_4

#define common_1 GPIO_NUM_23
#define common_2 GPIO_NUM_22
#define common_3 GPIO_NUM_21
#define common_4 GPIO_NUM_19

    const uint8_t pattern[] = {
        0b00111111, // 0
        0b00000110, // 1
        0b01011011, // 2
        0b01001111, // 3
        0b01100110, // 4
        0b01101101, // 5
        0b01111101, // 6
        0b00000111, // 7
        0b01111111, // 8
        0b01101111, // 9
    };

    // Common-cathode: a digit is OFF when its common (cathode) is driven HIGH.
    void all_commons_off()
    {
        gpio_set_level(common_1, 1);
        gpio_set_level(common_2, 1);
        gpio_set_level(common_3, 1);
        gpio_set_level(common_4, 1);
    }

    void set(uint8_t pattern)
    {
        gpio_set_level(seg_a, pattern & 0b00000001);
        gpio_set_level(seg_b, (pattern >> 1) & 0b00000001);
        gpio_set_level(seg_c, (pattern >> 2) & 0b00000001);
        gpio_set_level(seg_d, (pattern >> 3) & 0b00000001);
        gpio_set_level(seg_e, (pattern >> 4) & 0b00000001);
        gpio_set_level(seg_f, (pattern >> 5) & 0b00000001);
        gpio_set_level(seg_g, (pattern >> 6) & 0b00000001);
        gpio_set_level(seg_dp, (pattern >> 7) & 0b00000001);
    }

    // Common-cathode: a digit is ON when its common (cathode) is driven LOW.
    void common_on(int position)
    {
        switch (position)
        {
        case 0:
            gpio_set_level(common_1, 0);
            break;
        case 1:
            gpio_set_level(common_2, 0);
            break;
        case 2:
            gpio_set_level(common_3, 0);
            break;
        case 3:
            gpio_set_level(common_4, 0);
            break;
        }
    }

    uint8_t segments(uint8_t pattern)
    {
        return pattern;
    }

    void show_digit(int position, int value)
    {
        all_commons_off();
        set(segments(pattern[value]));
        common_on(position);
        esp_rom_delay_us(1500);
    }

    // Configure every segment and common pin as an output before driving it.
    const int seg_pins[] = {seg_a, seg_b, seg_c, seg_d, seg_e, seg_f, seg_g, seg_dp};
    const int common_pins[] = {common_1, common_2, common_3, common_4};

    for (int i = 0; i < 8; i++)
    {
        gpio_reset_pin(seg_pins[i]);
        gpio_set_direction(seg_pins[i], GPIO_MODE_OUTPUT);
    }
    for (int i = 0; i < 4; i++)
    {
        gpio_reset_pin(common_pins[i]);
        gpio_set_direction(common_pins[i], GPIO_MODE_OUTPUT);
    }

    while (1)
    {
        // Refresh the display many times, then briefly block so the idle task
        // can run and reset the task watchdog (esp_rom_delay_us never yields).
        for (int frame = 0; frame < 50; frame++)
        {
            for (int i = 0; i < 4; i++)
            {
                show_digit(i, i);
            }
        }
        vTaskDelay(1);
    }
}