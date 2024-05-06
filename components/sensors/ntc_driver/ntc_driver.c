/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "esp_adc/adc_cali_scheme.h"
#include "ntc_driver.h"

const static char *TAG = "ntc driver";

static bool if_init_adc_unit = false;
#define ADC_NTC_CHECK(a, str, ret_val)                          \
    if (!(a)) {                                                  \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val);                                          \
    }

typedef struct ntc_driver {
    ntc_config_t s_ntc_config;
    bool do_calibration1_chan3;
    adc_oneshot_unit_handle_t adc_handle;
    adc_cali_handle_t adc1_cali_chan3_handle;
} ntc_driver_dev_t;

static bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, out_handle);
        if (ret == ESP_OK) {
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
        ret = adc_cali_create_scheme_line_fitting(&cali_config, out_handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }
    return calibrated;
}

esp_err_t ntc_dev_create(ntc_config_t *config, ntc_device_handle_t *ntc_handle, adc_oneshot_unit_handle_t *adc_handle)
{
    ESP_LOGI(TAG, "IoT Ntc Driver Version: %d.%d.%d", NTC_DRIVER_VER_MAJOR, NTC_DRIVER_VER_MINOR, NTC_DRIVER_VER_PATCH);
    esp_err_t ret = ESP_OK;
    if (ntc_handle == NULL || config == NULL) {
        ESP_LOGW(TAG, "arg is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    ntc_driver_dev_t *ndd = (ntc_driver_dev_t *) calloc(1, sizeof(ntc_driver_dev_t));
    if (ndd == NULL) {
        ESP_LOGI(TAG, "Calloc device failed");
        return ESP_ERR_INVALID_ARG;
    }
    ndd->s_ntc_config = *config;
    if (*adc_handle != NULL && adc_handle != NULL) {
        ndd->adc_handle = *adc_handle;
    } else {
        adc_oneshot_unit_init_cfg_t init_config1 = {
            .unit_id = config->unit,
        };
        ret = adc_oneshot_new_unit(&init_config1, &ndd->adc_handle);
        ADC_NTC_CHECK(ret == ESP_OK, "adc oneshot new unit fail!", ESP_FAIL);
        if_init_adc_unit = true;
        if (adc_handle != NULL) {
            *adc_handle = ndd->adc_handle;
        }
    }

    adc_oneshot_chan_cfg_t channel_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = config->atten,
    };
    ret = adc_oneshot_config_channel(ndd->adc_handle, config->channel, &channel_config);
    ADC_NTC_CHECK(ret == ESP_OK, "adc oneshot config channel fail!", ESP_FAIL);

    ndd->do_calibration1_chan3 = adc_calibration_init(config->unit, config->atten, &ndd->adc1_cali_chan3_handle);
    *ntc_handle = (ntc_device_handle_t)ndd;

    return ret;
}

esp_err_t ntc_dev_get_adc_handle(ntc_device_handle_t ntc_handle, adc_oneshot_unit_handle_t *adc_handle)
{
    esp_err_t ret = ESP_OK;
    ntc_driver_dev_t *ndd = (ntc_driver_dev_t *)ntc_handle;
    *adc_handle = ndd->adc_handle;
    return ret;
}

esp_err_t ntc_dev_delete(ntc_device_handle_t ntc_handle)
{
    esp_err_t ret = ESP_OK;
    if (ntc_handle == NULL) {
        ESP_LOGW(TAG, "Pointer of handle is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    ntc_driver_dev_t *ndd = (ntc_driver_dev_t *)ntc_handle;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ret = adc_cali_delete_scheme_curve_fitting(ndd->adc1_cali_chan3_handle);
    ADC_NTC_CHECK(ret == ESP_OK, "adc curve calibration deinit fail", ESP_FAIL);
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ret = adc_cali_delete_scheme_line_fitting(ndd->adc1_cali_chan3_handle);
    ADC_NTC_CHECK(ret == ESP_OK, "adc line calibration deinit fail", ESP_FAIL);
#endif
    if (if_init_adc_unit) {
        ret = adc_oneshot_del_unit(ndd->adc_handle);
        ADC_NTC_CHECK(ret == ESP_OK, "adc oneshot deinit fail", ESP_FAIL);
    }
    free(ndd);
    ndd = NULL;
    return ret;
}

static void ntc_voltage_to_temperature(ntc_device_handle_t ntc_handle, uint32_t voltage_mv, float *temperature)
{
    ntc_driver_dev_t *ndd = (ntc_driver_dev_t *)ntc_handle;
    if (ndd->s_ntc_config.b_value) {
        uint32_t r_ntc_ohm = 0;
        r_ntc_ohm = (ndd->s_ntc_config.vdd_mv - voltage_mv) * ndd->s_ntc_config.fixed_ohm / voltage_mv;
        float temp = 1.0 / (log(1.0 * r_ntc_ohm / ndd->s_ntc_config.r25_ohm) * 1.0 / ndd->s_ntc_config.b_value + 1.0 / 298.15) - 273.0;
        *temperature = temp;
    }
}

float ntc_dev_get_temperature(ntc_device_handle_t ntc_handle)
{
    int voltage = 0;
    int adc_raw = 0;
    float temperature = 0.0;
    esp_err_t ret = ESP_OK;

    ntc_driver_dev_t *ndd = (ntc_driver_dev_t *)ntc_handle;
    ret = adc_oneshot_read(ndd->adc_handle, ndd->s_ntc_config.channel, &adc_raw);
    ADC_NTC_CHECK(ret == ESP_OK, "adc oneshot read fail", ESP_FAIL);
    if (ndd->do_calibration1_chan3) {
        ret = adc_cali_raw_to_voltage(ndd->adc1_cali_chan3_handle, adc_raw, &voltage);
        ADC_NTC_CHECK(ret == ESP_OK, "adc calibration raw to voltage fail", ESP_FAIL);
        ntc_voltage_to_temperature(ndd, voltage, &temperature);
    }
    return temperature;
}
