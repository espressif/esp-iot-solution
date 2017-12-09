/* Smart device Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"

#include "esp_alink.h"
#include "esp_alink_log.h"
#include "esp_info_store.h"

#include "iot_button.h"

typedef enum {
    ALINK_KEY_SHORT_PRESS = 1,
    ALINK_KEY_MEDIUM_PRESS,
    ALINK_KEY_LONG_PRESS,
} alink_key_t;

#define ESP_INTR_FLAG_DEFAULT 0
#define ALINK_RESET_KEY_IO    0

static xQueueHandle gpio_evt_queue = NULL;
static button_handle_t btn = NULL;

void alink_button_tap_cb(void* arg)
{
    ets_printf("tap cb, heap: %d\n", esp_get_free_heap_size());
    alink_key_t evt = ALINK_KEY_SHORT_PRESS;
    xQueueSend(gpio_evt_queue, &evt, portMAX_DELAY);
}

void alink_button_press_2s_cb(void* arg)
{
    ets_printf("press 2s, heap: %d\n", esp_get_free_heap_size());
    alink_key_t evt = ALINK_KEY_MEDIUM_PRESS;
    xQueueSend(gpio_evt_queue, &evt, portMAX_DELAY);
}

void alink_button_press_5s_cb(void* arg)
{
    ets_printf("press 5s, heap: %d\n", esp_get_free_heap_size());
    alink_key_t evt = ALINK_KEY_LONG_PRESS;
    xQueueSend(gpio_evt_queue, &evt, portMAX_DELAY);
}

void alink_key_trigger(void *arg)
{
    alink_err_t ret = 0;
    alink_key_t evt;
    if (gpio_evt_queue == NULL) {
        gpio_evt_queue = xQueueCreate(2, sizeof(alink_key_t));
    }
    btn = iot_button_create(ALINK_RESET_KEY_IO, 0);
    iot_button_set_evt_cb(btn, BUTTON_CB_TAP, alink_button_tap_cb, NULL);
    iot_button_add_custom_cb(btn, 2, alink_button_press_2s_cb, NULL);
    iot_button_add_custom_cb(btn, 5, alink_button_press_5s_cb, NULL);

    for (;;) {
        ret = xQueueReceive(gpio_evt_queue, &evt, portMAX_DELAY);
        if (ret == pdFALSE) {
            continue;
        }
        switch (evt) {
            case ALINK_KEY_SHORT_PRESS:
                alink_event_send(ALINK_EVENT_ACTIVATE_DEVICE);
                break;

            case ALINK_KEY_MEDIUM_PRESS:
                alink_event_send(ALINK_EVENT_UPDATE_ROUTER);
                break;

            case ALINK_KEY_LONG_PRESS:
                alink_event_send(ALINK_EVENT_FACTORY_RESET);
                break;

            default:
                break;
        }
    }

    vTaskDelete(NULL);
}

