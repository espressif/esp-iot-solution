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
 * @brief Initialize the xz decompressor
 *
 * @param params Pointer to the ota params
 *
 * @return 0
 */
int bootloader_decompressor_xz_init(bootloader_custom_ota_params_t *params);

/**
 * @brief The xz decompressor start to decompress
 *
 * @param params Pointer to the ota params
 *
 * @return 0: Decompress successfully, -1: The decompression started, but an error occurred before the decompression data was generated.
 * , -2: The decompression started, and the decompression data has been generated, but the decompression has not been completed.
 */
int bootloader_decompressor_xz_input(void *addr, int size);

#ifdef __cplusplus
}
#endif
