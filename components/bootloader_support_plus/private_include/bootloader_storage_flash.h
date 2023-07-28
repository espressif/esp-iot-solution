/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "bootloader_custom_utility.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the parameters for writing decompressed data to the specified partition
 *
 * @param params Pointer to the ota params
 *
 * @return 0
 */
int bootloader_storage_flash_init(bootloader_custom_ota_params_t *params);

/**
 * @brief Store the received data to flash
 *
 * @param buf  Pointer to the data to be written
 * @param size The data size
 *
 * @return The length of the data written, 0 on error.
 */
int bootloader_storage_flash_input(void *buf, int size);

/**
 * @brief Write data to Flash in byte-stream.
 *
 * @note In bootloader, when write_encrypted == true, the src buffer is encrypted in place.
 *
 * @param dest_addr Destination address to write in Flash.
 * @param src Pointer to the data to write to flash
 * @param size Length of data in bytes.
 * @param allow_decrypt If true, data will be written encrypted on flash.
 *
 * @return ESP_OK on success, ESP_ERR_FLASH_OP_FAIL on SPI failure,
 * ESP_ERR_FLASH_OP_TIMEOUT on SPI timeout.
 */
esp_err_t bootloader_storage_flash_write(size_t dest_addr, void *src, size_t size, bool allow_decrypt);

#ifdef __cplusplus
}
#endif
