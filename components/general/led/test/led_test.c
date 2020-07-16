// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "esp_system.h"
#include "esp_log.h"
#include "iot_led.h"
#include "unity.h"

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

TEST_CASE("led test", "[led][iot]")
{
    led_test();
}
