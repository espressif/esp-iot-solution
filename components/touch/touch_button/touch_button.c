/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "iot_button.h"
#include "touch_button_sensor.h"
#include "touch_button.h"

static const char *TAG = "button_touch";
typedef struct {
    button_driver_t base;
    int32_t touch_channel;
    touch_button_handle_t touch_handle;  /**< touch handle */
} button_touch_obj;

static uint8_t button_touch_get_key_level(button_driver_t *button_driver)
{
    button_touch_obj *touch_btn = __containerof(button_driver, button_touch_obj, base);
    touch_button_sensor_handle_events(touch_btn->touch_handle);
    touch_state_t state;
    touch_button_sensor_get_state(touch_btn->touch_handle, touch_btn->touch_channel, &state);
    return state == TOUCH_STATE_ACTIVE ? BUTTON_ACTIVE : BUTTON_INACTIVE;
}

static esp_err_t button_touch_delete(button_driver_t *button_driver)
{
    button_touch_obj *touch_btn = __containerof(button_driver, button_touch_obj, base);
    esp_err_t ret = touch_button_sensor_delete(touch_btn->touch_handle);
    free(touch_btn);
    return ret;
}

esp_err_t iot_button_new_touch_button_device(const button_config_t *button_config, const button_touch_config_t *touch_config, button_handle_t *ret_button)
{
    ESP_LOGI(TAG, "Touch Button Version: %d.%d.%d", TOUCH_BUTTON_VER_MAJOR, TOUCH_BUTTON_VER_MINOR, TOUCH_BUTTON_VER_PATCH);
    button_touch_obj *touch_btn = NULL;
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(button_config && touch_config && ret_button, ESP_ERR_INVALID_ARG, err, TAG, "Invalid argument");

    touch_btn = (button_touch_obj *)calloc(1, sizeof(button_touch_obj));
    ESP_GOTO_ON_FALSE(touch_btn, ESP_ERR_NO_MEM, err, TAG, "No memory for gpio button");

    touch_btn->touch_channel = touch_config->touch_channel;

    uint32_t channel_list[1] = {touch_config->touch_channel};
    float button_threshold[1] = {touch_config->channel_threshold};

    touch_button_config_t config = {
        .channel_num = 1,
        .channel_list = channel_list,
        .channel_threshold = button_threshold,
        .skip_lowlevel_init = touch_config->skip_lowlevel_init,
        .debounce_times = 1,
    };
    ret = touch_button_sensor_create(&config, &touch_btn->touch_handle, NULL, NULL);
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "Create touch button sensor failed");
    ret = iot_button_create(button_config, &touch_btn->base, ret_button);
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "Create button failed");

    touch_btn->base.get_key_level = button_touch_get_key_level;
    touch_btn->base.del = button_touch_delete;

    return ESP_OK;
err:
    if (touch_btn) {
        free(touch_btn);
    }
    return ret;
}
