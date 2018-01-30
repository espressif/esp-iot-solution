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
#include "print_to_scope.h"
#include "evb.h"

static const char *TAG = "touch_matrix";

#define TOUCH_MATRIX_X_0 TOUCH_PAD_NUM9
#define TOUCH_MATRIX_X_1 TOUCH_PAD_NUM7
#define TOUCH_MATRIX_X_2 TOUCH_PAD_NUM5
#define TOUCH_MATRIX_Y_0 TOUCH_PAD_NUM3
#define TOUCH_MATRIX_Y_1 TOUCH_PAD_NUM2
#define TOUCH_MATRIX_Y_2 TOUCH_PAD_NUM0
#define CHANNEL_R_IO   ((gpio_num_t)18)
#define CHANNEL_G_IO   ((gpio_num_t)21)
#define CHANNEL_B_IO   ((gpio_num_t)19)

/* RGB light value. */
static const uint16_t rgb_light_temp[9][3] = {
		{255, 255, 153},	// Yellow
		{255, 102,   0},	// Orange
		{255,  51,  51},	// Red
		{255,   0, 204},	// Pink
		{255,  51, 255},	// Amaranth
		{102, 102, 255},	// Blue
		{  0, 204, 255},	// Sky blue
		{ 51, 255, 153},	// Blue-green
		{204, 255 ,  0}		// Yellow-green
};

void evb_touch_matrix_handle(int x, int y, int type)
{
    static int t = 0;
    if (type == TOUCH_EVT_TYPE_MATRIX) {
        ESP_LOGI(TAG, "matrix tp evt[%d][%d]", x, y);
        t = 0;
        ch450_write_dig(5, x);
        ch450_write_dig(4, y);
        ch450_write_dig(3, -1);
        evb_rgb_led_set(0, rgb_light_temp[x + 3 * y][0] * 100 / 255);
        evb_rgb_led_set(1, rgb_light_temp[x + 3 * y][1] * 100 / 255);
        evb_rgb_led_set(2, rgb_light_temp[x + 3 * y][2] * 100 / 255);
    } else if (type == TOUCH_EVT_TYPE_MATRIX_RELEASE) {
        ch450_write_dig(3, -1);
    } else if (type == TOUCH_EVT_TYPE_MATRIX_SERIAL) {
        t++;
        ch450_write_dig(3, t % 10);
    }
}

static void app_matrix_cb(void *arg, uint8_t x, uint8_t y)
{
    const char *event = (char*) arg;
    ESP_LOGI(TAG, "%s of touchpad matrix (%d, %d)", event, x, y);
    touch_evt_t evt;
    evt.type = TOUCH_EVT_TYPE_MATRIX;
    evt.matrix.x = x;
    evt.matrix.y = y;
    xQueueSend(q_touch, &evt, portMAX_DELAY);
}

static void app_matrix_release_cb(void *arg, uint8_t x, uint8_t y)
{
    const char *event = (char*) arg;
    ESP_LOGI(TAG, "%s of touchpad matrix released (%d, %d)", event, x, y);
    touch_evt_t evt;
    evt.type = TOUCH_EVT_TYPE_MATRIX_RELEASE;
    evt.matrix.x = x;
    evt.matrix.y = y;
    xQueueSend(q_touch, &evt, portMAX_DELAY);
}

static void app_matrix_serial_cb(void *arg, uint8_t x, uint8_t y)
{
    const char *event = (char*) arg;
    ESP_LOGI(TAG, "%s of touchpad matrix released (%d, %d)", event, x, y);
    touch_evt_t evt;
    evt.type = TOUCH_EVT_TYPE_MATRIX_SERIAL;
    evt.matrix.x = x;
    evt.matrix.y = y;
    xQueueSend(q_touch, &evt, portMAX_DELAY);
}

#if SCOPE_DEBUG
static void scope_task(void *paramater)
{
    uint16_t data[4];
    while (1) {
        touch_pad_read_filtered(TOUCH_MATRIX_X_0, &data[0]);
        touch_pad_read_filtered(TOUCH_MATRIX_X_1, &data[1]);
        touch_pad_read_filtered(TOUCH_MATRIX_Y_0, &data[2]);
        touch_pad_read_filtered(TOUCH_MATRIX_Y_1, &data[3]);
        print_to_scope(1, data);
        vTaskDelay(50 / portTICK_RATE_MS);
    }
}
#endif

void evb_touch_matrix_init()
{
    //matrix touch
    const touch_pad_t x_tps[] = { TOUCH_MATRIX_X_0, TOUCH_MATRIX_X_1, TOUCH_MATRIX_X_2 };
    const touch_pad_t y_tps[] = { TOUCH_MATRIX_Y_0, TOUCH_MATRIX_Y_1, TOUCH_MATRIX_Y_2 };
    const uint16_t *thresh = NULL;
    CTouchPadMatrix *tp_matrix = new CTouchPadMatrix(sizeof(x_tps) / sizeof(x_tps[0]), sizeof(y_tps) / sizeof(y_tps[0]),
            x_tps, y_tps, MATRIX_BUTTON_THRESH_PERCENT, thresh, MATRIX_BUTTON_FILTER_VALUE);
    tp_matrix->add_cb(TOUCHPAD_CB_PUSH, app_matrix_cb, (void*) "push_event");
    tp_matrix->add_cb(TOUCHPAD_CB_RELEASE, app_matrix_release_cb, (void*) "release_event");
    tp_matrix->set_serial_trigger(1, 1000, app_matrix_serial_cb,  (void*) "serial_event");
#if SCOPE_DEBUG
    xTaskCreate(scope_task, "scope", 1024*4, NULL, 3, NULL);
#endif
}

