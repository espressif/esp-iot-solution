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
#include "esp_adc_cal.h"
#include "driver/adc.h"
#include "driver/gpio.h"
#include "iot_touchpad.h"
#include "led.h"
#include "evb.h"

#define COVER_DEBUG  0

static const char* TAG = "evb_main";

#if COVER_DEBUG
#define TOUCHPAD_THRES_PERCENT  990
#else
#define TOUCHPAD_THRES_PERCENT  900
#endif
#define TOUCHPAD_FILTER_VALUE   10
QueueHandle_t q_touch = NULL;
CLED *led[4] = { 0 };



//--------touch -------------
typedef enum {
    TOUCH_EVT_TYPE_SINGLE,
    TOUCH_EVT_TYPE_MATRIX,
    TOUCH_EVT_TYPE_MATRIX_RELEASE,
    TOUCH_EVT_TYPE_MATRIX_SERIAL,
    TOUCH_EVT_TYPE_SLIDE,
} touch_evt_type_t;

typedef struct {
    touch_evt_type_t type;
    union {
        struct {
            int idx;
        } single;
        struct {
            int x;
            int y;
        } matrix;
    };

} touch_evt_t;

static void tap_cb(void *arg)
{
    CTouchPad* tp_dev = (CTouchPad*) arg;
    touch_pad_t tp_num = tp_dev->tp_num();
    ESP_LOGI(TAG, "tap callback of touch pad num %d", tp_num);
    touch_evt_t evt;
    evt.type = TOUCH_EVT_TYPE_SINGLE;
    evt.single.idx = (int) tp_num;
    xQueueSend(q_touch, &evt, portMAX_DELAY);
}

void touch_single_handle(int idx)
{
    ESP_LOGI(TAG, "single tp evt[%d]", idx);

    for (int i = 0; i < 4; i++) {
        if (i == idx) {
            led[i]->toggle();
        } else if (idx > 0) {
            led[i]->off();
        }
    }
    ch450_write_dig(0, idx);
    ch450_write_dig(1, -1);
    ch450_write_dig(2, -1);
    ch450_write_dig(3, -1);
    ch450_write_dig(4, -1);
    ch450_write_dig(5, -1);
}

//-------- touch matrix ---------
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


void touch_matrix_handle(int x, int y, int type)
{
    static int t = 0;
    if (type == TOUCH_EVT_TYPE_MATRIX) {
        ESP_LOGI(TAG, "matrix tp evt[%d][%d]", x, y);
        t = 0;
        ch450_write_dig(0, 0);
        ch450_write_dig(1, -1);
        ch450_write_dig(4, -1);
        ch450_write_dig(5, 0);
        ch450_write_dig(2, x);
        ch450_write_dig(3, y);
    } else if (type == TOUCH_EVT_TYPE_MATRIX_RELEASE) {
        ch450_write_dig(0, -1);
        ch450_write_dig(5, -1);
    } else if (type == TOUCH_EVT_TYPE_MATRIX_SERIAL) {
        t++;
        ch450_write_dig(0, t % 10);
        ch450_write_dig(5, t % 10);
    }
}

//---------- touch slide --------------
void touch_slide_handle(int pos)
{
    static int pos_prev = 0;
    if (pos_prev != pos && pos != 0xff) {
        pos_prev = pos;
        ESP_LOGI(TAG, "slide pos evt[%d]", pos);
        ch450_write_dig(0, -1);
        ch450_write_dig(1, -1);
        ch450_write_dig(2, -1);
        ch450_write_dig(3, -1);
        ch450_write_dig(4, pos % 10);
        ch450_write_dig(5, pos / 10);
    }
}

void touch_evt_handle_task(void* arg)
{
    touch_evt_t evt;
    while (1) {
        portBASE_TYPE ret = xQueueReceive(q_touch, &evt, portMAX_DELAY);
        if (ret == pdTRUE) {
            switch (evt.type) {
                case TOUCH_EVT_TYPE_SINGLE:
                    touch_single_handle(evt.single.idx);
                    break;
                case TOUCH_EVT_TYPE_MATRIX:
                    touch_matrix_handle(evt.matrix.x, evt.matrix.y, TOUCH_EVT_TYPE_MATRIX);
                    break;
                case TOUCH_EVT_TYPE_MATRIX_RELEASE:
                    touch_matrix_handle(evt.matrix.x, evt.matrix.y, TOUCH_EVT_TYPE_MATRIX_RELEASE);
                    break;
                case TOUCH_EVT_TYPE_MATRIX_SERIAL:
                    touch_matrix_handle(evt.matrix.x, evt.matrix.y, TOUCH_EVT_TYPE_MATRIX_SERIAL);
                    break;
                default:
                    break;
            }
        }
    }
}

extern "C" void app_main()
{
    printf("touch pad evb...\n");
    evb_adc_init();
    if (q_touch == NULL) {
        q_touch = xQueueCreate(10, sizeof(touch_evt_t));
    }
    xTaskCreate(touch_evt_handle_task, "touch_evt_handle_task", 1024 * 8, NULL, 6, NULL);
    ch450_dev_init();
    touch_single_handle(-1);
    led[0] = new CLED(23, LED_DARK_LOW);
    led[1] = new CLED(25, LED_DARK_LOW);
    led[2] = new CLED(26, LED_DARK_LOW);
    led[3] = new CLED(22, LED_DARK_LOW);
    //single touch
    CTouchPad* tp_dev0 = new CTouchPad(TOUCH_PAD_NUM0, TOUCHPAD_THRES_PERCENT, 0, TOUCHPAD_FILTER_VALUE);
    CTouchPad* tp_dev2 = new CTouchPad(TOUCH_PAD_NUM2, TOUCHPAD_THRES_PERCENT, 0, TOUCHPAD_FILTER_VALUE);
    CTouchPad* tp_dev3 = new CTouchPad(TOUCH_PAD_NUM3, TOUCHPAD_THRES_PERCENT, 0, TOUCHPAD_FILTER_VALUE);

    tp_dev0->add_cb(TOUCHPAD_CB_TAP, tap_cb, tp_dev0);
    tp_dev2->add_cb(TOUCHPAD_CB_TAP, tap_cb, tp_dev2);
    tp_dev3->add_cb(TOUCHPAD_CB_TAP, tap_cb, tp_dev3);

    int mode = evb_adc_get_mode();
    if (mode == TOUCH_EVB_MODE_MATRIX) {
        //matrix touch
        const touch_pad_t x_tps[] = { TOUCH_PAD_NUM7, TOUCH_PAD_NUM9, TOUCH_PAD_NUM8 };
        const touch_pad_t y_tps[] = { TOUCH_PAD_NUM4, TOUCH_PAD_NUM5, TOUCH_PAD_NUM6 };
#if COVER_DEBUG
        const uint16_t thresh[]   = { 2, 2, 2, 2, 2, 2 };
#else
        const uint16_t *thresh = NULL;
#endif
        CTouchPadMatrix *tp_matrix = new CTouchPadMatrix(sizeof(x_tps) / sizeof(x_tps[0]),
                sizeof(y_tps) / sizeof(y_tps[0]), x_tps, y_tps, TOUCHPAD_THRES_PERCENT, thresh, TOUCHPAD_FILTER_VALUE);
        tp_matrix->add_cb(TOUCHPAD_CB_PUSH, app_matrix_cb, (void*) "push_event");
        tp_matrix->add_cb(TOUCHPAD_CB_RELEASE, app_matrix_release_cb, (void*) "release_event");
        tp_matrix->add_custom_cb(1, app_matrix_serial_cb, (void*) "serial_event");
        tp_matrix->add_custom_cb(2, app_matrix_serial_cb, (void*) "serial_event");
        tp_matrix->add_custom_cb(3, app_matrix_serial_cb, (void*) "serial_event");
        tp_matrix->add_custom_cb(4, app_matrix_serial_cb, (void*) "serial_event");
    } else if (mode == TOUCH_EVB_MODE_SLIDE) {
        //slide touch
        const touch_pad_t tps[] = { TOUCH_PAD_NUM4, TOUCH_PAD_NUM5, TOUCH_PAD_NUM6, TOUCH_PAD_NUM7, TOUCH_PAD_NUM9,
                TOUCH_PAD_NUM8 };
        CTouchPadSlide *tp_slide = new CTouchPadSlide(sizeof(tps) / sizeof(TOUCH_PAD_NUM4), tps, 5, TOUCHPAD_THRES_PERCENT, NULL);
        while (1) {
            uint8_t pos = tp_slide->get_position();
            touch_slide_handle(pos);
            vTaskDelay(50 / portTICK_RATE_MS);
        }
    }
}
