/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "bootloader_custom_utility.h"

#ifdef __cplusplus
extern "C" {
#endif

int bootloader_diff_none_init(bootloader_custom_ota_params_t *params);

int bootloader_diff_none_input(void *buf, int size);

#ifdef __cplusplus
}
#endif
