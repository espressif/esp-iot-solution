/*
 * ESPRSSIF MIT License

 *
 * Copyright (c) 2015 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
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
#define BUTTONE_DEV_TEST_CODE 1
#if BUTTONE_DEV_TEST_CODE

#define BUTTON_IO_NUM  0
#define BUTTON_ACTIVE_LEVEL   0
#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

#include "unity.h"
#include "button.h"
#include "esp_system.h"
#include "esp_log.h"

static const char* TAG_BTN = "BTN_TEST";

void button_tap_cb(void* arg)
{
    char* pstr = (char*) arg;
    ESP_EARLY_LOGI(TAG_BTN, "tap cb (%s), heap: %d\n", pstr, esp_get_free_heap_size());
}

void button_press_2s_cb(void* arg)
{
    ESP_EARLY_LOGI(TAG_BTN, "press 2s, heap: %d\n", esp_get_free_heap_size());
}

void button_press_5s_cb(void* arg)
{
    ESP_EARLY_LOGI(TAG_BTN, "press 5s, heap: %d\n", esp_get_free_heap_size());
}

void button_test()
{
    printf("before btn init, heap: %d\n", esp_get_free_heap_size());
    button_handle_t btn_handle = button_create(BUTTON_IO_NUM, BUTTON_ACTIVE_LEVEL, BUTTON_SERIAL_TRIGGER, 3);
    button_add_cb(btn_handle, BUTTON_PUSH_CB, button_tap_cb, "PUSH", 50 / portTICK_PERIOD_MS);
    button_add_cb(btn_handle, BUTTON_RELEASE_CB, button_tap_cb, "RELEASE", 50 / portTICK_PERIOD_MS);
    button_add_cb(btn_handle, BUTTON_TAP_CB, button_tap_cb, "TAP", 50 / portTICK_PERIOD_MS);
    button_add_cb(btn_handle, BUTTON_SERIAL_CB, button_tap_cb, "SERIAL", 1000 / portTICK_PERIOD_MS);

    button_add_custom_cb(btn_handle, 2, button_press_2s_cb, NULL);
    button_add_custom_cb(btn_handle, 5, button_press_5s_cb, NULL);
    printf("after btn init, heap: %d\n", esp_get_free_heap_size());

    vTaskDelay(40000 / portTICK_PERIOD_MS);
    printf("free btn: heap:%d\n", esp_get_free_heap_size());
    button_delete(btn_handle);
    printf("after free btn: heap:%d\n", esp_get_free_heap_size());
}

TEST_CASE("Button test", "[button][iot]")
{
    button_test();
}

#endif
