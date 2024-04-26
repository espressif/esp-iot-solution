/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_check.h"
#include "kbd_gptimer.h"

static const char *TAG = "kbd_gptimer";

esp_err_t kbd_gptimer_init(kbd_gptimer_config_t *config)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "Pointer of config is NULL");
    ESP_RETURN_ON_FALSE(config->gptimer != NULL, ESP_ERR_INVALID_ARG, TAG, "Pointer of gptimer is NULL");
    gptimer_config_t timer_config = GPTIMER_CONFIG_DEFAULT();
    esp_err_t err = gptimer_new_timer(&timer_config, config->gptimer);
    ESP_RETURN_ON_FALSE(err == ESP_OK, err, TAG, "Failed to create gptimer");

    err = gptimer_register_event_callbacks(*config->gptimer, &config->cbs, config->user_data);
    ESP_RETURN_ON_FALSE(err == ESP_OK, err, TAG, "Failed to register gptimer callbacks");

    gptimer_alarm_config_t alarm_config = GPTIMER_ALARM_CONFIG_DEFAULT(config->alarm_count_us);
    err = gptimer_set_alarm_action(*config->gptimer, &alarm_config);
    ESP_RETURN_ON_FALSE(err == ESP_OK, err, TAG, "Failed to set gptimer alarm action");

    return ESP_OK;
}

esp_err_t kbd_gptimer_start(gptimer_handle_t gptimer)
{
    ESP_RETURN_ON_FALSE(gptimer != NULL, ESP_ERR_INVALID_ARG, TAG, "Pointer of gptimer is NULL");
    esp_err_t err = gptimer_enable(gptimer);
    ESP_RETURN_ON_FALSE(err == ESP_OK, err, TAG, "Failed to enable gptimer");
    err = gptimer_start(gptimer);
    ESP_RETURN_ON_FALSE(err == ESP_OK, err, TAG, "Failed to start gptimer");
    return ESP_OK;
}

esp_err_t kbd_gptimer_stop(gptimer_handle_t gptimer)
{
    ESP_RETURN_ON_FALSE(gptimer != NULL, ESP_ERR_INVALID_ARG, TAG, "Pointer of gptimer is NULL");
    esp_err_t err = gptimer_stop(gptimer);
    ESP_RETURN_ON_FALSE(err == ESP_OK, err, TAG, "Failed to stop gptimer");
    err = gptimer_disable(gptimer);
    ESP_RETURN_ON_FALSE(err == ESP_OK, err, TAG, "Failed to disable gptimer");
    return ESP_OK;
}

esp_err_t kbd_gptimer_deinit(gptimer_handle_t gptimer)
{
    ESP_RETURN_ON_FALSE(gptimer != NULL, ESP_ERR_INVALID_ARG, TAG, "Pointer of gptimer is NULL");
    esp_err_t err = gptimer_stop(gptimer);
    ESP_RETURN_ON_FALSE(err == ESP_OK, err, TAG, "Failed to stop gptimer");
    err = gptimer_disable(gptimer);
    ESP_RETURN_ON_FALSE(err == ESP_OK, err, TAG, "Failed to disable gptimer");
    err = gptimer_del_timer(gptimer);
    ESP_RETURN_ON_FALSE(err == ESP_OK, err, TAG, "Failed to delete gptimer");
    return ESP_OK;
}
