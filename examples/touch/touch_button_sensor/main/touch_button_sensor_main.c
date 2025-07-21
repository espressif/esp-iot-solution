/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "touch_button_sensor.h"

#if CONFIG_IDF_TARGET_ESP32P4
uint32_t channel_list[] = {7, 9, 11};           // gpio 9, 11, 13
float channel_threshold[] = {0.01, 0.01, 0.01}; // 1%
#define TOUCH_DEBOUNCE_TIMES 2
#elif CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32S2
uint32_t channel_list[] = {8, 10, 12}; // gpio 8, 10, 12
float channel_threshold[] = {0.02, 0.02, 0.02}; // 2%
#define TOUCH_DEBOUNCE_TIMES 2
#elif CONFIG_IDF_TARGET_ESP32
uint32_t channel_list[] = {8, 6, 4}; // gpio 33, 14, 13
float channel_threshold[] = {0.01, 0.01, 0.01}; // 1%
#define TOUCH_DEBOUNCE_TIMES 2
#endif

static const char *TAG = "touch_button_sensor";

static void touch_state_callback(touch_button_handle_t handle, uint32_t channel, touch_state_t state, void *user_data)
{
    if (state == TOUCH_STATE_ACTIVE) {
        ESP_LOGI(TAG, "Channel %"PRIu32" is Active", channel);
    } else if (state == TOUCH_STATE_INACTIVE) {
        ESP_LOGI(TAG, "Channel %"PRIu32" is Inactive", channel);
    }
}

/* Task function to handle touch button events */
static void touch_button_task(void *pvParameters)
{
    touch_button_handle_t handle = (touch_button_handle_t)pvParameters;
    while (1) {
        touch_button_sensor_handle_events(handle);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void app_main(void)
{
    touch_button_config_t config = {
        .channel_num = sizeof(channel_list) / sizeof(channel_list[0]),
        .channel_list = channel_list,
        .channel_threshold = channel_threshold,
        .debounce_times = TOUCH_DEBOUNCE_TIMES,
        .skip_lowlevel_init = false
    };

    touch_button_handle_t handle;
    ESP_ERROR_CHECK(touch_button_sensor_create(&config, &handle, touch_state_callback, NULL));

    // Create a task to handle touch button events
    xTaskCreate(touch_button_task, "touch_button_sensor", 3072, handle, 2, NULL);

    // Main loop can do other things now
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Main task can do other work or just idle
    }
}
