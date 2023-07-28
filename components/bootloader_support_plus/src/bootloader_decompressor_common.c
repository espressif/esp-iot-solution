/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>

#include "esp_log.h"

#include "bootloader_decompressor_common.h"
#include "bootloader_custom_utility.h"

#include "bootloader_flash_priv.h"

static bootloader_custom_ota_config_t *custom_ota_config;
static bootloader_custom_ota_header_t *custom_ota_header;
static bootloader_custom_ota_engine_t **custom_ota_engines;

static const char *TAG = "boot_com";

int bootloader_decompressor_read(void *buf, unsigned int size)
{
    uint32_t len = (custom_ota_header->length - custom_ota_config->src_offset > size) ? \
                   size : custom_ota_header->length - custom_ota_config->src_offset;
    uint32_t header_len = sizeof(bootloader_custom_ota_header_t);

    if (len > 0) {
        uint32_t src_addr = custom_ota_config->src_addr + header_len + custom_ota_config->src_offset;
        ESP_LOGD(TAG, "read from %" PRIx32 ", len %" PRIu32 "", src_addr, len);
        bootloader_flash_read(src_addr, buf, len, true);
        custom_ota_config->src_offset += len;
    }

    return len;
}

int bootloader_decompressor_write(void *buf, unsigned int size)
{
    // write to buffer if necessary
    // Now we should try to run next engine
    ESP_LOGD(TAG, "write buffer %p, len %d", buf, size);

    return custom_ota_engines[DECOMPRESS_ENGINE + 1]->input(buf, size);
}

int bootloader_decompressor_init(bootloader_custom_ota_params_t *params)
{
    custom_ota_config = params->config;
    custom_ota_header = (bootloader_custom_ota_header_t *)params->header;
    custom_ota_engines = (bootloader_custom_ota_engine_t **)params->engines;

    return 0;
}
