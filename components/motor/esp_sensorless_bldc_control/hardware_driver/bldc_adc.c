/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bldc_adc.h"
#include "bldc_helper.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "bldc adc";

static bool if_init_adc_unit = false;

esp_err_t bldc_adc_init(const bldc_adc_config_t *config, adc_oneshot_unit_handle_t *adc_handle)
{
    BLDC_CHECK(config, "ADC config is NULL", ESP_ERR_INVALID_ARG);
    esp_err_t err = ESP_OK;

    if (config->adc_handle != NULL) {
        adc_handle = config->adc_handle;
    } else {
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = config->adc_unit,
        };

        err = adc_oneshot_new_unit(&init_config, adc_handle);
        BLDC_CHECK(err == ESP_OK, "ADC unit initialization failed", ESP_ERR_INVALID_STATE);
        if_init_adc_unit = true;
    }

    for (int i = 0; i < config->adc_channel_num; i++) {
        err = adc_oneshot_config_channel(*adc_handle, config->adc_channel[i], &config->chan_cfg);
        BLDC_CHECK(err == ESP_OK, "ADC channel configuration failed", ESP_ERR_INVALID_STATE);
    }

    return ESP_OK;
}

esp_err_t bldc_adc_deinit(adc_oneshot_unit_handle_t adc_handle)
{
    BLDC_CHECK(adc_handle, "ADC handle is NULL", ESP_ERR_INVALID_ARG);
    esp_err_t err = ESP_OK;
    if (if_init_adc_unit) {
        adc_oneshot_del_unit(adc_handle);
    }
    return err;
}

int bldc_adc_read(adc_oneshot_unit_handle_t adc_handle, adc_channel_t adc_channel)
{
    int adc_raw_value = 0;
    adc_oneshot_read(adc_handle, adc_channel, &adc_raw_value);
    return adc_raw_value;
}

int bldc_adc_read_isr(adc_oneshot_unit_handle_t adc_handle, adc_channel_t adc_channel)
{
    int adc_raw_value = 0;
    adc_oneshot_read_isr(adc_handle, adc_channel, &adc_raw_value);
    return adc_raw_value;
}
