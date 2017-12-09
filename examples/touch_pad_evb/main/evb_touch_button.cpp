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
#include "iot_led.h"
#include "evb.h"

static const char *TAG = "touch_button";
#if CONFIG_TOUCH_EB_V1
extern CLED *led[4];
#define TOUCH_BUTTON_0  TOUCH_PAD_NUM0
#define TOUCH_BUTTON_1  TOUCH_PAD_NUM2
#define TOUCH_BUTTON_2  TOUCH_PAD_NUM3
#elif CONFIG_TOUCH_EB_V2
#define TOUCH_BUTTON_0  TOUCH_PAD_NUM0
#define TOUCH_BUTTON_1  TOUCH_PAD_NUM2
#define TOUCH_BUTTON_2  TOUCH_PAD_NUM3
#endif
void evb_touch_button_handle(int idx, int type)
{
    ESP_LOGI(TAG, "single tp evt[%d]", idx);
#if CONFIG_TOUCH_EB_V1
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
#elif CONFIG_TOUCH_EB_V2
    idx = idx == TOUCH_BUTTON_0 ? 0 :
          idx == TOUCH_BUTTON_1 ? 1 :
          idx == TOUCH_BUTTON_2 ? 2 : 0;
    ch450_write_dig(2, -1);
    ch450_write_dig(3, -1);
    if (type == TOUCH_EVT_TYPE_SINGLE_PUSH) {
        ch450_write_dig(0, idx);
        ch450_write_dig(1, -1);

    } else if (type == TOUCH_EVT_TYPE_SINGLE_RELEASE) {
        ch450_write_dig(1, idx);
        ch450_write_dig(0, -1);
    } else {

    }
#endif
}

static void push_cb(void *arg)
{
    CTouchPad* tp_dev = (CTouchPad*) arg;
    touch_pad_t tp_num = tp_dev->tp_num();
    ESP_LOGI(TAG, "push callback of touch pad num %d", tp_num);
    touch_evt_t evt;
    evt.type = TOUCH_EVT_TYPE_SINGLE_PUSH;
    evt.single.idx = (int) tp_num;
    xQueueSend(q_touch, &evt, portMAX_DELAY);
}

static void release_cb(void *arg)
{
    CTouchPad* tp_dev = (CTouchPad*) arg;
    touch_pad_t tp_num = tp_dev->tp_num();
    ESP_LOGI(TAG, "release callback of touch pad num %d", tp_num);
    touch_evt_t evt;
    evt.type = TOUCH_EVT_TYPE_SINGLE_RELEASE;
    evt.single.idx = (int) tp_num;
    xQueueSend(q_touch, &evt, portMAX_DELAY);
}

void evb_touch_button_init()
{
    //single touch
    CTouchPad* tp_dev0 = new CTouchPad(TOUCH_BUTTON_0, TOUCHPAD_THRES_PERCENT, 0, TOUCHPAD_FILTER_VALUE);
    CTouchPad* tp_dev1 = new CTouchPad(TOUCH_BUTTON_1, TOUCHPAD_THRES_PERCENT, 0, TOUCHPAD_FILTER_VALUE);
    CTouchPad* tp_dev2 = new CTouchPad(TOUCH_BUTTON_2, TOUCHPAD_THRES_PERCENT, 0, TOUCHPAD_FILTER_VALUE);
    tp_dev0->add_cb(TOUCHPAD_CB_PUSH, push_cb, tp_dev0);
    tp_dev1->add_cb(TOUCHPAD_CB_PUSH, push_cb, tp_dev1);
    tp_dev2->add_cb(TOUCHPAD_CB_PUSH, push_cb, tp_dev2);
    tp_dev0->add_cb(TOUCHPAD_CB_RELEASE, release_cb, tp_dev0);
    tp_dev1->add_cb(TOUCHPAD_CB_RELEASE, release_cb, tp_dev1);
    tp_dev2->add_cb(TOUCHPAD_CB_RELEASE, release_cb, tp_dev2);
}

