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
