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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "unity.h"
#include "esp_deep_sleep.h"
#include "touchpad.h"

#define TAG "deep_sleep_test"

void deep_sleep_gpio_test()
{
    ESP_LOGI(TAG, "waiting for 5 seconds...\n");
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "start deep sleep, press key 34 and 35 at the same time to wakeup\n");
    uint64_t wakeup_pin_mask = 1ULL << 35;
    wakeup_pin_mask |= 1ULL << 34;
    esp_deep_sleep_enable_ext1_wakeup(wakeup_pin_mask, ESP_EXT1_WAKEUP_ALL_LOW);
    esp_deep_sleep_start();
}

void deep_sleep_touch_test()
{
    touchpad_create(TOUCH_PAD_NUM8, 900, 100);
    touchpad_create(TOUCH_PAD_NUM9, 900, 100);
    touchpad_create(TOUCH_PAD_NUM6, 900, 100);
    touchpad_create(TOUCH_PAD_NUM7, 900, 100);
    touch_pad_set_meas_time(0xffff, TOUCH_PAD_MEASURE_CYCLE_DEFAULT >> 3);
    ESP_LOGI(TAG, "waiting for 5 seconds...\n");
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "start deep sleep, press TP1, TP2, TP3 or TP4 to wakeup\n");
    esp_deep_sleep_enable_touchpad_wakeup();
    esp_deep_sleep_start();
}

TEST_CASE("Deep sleep gpio test", "[deep_sleep][rtc_gpio][current][iot]")
{
    deep_sleep_gpio_test();
}

TEST_CASE("Deep sleep touch test", "[deep_sleep][touch][current][iot]")
{
    deep_sleep_touch_test();
}