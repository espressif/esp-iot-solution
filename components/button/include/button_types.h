/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "button_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    BUTTON_INACTIVE = 0,
    BUTTON_ACTIVE,
};

typedef struct button_dev_t *button_handle_t;

/**
 * @brief Button configuration
 *
 */
typedef struct {
    uint16_t long_press_time;                         /**< Trigger time(ms) for long press, if 0 default to BUTTON_LONG_PRESS_TIME_MS */
    uint16_t short_press_time;                        /**< Trigger time(ms) for short press, if 0 default to BUTTON_SHORT_PRESS_TIME_MS */
} button_config_t;

/**
 * @brief Create a new IoT button instance
 *
 * This function initializes a new button instance with the specified configuration
 * and driver. It also sets up internal resources such as the button timer if not
 * already initialized.
 *
 * @param[in] config        Pointer to the button configuration structure
 * @param[in] driver        Pointer to the button driver structure
 * @param[out] ret_button   Pointer to where the handle of the created button will be stored
 *
 * @return
 *      - ESP_OK: Successfully created the button
 *      - ESP_ERR_INVALID_ARG: Invalid arguments passed to the function
 *      - ESP_ERR_NO_MEM: Memory allocation failed
 *
 * @note
 * - The first call to this function logs the IoT Button version.
 * - The function initializes a global button timer if it is not already running.
 * - Timer is started only if the driver does not enable power-saving mode.
 */
esp_err_t iot_button_create(const button_config_t *config, const button_driver_t *driver, button_handle_t *ret_button);

#ifdef __cplusplus
}
#endif
