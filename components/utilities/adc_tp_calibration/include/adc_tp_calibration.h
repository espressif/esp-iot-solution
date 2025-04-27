/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "hal/adc_types.h"

/**
 * @brief ADC calibration configuration structure
 *
 * This structure contains the raw ADC values measured at specific voltage points
 * for calibration. The values are different for ESP32 and ESP32-S2.
 */
typedef struct {
    adc_unit_t adc_unit;               /*!< ADC unit (ADC1 or ADC2) */
#if CONFIG_IDF_TARGET_ESP32
    int adc_raw_value_150mv_atten0;    /*!< ADC raw value at 150mV with atten0 */
    int adc_raw_value_850mv_atten0;    /*!< ADC raw value at 850mV with atten0 */
#elif CONFIG_IDF_TARGET_ESP32S2
    int adc_raw_value_600mv_atten0;    /*!< ADC raw value at 600mV with atten0 */
    int adc_raw_value_800mv_atten2_5;  /*!< ADC raw value at 800mV with atten2_5 */
    int adc_raw_value_1000mv_atten6;   /*!< ADC raw value at 1000mV with atten6 */
    int adc_raw_value_2000mv_atten12;  /*!< ADC raw value at 2000mV with atten12 */
#endif
} adc_tp_cali_config_t;

typedef void *adc_tp_cali_handle_t;    /*!< ADC calibration handle */

/**
 * @brief Create ADC calibration handle
 *
 * @param config Pointer to ADC calibration configuration
 * @param atten ADC attenuation level
 * @return adc_tp_cali_handle_t Handle of ADC calibration, NULL if failed
 */
adc_tp_cali_handle_t adc_tp_cali_create(adc_tp_cali_config_t *config, adc_atten_t atten);

/**
 * @brief Delete ADC calibration handle
 *
 * @param adc_tp_cali_handle Pointer to ADC calibration handle
 * @return esp_err_t
 *         - ESP_OK: Always returns ESP_OK
 */
esp_err_t adc_tp_cali_delete(adc_tp_cali_handle_t *adc_tp_cali_handle);

/**
 * @brief Convert ADC raw value to voltage in millivolts
 *
 * @param adc_tp_cali_handle ADC calibration handle
 * @param raw_value ADC raw value to convert
 * @param voltage Pointer to store the converted voltage in millivolts
 * @return esp_err_t
 *         - ESP_OK: Success
 *         - ESP_ERR_INVALID_ARG: Invalid argument
 */
esp_err_t adc_tp_cali_raw_to_voltage(adc_tp_cali_handle_t adc_tp_cali_handle, int raw_value, int *voltage);
