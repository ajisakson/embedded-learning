#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define ONBOARD_LED GPIO_NUM_2
#define EXTERNAL_LED GPIO_NUM_5
#define BLINK_DELAY_MS 600

void app_main(void)
{
    gpio_reset_pin(ONBOARD_LED);
    gpio_set_direction(ONBOARD_LED, GPIO_MODE_OUTPUT);
    gpio_reset_pin(EXTERNAL_LED);
    gpio_set_direction(EXTERNAL_LED, GPIO_MODE_OUTPUT);

    while (1)
    {
        gpio_set_level(ONBOARD_LED, 1);
        gpio_set_level(EXTERNAL_LED, 0);
        vTaskDelay(BLINK_DELAY_MS / portTICK_PERIOD_MS);
        gpio_set_level(ONBOARD_LED, 0);
        gpio_set_level(EXTERNAL_LED, 1);
        vTaskDelay(BLINK_DELAY_MS / portTICK_PERIOD_MS);
    }
}