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

#define TOUCHPAD_THRES_PERCENT  0.20
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
const uint16_t thresh[] = {  0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2 };
#endif

#if TOUCHPAD_MATRIX_TEST
const touch_pad_t x_tps[] = {0, 2, 3, 4, 8};
const touch_pad_t y_tps[] = {5, 6, 7};
const uint16_t thresh[] = {  0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2 };

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
    tp_dev0 = iot_tp_create(TOUCH_PAD_NUM8, TOUCHPAD_THRES_PERCENT);
    tp_dev1 = iot_tp_create(TOUCH_PAD_NUM9, TOUCHPAD_THRES_PERCENT);
    
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
    tp_slide_handle_t tp_slide = iot_tp_slide_create(num, tps, 255, thresh);
    while (1) {
        uint8_t pos = iot_tp_slide_position(tp_slide);
        printf("%d\n", pos);
        vTaskDelay(100 / portTICK_RATE_MS);
    }
#endif

#if TOUCHPAD_MATRIX_TEST
    tp_matrix_handle_t tp_matrix = iot_tp_matrix_create(sizeof(x_tps)/sizeof(x_tps[0]), sizeof(y_tps)/sizeof(y_tps[0]),
                                                                x_tps, y_tps, thresh);
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
