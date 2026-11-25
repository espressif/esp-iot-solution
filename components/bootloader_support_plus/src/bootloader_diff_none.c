/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bootloader_diff_none.h"

static bootloader_custom_ota_engine_t **custom_ota_engines;

int bootloader_diff_none_init(bootloader_custom_ota_params_t *params)
{
    custom_ota_engines = (bootloader_custom_ota_engine_t **)params->engines;
    return 0;
}

int bootloader_diff_none_input(void *buf, int size)
{
    return custom_ota_engines[DIFF_ENGINE + 1]->input(buf, size);
}
