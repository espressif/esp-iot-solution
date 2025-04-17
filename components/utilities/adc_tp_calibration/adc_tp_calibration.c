/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_check.h"
#include "adc_tp_calibration.h"

static const char* TAG = "adc_tp_cal";

#if CONFIG_IDF_TARGET_ESP32
#define TP_LOW_VOLTAGE  150
#define TP_HIGH_VOLTAGE 850
#define LIN_COEFF_A_SCALE   65536
#define LIN_COEFF_A_ROUND   (LIN_COEFF_A_SCALE/2)
static const uint32_t adc1_tp_atten_scale[4] = {65504, 86975, 120389, 224310};
static const uint32_t adc2_tp_atten_scale[4] = {65467, 86861, 120416, 224708};
static const uint32_t adc1_tp_atten_offset[4] = {0, 1, 27, 54};
static const uint32_t adc2_tp_atten_offset[4] = {0, 9, 26, 66};
#elif CONFIG_IDF_TARGET_ESP32S2
static const int coeff_a_scaling = 65536;
static const int coeff_b_scaling = 1024;
static const uint32_t v_high[] = {600, 800, 1000, 2000};
#endif

typedef struct {
    adc_atten_t atten;
    adc_tp_cali_config_t config;
    uint32_t coeff_a;
    uint32_t coeff_b;
#if CONFIG_IDF_TARGET_ESP32
    const uint32_t *atten_scales;
    const uint32_t *atten_offsets;
#elif CONFIG_IDF_TARGET_ESP32S2
    uint32_t high[4];
#endif
} adc_tp_cali_t;

adc_tp_cali_handle_t adc_tp_cali_create(adc_tp_cali_config_t *config, adc_atten_t atten)
{
    ESP_RETURN_ON_FALSE(config, NULL, TAG, "config is NULL");
    ESP_RETURN_ON_FALSE(config->adc_unit == ADC_UNIT_1 || config->adc_unit == ADC_UNIT_2, NULL, TAG, "invalid ADC unit");

    adc_tp_cali_t *adc_tp_cal = (adc_tp_cali_t *)calloc(1, sizeof(adc_tp_cali_t));
    ESP_RETURN_ON_FALSE(adc_tp_cal, NULL, TAG, "memory allocation for device handler failed");

    adc_tp_cal->atten = atten;
    adc_tp_cal->config = *config;

#if CONFIG_IDF_TARGET_ESP32
    if (adc_tp_cal->config.adc_unit == ADC_UNIT_1) {
        adc_tp_cal->atten_scales = adc1_tp_atten_scale;
        adc_tp_cal->atten_offsets = adc1_tp_atten_offset;
    } else {
        adc_tp_cal->atten_scales = adc2_tp_atten_scale;
        adc_tp_cal->atten_offsets = adc2_tp_atten_offset;
    }

    uint32_t delta_x = config->adc_raw_value_850mv_atten0 - config->adc_raw_value_150mv_atten0;
    uint32_t delta_v = TP_HIGH_VOLTAGE - TP_LOW_VOLTAGE;
    adc_tp_cal->coeff_a = (delta_v * adc_tp_cal->atten_scales[atten] + (delta_x / 2)) / delta_x;
    adc_tp_cal->coeff_b = TP_HIGH_VOLTAGE - ((delta_v * config->adc_raw_value_850mv_atten0 + (delta_x / 2)) / delta_x) + adc_tp_cal->atten_offsets[atten];
#elif CONFIG_IDF_TARGET_ESP32S2
    const uint32_t high_values[] = {
        config->adc_raw_value_600mv_atten0,
        config->adc_raw_value_800mv_atten2_5,
        config->adc_raw_value_1000mv_atten6,
        config->adc_raw_value_2000mv_atten12
    };
    memcpy(adc_tp_cal->high, high_values, sizeof(high_values));

    adc_tp_cal->coeff_a = coeff_a_scaling * v_high[atten] / adc_tp_cal->high[atten];
    adc_tp_cal->coeff_b = 0;
#endif
    return (adc_tp_cali_handle_t)adc_tp_cal;
}

esp_err_t adc_tp_cali_delete(adc_tp_cali_handle_t *adc_tp_cali_handle)
{
    if (*adc_tp_cali_handle == NULL) {
        return ESP_OK;
    }

    adc_tp_cali_t *adc_tp_cal = (adc_tp_cali_t *)(*adc_tp_cali_handle);
    free(adc_tp_cal);
    *adc_tp_cali_handle = NULL;
    return ESP_OK;
}

esp_err_t adc_tp_cali_raw_to_voltage(adc_tp_cali_handle_t adc_tp_cali_handle, int raw_value, int *voltage)
{
    ESP_RETURN_ON_FALSE(adc_tp_cali_handle && voltage, ESP_ERR_INVALID_ARG, TAG, "invalid argument: null pointer");

    adc_tp_cali_t *adc_tp_cal = (adc_tp_cali_t *)adc_tp_cali_handle;
#if CONFIG_IDF_TARGET_ESP32
    *voltage = (((adc_tp_cal->coeff_a * raw_value) + LIN_COEFF_A_ROUND) / LIN_COEFF_A_SCALE) + adc_tp_cal->coeff_b;
#elif CONFIG_IDF_TARGET_ESP32S2
    *voltage = (raw_value * adc_tp_cal->coeff_a / (coeff_a_scaling / coeff_b_scaling) + adc_tp_cal->coeff_b) / coeff_b_scaling;
#endif
    return ESP_OK;
}
