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
#include "evb.h"

static const char *TAG = "touch_button";

#define TOUCH_BUTTON_0  TOUCH_PAD_NUM8
#define TOUCH_BUTTON_1  TOUCH_PAD_NUM6
#define TOUCH_BUTTON_2  TOUCH_PAD_NUM4

void evb_touch_button_handle(int idx, int type)
{
	uint8_t color = 0;
	uint8_t bright_percent = 0;
    ESP_LOGI(TAG, "single tp evt[%d]", idx);

    idx = idx == TOUCH_BUTTON_0 ? 0 :
          idx == TOUCH_BUTTON_1 ? 1 :
          idx == TOUCH_BUTTON_2 ? 2 : 0;
    ch450_write_dig(4, -1);
    ch450_write_dig(3, -1);
    if (type == TOUCH_EVT_TYPE_SINGLE_PUSH) {
        ch450_write_dig(5, idx);
        ch450_write_dig(4, -1);
        // Set RGB LED info
        color = evb_rgb_led_color_get();
        evb_rgb_led_get(color, &bright_percent);
        evb_rgb_led_clear();
        evb_rgb_led_set(idx, bright_percent);
    } else if (type == TOUCH_EVT_TYPE_SINGLE_RELEASE) {
        ch450_write_dig(4, idx);
        ch450_write_dig(5, -1);
    }
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
    CTouchPad* tp_dev0 = new CTouchPad(TOUCH_BUTTON_0, LINEAR_SLIDER_THRESH_PERCENT, 0, LINEAR_SLIDER_FILTER_VALUE);
    CTouchPad* tp_dev1 = new CTouchPad(TOUCH_BUTTON_1, LINEAR_SLIDER_THRESH_PERCENT, 0, LINEAR_SLIDER_FILTER_VALUE);
    CTouchPad* tp_dev2 = new CTouchPad(TOUCH_BUTTON_2, LINEAR_SLIDER_THRESH_PERCENT, 0, LINEAR_SLIDER_FILTER_VALUE);
    tp_dev0->add_cb(TOUCHPAD_CB_PUSH, push_cb, tp_dev0);
    tp_dev1->add_cb(TOUCHPAD_CB_PUSH, push_cb, tp_dev1);
    tp_dev2->add_cb(TOUCHPAD_CB_PUSH, push_cb, tp_dev2);
    tp_dev0->add_cb(TOUCHPAD_CB_RELEASE, release_cb, tp_dev0);
    tp_dev1->add_cb(TOUCHPAD_CB_RELEASE, release_cb, tp_dev1);
    tp_dev2->add_cb(TOUCHPAD_CB_RELEASE, release_cb, tp_dev2);
}

