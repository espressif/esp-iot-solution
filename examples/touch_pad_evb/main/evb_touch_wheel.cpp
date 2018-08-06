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
#include "iot_touchpad.h"
#include "iot_led.h"
#include "iot_light.h"
#include "evb.h"

static const char *TAG = "touch_wheel";
static CTouchPadSlide *tp_wheel = NULL;

#define TOUCH_SLIDE_0 TOUCH_PAD_NUM3
#define TOUCH_SLIDE_1 TOUCH_PAD_NUM4
#define TOUCH_SLIDE_2 TOUCH_PAD_NUM5
#define TOUCH_SLIDE_3 TOUCH_PAD_NUM6
#define TOUCH_SLIDE_4 TOUCH_PAD_NUM7
#define TOUCH_SLIDE_5 TOUCH_PAD_NUM9
#define TOUCH_SLIDE_6 TOUCH_PAD_NUM8
#define TOUCH_SLIDE_7 TOUCH_PAD_NUM0
#define TOUCH_SLIDE_8 TOUCH_PAD_NUM2
#define TOUCH_SLIDE_LED_NUM_0	3
#define TOUCH_SLIDE_LED_NUM_1	4
#define TOUCH_SLIDE_LED_NUM_2   5
#define TOUCH_WHEEL_MAX_POS 	TOUCH_WHEEL_PAD_RANGE

void touch_wheel_handle(int pos)
{
    static int pre_pos = pos;
    if (pos != 0xff && pre_pos != pos ) {
        pre_pos = pos;
        ESP_LOGI(TAG, "wheel pos evt[%d]", pos);
        ch450_write_dig(0, -1);
        ch450_write_dig(1, -1);
        ch450_write_dig(2, -1);
        ch450_write_dig(3, -1);
        ch450_write_dig(TOUCH_SLIDE_LED_NUM_0, pos % 10);
        ch450_write_dig(TOUCH_SLIDE_LED_NUM_1, pos%100/10);
        ch450_write_dig(TOUCH_SLIDE_LED_NUM_2, pos/100);
        // Set RGB light
        evb_rgb_led_set(0, pos * 100 / TOUCH_WHEEL_MAX_POS);
        evb_rgb_led_set(1, pos * 100 / TOUCH_WHEEL_MAX_POS);
        evb_rgb_led_set(2, pos * 100 / TOUCH_WHEEL_MAX_POS);
    }
}

static void scope_task(void *paramater)
{
    uint8_t pos = 0;
    while (1) {
        pos = tp_wheel->get_position();
        touch_wheel_handle(pos);
        vTaskDelay(50 / portTICK_RATE_MS);
    }
}

void evb_touch_wheel_init_then_run()
{
    const float variation[] = { WHEEL_SLIDER_MAX_CHANGE_RATE_0,
                                WHEEL_SLIDER_MAX_CHANGE_RATE_1,
                                WHEEL_SLIDER_MAX_CHANGE_RATE_2,
                                WHEEL_SLIDER_MAX_CHANGE_RATE_3,
                                WHEEL_SLIDER_MAX_CHANGE_RATE_4,
                                WHEEL_SLIDER_MAX_CHANGE_RATE_5,
                                WHEEL_SLIDER_MAX_CHANGE_RATE_6,
                                WHEEL_SLIDER_MAX_CHANGE_RATE_7,
                                WHEEL_SLIDER_MAX_CHANGE_RATE_8 };
    const touch_pad_t tps[] = { TOUCH_SLIDE_0, TOUCH_SLIDE_1, TOUCH_SLIDE_2,
            TOUCH_SLIDE_3, TOUCH_SLIDE_4, TOUCH_SLIDE_5, TOUCH_SLIDE_6,
            TOUCH_SLIDE_7, TOUCH_SLIDE_8 };
    tp_wheel = new CTouchPadSlide(sizeof(tps) / sizeof(TOUCH_PAD_NUM4),
            tps, TOUCH_WHEEL_PAD_RANGE, variation);
    xTaskCreate(scope_task, "scope", 1024*4, NULL, 3, NULL);
#ifdef CONFIG_DATA_SCOPE_DEBUG
    tune_dev_comb_t ch_comb = {};
    ch_comb.dev_comb = TUME_CHAR_SLIDE;
    ch_comb.ch_num_h = 1;
    ch_comb.ch_num_l = sizeof(tps) / sizeof(TOUCH_SLIDE_0);
    for(int i=0; i<ch_comb.ch_num_l; i++) {
        ch_comb.ch[i] = tps[i];
    }
    tune_tool_add_device_setting(&ch_comb);
#endif
}
