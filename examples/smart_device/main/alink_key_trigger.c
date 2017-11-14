/* GPIO Example

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
#include "driver/gpio.h"
#include "esp_system.h"

#include "esp_alink.h"
#include "esp_alink_log.h"
#include "esp_info_store.h"

typedef enum {
    ALINK_KEY_SHORT_PRESS = 1,
    ALINK_KEY_MEDIUM_PRESS,
    ALINK_KEY_LONG_PRESS,
} alink_key_t;

#define ESP_INTR_FLAG_DEFAULT 0
#define ALINK_RESET_KEY_IO    0

static const char *TAG = "alink_factory_reset";
static xQueueHandle gpio_evt_queue = NULL;

void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

void alink_key_init(uint32_t key_gpio_pin)
{
    gpio_config_t io_conf;
    //interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = 1 << key_gpio_pin;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    //change gpio intrrupt type for one pin
    gpio_set_intr_type(key_gpio_pin, GPIO_INTR_ANYEDGE);

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(2, sizeof(uint32_t));

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(key_gpio_pin, gpio_isr_handler, (void *) key_gpio_pin);
}

alink_err_t alink_key_scan(TickType_t ticks_to_wait)
{
    uint32_t io_num;
    alink_err_t ret;
    BaseType_t press_key = pdFALSE;
    BaseType_t lift_key = pdFALSE;

    int backup_time = 0;

    for (;;) {
        ret = xQueueReceive(gpio_evt_queue, &io_num, ticks_to_wait);
        ALINK_ERROR_CHECK(ret != pdTRUE, ALINK_ERR, "xQueueReceive, ret:%d", ret);

        if (gpio_get_level(io_num) == 0) {
            press_key = pdTRUE;
            backup_time = system_get_time();
        } else if (press_key) {
            lift_key = pdTRUE;
            backup_time = system_get_time() - backup_time;
        }

        if (press_key & lift_key) {
            press_key = pdFALSE;
            lift_key = pdFALSE;

            if (backup_time > 5000000) {
                return ALINK_KEY_LONG_PRESS;
            } else if (backup_time > 1000000) {
                return ALINK_KEY_MEDIUM_PRESS;
            } else {
                return ALINK_KEY_SHORT_PRESS;
            }
        }
    }
}

void alink_key_trigger(void *arg)
{
    alink_err_t ret = 0;
    alink_key_init(ALINK_RESET_KEY_IO);

    for (;;) {
        ret = alink_key_scan(portMAX_DELAY);
        ALINK_ERROR_CHECK(ret == ALINK_ERR, vTaskDelete(NULL), "alink_key_scan ret:%d", ret);

        switch (ret) {
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

