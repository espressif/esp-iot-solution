/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_rmaker_core.h"
#include "esp_rmaker_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the rainmaker
 *
 * @return
 *    - ESP_OK: Success in initializing the rainmaker
 *    - other: Specific error what went wrong during initialization.
 */
esp_err_t app_rainmaker_init();

/**
 * @brief Return rainmaker parameter
 *
 * @param name rainmaker parameter name
 * @return
 *    - esp_rmaker_param_t*: Success return rainmaker parameter
 *    - NULL: Fail to get the parameter
 */
esp_rmaker_param_t *app_rainmaker_get_param(const char *name);

#ifdef __cplusplus
}
#endif
