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

static const char *TAG = "touch_spring";
#define TOUCH_BUTTON_0  TOUCH_PAD_NUM9
#define TOUCH_BUTTON_1  TOUCH_PAD_NUM6
#define TOUCH_BUTTON_2  TOUCH_PAD_NUM5
#define TOUCH_BUTTON_3  TOUCH_PAD_NUM8
#define TOUCH_BUTTON_4  TOUCH_PAD_NUM7
#define TOUCH_BUTTON_5  TOUCH_PAD_NUM4
static CTouchPad *tp_dev[6] = {0};

void evb_touch_spring_led_toggle(int idx)
{
    static uint8_t val[6] = {0};
    val[idx] = !val[idx];
    ch450_write_led(BIT(idx), val[idx]);
}

void evb_touch_spring_handle(int idx, int type)
{
    ESP_LOGI(TAG, "spring tp evt[%d]", idx);
    idx = idx == TOUCH_BUTTON_0 ? 0 :
          idx == TOUCH_BUTTON_1 ? 1 :
          idx == TOUCH_BUTTON_2 ? 2 :
          idx == TOUCH_BUTTON_3 ? 3 :
          idx == TOUCH_BUTTON_4 ? 4 :
          idx == TOUCH_BUTTON_5 ? 5 : 0;
    ch450_write_dig(2, -1);
    ch450_write_dig(3, -1);
    if (type == TOUCH_EVT_TYPE_SPRING_PUSH) {
        ch450_write_dig(0, idx);
        ch450_write_dig(1, -1);
        ch450_write_led(BIT(idx), 1);
        evb_touch_spring_led_toggle(idx);
    } else if (type == TOUCH_EVT_TYPE_SPRING_RELEASE) {
        ch450_write_dig(1, idx);
        ch450_write_dig(0, -1);
    } else {

    }
}

static void spring_push_cb(void *arg)
{
    CTouchPad* tp_dev = (CTouchPad*) arg;
    touch_pad_t tp_num = tp_dev->tp_num();
    ESP_LOGI(TAG, "push callback of touch pad num %d", tp_num);
    touch_evt_t evt;
    evt.type = TOUCH_EVT_TYPE_SPRING_PUSH;
    evt.single.idx = (int) tp_num;
    xQueueSend(q_touch, &evt, portMAX_DELAY);
}

static void spring_release_cb(void *arg)
{
    CTouchPad* tp_dev = (CTouchPad*) arg;
    touch_pad_t tp_num = tp_dev->tp_num();
    ESP_LOGI(TAG, "release callback of touch pad num %d", tp_num);
    touch_evt_t evt;
    evt.type = TOUCH_EVT_TYPE_SPRING_RELEASE;
    evt.single.idx = (int) tp_num;
    xQueueSend(q_touch, &evt, portMAX_DELAY);
}


void evb_touch_spring_init()
{
    //single touch
    tp_dev[0] = new CTouchPad(TOUCH_BUTTON_0, TOUCHPAD_THRES_PERCENT, 0, TOUCHPAD_FILTER_VALUE);
    tp_dev[1] = new CTouchPad(TOUCH_BUTTON_1, TOUCHPAD_THRES_PERCENT, 0, TOUCHPAD_FILTER_VALUE);
    tp_dev[2] = new CTouchPad(TOUCH_BUTTON_2, TOUCHPAD_THRES_PERCENT, 0, TOUCHPAD_FILTER_VALUE);
    tp_dev[3] = new CTouchPad(TOUCH_BUTTON_3, TOUCHPAD_THRES_PERCENT, 0, TOUCHPAD_FILTER_VALUE);
    tp_dev[4] = new CTouchPad(TOUCH_BUTTON_4, TOUCHPAD_THRES_PERCENT, 0, TOUCHPAD_FILTER_VALUE);
    tp_dev[5] = new CTouchPad(TOUCH_BUTTON_5, TOUCHPAD_THRES_PERCENT, 0, TOUCHPAD_FILTER_VALUE);

    tp_dev[0]->add_cb(TOUCHPAD_CB_PUSH, spring_push_cb, tp_dev[0]);
    tp_dev[1]->add_cb(TOUCHPAD_CB_PUSH, spring_push_cb, tp_dev[1]);
    tp_dev[2]->add_cb(TOUCHPAD_CB_PUSH, spring_push_cb, tp_dev[2]);
    tp_dev[3]->add_cb(TOUCHPAD_CB_PUSH, spring_push_cb, tp_dev[3]);
    tp_dev[4]->add_cb(TOUCHPAD_CB_PUSH, spring_push_cb, tp_dev[4]);
    tp_dev[5]->add_cb(TOUCHPAD_CB_PUSH, spring_push_cb, tp_dev[5]);

    tp_dev[0]->add_cb(TOUCHPAD_CB_RELEASE, spring_release_cb, tp_dev[0]);
    tp_dev[1]->add_cb(TOUCHPAD_CB_RELEASE, spring_release_cb, tp_dev[1]);
    tp_dev[2]->add_cb(TOUCHPAD_CB_RELEASE, spring_release_cb, tp_dev[2]);
    tp_dev[3]->add_cb(TOUCHPAD_CB_RELEASE, spring_release_cb, tp_dev[3]);
    tp_dev[4]->add_cb(TOUCHPAD_CB_RELEASE, spring_release_cb, tp_dev[4]);
    tp_dev[5]->add_cb(TOUCHPAD_CB_RELEASE, spring_release_cb, tp_dev[5]);
}

