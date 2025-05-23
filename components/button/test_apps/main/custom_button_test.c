/* SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "unity.h"
#include "iot_button.h"
#include "driver/gpio.h"

static const char *TAG = "CUSTOM BUTTON TEST";

#define BUTTON_IO_NUM  0
#define BUTTON_ACTIVE_LEVEL   0

static void button_event_cb(void *arg, void *data)
{
    button_event_t event = iot_button_get_event(arg);
    ESP_LOGI(TAG, "%s", iot_button_get_event_str(event));
    if (BUTTON_PRESS_REPEAT == event || BUTTON_PRESS_REPEAT_DONE == event) {
        ESP_LOGI(TAG, "\tREPEAT[%d]", iot_button_get_repeat(arg));
    }

    if (BUTTON_PRESS_UP == event || BUTTON_LONG_PRESS_HOLD == event || BUTTON_LONG_PRESS_UP == event) {
        ESP_LOGI(TAG, "\tTICKS[%"PRIu32"]", iot_button_get_ticks_time(arg));
    }

    if (BUTTON_MULTIPLE_CLICK == event) {
        ESP_LOGI(TAG, "\tMULTIPLE[%d]", (int)data);
    }
}

typedef struct {
    button_driver_t base;
    int32_t gpio_num;              /**< num of gpio */
    uint8_t active_level;          /**< gpio level when press down */
} custom_gpio_obj;

static uint8_t button_get_key_level(button_driver_t *button_driver)
{
    custom_gpio_obj *custom_btn = __containerof(button_driver, custom_gpio_obj, base);
    int level = gpio_get_level(custom_btn->gpio_num);
    return level == custom_btn->active_level ? 1 : 0;
}

static esp_err_t button_del(button_driver_t *button_driver)
{
    return ESP_OK;
}

TEST_CASE("custom button test", "[button][custom]")
{
    gpio_config_t gpio_conf = {
        .pin_bit_mask = 1ULL << BUTTON_IO_NUM,
                             .mode = GPIO_MODE_INPUT,
                             .pull_up_en = 1,
                             .pull_down_en = 0,
                             .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&gpio_conf);

    custom_gpio_obj *custom_btn = (custom_gpio_obj *)calloc(1, sizeof(custom_gpio_obj));
    TEST_ASSERT_NOT_NULL(custom_btn);
    custom_btn->active_level = BUTTON_ACTIVE_LEVEL;
    custom_btn->gpio_num = BUTTON_IO_NUM;

    button_handle_t btn = NULL;
    const button_config_t btn_cfg = {0};
    custom_btn->base.get_key_level = button_get_key_level;
    custom_btn->base.del = button_del;
    esp_err_t ret = iot_button_create(&btn_cfg, &custom_btn->base, &btn);
    TEST_ASSERT(ESP_OK == ret);
    TEST_ASSERT_NOT_NULL(btn);
    iot_button_register_cb(btn, BUTTON_PRESS_DOWN, NULL, button_event_cb, NULL);
    iot_button_register_cb(btn, BUTTON_PRESS_UP, NULL, button_event_cb, NULL);
    iot_button_register_cb(btn, BUTTON_PRESS_REPEAT, NULL, button_event_cb, NULL);
    iot_button_register_cb(btn, BUTTON_PRESS_REPEAT_DONE, NULL, button_event_cb, NULL);
    iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, NULL, button_event_cb, NULL);
    iot_button_register_cb(btn, BUTTON_DOUBLE_CLICK, NULL, button_event_cb, NULL);

    /*!< Multiple Click must provide button_event_args_t */
    /*!< Double Click */
    button_event_args_t args = {
        .multiple_clicks.clicks = 2,
    };
    iot_button_register_cb(btn, BUTTON_MULTIPLE_CLICK, &args, button_event_cb, (void *)2);
    /*!< Triple Click */
    args.multiple_clicks.clicks = 3;
    iot_button_register_cb(btn, BUTTON_MULTIPLE_CLICK, &args, button_event_cb, (void *)3);
    iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, NULL, button_event_cb, NULL);
    iot_button_register_cb(btn, BUTTON_LONG_PRESS_HOLD, NULL, button_event_cb, NULL);
    iot_button_register_cb(btn, BUTTON_LONG_PRESS_UP, NULL, button_event_cb, NULL);
    iot_button_register_cb(btn, BUTTON_PRESS_END, NULL, button_event_cb, NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    iot_button_delete(btn);
}
