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
#include "driver/gpio.h"
#include "iot_touchpad.h"
#include "iot_led.h"
#include "iot_light.h"
#include "evb.h"

static const char *TAG = "touch_slide";
#if CONFIG_TOUCH_EB_V2
#define TOUCH_SLIDE_0 TOUCH_PAD_NUM1
#define TOUCH_SLIDE_1 TOUCH_PAD_NUM3
#define TOUCH_SLIDE_2 TOUCH_PAD_NUM2
#define TOUCH_SLIDE_3 TOUCH_PAD_NUM5
#define TOUCH_SLIDE_4 TOUCH_PAD_NUM4
#define TOUCH_SLIDE_5 TOUCH_PAD_NUM7
#define TOUCH_SLIDE_6 TOUCH_PAD_NUM9
#define TOUCH_SLIDE_7 TOUCH_PAD_NUM8
#define TOUCH_SLIDE_8 TOUCH_PAD_NUM1
#define TOUCH_SLIDE_9 TOUCH_PAD_NUM5
#define TOUCH_SLIDE_10 TOUCH_PAD_NUM9
#define TOUCH_SLIDE_11 TOUCH_PAD_NUM3
#define TOUCH_SLIDE_12 TOUCH_PAD_NUM4
#define TOUCH_SLIDE_13 TOUCH_PAD_NUM8
#define TOUCH_SLIDE_14 TOUCH_PAD_NUM2
#define TOUCH_SLIDE_15 TOUCH_PAD_NUM7
#define TOUCH_SLIDE_LED_NUM_0     3
#define TOUCH_SLIDE_LED_NUM_1     4
#elif CONFIG_TOUCH_EB_V3
#define TOUCH_SLIDE_0 TOUCH_PAD_NUM1
#define TOUCH_SLIDE_1 TOUCH_PAD_NUM3
#define TOUCH_SLIDE_2 TOUCH_PAD_NUM2
#define TOUCH_SLIDE_3 TOUCH_PAD_NUM5
#define TOUCH_SLIDE_4 TOUCH_PAD_NUM4
#define TOUCH_SLIDE_5 TOUCH_PAD_NUM7
#define TOUCH_SLIDE_6 TOUCH_PAD_NUM9
#define TOUCH_SLIDE_7 TOUCH_PAD_NUM8
#define TOUCH_SLIDE_8 TOUCH_PAD_NUM1
#define TOUCH_SLIDE_9 TOUCH_PAD_NUM5
#define TOUCH_SLIDE_10 TOUCH_PAD_NUM9
#define TOUCH_SLIDE_11 TOUCH_PAD_NUM3
#define TOUCH_SLIDE_12 TOUCH_PAD_NUM4
#define TOUCH_SLIDE_13 TOUCH_PAD_NUM8
#define TOUCH_SLIDE_14 TOUCH_PAD_NUM2
#define TOUCH_SLIDE_15 TOUCH_PAD_NUM7
#define TOUCH_SLIDE_LED_NUM_0     3
#define TOUCH_SLIDE_LED_NUM_1     4
#define CHANNEL_R_IO   ((gpio_num_t)18)
#define CHANNEL_G_IO   ((gpio_num_t)21)
#define CHANNEL_B_IO   ((gpio_num_t)19)
#endif

static CLight *light = NULL;
void evb_touch_seq_slide_handle(int pos)
{
    static int pos_prev = 0;
    if (pos_prev != pos && pos != 0xff) {
        pos_prev = pos;
        ESP_LOGI(TAG, "slide pos evt[%d]", pos);
        ch450_write_dig(5, -1);
        ch450_write_dig(4, -1);
        ch450_write_dig(3, -1);
        ch450_write_dig(TOUCH_SLIDE_LED_NUM_0, pos % 10);
        ch450_write_dig(TOUCH_SLIDE_LED_NUM_1, pos / 10);
        int red = pos < 25 ? pos * light->get_full_duty() / 25:
                  pos < 50 ? (50 - pos) * light->get_full_duty() / 25 : 0;
        light->red.duty(red, LIGHT_DUTY_FADE_1S);
        int green = pos < 25 ? 0:
                  pos < 50 ? (pos - 25) * light->get_full_duty() / 25 :
                  pos < 75 ? (75 - pos) * light->get_full_duty() / 25 : 0;
        light->green.duty(green, LIGHT_DUTY_FADE_1S);
    }
}

void evb_touch_seq_slide_init_then_run()
{
    //slide touch
    const touch_pad_t tps[] = { TOUCH_SLIDE_0, TOUCH_SLIDE_1, TOUCH_SLIDE_2, TOUCH_SLIDE_3, TOUCH_SLIDE_4,
            TOUCH_SLIDE_5, TOUCH_SLIDE_6, TOUCH_SLIDE_7, TOUCH_SLIDE_8, TOUCH_SLIDE_9, TOUCH_SLIDE_10,
            TOUCH_SLIDE_11, TOUCH_SLIDE_12, TOUCH_SLIDE_13, TOUCH_SLIDE_14, TOUCH_SLIDE_15 };
    CTouchPadSlide *tp_slide = new CTouchPadSlide(sizeof(tps) / sizeof(TOUCH_PAD_NUM4), tps, 5, TOUCHPAD_THRES_PERCENT,
            NULL);
#if CONFIG_TOUCH_EB_V3
    if (light == NULL) {
        light = new CLight(LIGHT_CH_NUM_3);
    }
    light->red.init(CHANNEL_R_IO, LEDC_CHANNEL_0);
    light->green.init(CHANNEL_G_IO, LEDC_CHANNEL_1);
    light->blue.init(CHANNEL_B_IO, LEDC_CHANNEL_2);
#endif
    while (1) {
        uint8_t pos = tp_slide->get_position();
        evb_touch_seq_slide_handle(pos);
        vTaskDelay(50 / portTICK_RATE_MS);
    }
}
