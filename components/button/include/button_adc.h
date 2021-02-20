// Copyright 2020 Espressif Systems (Shanghai) Co. Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef _IOT_BUTTON_ADC_H_
#define _IOT_BUTTON_ADC_H_

#include "driver/gpio.h"
#include "driver/adc.h"

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
    adc1_channel_t adc_channel;  /**< Channel of ADC */
    uint8_t button_index;        /**< button index on the channel */
    uint16_t min;                /**< min voltage in mv corresponding to the button */
    uint16_t max;                /**< max voltage in mv corresponding to the button */
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
esp_err_t button_adc_deinit(adc1_channel_t channel, int button_index);

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

#endif