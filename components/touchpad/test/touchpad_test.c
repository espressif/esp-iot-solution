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
#include "driver/touch_pad.h"
#include "iot_touchpad.h"
#include "esp_log.h"
#include "unity.h"

#define TOUCH_PAD_TEST 1
#if TOUCH_PAD_TEST

#define SINGLE_TOUCHPAD_TEST    1
#define TOUCHPAD_SLIDE_TEST     0
#define TOUCHPAD_MATRIX_TEST    0
#define PROXIMITY_SENSOR_TEST   0
#define GESTURE_SENSOR_TEST     0

#define TOUCHPAD_THRES_PERCENT  950
#define TOUCHPAD_FILTER_VALUE   150
static const char* TAG = "touchpad_test";

#if SINGLE_TOUCHPAD_TEST
static tp_handle_t tp_dev0, tp_dev1;
static void tap_cb(void *arg)
{
    tp_handle_t tp_dev = (tp_handle_t) arg;
    touch_pad_t tp_num = iot_tp_num_get(tp_dev);
    ESP_LOGI(TAG, "tap callback of touch pad num %d", tp_num);
}

static void serial_trigger_cb(void *arg)
{
    tp_handle_t tp_dev = (tp_handle_t) arg;
    touch_pad_t tp_num = iot_tp_num_get(tp_dev);
    ESP_LOGI(TAG, "serial trigger callback of touch pad num %d", tp_num);
}

static void push_cb(void *arg)
{
    tp_handle_t tp_dev = (tp_handle_t) arg;
    touch_pad_t tp_num = iot_tp_num_get(tp_dev);
    ESP_LOGI(TAG, "push callback of touch pad num %d", tp_num);
}

static void release_cb(void *arg)
{
    tp_handle_t tp_dev = (tp_handle_t) arg;
    touch_pad_t tp_num = iot_tp_num_get(tp_dev);
    ESP_LOGI(TAG, "release callback of touch pad num %d", tp_num);
}

static void press_3s_cb(void *arg)
{
    tp_handle_t tp_dev = (tp_handle_t) arg;
    touch_pad_t tp_num = iot_tp_num_get(tp_dev);
    ESP_LOGI(TAG, "press 3s callback of touch pad num %d", tp_num);
}

static void press_5s_cb(void *arg)
{
    tp_handle_t tp_dev = (tp_handle_t) arg;
    touch_pad_t tp_num = iot_tp_num_get(tp_dev);
    ESP_LOGI(TAG, "press 5s callback of touch pad num %d", tp_num);
}
#endif

#if TOUCHPAD_SLIDE_TEST
const touch_pad_t tps[] = {0, 2, 3, 4, 5, 6, 7, 8, 0, 4, 7, 2, 5, 8, 3, 6};
#endif

#if TOUCHPAD_MATRIX_TEST
const touch_pad_t x_tps[] = {0, 2, 3, 4, 8};
const touch_pad_t y_tps[] = {5, 6, 7};

static void tp_matrix_cb(void *arg, uint8_t x, uint8_t y)
{
    const char *event = (char*) arg;
    printf("%s of touchpad matrix (%d, %d)\n", event, x, y);
}
#endif

#if PROXIMITY_SENSOR_TEST
void prox_cb(void *arg, proximity_status_t type)
{
    switch (type) {
        case PROXIMITY_IDLE:
            ets_printf("hand is already left!\n");
            break;
        case PROXIMITY_APPROACHING:
            ets_printf("hand is approaching!\n");
            break;
        case PROXIMITY_LEAVING:
            ets_printf("hand is leaving!\n");
            break;
        default:
            break;
    }
}
#endif

#if GESTURE_SENSOR_TEST
void gest_cb(void *arg, gesture_type_t type)
{
    switch (type) {
        case GESTURE_TOP_TO_BOTTOM:
            ets_printf("gesture from top to bottom\n");
            break;
        case GESTURE_BOTTOM_TO_TOP:
            ets_printf("gesture from bottom to top\n");
            break;
        case GESTURE_LEFT_TO_RIGHT:
            ets_printf("gesture from left to right\n");
            break;
        case GESTURE_RIGHT_TO_LEFT:
            ets_printf("gesture from right to left\n");
            break;
        default:
            break;
    }
}
#endif

void tp_test()
{
#if SINGLE_TOUCHPAD_TEST
    tp_dev0 = iot_tp_create(TOUCH_PAD_NUM8, TOUCHPAD_THRES_PERCENT, 0, TOUCHPAD_FILTER_VALUE);
    tp_dev1 = iot_tp_create(TOUCH_PAD_NUM9, TOUCHPAD_THRES_PERCENT, 0, TOUCHPAD_FILTER_VALUE);
    
    iot_tp_add_cb(tp_dev0, TOUCHPAD_CB_TAP, tap_cb, tp_dev0);
    iot_tp_add_cb(tp_dev0, TOUCHPAD_CB_PUSH, push_cb, tp_dev0);
    iot_tp_add_cb(tp_dev0, TOUCHPAD_CB_RELEASE, release_cb, tp_dev0);
    iot_tp_add_custom_cb(tp_dev0, 3, press_3s_cb, tp_dev0);
    iot_tp_add_custom_cb(tp_dev0, 5, press_5s_cb, tp_dev0);

    iot_tp_add_cb(tp_dev1, TOUCHPAD_CB_TAP, tap_cb, tp_dev1);
    iot_tp_set_serial_trigger(tp_dev1, 4, 1000, serial_trigger_cb, tp_dev1);
    iot_tp_add_custom_cb(tp_dev1, 3, press_3s_cb, tp_dev1);
    iot_tp_add_custom_cb(tp_dev1, 5, press_5s_cb, tp_dev1);

    vTaskDelay((30 * 1000) / portTICK_RATE_MS);
    ESP_LOGI(TAG, "touchpad 0 deleted");
    iot_tp_delete(tp_dev0);
    vTaskDelay((30 * 1000) / portTICK_RATE_MS);
    ESP_LOGI(TAG, "touchpad 1 deleted"); 
    iot_tp_delete(tp_dev1);
#endif

#if TOUCHPAD_SLIDE_TEST
    uint8_t num = sizeof(tps) / sizeof(tps[0]);
    tp_slide_handle_t tp_slide = iot_tp_slide_create(num, tps, 2,TOUCHPAD_THRES_PERCENT, NULL, TOUCHPAD_FILTER_VALUE);
    while (1) {
        uint8_t pos = iot_tp_slide_position(tp_slide);
        printf("%d\n", pos);
        vTaskDelay(100 / portTICK_RATE_MS);
    }
#endif

#if TOUCHPAD_MATRIX_TEST
    tp_matrix_handle_t tp_matrix = iot_tp_matrix_create(sizeof(x_tps)/sizeof(x_tps[0]), sizeof(y_tps)/sizeof(y_tps[0]),
                                                                x_tps, y_tps, TOUCHPAD_THRES_PERCENT, NULL, TOUCHPAD_FILTER_VALUE);
    iot_tp_matrix_add_cb(tp_matrix, TOUCHPAD_CB_PUSH, tp_matrix_cb, "push_event");
    iot_tp_matrix_add_cb(tp_matrix, TOUCHPAD_CB_RELEASE, tp_matrix_cb, "release_event");
    iot_tp_matrix_add_cb(tp_matrix, TOUCHPAD_CB_TAP, tp_matrix_cb, "tap_event");
    iot_tp_matrix_add_custom_cb(tp_matrix, 3, tp_matrix_cb, "press 3s");
    iot_tp_matrix_add_custom_cb(tp_matrix, 5, tp_matrix_cb, "press 5s");
    iot_tp_matrix_set_serial_trigger(tp_matrix, 4, 500, tp_matrix_cb, "serial trigger");
    vTaskDelay(60000 / portTICK_RATE_MS);
    ESP_LOGI("touchpad", "delete touchpad matrix");
    iot_tp_matrix_delete(tp_matrix);
#endif
}

TEST_CASE("Touch sensor test", "[touch][iot][rtc]")
{
    tp_test();
}
#endif
