/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"

#include "xz_decompress.h"

#include "bootloader_flash_priv.h"

#include "bootloader_decompressor_xz.h"

#include "bootloader_decompressor_common.h"

static const char *TAG = "boot_xz";

static void bootloader_decompressor_xz_error(char *msg)
{
    ESP_LOGE(TAG, "%s", msg);
}

int bootloader_decompressor_xz_init(bootloader_custom_ota_params_t *params)
{
    return bootloader_decompressor_init(params);
}

int bootloader_decompressor_xz_input(void *addr, int size)
{
    int ret = -1;
    int in_used = 0;

    ret = xz_decompress(NULL, 0, &bootloader_decompressor_read, &bootloader_decompressor_write, NULL, &in_used, &bootloader_decompressor_xz_error);

    if ((ret != 0) && (in_used != 0)) {
        return -2;
    }

    return ret;
}
