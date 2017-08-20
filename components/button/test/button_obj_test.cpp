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
#include "esp_system.h"
#include "unity.h"
#include "button.h"
#include "esp_log.h"

#define BUTTON_IO_NUM  GPIO_NUM_0
#define BUTTON_ACTIVE_LEVEL   BUTTON_ACTIVE_LOW
static const char* TAG_BTN = "BTN_TEST";

static void button_tap_cb(void* arg)
{
    char* pstr = (char*) arg;
    ESP_EARLY_LOGI(TAG_BTN, "tap cb (%s), heap: %d\n", pstr, esp_get_free_heap_size());
}

static void button_press_2s_cb(void* arg)
{
    ESP_EARLY_LOGI(TAG_BTN, "press 2s, heap: %d\n", esp_get_free_heap_size());
}

static void button_press_5s_cb(void* arg)
{
    ESP_EARLY_LOGI(TAG_BTN, "press 5s, heap: %d\n", esp_get_free_heap_size());
}

extern "C" void button_obj_test()
{
    const char *push = "PUSH";
    const char *release = "RELEASE";
    const char *tap = "TAP";
    const char *serial = "SERIAL";
    CButton* btn = new CButton(BUTTON_IO_NUM, BUTTON_ACTIVE_LEVEL, BUTTON_SERIAL_TRIGGER, 3);
    btn->add_cb(BUTTON_PUSH_CB, button_tap_cb, (void*) push, 50 / portTICK_PERIOD_MS);
    btn->add_cb(BUTTON_RELEASE_CB, button_tap_cb, (void*) release, 50 / portTICK_PERIOD_MS);
    btn->add_cb(BUTTON_TAP_CB, button_tap_cb, (void*) tap, 50 / portTICK_PERIOD_MS);
    btn->add_cb(BUTTON_SERIAL_CB, button_tap_cb, (void*) serial, 1000 / portTICK_PERIOD_MS);
    
    btn->add_custom_cb(2, button_press_2s_cb, NULL);
    btn->add_custom_cb(5, button_press_5s_cb, NULL);

    vTaskDelay(40000 / portTICK_PERIOD_MS);
    delete btn;
    ESP_LOGI(TAG_BTN, "button is deleted");
}

TEST_CASE("Button cpp test", "[button_cpp][iot]")
{
    button_obj_test();
}
