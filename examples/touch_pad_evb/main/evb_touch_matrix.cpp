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
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "iot_touchpad.h"
#include "evb.h"

static const char *TAG = "touch_matrix";
#if CONFIG_TOUCH_EB_V1
#define TOUCH_MATRIX_X_0 TOUCH_PAD_NUM7
#define TOUCH_MATRIX_X_1 TOUCH_PAD_NUM8
#define TOUCH_MATRIX_X_2 TOUCH_PAD_NUM9
#define TOUCH_MATRIX_Y_0 TOUCH_PAD_NUM4
#define TOUCH_MATRIX_Y_1 TOUCH_PAD_NUM5
#define TOUCH_MATRIX_Y_2 TOUCH_PAD_NUM6
#elif CONFIG_TOUCH_EB_V2
#define TOUCH_MATRIX_X_0 TOUCH_PAD_NUM9
#define TOUCH_MATRIX_X_1 TOUCH_PAD_NUM8
#define TOUCH_MATRIX_X_2 TOUCH_PAD_NUM7
#define TOUCH_MATRIX_Y_0 TOUCH_PAD_NUM6
#define TOUCH_MATRIX_Y_1 TOUCH_PAD_NUM4
#define TOUCH_MATRIX_Y_2 TOUCH_PAD_NUM5
#endif

static void app_matrix_cb(void *arg, uint8_t x, uint8_t y)
{
    const char *event = (char*) arg;
    ESP_LOGI(TAG, "%s of touchpad matrix (%d, %d)", event, x, y);
    touch_evt_t evt;
    evt.type = TOUCH_EVT_TYPE_MATRIX;
    evt.matrix.x = x;
    evt.matrix.y = y;
    xQueueSend(q_touch, &evt, portMAX_DELAY);
}

static void app_matrix_release_cb(void *arg, uint8_t x, uint8_t y)
{
    const char *event = (char*) arg;
    ESP_LOGI(TAG, "%s of touchpad matrix released (%d, %d)", event, x, y);
    touch_evt_t evt;
    evt.type = TOUCH_EVT_TYPE_MATRIX_RELEASE;
    evt.matrix.x = x;
    evt.matrix.y = y;
    xQueueSend(q_touch, &evt, portMAX_DELAY);
}

static void app_matrix_serial_cb(void *arg, uint8_t x, uint8_t y)
{
    const char *event = (char*) arg;
    ESP_LOGI(TAG, "%s of touchpad matrix released (%d, %d)", event, x, y);
    touch_evt_t evt;
    evt.type = TOUCH_EVT_TYPE_MATRIX_SERIAL;
    evt.matrix.x = x;
    evt.matrix.y = y;
    xQueueSend(q_touch, &evt, portMAX_DELAY);
}

void evb_touch_matrix_handle(int x, int y, int type)
{
    static int t = 0;
    if (type == TOUCH_EVT_TYPE_MATRIX) {
        ESP_LOGI(TAG, "matrix tp evt[%d][%d]", x, y);
        t = 0;
        ch450_write_dig(0, 0);
        ch450_write_dig(1, -1);
        ch450_write_dig(2, x);
        ch450_write_dig(3, y);
        ch450_write_dig(4, -1);
        ch450_write_dig(5, 0);
    } else if (type == TOUCH_EVT_TYPE_MATRIX_RELEASE) {
        ch450_write_dig(0, -1);
        ch450_write_dig(5, -1);
    } else if (type == TOUCH_EVT_TYPE_MATRIX_SERIAL) {
        t++;
        ch450_write_dig(0, t % 10);
        ch450_write_dig(5, t % 10);
    }
}

void evb_touch_matrix_init()
{
    //matrix touch
    const touch_pad_t x_tps[] = { TOUCH_MATRIX_X_0, TOUCH_MATRIX_X_1, TOUCH_MATRIX_X_2 };
    const touch_pad_t y_tps[] = { TOUCH_MATRIX_Y_0, TOUCH_MATRIX_Y_1, TOUCH_MATRIX_Y_2 };
#if COVER_DEBUG
    const uint16_t thresh[] = {2, 2, 2, 2, 2, 2};
#else
    const uint16_t *thresh = NULL;
#endif
    CTouchPadMatrix *tp_matrix = new CTouchPadMatrix(sizeof(x_tps) / sizeof(x_tps[0]), sizeof(y_tps) / sizeof(y_tps[0]),
            x_tps, y_tps, TOUCHPAD_THRES_PERCENT, thresh, TOUCHPAD_FILTER_VALUE);
    tp_matrix->add_cb(TOUCHPAD_CB_PUSH, app_matrix_cb, (void*) "push_event");
    tp_matrix->add_cb(TOUCHPAD_CB_RELEASE, app_matrix_release_cb, (void*) "release_event");
    tp_matrix->set_serial_trigger(1, 1000, app_matrix_serial_cb,  (void*) "serial_event");
}

