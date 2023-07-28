/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define bootloader_custom_malloc(size) malloc(size)
#define bootloader_custom_free(ptr) free(ptr)

#ifdef __cplusplus
}
#endif
