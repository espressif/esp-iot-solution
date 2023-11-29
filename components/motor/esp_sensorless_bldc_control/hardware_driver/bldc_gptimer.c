/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bldc_gptimer.h"

static const char *TAG = "bldc_gptimer";

esp_err_t bldc_gptimer_init(bldc_gptimer_config_t *config)
{
    BLDC_CHECK(config != NULL, "Pointer of config is NULL", ESP_ERR_INVALID_ARG);
    BLDC_CHECK(config->gptimer != NULL, "Pointer of gptimer is NULL", ESP_ERR_INVALID_ARG);
    gptimer_config_t timer_config = GPTIMER_CONFIG_DEFAULT();
    esp_err_t err = gptimer_new_timer(&timer_config, config->gptimer);
    BLDC_CHECK(err == ESP_OK, "Failed to create gptimer", err);

    err = gptimer_register_event_callbacks(*config->gptimer, &config->cbs, config->user_data);
    BLDC_CHECK(err == ESP_OK, "Failed to register gptimer callbacks", err);

    gptimer_alarm_config_t alarm_config = GPTIMER_ALARM_CONFIG_DEFAULT(config->alarm_count_us);
    err = gptimer_set_alarm_action(*config->gptimer, &alarm_config);
    BLDC_CHECK(err == ESP_OK, "Failed to set gptimer alarm action", err);
    err = gptimer_enable(*config->gptimer);
    BLDC_CHECK(err == ESP_OK, "Failed to enable gptimer", err);
    return ESP_OK;
}

esp_err_t bldc_gptimer_start(gptimer_handle_t gptimer)
{
    BLDC_CHECK(gptimer != NULL, "Pointer of gptimer is NULL", ESP_ERR_INVALID_ARG);
    esp_err_t err = gptimer_start(gptimer);
    BLDC_CHECK(err == ESP_OK, "Failed to start gptimer", err);
    return ESP_OK;
}

esp_err_t bldc_gptimer_stop(gptimer_handle_t gptimer)
{
    BLDC_CHECK(gptimer != NULL, "Pointer of gptimer is NULL", ESP_ERR_INVALID_ARG);
    esp_err_t err = gptimer_stop(gptimer);
    BLDC_CHECK(err == ESP_OK, "Failed to stop gptimer", err);
    return ESP_OK;
}

esp_err_t bldc_gptimer_deinit(gptimer_handle_t gptimer)
{
    BLDC_CHECK(gptimer != NULL, "Pointer of gptimer is NULL", ESP_ERR_INVALID_ARG);
    esp_err_t err = gptimer_stop(gptimer);
    BLDC_CHECK(err == ESP_OK, "Failed to stop gptimer", err);
    err = gptimer_disable(gptimer);
    BLDC_CHECK(err == ESP_OK, "Failed to delete gptimer", err);
    err = gptimer_del_timer(gptimer);
    BLDC_CHECK(err == ESP_OK, "Failed to delete gptimer", err);
    return ESP_OK;
}
