/* SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "button_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief gpio button configuration
 *
 */
typedef struct {
    int32_t gpio_num;              /**< num of gpio */
    uint8_t active_level;          /**< gpio level when press down */
    bool enable_power_save;        /**< enable power save mode */
    bool disable_pull;             /**< disable internal pull up or down */
} button_gpio_config_t;

/**
 * @brief Create a new GPIO button device
 *
 * This function initializes and configures a GPIO-based button device using the given configuration parameters.
 * It sets up the GPIO pin, configures its input mode, and optionally enables power-saving features or wake-up functionality.
 *
 * @param[in] button_config Configuration for the button device, including callbacks and debounce parameters.
 * @param[in] gpio_cfg Configuration for the GPIO, including the pin number, active level, and power-save options.
 * @param[out] ret_button Handle to the newly created GPIO button device.
 *
 * @return
 *     - ESP_OK: Successfully created the GPIO button device.
 *     - ESP_ERR_INVALID_ARG: Invalid argument provided, such as an invalid GPIO number.
 *     - ESP_ERR_NO_MEM: Memory allocation failed.
 *     - ESP_ERR_INVALID_STATE: Failed to configure GPIO wake-up or interrupt settings.
 *     - ESP_FAIL: General failure, such as unsupported wake-up configuration on the target.
 *
 * @note
 * - If power-saving is enabled, the GPIO will be configured as a wake-up source for light sleep.
 * - Pull-up or pull-down resistors are configured based on the `active_level` and the `disable_pull` flag.
 * - This function checks for the validity of the GPIO as a wake-up source when power-saving is enabled.
 * - If power-saving is not supported by the hardware or configuration, the function will return an error.
 */
esp_err_t iot_button_new_gpio_device(const button_config_t *button_config, const button_gpio_config_t *gpio_config, button_handle_t *ret_button);

#ifdef __cplusplus
}
#endif
