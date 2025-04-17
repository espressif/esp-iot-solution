/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

typedef void* adc_driver_handle_t;

/**
 * @brief Create an ADC driver instance with specified configuration
 *
 * @param unit ADC unit number (ADC_UNIT_1 or ADC_UNIT_2)
 * @param channel ADC channel number
 * @param atten ADC attenuation level
 * @param bitwidth ADC bit width configuration
 * @return adc_driver_handle_t Handle to the created ADC driver instance, NULL if creation failed
 */
adc_driver_handle_t adc_driver_create(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_bitwidth_t bitwidth);

/**
 * @brief Read both raw and calibrated ADC values
 *
 * @param handle ADC driver handle created by adc_driver_create()
 * @param raw_value Pointer to store the raw ADC reading
 * @param cali_value Pointer to store the calibrated voltage value in millivolts
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t adc_driver_read_calibrated_value(adc_driver_handle_t handle, int *raw_value, int *cali_value);

/**
 * @brief Delete an ADC driver instance and free associated resources
 *
 * @param handle Pointer to the ADC driver handle to be deleted
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t adc_driver_delete(adc_driver_handle_t *handle);
