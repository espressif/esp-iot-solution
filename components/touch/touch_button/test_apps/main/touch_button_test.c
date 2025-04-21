/* SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
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
#include "touch_button.h"
#include "touch_sensor_lowlevel.h"

static const char *TAG = "GPIO BUTTON TEST";

#define TOUCH_CHANNEL_1  1
#define TOUCH_CHANNEL_2  2

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

TEST_CASE("touch button test", "[button][touch]")
{
    const button_config_t btn_cfg = {0};
    const button_touch_config_t touch_cfg = {
        .touch_channel = TOUCH_CHANNEL_1,
        .channel_threshold = 0.1,
        .skip_lowlevel_init = false,
    };

    button_handle_t btn = NULL;
    esp_err_t ret = iot_button_new_touch_button_device(&btn_cfg, &touch_cfg, &btn);
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

static void multiple_button_event_cb(void *arg, void *data)
{
    button_event_t event = iot_button_get_event(arg);
    ESP_LOGI(TAG, "touch button[%d]: %s", (int)data, iot_button_get_event_str(event));
}

TEST_CASE("touch multiple button test", "[button][touch][multiple]")
{
    touch_lowlevel_type_t *channel_type = calloc(2, sizeof(touch_lowlevel_type_t));
    TEST_ASSERT_NOT_NULL(channel_type);
    for (int i = 0; i < 2; i++) {
        channel_type[i] = TOUCH_LOWLEVEL_TYPE_TOUCH;
    }

    uint32_t touch_channel_list[] = {TOUCH_CHANNEL_1, TOUCH_CHANNEL_2};
    touch_lowlevel_config_t low_config = {
        .channel_num = 2,
        .channel_list = touch_channel_list,
        .channel_type = channel_type,
    };
    esp_err_t ret = touch_sensor_lowlevel_create(&low_config);
    TEST_ASSERT(ret == ESP_OK);
    free(channel_type);

    const button_config_t btn_cfg = {0};
    button_touch_config_t touch_cfg = {
        .touch_channel = TOUCH_CHANNEL_1,
        .channel_threshold = 0.1,
        .skip_lowlevel_init = true,
    };

    button_handle_t btn_1 = NULL;
    ret = iot_button_new_touch_button_device(&btn_cfg, &touch_cfg, &btn_1);
    TEST_ASSERT(ret == ESP_OK);
    TEST_ASSERT_NOT_NULL(btn_1);

    touch_cfg.touch_channel = TOUCH_CHANNEL_2;
    button_handle_t btn_2 = NULL;
    ret = iot_button_new_touch_button_device(&btn_cfg, &touch_cfg, &btn_2);
    TEST_ASSERT(ret == ESP_OK);
    TEST_ASSERT_NOT_NULL(btn_2);

    touch_sensor_lowlevel_start();

    iot_button_register_cb(btn_1, BUTTON_PRESS_DOWN, NULL, multiple_button_event_cb, (void *)1);
    iot_button_register_cb(btn_1, BUTTON_PRESS_UP, NULL, multiple_button_event_cb, (void *)1);
    iot_button_register_cb(btn_2, BUTTON_PRESS_DOWN, NULL, multiple_button_event_cb, (void *)2);
    iot_button_register_cb(btn_2, BUTTON_PRESS_UP, NULL, multiple_button_event_cb, (void *)2);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    iot_button_delete(btn_1);
    iot_button_delete(btn_2);
}

static void light_and_heavy_button_event_cb(void *arg, void *data)
{
    button_event_t event = iot_button_get_event(arg);
    ESP_LOGI(TAG, "touch button[%s]: %s", (int)data == 1 ? "light" : "heavy", iot_button_get_event_str(event));
}

TEST_CASE("touch button light and heavy press test", "[button][touch][light and heavy]")
{
    touch_lowlevel_type_t *channel_type = calloc(1, sizeof(touch_lowlevel_type_t));
    TEST_ASSERT_NOT_NULL(channel_type);
    for (int i = 0; i < 1; i++) {
        channel_type[i] = TOUCH_LOWLEVEL_TYPE_TOUCH;
    }

    uint32_t touch_channel_list[] = {TOUCH_CHANNEL_1};
    touch_lowlevel_config_t low_config = {
        .channel_num = 1,
        .channel_list = touch_channel_list,
        .channel_type = channel_type,
    };
    esp_err_t ret = touch_sensor_lowlevel_create(&low_config);
    TEST_ASSERT(ret == ESP_OK);
    free(channel_type);

    const button_config_t btn_cfg = {0};
    button_touch_config_t touch_cfg = {
        .touch_channel = TOUCH_CHANNEL_1,
        .channel_threshold = 0.1,
        .skip_lowlevel_init = true,
    };

    button_handle_t btn_1 = NULL;
    ret = iot_button_new_touch_button_device(&btn_cfg, &touch_cfg, &btn_1);
    TEST_ASSERT(ret == ESP_OK);
    TEST_ASSERT_NOT_NULL(btn_1);

    touch_cfg.touch_channel = TOUCH_CHANNEL_1;
    touch_cfg.channel_threshold = 0.2;
    button_handle_t btn_2 = NULL;
    ret = iot_button_new_touch_button_device(&btn_cfg, &touch_cfg, &btn_2);
    TEST_ASSERT(ret == ESP_OK);
    TEST_ASSERT_NOT_NULL(btn_2);
    touch_sensor_lowlevel_start();

    iot_button_register_cb(btn_1, BUTTON_PRESS_DOWN, NULL, light_and_heavy_button_event_cb, (void *)1);
    iot_button_register_cb(btn_1, BUTTON_PRESS_UP, NULL, light_and_heavy_button_event_cb, (void *)1);
    iot_button_register_cb(btn_2, BUTTON_PRESS_DOWN, NULL, light_and_heavy_button_event_cb, (void *)2);
    iot_button_register_cb(btn_2, BUTTON_PRESS_UP, NULL, light_and_heavy_button_event_cb, (void *)2);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    iot_button_delete(btn_1);
    iot_button_delete(btn_2);
}

TEST_CASE("touch button test memory leak", "[button][touch][memory leak]")
{
    const button_config_t btn_cfg = {0};
    const button_touch_config_t touch_cfg = {
        .touch_channel = TOUCH_CHANNEL_1,
        .channel_threshold = 0.1,
        .skip_lowlevel_init = false,
    };

    button_handle_t btn = NULL;
    esp_err_t ret = iot_button_new_touch_button_device(&btn_cfg, &touch_cfg, &btn);
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
