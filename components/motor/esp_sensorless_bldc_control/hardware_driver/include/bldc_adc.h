/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BLDC_ADC_H
#define BLDC_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_private/adc_private.h"
#include "esp_attr.h"

#define GPTIMER_CONFIG_DEFAULT()        \
{                                       \
    .clk_src = GPTIMER_CLK_SRC_DEFAULT, \
    .direction = GPTIMER_COUNT_UP,      \
    .resolution_hz = 1 * 1000 * 1000,   \
}

/**
 * @brief ADC configuration structure
 */
typedef struct {
    adc_oneshot_unit_handle_t *adc_handle; /**< ADC handle */
    adc_unit_t adc_unit;                   /**< ADC unit */
    adc_oneshot_chan_cfg_t chan_cfg;       /**< ADC channel configuration */
    adc_channel_t *adc_channel;            /**< ADC channel */
    size_t adc_channel_num;                /**< ADC channel number */
} bldc_adc_config_t;

/**
 * @brief Initialize ADC
 *
 * @param config Pointer to the configuration structure
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG   if arguments are NULL
 *      - ESP_ERR_NOT_SUPPORTED if arguments are out of range
 *      - ESP_ERR_INVALID_STATE if there is an error in the state
 */
esp_err_t bldc_adc_init(const bldc_adc_config_t *config, adc_oneshot_unit_handle_t *adc_handle);

/**
 * @brief Deinitialize ADC
 *
 * @param adc_handle ADC handle
 * @return
 *     - ESP_OK on success
 *    - ESP_ERR_INVALID_ARG if arguments are NULL
 */
esp_err_t bldc_adc_deinit(adc_oneshot_unit_handle_t adc_handle);

/**
 * @brief Get the raw ADC value
 *
 * @param adc_handle ADC handle
 * @param adc_channel ADC channel
 * @note The runtime of this function is about 29us/run on esp32s3
 * @return Raw ADC value
 */
int bldc_adc_read(adc_oneshot_unit_handle_t adc_handle, adc_channel_t adc_channel);

/**
 * @brief Get the raw ADC value in ISR
 *
 * @param adc_handle ADC handle
 * @param adc_channel ADC channel
 * @note This function takes about 26us/run on s3.
 * @return Raw ADC value
 */
IRAM_ATTR int bldc_adc_read_isr(adc_oneshot_unit_handle_t adc_handle, adc_channel_t adc_channel);

#ifdef __cplusplus
}
#endif

#endif
