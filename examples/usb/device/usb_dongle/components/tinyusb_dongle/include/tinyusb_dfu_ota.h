/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"

/**
 * @brief Starts the OTA (Over-The-Air) update process.
 *
 * This function is used to start the OTA update process by providing the OTA write data and the amount of data read.
 *
 * @param ota_write_data Pointer to the OTA write data.
 * @param data_read The amount of data read.
 * @return `esp_err_t` indicating the status of the OTA start process.
 */
esp_err_t ota_start(uint8_t const *ota_write_data, uint32_t data_read);

/**
 * @brief Completes the OTA (Over-The-Air) update process.
 *
 * This function is used to complete the OTA update process.
 *
 * @return `esp_err_t` indicating the status of the OTA completion process.
 */
esp_err_t ota_complete(void);

#ifdef __cplusplus
}
#endif
