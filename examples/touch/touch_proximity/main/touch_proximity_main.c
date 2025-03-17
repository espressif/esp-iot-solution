/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "touch_proximity_sensor.h"
#include "buzzer.h"

static const char *TAG = "touch-prox-example";
static touch_proximity_handle_t s_touch_proximity_sensor = NULL;
#define IO_BUZZER_CTRL          36

void example_proxi_callback(uint32_t channel, proxi_state_t event, void *cb_arg)
{
    switch (event) {
    case PROXI_STATE_ACTIVE:
        buzzer_set_voice(1);
        ESP_LOGI(TAG, "CH%"PRIu32", active!", channel);
        break;
    case PROXI_STATE_INACTIVE:
        buzzer_set_voice(0);
        ESP_LOGI(TAG, "CH%"PRIu32", inactive!", channel);
        break;
    default:
        break;
    }
}

static void proximity_task(void *arg)
{
    while (1) {
        touch_proximity_sensor_handle_events(s_touch_proximity_sensor);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void app_main(void)
{
    buzzer_driver_install(IO_BUZZER_CTRL);

    uint32_t channel_list[] = {TOUCH_PAD_NUM8, TOUCH_PAD_NUM10, TOUCH_PAD_NUM12};
    float channel_threshold[] = {0.008f, 0.008f, 0.008f};
    touch_proxi_config_t config = {
        .channel_num = 3,
        .channel_list = channel_list,
        .channel_threshold = channel_threshold,
        .debounce_times = 2,
        .skip_lowlevel_init = false,
    };

    esp_err_t ret = touch_proximity_sensor_create(&config, &s_touch_proximity_sensor, &example_proxi_callback, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "touch proximity sensor create failed");
        return;
    }

    xTaskCreate(proximity_task, "proximity_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "touch proximity sensor started - approach the touch sub-board to trigger buzzer");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
