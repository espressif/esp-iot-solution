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
#include "driver/gpio.h"
#include "iot_touchpad.h"
#include "print_to_scope.h"
#include "evb.h"

static const char *TAG = "touch_slide";
static CTouchPadSlide *tp_slide = NULL;

#define TOUCH_SLIDE_0 TOUCH_PAD_NUM9
#define TOUCH_SLIDE_1 TOUCH_PAD_NUM7
#define TOUCH_SLIDE_2 TOUCH_PAD_NUM5
#define TOUCH_SLIDE_3 TOUCH_PAD_NUM3
#define TOUCH_SLIDE_4 TOUCH_PAD_NUM2
#define TOUCH_SLIDE_5 TOUCH_PAD_NUM0
#define TOUCH_SLIDE_LED_NUM_0   3
#define TOUCH_SLIDE_LED_NUM_1   4
#define TOUCH_SLIDE_PAD_NUM     6
#define TOUCH_SLIDE_MAX_POS   \
    ((TOUCH_SLIDE_PAD_NUM - 1) * TOUCH_SLIDE_PAD_SCALE)

void evb_touch_slide_handle(int pos)
{
    static int pos_prev = 0;
    if (pos_prev != pos && pos != 0xff) {
        pos_prev = pos;
        ESP_LOGI(TAG, "slide pos evt[%d]", pos);
        ch450_write_dig(0, -1);
        ch450_write_dig(1, -1);
        ch450_write_dig(2, -1);
        ch450_write_dig(3, -1);
        ch450_write_dig(TOUCH_SLIDE_LED_NUM_0, pos % 10);
        ch450_write_dig(TOUCH_SLIDE_LED_NUM_1, pos / 10);
        evb_rgb_led_set(evb_rgb_led_color_get(), pos*100/TOUCH_SLIDE_MAX_POS);
    }
}

static void scope_task(void *paramater)
{
    uint16_t data[4];
    uint8_t pos = 0;
    while (1) {
        pos = tp_slide->get_position();
        evb_touch_slide_handle(pos);
#if SCOPE_DEBUG
        touch_pad_read_filtered(TOUCH_SLIDE_1, &data[0]);
        touch_pad_read_filtered(TOUCH_SLIDE_2, &data[1]);
        touch_pad_read_filtered(TOUCH_SLIDE_3, &data[2]);
        touch_pad_read_filtered(TOUCH_SLIDE_4, &data[3]);
        print_to_scope(1, data);
#endif
        vTaskDelay(50 / portTICK_RATE_MS);
    }
}

void evb_touch_slide_init_then_run()
{
    evb_touch_button_init();
    //slide touch
    const touch_pad_t tps[] = { TOUCH_SLIDE_0, TOUCH_SLIDE_1, TOUCH_SLIDE_2,
            TOUCH_SLIDE_3, TOUCH_SLIDE_4, TOUCH_SLIDE_5 };
    tp_slide = new CTouchPadSlide(sizeof(tps) / sizeof(TOUCH_PAD_NUM4),
    		tps, TOUCH_SLIDE_PAD_SCALE, LINEAR_SLIDER_THRESH_PERCENT, NULL, LINEAR_SLIDER_FILTER_VALUE);
    xTaskCreate(scope_task, "scope", 1024*4, NULL, 3, NULL);
}
