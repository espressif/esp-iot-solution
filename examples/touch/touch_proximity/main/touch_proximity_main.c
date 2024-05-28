/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
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

const static char *TAG = "touch-prox-example";

#define IO_BUZZER_CTRL  36

static touch_proximity_handle_t s_touch_proximity_sensor = NULL;

void example_proxi_callback(uint32_t channel, proxi_evt_t event, void *cb_arg)
{
    switch (event) {
    case PROXI_EVT_ACTIVE:
        buzzer_set_voice(1);
        ESP_LOGI(TAG, "CH%"PRIu32", active!", channel);
        break;
    case PROXI_EVT_INACTIVE:
        buzzer_set_voice(0);
        ESP_LOGI(TAG, "CH%"PRIu32", inactive!", channel);
        break;
    default:
        break;
    }
}

void app_main(void)
{
    buzzer_driver_install(IO_BUZZER_CTRL);
    proxi_config_t config = (proxi_config_t)DEFAULTS_PROX_CONFIGS();
    config.channel_num = 1;
    config.response_ms = 20;
    config.channel_list[0] = TOUCH_PAD_NUM8;
    config.max_p = 0.01;
    config.min_n = 0.01;
    config.baseline_coef = 0.25;
    config.threshold_p[0] = 0.00015;
    config.threshold_n[0] = 0.0002;
    config.hysteresis_p = 0.2;
    config.noise_p = 0.01;
    config.noise_n = 0.01;
    config.debounce_p = 6;
    esp_err_t ret = touch_proximity_sensor_create(&config, &s_touch_proximity_sensor, &example_proxi_callback, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "touch proximity sense create failed");
    }
    touch_proximity_sensor_start(s_touch_proximity_sensor);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "touch proximity sensor has started! when you approach the touch sub-board, the buzzer will sound.");
}
