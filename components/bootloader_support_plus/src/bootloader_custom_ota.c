/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include <inttypes.h>

#include "esp_log.h"
#include "esp_rom_md5.h"
#include "esp_rom_crc.h"

#include "bootloader_flash_priv.h"

#include "bootloader_custom_ota.h"

#include "bootloader_custom_utility.h"

#ifdef CONFIG_BOOTLOADER_DECOMPRESSOR_XZ
#include "bootloader_decompressor_xz.h"
#endif

#include "bootloader_storage_flash.h"

static const char *TAG = "boot_custom";

static const bootloader_custom_ota_engine_t decompressor_engines[] = {
#ifdef CONFIG_BOOTLOADER_DECOMPRESSOR_XZ
    {COMPRESS_XZ, bootloader_decompressor_xz_init,     bootloader_decompressor_xz_input},
#endif
};

static const bootloader_custom_ota_engine_t storage_engines[] = {
    {STORAGE_FLASH, bootloader_storage_flash_init, bootloader_storage_flash_input},
};

static bootloader_custom_ota_config_t custom_ota_config;
static bootloader_custom_ota_header_t custom_ota_header;
static const bootloader_custom_ota_engine_t * custom_ota_engines[MAX_ENGINE];

static bootloader_custom_ota_params_t custom_ota_params;

static const bootloader_custom_ota_engine_t *bootloader_custom_ota_engine_get(bootloader_custom_ota_engine_type_t engine_type,
    const bootloader_custom_ota_engine_t *engines, uint8_t engines_num)
{
    const bootloader_custom_ota_engine_t *engine = NULL;

    for (uint32_t loop = 0; loop < engines_num; loop++) {
        if (engine_type == engines[loop].type) {
            engine = &engines[loop];
            break;
        }
    }

    return engine;
}

static bool bootloader_custom_ota_engines_init(bootloader_custom_ota_params_t *params)
{
    // Prepare engines
    custom_ota_engines[DECOMPRESS_ENGINE] = bootloader_custom_ota_engine_get(custom_ota_header.compress_type,
        decompressor_engines, sizeof(decompressor_engines) / sizeof(decompressor_engines[0]));

    if (!custom_ota_engines[DECOMPRESS_ENGINE]) {
        ESP_LOGE(TAG, "No supported decompressor %d type!", custom_ota_header.compress_type);
        return false;
    }
 
    custom_ota_engines[STORAGE_ENGINE] = &storage_engines[STORAGE_FLASH];

    for (uint32_t loop = 0; loop < MAX_ENGINE; loop++) {
        if (custom_ota_engines[loop]->init(params) != 0) {
            ESP_LOGE(TAG, "engin %" PRIu32 " init fail!", loop);
            return false;
        }
    }

    return true;
}

static int bootloader_custom_ota_engines_start()
{
    /*
    * Decompress engine return 0 if no errors; When an error occurs, it returns -1 if the original firmware is not damaged,
    * and returns -2 if the original firmware is damaged.
    * TODO: Unify the parameters used by the function.
    */
    return custom_ota_engines[DECOMPRESS_ENGINE]->input(NULL, 0);
}

bootloader_custom_ota_config_t *bootloader_custom_ota_get_config_param(void)
{
    return &custom_ota_config;
}

bootloader_custom_ota_header_t *bootloader_custom_ota_get_header_param(void)
{
    return &custom_ota_header;
}

int bootloader_custom_ota_main(bootloader_state_t *bs, int boot_index)
{
    int ota_index = 0; // By default, the extracted data is always placed in partition app_0
    int last_run_index = 0; // Default app_0 partition index = 0
    esp_partition_pos_t pos;

    // If not boot from the last partition, just do the quick boot from this boot index
    if ((boot_index + 1) != bs->app_count) {
        return boot_index;
    }
    ESP_LOGI(TAG, "bootloader plus version: %d.%d.%d", BOOTLOADER_SUPPORT_PLUS_VER_MAJOR, BOOTLOADER_SUPPORT_PLUS_VER_MINOR, BOOTLOADER_SUPPORT_PLUS_VER_PATCH);

    // Get the partition pos of the downloaded compressed image
    pos = bs->ota[boot_index];

    custom_ota_config.src_addr = pos.offset;
    custom_ota_config.src_size = pos.size;
    custom_ota_config.src_offset = 0;

    esp_err_t err_ret = esp_image_verify(ESP_IMAGE_VERIFY_SILENT, &pos, NULL);
    if (err_ret == ESP_FAIL) {
        ESP_LOGE(TAG, "image vefiry err!");
        goto exit;
    }

    custom_ota_config.dst_addr = bs->ota[ota_index].offset;
    custom_ota_config.dst_size = bs->ota[ota_index].size;
    custom_ota_config.dst_offset = 0;

    ESP_LOGD(TAG, "boot from slot %d, last run slot: %d, OTA to slot: %d", boot_index, last_run_index, ota_index);

    custom_ota_params.config = &custom_ota_config;
    custom_ota_params.header = &custom_ota_header;
    custom_ota_params.engines = (void **)custom_ota_engines;

    if (!bootloader_custom_ota_engines_init(&custom_ota_params)) {
        ESP_LOGE(TAG, "init err");
        goto exit;
    }

    ESP_LOGI(TAG, "start OTA to slot %d", ota_index);
    /*
    If the partition to be erased is too large, the watchdog reset may be triggered here. 
    However, the size of the partition to be erased each time is fixed, 
    so we can know whether the watchdog reset will be triggered here at the test stage.
    */
    bootloader_flash_erase_range(custom_ota_config.dst_addr, custom_ota_config.dst_size);

    int ret = bootloader_custom_ota_engines_start();

    if (ret < 0) {
        ESP_LOGE(TAG, "OTA failed!");
        return boot_index;
    }

    if (bootloader_custom_utility_updata_ota_data(bs, ota_index)) {
        ESP_LOGE(TAG, "update OTA data failed!");
        return ota_index;
    }

    ESP_LOGI(TAG, "OTA success, boot from slot %d", ota_index);

    return ota_index;
exit:
    bootloader_custom_utility_updata_ota_data(bs, last_run_index);
    return last_run_index;
}