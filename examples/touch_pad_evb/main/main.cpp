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
#include "evb.h"

static const char* TAG = "evb_main";
QueueHandle_t q_touch = NULL;

void touch_evt_handle_task(void* arg)
{
    touch_evt_t evt;
    while (1) {
        portBASE_TYPE ret = xQueueReceive(q_touch, &evt, portMAX_DELAY);
        if (ret == pdTRUE) {
            switch (evt.type) {
                case TOUCH_EVT_TYPE_SPRING_PUSH:
                case TOUCH_EVT_TYPE_SPRING_RELEASE:
                    evb_touch_spring_handle(evt.single.idx, evt.type);
                    break;
                case TOUCH_EVT_TYPE_SINGLE:
                case TOUCH_EVT_TYPE_SINGLE_PUSH:
                case TOUCH_EVT_TYPE_SINGLE_RELEASE:
                    evb_touch_button_handle(evt.single.idx, evt.type);
                    break;
                case TOUCH_EVT_TYPE_MATRIX:
                case TOUCH_EVT_TYPE_MATRIX_RELEASE:
                case TOUCH_EVT_TYPE_MATRIX_SERIAL:
                    evb_touch_matrix_handle(evt.matrix.x, evt.matrix.y, evt.type);
                    break;
                default:
                    break;
            }
        }
    }
}

void evb_component_init()
{
    ESP_LOGI(TAG, "components init...");
    evb_adc_init();
#if CONFIG_TOUCH_EB_V1
    evb_led_init();
    evb_touch_button_init();
#endif
    ch450_dev_init();
}

extern "C" void app_main()
{
    evb_component_init();
    if (q_touch == NULL) {
        q_touch = xQueueCreate(10, sizeof(touch_evt_t));
    }
    xTaskCreate(touch_evt_handle_task, "touch_evt_handle_task", 1024 * 8, NULL, 6, NULL);

    int mode = evb_adc_get_mode();
    if (mode == TOUCH_EVB_MODE_MATRIX) {
        evb_touch_matrix_init();
    } else if (mode == TOUCH_EVB_MODE_SLIDE) {
        evb_touch_slide_init_then_run();
    } else if (mode ==  TOUCH_EVB_MODE_SPRING) {
        evb_touch_spring_init();
    } else if (mode == TOUCH_EVB_MODE_CIRCLE) {
        evb_touch_circle_init_then_run();
    }
}
