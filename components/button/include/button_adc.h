/* SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_idf_version.h"
#include "driver/gpio.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include "esp_adc/adc_oneshot.h"
#else
#include "driver/adc.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ADC_BUTTON_COMBINE(channel, index) ((channel)<<8 | (index))
#define ADC_BUTTON_SPLIT_INDEX(data) ((uint32_t)(data)&0xff)
#define ADC_BUTTON_SPLIT_CHANNEL(data) (((uint32_t)(data) >> 8) & 0xff)

/**
 * @brief adc button configuration
 *
 */
typedef struct {
    uint8_t adc_channel;                             /**< Channel of ADC */
    uint8_t button_index;                            /**< button index on the channel */
    uint16_t min;                                    /**< min voltage in mv corresponding to the button */
    uint16_t max;                                    /**< max voltage in mv corresponding to the button */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    adc_oneshot_unit_handle_t *adc_handle;           /**< handle of adc unit, if NULL will create new one internal, else will use the handle */
#endif
} button_adc_config_t;

/**
 * @brief Initialize gpio button
 *
 * @param config pointer of configuration struct
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG   Arguments is NULL.
 *      - ESP_ERR_NOT_SUPPORTED Arguments out of range.
 *      - ESP_ERR_INVALID_STATE State is error.
 */
esp_err_t button_adc_init(const button_adc_config_t *config);

/**
 * @brief Deinitialize gpio button
 *
 * @param channel ADC channel
 * @param button_index Button index on the channel
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG   Arguments is invalid.
 */
esp_err_t button_adc_deinit(uint8_t channel, int button_index);

/**
 * @brief Get the adc button level
 *
 * @param button_index It is compressed by ADC channel and button index, use the macro ADC_BUTTON_COMBINE to generate. It will be treated as a uint32_t variable.
 *
 * @return
 *      - 0 Not pressed
 *      - 1 Pressed
 */
uint8_t button_adc_get_key_level(void *button_index);

#ifdef __cplusplus
}
#endif
