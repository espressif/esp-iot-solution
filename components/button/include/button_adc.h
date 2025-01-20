/* SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "button_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief adc button configuration
 *
 */
typedef struct {
    adc_oneshot_unit_handle_t *adc_handle;           /**< handle of adc unit, if NULL will create new one internal, else will use the handle */
    adc_unit_t unit_id;                              /**< ADC unit */
    uint8_t adc_channel;                             /**< Channel of ADC */
    uint8_t button_index;                            /**< button index on the channel */
    uint16_t min;                                    /**< min voltage in mv corresponding to the button */
    uint16_t max;                                    /**< max voltage in mv corresponding to the button */
} button_adc_config_t;

/**
 * @brief Create a new ADC button device
 *
 * This function initializes and configures a new ADC button device using the given configuration parameters.
 * It manages the ADC unit, channels, and button-specific parameters, and ensures proper resource allocation
 * for the ADC button object.
 *
 * @param[in] button_config Configuration for the button device, including callbacks and debounce parameters.
 * @param[in] adc_config Configuration for the ADC channel and button, including the ADC unit, channel,
 *                        button index, and voltage range (min and max).
 * @param[out] ret_button Handle to the newly created button device.
 *
 * @return
 *     - ESP_OK: Successfully created the ADC button device.
 *     - ESP_ERR_INVALID_ARG: Invalid argument provided.
 *     - ESP_ERR_NO_MEM: Memory allocation failed.
 *     - ESP_ERR_INVALID_STATE: The requested button index or channel is already in use, or no channels are available.
 *     - ESP_FAIL: Failed to initialize or configure the ADC or button device.
 *
 * @note
 * - If the ADC unit is not already configured, it will be initialized with the provided or default settings.
 * - If the ADC channel is not initialized, it will be configured for the specified unit and calibrated.
 * - This function ensures that ADC resources are reused whenever possible to optimize resource allocation.
 */
esp_err_t iot_button_new_adc_device(const button_config_t *button_config, const button_adc_config_t *adc_config, button_handle_t *ret_button);

#ifdef __cplusplus
}
#endif
