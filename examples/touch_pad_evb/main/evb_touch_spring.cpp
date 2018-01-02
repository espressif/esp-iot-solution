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

static const char *TAG = "touch_spring";
#if CONFIG_TOUCH_EB_V2
#define TOUCH_BUTTON_0  TOUCH_PAD_NUM9
#define TOUCH_BUTTON_1  TOUCH_PAD_NUM6
#define TOUCH_BUTTON_2  TOUCH_PAD_NUM5
#define TOUCH_BUTTON_3  TOUCH_PAD_NUM8
#define TOUCH_BUTTON_4  TOUCH_PAD_NUM7
#define TOUCH_BUTTON_5  TOUCH_PAD_NUM4
#define TOUCH_LED_IDX_0  0
#define TOUCH_LED_IDX_1  1
#elif CONFIG_TOUCH_EB_V3
#define TOUCH_BUTTON_0  TOUCH_PAD_NUM9
#define TOUCH_BUTTON_1  TOUCH_PAD_NUM3
#define TOUCH_BUTTON_2  TOUCH_PAD_NUM0
#define TOUCH_BUTTON_3  TOUCH_PAD_NUM7
#define TOUCH_BUTTON_4  TOUCH_PAD_NUM5
#define TOUCH_BUTTON_5  TOUCH_PAD_NUM1
#define TOUCH_LED_IDX_0  4
#define TOUCH_LED_IDX_1  3
#endif
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
#if CONFIG_TOUCH_EB_V2
    ch450_write_dig(2, -1);
    ch450_write_dig(3, -1);
#elif CONFIG_TOUCH_EB_V3
    ch450_write_dig(5, -1);
#endif
    if (type == TOUCH_EVT_TYPE_SPRING_PUSH) {
        ch450_write_dig(TOUCH_LED_IDX_0, idx);
        ch450_write_dig(TOUCH_LED_IDX_1, -1);
        ch450_write_led(BIT(idx), 1);
        evb_touch_spring_led_toggle(idx);
    } else if (type == TOUCH_EVT_TYPE_SPRING_RELEASE) {
        ch450_write_dig(TOUCH_LED_IDX_1, idx);
        ch450_write_dig(TOUCH_LED_IDX_0, -1);
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
    tp_dev[0] = new CTouchPad(TOUCH_BUTTON_0, TOUCHPAD_SPRING_THRESH_PERCENT, 0, TOUCHPAD_FILTER_VALUE);
    tp_dev[1] = new CTouchPad(TOUCH_BUTTON_1, TOUCHPAD_SPRING_THRESH_PERCENT, 0, TOUCHPAD_FILTER_VALUE);
    tp_dev[2] = new CTouchPad(TOUCH_BUTTON_2, TOUCHPAD_SPRING_THRESH_PERCENT, 0, TOUCHPAD_FILTER_VALUE);
    tp_dev[3] = new CTouchPad(TOUCH_BUTTON_3, TOUCHPAD_SPRING_THRESH_PERCENT, 0, TOUCHPAD_FILTER_VALUE);
    tp_dev[4] = new CTouchPad(TOUCH_BUTTON_4, TOUCHPAD_SPRING_THRESH_PERCENT, 0, TOUCHPAD_FILTER_VALUE);
    tp_dev[5] = new CTouchPad(TOUCH_BUTTON_5, TOUCHPAD_SPRING_THRESH_PERCENT, 0, TOUCHPAD_FILTER_VALUE);

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

