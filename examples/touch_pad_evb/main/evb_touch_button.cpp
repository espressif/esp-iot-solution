/* Touch Sensor Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
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

