/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize CDC device
 *
 * @return
 *       - ESP_OK
 *       - ESP_FAIL
 */
esp_err_t device_cdc_init(void);

/**
 * @brief Deinitialize CDC device
 *
 * @return
 *       - ESP_OK
 *       - ESP_FAIL
 */
esp_err_t device_cdc_deinit(void);

#ifdef __cplusplus
}
#endif
