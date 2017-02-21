#include "esp_system.h"
#include "esp_log.h"
#include "led.h"

#define LED_QUICK_BLINK_FRE     5
#define LED_SLOW_BLINK_FRE      1

#define LED_IO_NUM_0    18
#define LED_IO_NUM_1    19

#define TAG "led"

static led_handle_t led_0, led_1;

void led_test()
{
    led_setup(LED_QUICK_BLINK_FRE, LED_SLOW_BLINK_FRE);
    led_0 = led_create(LED_IO_NUM_0, LED_DARK_LOW);
    led_1 = led_create(LED_IO_NUM_1, LED_DARK_LOW);
    led_state_write(led_0, LED_NORMAL_ON);
    led_state_write(led_1, LED_NORMAL_OFF);
    ESP_LOGI(TAG, "led0 state:%d", led_state_read(led_0));
    ets_delay_us(5 * 1000 * 1000);
    led_state_write(led_0, LED_QUICK_BLINK);
    led_state_write(led_1, LED_NORMAL_OFF);
    ESP_LOGI(TAG, "led0 state:%d", led_state_read(led_0));
    ESP_LOGI(TAG, "led0 mode:%d", led_mode_read(led_0));
    ets_delay_us(5 * 1000 * 1000);
    led_mode_write(led_0, LED_NIGHT_MODE);
    ESP_LOGI(TAG, "led0 state:%d", led_state_read(led_0));
    ESP_LOGI(TAG, "led0 mode:%d", led_mode_read(led_0));
    ets_delay_us(5 * 1000 * 1000);
    led_mode_write(led_0, LED_NORMAL_MODE);
    led_state_write(led_0, LED_NORMAL_OFF);
    led_state_write(led_1, LED_SLOW_BLINK);
}