/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdlib.h>
#include "esp_log.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_oneshot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief NTC config data type
 */
typedef struct {
    uint32_t b_value;       /*!< beta value of NTC (K) */
    uint32_t r25_ohm;       /*!< 25℃ resistor value of NTC (K) */
    uint32_t fixed_ohm;     /*!< fixed resistor value (Ω) */
    uint32_t vdd_mv;        /*!< vdd voltage (mv) */
    adc_atten_t atten;      /*!< adc atten */
    adc_channel_t channel;  /*!< adc channel */
    adc_unit_t unit;        /*!< adc unit */
} ntc_config_t;

typedef void *ntc_device_handle_t;

/**
 * @brief Initialize the NTC and ADC channel config
 *
 * @param config
 * @param ntc_handle A ntc driver handle
 * @param adc_handle A adc handle
 *
 * @return
 *      - ESP_OK  Success
 *      - ESP_FAIL Failure
 */
esp_err_t ntc_dev_create(ntc_config_t *config, ntc_device_handle_t *ntc_handle, adc_oneshot_unit_handle_t *adc_handle);

/**
 * @brief Get the adc handle
 *
 * @param ntc_handle A ntc driver handle
 * @param adc_handle A adc handle
 *
 * @return
 *      - ESP_OK  Success
 *      - ESP_FAIL Failure
 */
esp_err_t ntc_dev_get_adc_handle(ntc_device_handle_t ntc_handle, adc_oneshot_unit_handle_t *adc_handle);

/**
 * @brief Delete ntc driver device and  ntc detect device
 *
 * @param ntc_handle A ntc driver handle
 *
 * @return
 *      - ESP_OK  Success
 *      - ESP_FAIL Failure
 */
esp_err_t ntc_dev_delete(ntc_device_handle_t ntc_handle);

/**
 * @brief Get the ntc temperature
 *
 * @param ntc_handle A ntc driver handle
 *
 * @return NTC temperature
 *
 */
float ntc_dev_get_temperature(ntc_device_handle_t ntc_handle);

#ifdef __cplusplus
}
#endif
