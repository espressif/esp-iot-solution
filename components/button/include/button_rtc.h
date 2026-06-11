/* SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
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
 * @brief RTC IO button configuration
 */
typedef struct {
    int32_t gpio_num;              /**< RTC GPIO number */
    uint8_t active_level;          /**< GPIO level when pressed */
    bool enable_power_save;        /**< Enable power save mode */
    bool disable_pull;             /**< Disable internal pull up or down */
} button_rtc_config_t;

/**
 * @brief Create a new RTC IO button device
 *
 * @param[in] button_config Button device configuration
 * @param[in] rtc_cfg RTC IO button configuration
 * @param[out] ret_button Created button handle
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG Invalid argument
 *     - ESP_ERR_NO_MEM Out of memory
 *     - ESP_FAIL Initialization failed
 */
esp_err_t iot_button_new_rtc_device(const button_config_t *button_config, const button_rtc_config_t *rtc_cfg, button_handle_t *ret_button);

#ifdef __cplusplus
}
#endif
