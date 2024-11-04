/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "esp_adc/adc_cali_scheme.h"
#include "ntc_driver.h"

#define NTC_DRIVER_CHECK(a, str, ret_val)                         \
    if (!(a)) {                                                   \
        ESP_LOGW(TAG, "%s:(%d): %s", __FUNCTION__, __LINE__, str);\
        return (ret_val);                                         \
    }

#define NTC_DRIVER_CHECK_GOTO(a, str, label)                      \
    if (!(a)) {                                                   \
        ESP_LOGW(TAG, "%s:(%d): %s", __FUNCTION__, __LINE__, str);\
        goto label;                                               \
    }

const static char *TAG = "ntc driver";

typedef struct ntc_driver {
    ntc_config_t s_ntc_config;
    adc_oneshot_unit_handle_t adc_handle;
    adc_cali_handle_t adc_cali_handle;
} ntc_driver_dev_t;

static bool if_init_adc_unit = false;

static esp_err_t adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    esp_err_t ret = ESP_FAIL;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = unit,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, out_handle);
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = unit,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_line_fitting(&cali_config, out_handle);
#endif

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return ret;
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
        ESP_LOGW(TAG, "Calloc device failed");
        return ESP_ERR_NO_MEM;
    }

    ndd->s_ntc_config = *config;
    if (adc_handle != NULL && *adc_handle != NULL) {
        ndd->adc_handle = *adc_handle;
    } else {
        adc_oneshot_unit_init_cfg_t init_config1 = {
            .unit_id = config->unit,
        };
        ret = adc_oneshot_new_unit(&init_config1, &ndd->adc_handle);
        NTC_DRIVER_CHECK_GOTO(ret == ESP_OK, "adc oneshot new unit fail!", init_fail);
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
    NTC_DRIVER_CHECK_GOTO(ret == ESP_OK, "adc oneshot config channel fail!", init_fail);

    ret = adc_calibration_init(config->unit, config->atten, &ndd->adc_cali_handle);
    NTC_DRIVER_CHECK_GOTO(ret == ESP_OK, "adc calibration init fail!", init_fail);

    *ntc_handle = (ntc_device_handle_t)ndd;

    return ret;

init_fail:
    if (if_init_adc_unit) {
        adc_oneshot_del_unit(ndd->adc_handle);
    }
    free(ndd);
    ndd = NULL;

    return ret;
}

esp_err_t ntc_dev_get_adc_handle(ntc_device_handle_t ntc_handle, adc_oneshot_unit_handle_t *adc_handle)
{
    esp_err_t ret = ESP_OK;
    if (ntc_handle == NULL) {
        ESP_LOGW(TAG, "Pointer of handle is invalid");
        return ESP_ERR_INVALID_ARG;
    }

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
    ret = adc_cali_delete_scheme_curve_fitting(ndd->adc_cali_handle);
    NTC_DRIVER_CHECK(ret == ESP_OK, "adc curve calibration deinit fail", ESP_FAIL);
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ret = adc_cali_delete_scheme_line_fitting(ndd->adc_cali_handle);
    NTC_DRIVER_CHECK(ret == ESP_OK, "adc line calibration deinit fail", ESP_FAIL);
#endif

    if (if_init_adc_unit) {
        ret = adc_oneshot_del_unit(ndd->adc_handle);
        NTC_DRIVER_CHECK(ret == ESP_OK, "adc oneshot deinit fail", ESP_FAIL);
    }
    free(ndd);
    ndd = NULL;

    return ret;
}

static esp_err_t ntc_voltage_to_temperature(ntc_device_handle_t ntc_handle, uint32_t voltage_mv, float *temperature)
{
    ntc_driver_dev_t *ndd = (ntc_driver_dev_t *)ntc_handle;
    if (ndd->s_ntc_config.b_value && voltage_mv) {
        uint32_t r_ntc_ohm = 0;
        if (ndd->s_ntc_config.circuit_mode == CIRCUIT_MODE_NTC_VCC) {
            r_ntc_ohm = (ndd->s_ntc_config.vdd_mv - voltage_mv) * ndd->s_ntc_config.fixed_ohm / voltage_mv;
        } else if (ndd->s_ntc_config.circuit_mode == CIRCUIT_MODE_NTC_GND) {
            r_ntc_ohm = voltage_mv * ndd->s_ntc_config.fixed_ohm / (ndd->s_ntc_config.vdd_mv - voltage_mv);
        }
        *temperature = 1.0 / (log(1.0 * r_ntc_ohm / ndd->s_ntc_config.r25_ohm) * 1.0 / ndd->s_ntc_config.b_value + 1.0 / 298.15) - 273.0;
        return ESP_OK;
    }
    return ESP_FAIL;
}

#ifdef CONFIG_ENABLE_NTC_ADC_FILTER
static esp_err_t adc_oneshot_get_filtered_result(adc_oneshot_unit_handle_t handle, adc_cali_handle_t cali_handle, adc_channel_t chan, int *filtered_result)
{
    NTC_DRIVER_CHECK(filtered_result != NULL, "Pointer of filtered_result is invalid", ESP_FAIL);
    int vol[CONFIG_NTC_FILTER_WINDOW_SIZE] = {0};
    int avg = 0;
    for (int i = 0; i < sizeof(vol) / sizeof(vol[0]); i++) {
        int raw = 0;
        NTC_DRIVER_CHECK(adc_oneshot_read(handle, chan, &raw) == ESP_OK, "adc oneshot read fail", ESP_FAIL);
        ESP_LOGV(TAG, "raw: %d", raw);
        NTC_DRIVER_CHECK(adc_cali_raw_to_voltage(cali_handle, raw, &vol[i]) == ESP_OK, "adc calibration fail", ESP_FAIL);
        ESP_LOGV(TAG, "vol: %d", vol[i]);
        avg += vol[i];
    }
    // calculate the average value
    avg /= sizeof(vol) / sizeof(vol[0]);
    *filtered_result = avg;
    // calculate the standard deviation
    int std_vol = 0;
    for (int i = 0; i < sizeof(vol) / sizeof(vol[0]); i++) {
        std_vol += (vol[i] - avg) * (vol[i] - avg);
    }
    std_vol = (int)sqrt(std_vol / (sizeof(vol) / sizeof(vol[0])));
    // filter the result, if the difference between the value and the average value is greater than the standard deviation, the value is discarded
    int count = 0;
    int filtered_vol = 0;
    for (int i = 0; i < sizeof(vol) / sizeof(vol[0]); i++) {
        if (abs(vol[i] - avg) < std_vol) {
            count++;
            filtered_vol += vol[i];
        } else {
            ESP_LOGV(TAG, "filtered out: %d", vol[i]);
        }
    }
    if (count > 0) {
        *filtered_result = filtered_vol / count;
    }

    return ESP_OK;
}
#endif

esp_err_t ntc_dev_get_temperature(ntc_device_handle_t ntc_handle, float *temperature)
{
    int voltage = 0;

    if (ntc_handle == NULL) {
        ESP_LOGW(TAG, "Pointer of handle is invalid");
        return ESP_ERR_INVALID_ARG;
    }

    ntc_driver_dev_t *ndd = (ntc_driver_dev_t *)ntc_handle;
#ifdef CONFIG_ENABLE_NTC_ADC_FILTER
    esp_err_t ret = adc_oneshot_get_filtered_result(ndd->adc_handle, ndd->adc_cali_handle, ndd->s_ntc_config.channel, &voltage);
#else
    int adc_raw = 0;
    esp_err_t ret = adc_oneshot_read(ndd->adc_handle, ndd->s_ntc_config.channel, &adc_raw);
    NTC_DRIVER_CHECK(ret == ESP_OK, "adc oneshot read fail", ESP_FAIL);
    NTC_DRIVER_CHECK(adc_raw != 0, "adc raw data equal to 0", ESP_FAIL);
    ret = adc_cali_raw_to_voltage(ndd->adc_cali_handle, adc_raw, &voltage);
#endif
    NTC_DRIVER_CHECK(ret == ESP_OK, "adc calibration raw to voltage fail", ESP_FAIL);
    NTC_DRIVER_CHECK(voltage != 0, "voltage equal to 0", ESP_FAIL);
    ntc_voltage_to_temperature(ndd, voltage, temperature);

    return ESP_OK;
}
