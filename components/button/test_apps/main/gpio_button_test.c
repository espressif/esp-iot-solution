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
#include "button_gpio.h"

static const char *TAG = "GPIO BUTTON TEST";

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

TEST_CASE("gpio button test", "[button][gpio]")
{
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = BUTTON_IO_NUM,
        .active_level = BUTTON_ACTIVE_LEVEL,
    };

    button_handle_t btn = NULL;
    esp_err_t ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &btn);
    TEST_ASSERT(ret == ESP_OK);
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

    uint8_t level = 0;
    level = iot_button_get_key_level(btn);
    ESP_LOGI(TAG, "button level is %d", level);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    iot_button_delete(btn);
}

TEST_CASE("gpio button get event test", "[button][gpio][event test]")
{
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = BUTTON_IO_NUM,
        .active_level = BUTTON_ACTIVE_LEVEL,
    };

    button_handle_t btn = NULL;
    esp_err_t ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &btn);
    TEST_ASSERT(ret == ESP_OK);
    TEST_ASSERT_NOT_NULL(btn);

    uint8_t level = 0;
    level = iot_button_get_key_level(btn);
    ESP_LOGI(TAG, "button level is %d", level);

    while (1) {
        button_event_t event = iot_button_get_event(btn);
        if (event != BUTTON_NONE_PRESS) {
            ESP_LOGI(TAG, "event is %s", iot_button_get_event_str(event));
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    iot_button_delete(btn);
}

TEST_CASE("gpio button test power save", "[button][gpio][power save]")
{
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = BUTTON_IO_NUM,
        .active_level = BUTTON_ACTIVE_LEVEL,
        .enable_power_save = true,
    };

    button_handle_t btn = NULL;
    esp_err_t ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &btn);
    TEST_ASSERT(ret == ESP_OK);
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

TEST_CASE("gpio button test memory leak", "[button][gpio][memory leak]")
{
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = BUTTON_IO_NUM,
        .active_level = BUTTON_ACTIVE_LEVEL,
    };

    button_handle_t btn = NULL;
    esp_err_t ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &btn);
    TEST_ASSERT(ret == ESP_OK);
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

    iot_button_delete(btn);
}
