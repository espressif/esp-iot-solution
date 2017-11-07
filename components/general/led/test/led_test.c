/*
  * ESPRESSIF MIT License
  *
  * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
  *
  * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
  * it is free of charge, to any person obtaining a copy of this software and associated
  * documentation files (the "Software"), to deal in the Software without restriction, including
  * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
  * to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in all copies or
  * substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  *
  */
  
#include "esp_system.h"
#include "esp_log.h"
#include "iot_led.h"

#define LED_IO_NUM_0    16
#define LED_IO_NUM_1    17

#define TAG "led"

static led_handle_t led_0, led_1;

void led_test()
{
    led_0 = iot_led_create(LED_IO_NUM_0, LED_DARK_LOW);
    led_1 = iot_led_create(LED_IO_NUM_1, LED_DARK_LOW);
    iot_led_state_write(led_0, LED_ON);
    iot_led_state_write(led_1, LED_OFF);
    ESP_LOGI(TAG, "led0 state:%d", iot_led_state_read(led_0));
    ets_delay_us(5 * 1000 * 1000);
    iot_led_state_write(led_0, LED_QUICK_BLINK);
    iot_led_state_write(led_1, LED_OFF);
    ESP_LOGI(TAG, "led0 state:%d", iot_led_state_read(led_0));
    ESP_LOGI(TAG, "led0 mode:%d", iot_led_mode_read(led_0));
    ets_delay_us(5 * 1000 * 1000);
    iot_led_state_write(led_0, LED_ON);
    iot_led_state_write(led_1, LED_ON);
    ets_delay_us(5 * 1000 * 1000);
    iot_led_night_duty_write(20);
    iot_led_mode_write(led_1, LED_NIGHT_MODE);
    ESP_LOGI(TAG, "led0 state:%d", iot_led_state_read(led_0));
    ESP_LOGI(TAG, "led0 mode:%d", iot_led_mode_read(led_0));
    ets_delay_us(5 * 1000 * 1000);
    iot_led_state_write(led_1, LED_SLOW_BLINK);
    ets_delay_us(5 * 1000 * 1000);
    iot_led_mode_write(led_0, LED_NORMAL_MODE);
}
