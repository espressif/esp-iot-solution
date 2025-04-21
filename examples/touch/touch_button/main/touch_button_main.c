/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iot_button.h"
#include "touch_button.h"
#include "touch_sensor_lowlevel.h"

#define TOUCH_CHANNEL_1        (1)
#define LIGHT_TOUCH_THRESHOLD  (0.15)
#define HEAVY_TOUCH_THRESHOLD  (0.25)

static const char *TAG = "touch_button";

static void light_button_event_cb(void *arg, void *data)
{
    button_event_t event = iot_button_get_event(arg);
    ESP_LOGI(TAG, "Light Button: %s", iot_button_get_event_str(event));
}

static void heavy_button_event_cb(void *arg, void *data)
{
    button_event_t event = iot_button_get_event(arg);
    ESP_LOGI(TAG, "Heavy Button: %s", iot_button_get_event_str(event));
}

void app_main(void)
{
    touch_lowlevel_type_t *channel_type = calloc(1, sizeof(touch_lowlevel_type_t));
    assert(channel_type);
    channel_type[0] = TOUCH_LOWLEVEL_TYPE_TOUCH;
    uint32_t touch_channel_list[] = {TOUCH_CHANNEL_1};
    touch_lowlevel_config_t low_config = {
        .channel_num = 1,
        .channel_list = touch_channel_list,
        .channel_type = channel_type,
    };
    esp_err_t ret = touch_sensor_lowlevel_create(&low_config);
    assert(ret == ESP_OK);
    free(channel_type);

    const button_config_t btn_cfg = {0};
    button_touch_config_t touch_cfg = {
        .touch_channel = TOUCH_CHANNEL_1,
        .channel_threshold = LIGHT_TOUCH_THRESHOLD,
        .skip_lowlevel_init = true,
    };

    button_handle_t light_btn = NULL;
    ret = iot_button_new_touch_button_device(&btn_cfg, &touch_cfg, &light_btn);
    assert(ret == ESP_OK);

    touch_cfg.channel_threshold = HEAVY_TOUCH_THRESHOLD;
    button_handle_t heavy_btn = NULL;
    ret = iot_button_new_touch_button_device(&btn_cfg, &touch_cfg, &heavy_btn);
    assert(ret == ESP_OK);

    touch_sensor_lowlevel_start();

    iot_button_register_cb(light_btn, BUTTON_PRESS_DOWN, NULL, light_button_event_cb, NULL);
    iot_button_register_cb(light_btn, BUTTON_PRESS_UP, NULL, light_button_event_cb, NULL);
    iot_button_register_cb(heavy_btn, BUTTON_PRESS_DOWN, NULL, heavy_button_event_cb, NULL);
    iot_button_register_cb(heavy_btn, BUTTON_PRESS_UP, NULL, heavy_button_event_cb, NULL);

    // Main loop can do other things now
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Main task can do other work or just idle
    }
}
