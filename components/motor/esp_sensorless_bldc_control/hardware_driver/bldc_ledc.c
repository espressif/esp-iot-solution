/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bldc_helper.h"
#include "bldc_ledc.h"
#include "esp_log.h"

static const char *TAG = "bldc_ledc";

esp_err_t bldc_ledc_init(bldc_ledc_config_t *ledc_config)
{
    BLDC_CHECK(ledc_config != NULL, "Invalid ledc configuration", ESP_ERR_INVALID_STATE);
    ledc_timer_config_t ledc_timer = LEDC_TIMER_CONFIG_DEFAULT();
    ledc_channel_config_t ledc_channel_cfg = LEDC_CHANNEL_CONFIG_DEFAULT();
    esp_err_t err;
    err = ledc_timer_config(&ledc_timer);
    BLDC_CHECK(err == ESP_OK, "Invalid ledc timer configuration", ESP_FAIL);
    for (int i = 0; i < 3; i++) {
        ledc_channel_cfg.channel = ledc_config->ledc_channel[i];
        ledc_channel_cfg.gpio_num = ledc_config->gpio_num[i];
        err = ledc_channel_config(&ledc_channel_cfg);
        BLDC_CHECK(err == ESP_OK, "Invalid ledc channel configuration", ESP_FAIL);
    }

    return ESP_OK;
}

esp_err_t bldc_ledc_deinit()
{
    return ESP_OK;
}

esp_err_t bldc_ledc_set_duty(void *channel, uint32_t duty)
{
    uint32_t channel_num = (uint32_t)channel;
    ledc_set_duty(BLDC_LEDC_MODE, channel_num, duty);
    ledc_update_duty(BLDC_LEDC_MODE, channel_num);
    return ESP_OK;
}
