/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "touch_proximity_sensor.h"
#include "buzzer.h"

const char *TAG = "main";
#define IO_BUZZER_CTRL 36

void example_proxi_callback(uint32_t channel, proxi_evt_t event, void *cb_arg)
{
    switch (event) {
    case PROXI_EVT_ACTIVE:
        buzzer_set_voice(1);
        ESP_LOGI(TAG, "CH%u, active!", channel);
        break;
    case PROXI_EVT_INACTIVE:
        buzzer_set_voice(0);
        ESP_LOGI(TAG, "CH%u, inactive!", channel);
        break;
    default:
        break;
    }
}

void app_main(void)
{
    buzzer_driver_install(IO_BUZZER_CTRL);
    buzzer_set_voice(0);
    proxi_config_t config = (proxi_config_t)DEFAULTS_PROX_CONFIGS();
    config.channel_num = 1;
    config.response_ms = 50;
    config.channel_list[0] = TOUCH_PAD_NUM8;
    config.threshold_p[0] = 0.004;
    config.threshold_n[0] = 0.004;
    config.noise_p = 0.001;
    config.debounce_p = 1;
    touch_proximity_sense_start(&config, &example_proxi_callback, NULL);
    while (1) {
        vTaskDelay(40 / portTICK_PERIOD_MS);
    }
    //just for example, never comes here
    touch_proximity_sense_stop();
}
