/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "bootloader_custom_utility.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read data from the partition holding the compressed firmware.
 *
 * @param buf  pointer to the destination buffer
 * @param size length of data
 *
 * @return ESP_OK on success, ESP_ERR_FLASH_OP_FAIL on SPI failure,
 * ESP_ERR_FLASH_OP_TIMEOUT on SPI timeout.
 */
int bootloader_decompressor_read(void *buf, unsigned int size);

/**
 * @brief Write data to the partition storing the decompressed data.
 *
 * @param buf Pointer to the data to write to flash
 * @param size Length of data in bytes.
 *
 * @return ESP_OK on success, ESP_ERR_FLASH_OP_FAIL on SPI failure,
 * ESP_ERR_FLASH_OP_TIMEOUT on SPI timeout.
 */
int bootloader_decompressor_write(void *buf, unsigned int size);

/**
 * @brief Initialize the parameters required for decompressor
 *
 * @param params Pointer to the ota params
 *
 * @return 0
 */
int bootloader_decompressor_init(bootloader_custom_ota_params_t *params);

#ifdef __cplusplus
}
#endif
