/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
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
 * @brief Configuration structure for a touch button.
 *
 * This structure defines the configuration parameters required to initialize a touch button.
 * It specifies the touch channel, activation threshold, and whether to skip low-level initialization.
 */
typedef struct {
    int32_t touch_channel;      /*!< The touch sensor channel assigned to the button */
    float channel_threshold;    /*!< Activation threshold for the touch channel (range: 0.0 - 1.0) */
    bool skip_lowlevel_init;    /*!< If true, skips low-level initialization (useful for pre-configured setups) */
} button_touch_config_t;

/**
 * @brief Create a new touch button device.
 *
 * This function initializes a new touch button device based on the given configurations.
 * It sets up the touch channel, configures the touch button sensor, and integrates it
 * with the iot_button framework.
 *
 * @param[in] button_config  Pointer to the general button configuration.
 * @param[in] touch_config   Pointer to the touch-specific button configuration.
 * @param[out] ret_button    Pointer to store the created button handle.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if any argument is NULL
 *      - ESP_ERR_NO_MEM if memory allocation fails
 *      - ESP_FAIL if touch button sensor or button creation fails
 */
esp_err_t iot_button_new_touch_button_device(const button_config_t *button_config, const button_touch_config_t *touch_config, button_handle_t *ret_button);

#ifdef __cplusplus
}
#endif
