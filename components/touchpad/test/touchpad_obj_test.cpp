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

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "touchpad.h"
#include "esp_log.h"
#include "unity.h"

#define SINGLE_TOUCHPAD_TEST    1
#define TOUCHPAD_SLIDE_TEST     0
#define TOUCHPAD_MATRIX_TEST    0

#define TOUCHPAD_THRES_PERCENT  950
#define TOUCHPAD_FILTER_VALUE   150
static const char* TAG = "touchpad_test";

#if SINGLE_TOUCHPAD_TEST
static void tap_cb(void *arg)
{
    CTouchPad* touchpad_dev = (CTouchPad*) arg;
    touch_pad_t tp_num = touchpad_dev->touchpad_num();
    ESP_LOGI(TAG, "tap callback of touch pad num %d", tp_num);
}

static void serial_trigger_cb(void *arg)
{
    CTouchPad* touchpad_dev = (CTouchPad*) arg;
    touch_pad_t tp_num = touchpad_dev->touchpad_num();
    ESP_LOGI(TAG, "serial trigger callback of touch pad num %d", tp_num);
    ESP_LOGI(TAG, "touch pad value is %d", touchpad_dev->value());
}

static void push_cb(void *arg)
{
    CTouchPad* touchpad_dev = (CTouchPad*) arg;
    touch_pad_t tp_num = touchpad_dev->touchpad_num();
    ESP_LOGI(TAG, "push callback of touch pad num %d", tp_num);
}

static void release_cb(void *arg)
{
    CTouchPad* touchpad_dev = (CTouchPad*) arg;
    touch_pad_t tp_num = touchpad_dev->touchpad_num();
    ESP_LOGI(TAG, "release callback of touch pad num %d", tp_num);
}

static void press_3s_cb(void *arg)
{
    CTouchPad* touchpad_dev = (CTouchPad*) arg;
    touch_pad_t tp_num = touchpad_dev->touchpad_num();
    ESP_LOGI(TAG, "press 3s callback of touch pad num %d", tp_num);
}

static void press_5s_cb(void *arg)
{
    CTouchPad* touchpad_dev = (CTouchPad*) arg;
    touch_pad_t tp_num = touchpad_dev->touchpad_num();
    ESP_LOGI(TAG, "press 5s callback of touch pad num %d", tp_num);
}
#endif

#if TOUCHPAD_MATRIX_TEST
static void tp_matrix_cb(void *arg, uint8_t x, uint8_t y)
{
    const char *event = (char*) arg;
    printf("%s of touchpad matrix (%d, %d)\n", event, x, y);
}
#endif

extern "C" void touchpad_obj_test()
{
#if SINGLE_TOUCHPAD_TEST
    CTouchPad* tp_dev0 = new CTouchPad(TOUCH_PAD_NUM8, TOUCHPAD_THRES_PERCENT, TOUCHPAD_FILTER_VALUE);
    CTouchPad* tp_dev1 = new CTouchPad(TOUCH_PAD_NUM9, TOUCHPAD_THRES_PERCENT);

    tp_dev0->add_cb(TOUCHPAD_CB_TAP, tap_cb, tp_dev0);
    tp_dev0->add_cb(TOUCHPAD_CB_PUSH, push_cb, tp_dev0);
    tp_dev0->add_cb(TOUCHPAD_CB_RELEASE, release_cb, tp_dev0);
    tp_dev0->add_custom_cb(3, press_3s_cb, tp_dev0);
    tp_dev0->add_custom_cb(5, press_5s_cb, tp_dev0);

    tp_dev1->add_cb(TOUCHPAD_CB_TAP, tap_cb, tp_dev1);
    tp_dev1->set_serial_trigger(4, 1000, serial_trigger_cb, tp_dev1);
    tp_dev1->add_custom_cb(3, press_3s_cb, tp_dev1);
    tp_dev1->add_custom_cb(5, press_5s_cb, tp_dev1);

    vTaskDelay((30 * 1000) / portTICK_RATE_MS);
    ESP_LOGI(TAG, "touchpad 0 deleted");
    delete tp_dev0;
    tp_dev0 = NULL;
    vTaskDelay((30 * 1000) / portTICK_RATE_MS);
    ESP_LOGI(TAG, "touchpad 1 deleted"); 
    delete tp_dev1;
    tp_dev0 = NULL;
#endif

#if TOUCHPAD_SLIDE_TEST
    const touch_pad_t tps[] = {TOUCH_PAD_NUM5, TOUCH_PAD_NUM6, TOUCH_PAD_NUM7};
    CTouchPadSlide *tp_slide = new CTouchPadSlide(3, tps);
    while (1) {
        uint8_t pos = tp_slide->get_position();
        printf("%d\n", pos);
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
#endif

#if TOUCHPAD_MATRIX_TEST
    const touch_pad_t x_tps[] = {TOUCH_PAD_NUM0, TOUCH_PAD_NUM2, TOUCH_PAD_NUM3, TOUCH_PAD_NUM4, TOUCH_PAD_NUM8};
    const touch_pad_t y_tps[] = {TOUCH_PAD_NUM5, TOUCH_PAD_NUM6, TOUCH_PAD_NUM7};
    CTouchPadMatrix *tp_matrix = new CTouchPadMatrix(sizeof(x_tps)/sizeof(x_tps[0]), sizeof(y_tps)/sizeof(y_tps[0]),
                                                x_tps, y_tps, TOUCHPAD_THRES_PERCENT, TOUCHPAD_FILTER_VALUE);
    char push[20];
    strcpy(push, "push_event");
    char press[20];
    strcpy(press, "press 3s");
    char serial[20];
    strcpy(serial, "serial trigger");
    tp_matrix->add_cb(TOUCHPAD_CB_PUSH, tp_matrix_cb, push);
    tp_matrix->add_custom_cb(3, tp_matrix_cb, press);
    tp_matrix->set_serial_trigger(4, 500, tp_matrix_cb, serial);
    vTaskDelay(60000 / portTICK_RATE_MS);
    ESP_LOGI(TAG, "delete touchpad matrix");
    delete tp_matrix;
#endif
}

TEST_CASE("Touch sensor cpp test", "[touch][iot][rtc]")
{
    touchpad_obj_test();
}
