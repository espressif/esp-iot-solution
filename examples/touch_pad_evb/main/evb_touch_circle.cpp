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
#include "iot_touchpad.h"
#include "evb.h"

static const char *TAG = "touch_circle";
#define TOUCH_SLIDE_0 TOUCH_PAD_NUM8
#define TOUCH_SLIDE_1 TOUCH_PAD_NUM2
#define TOUCH_SLIDE_2 TOUCH_PAD_NUM7
#define TOUCH_SLIDE_3 TOUCH_PAD_NUM6
#define TOUCH_SLIDE_4 TOUCH_PAD_NUM3
#define TOUCH_SLIDE_5 TOUCH_PAD_NUM4
#define TOUCH_SLIDE_6 TOUCH_PAD_NUM5
#define TOUCH_SLIDE_7 TOUCH_PAD_NUM0
#define TOUCH_SLIDE_8 TOUCH_PAD_NUM9
#define TOUCH_SLIDE_LED_NUM_0     3
#define TOUCH_SLIDE_LED_NUM_1     2

void touch_circle_handle(int pos)
{
    static int pos_prev = 0;
    if (pos_prev != pos && pos != 0xff) {
        pos_prev = pos;
        ESP_LOGI(TAG, "circle pos evt[%d]", pos);
        ch450_write_dig(0, -1);
        ch450_write_dig(1, -1);
        ch450_write_dig(2, -1);
        ch450_write_dig(3, -1);
        ch450_write_dig(TOUCH_SLIDE_LED_NUM_0, pos % 10);
        ch450_write_dig(TOUCH_SLIDE_LED_NUM_1, pos / 10);
    }
}

void evb_touch_circle_init_then_run()
{
    //slide touch
    const touch_pad_t tps[] = { TOUCH_SLIDE_0, TOUCH_SLIDE_1, TOUCH_SLIDE_2, TOUCH_SLIDE_3, TOUCH_SLIDE_4,
            TOUCH_SLIDE_5, TOUCH_SLIDE_6, TOUCH_SLIDE_7, TOUCH_SLIDE_8 };
    CTouchPadSlide *tp_slide = new CTouchPadSlide(sizeof(tps) / sizeof(TOUCH_PAD_NUM4), tps, 5, TOUCHPAD_THRES_PERCENT,
            NULL);
    while (1) {
        uint8_t pos = tp_slide->get_position();
        touch_circle_handle(pos);
        vTaskDelay(50 / portTICK_RATE_MS);
    }
}
