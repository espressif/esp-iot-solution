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
#include "print_to_scope.h"
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
#define TOUCH_WHEEL_MAX_POS 	40

void touch_wheel_handle(int pos)
{
    static uint8_t red = 0, green = 0, blue = 0;
    static int pos_prev = 0;
    static int pos_rgb_led = 10;
    if (pos_prev != pos && pos != 0xff) {
        if (abs(pos - pos_prev) > TOUCH_WHEEL_MAX_POS - 10) {
            pos_rgb_led += TOUCH_WHEEL_MAX_POS;
        }
        pos_prev = pos;
        ESP_LOGI(TAG, "wheel pos evt[%d]", pos);
        ch450_write_dig(0, -1);
        ch450_write_dig(1, -1);
        ch450_write_dig(2, -1);
        ch450_write_dig(3, -1);
        ch450_write_dig(TOUCH_SLIDE_LED_NUM_0, pos % 10);
        ch450_write_dig(TOUCH_SLIDE_LED_NUM_1, pos / 10);
        if (abs(pos_rgb_led) <= TOUCH_WHEEL_MAX_POS) {
            red = pos * 100 / TOUCH_WHEEL_MAX_POS;
        } else if (abs(pos_rgb_led) <= 2 * TOUCH_WHEEL_MAX_POS) {
            green = pos * 100 / TOUCH_WHEEL_MAX_POS;
        } else if (abs(pos_rgb_led) <= 3 * TOUCH_WHEEL_MAX_POS) {
            blue = pos * 100 / TOUCH_WHEEL_MAX_POS;
        } else {
            pos_rgb_led = 10;
            red = 0;
            blue = 0;
            green = 0;
        }
        // Set RGB light
        evb_rgb_led_set(0, red);
        evb_rgb_led_set(1, green);
        evb_rgb_led_set(2, blue);
    }
}

static void scope_task(void *paramater)
{
    uint16_t data[4];
    uint8_t pos = 0;
    while (1) {
        pos = tp_wheel->get_position();
        touch_wheel_handle(pos);
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

void evb_touch_wheel_init_then_run()
{
    //slide touch
    const touch_pad_t tps[] = { TOUCH_SLIDE_0, TOUCH_SLIDE_1, TOUCH_SLIDE_2,
            TOUCH_SLIDE_3, TOUCH_SLIDE_4, TOUCH_SLIDE_5, TOUCH_SLIDE_6,
            TOUCH_SLIDE_7, TOUCH_SLIDE_8 };
    tp_wheel = new CTouchPadSlide(sizeof(tps) / sizeof(TOUCH_PAD_NUM4),
            tps, 5, TOUCH_WHEEL_THRESH_PERCENT, NULL, TOUCH_WHEEL_FILTER_VALUE);
    xTaskCreate(scope_task, "scope", 1024*4, NULL, 3, NULL);
}
