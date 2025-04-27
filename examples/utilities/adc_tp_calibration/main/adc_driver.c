/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_log.h"
#include "esp_check.h"
#include "adc_driver.h"

static const char* TAG = "ADC_DRIVER";

typedef struct {
    adc_oneshot_unit_handle_t adc_handle;
    adc_cali_handle_t adc_cali_handle;
    adc_channel_t channel;
    adc_atten_t atten;
    adc_bitwidth_t bitwidth;
    adc_unit_t unit;
} adc_driver_t;

adc_driver_handle_t adc_driver_create(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_bitwidth_t bitwidth)
{
    if (unit >= SOC_ADC_PERIPH_NUM) {
        ESP_LOGE(TAG, "invalid unit");
        return NULL;
    }
    if (atten >= SOC_ADC_ATTEN_NUM) {
        ESP_LOGE(TAG, "invalid attenuation");
        return NULL;
    }
    if (!((bitwidth >= SOC_ADC_RTC_MIN_BITWIDTH && bitwidth <= SOC_ADC_RTC_MAX_BITWIDTH) || bitwidth == ADC_BITWIDTH_DEFAULT)) {
        ESP_LOGE(TAG, "invalid bitwidth");
        return NULL;
    }
    if (channel >= SOC_ADC_CHANNEL_NUM(unit)) {
        ESP_LOGE(TAG, "invalid channel");
        return NULL;
    }

    adc_driver_t *driver = (adc_driver_t *)calloc(1, sizeof(adc_driver_t));
    if (!driver) {
        ESP_LOGE(TAG, "memory allocation for driver failed");
        return NULL;
    }

    driver->channel = channel;
    driver->atten = atten;
    driver->bitwidth = bitwidth;
    driver->unit = unit;

    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = unit,
    };
    if (adc_oneshot_new_unit(&init_cfg, &driver->adc_handle) != ESP_OK) {
        free(driver);
        return NULL;
    }

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = bitwidth,
        .atten = atten,
    };
    if (adc_oneshot_config_channel(driver->adc_handle, channel, &config) != ESP_OK) {
        adc_oneshot_del_unit(driver->adc_handle);
        free(driver);
        return NULL;
    }

    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };

        if (adc_cali_create_scheme_curve_fitting(&cali_config, &driver->adc_cali_handle) == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };

        if (adc_cali_create_scheme_line_fitting(&cali_config, &driver->adc_cali_handle) == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    if (!calibrated) {
        ESP_LOGE(TAG, "Failed to create calibration scheme");
        adc_oneshot_del_unit(driver->adc_handle);
        free(driver);
        return NULL;
    }
    return (adc_driver_handle_t)driver;
}

esp_err_t adc_driver_read_calibrated_value(adc_driver_handle_t handle, int *raw_value, int *cali_value)
{
    adc_driver_t *driver = (adc_driver_t *)handle;
    if (driver == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_FAIL;
    ret = adc_oneshot_read(driver->adc_handle, driver->channel, raw_value);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = adc_cali_raw_to_voltage(driver->adc_cali_handle, *raw_value, cali_value);
    if (ret != ESP_OK) {
        return ret;
    }
    return ESP_OK;
}

esp_err_t adc_driver_delete(adc_driver_handle_t *handle)
{
    adc_driver_t *driver = (adc_driver_t *)*handle;
    if (driver == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_FAIL;
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(driver->adc_cali_handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(driver->adc_cali_handle));
#endif

    ret = adc_oneshot_del_unit(driver->adc_handle);
    if (ret != ESP_OK) {
        return ret;
    }

    free(driver);
    *handle = NULL;
    return ESP_OK;
}
