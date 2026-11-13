/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
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
#ifdef CONFIG_BOOTLOADER_WDT_ENABLE
#include "hal/wdt_hal.h"
#endif
#include "bootloader_flash_priv.h"
#include "esp_image_format.h"

#include "bootloader_custom_ota.h"

#include "bootloader_custom_utility.h"

#include "bootloader_custom_image_format.h"

#ifdef CONFIG_BOOTLOADER_DECOMPRESSOR_XZ
#include "bootloader_decompressor_xz.h"
#endif

#include "bootloader_storage_flash.h"
#include "bootloader_diff_none.h"
#ifdef CONFIG_BOOTLOADER_DIFF_DDELTA
#include "bootloader_diff_ddelta.h"
#endif

#ifndef CONFIG_ENABLE_LEGACY_ESP_BOOTLOADER_PLUS_V2_SUPPORT
#include "bootloader_common.h"
#endif

static const char *TAG = "boot_custom";

static const bootloader_custom_ota_engine_t decompressor_engines[] = {
#ifdef CONFIG_BOOTLOADER_DECOMPRESSOR_XZ
    {COMPRESS_XZ, bootloader_decompressor_xz_init,     bootloader_decompressor_xz_input},
#endif
};

static const bootloader_custom_ota_engine_t diff_engines[] = {
    {DIFF_NONE, bootloader_diff_none_init, bootloader_diff_none_input},
#ifdef CONFIG_BOOTLOADER_DIFF_DDELTA
    {DIFF_DDELTA, bootloader_diff_ddelta_init, bootloader_diff_ddelta_input},
#endif
};

static const bootloader_custom_ota_engine_t storage_engines[] = {
    {STORAGE_FLASH, bootloader_storage_flash_init, bootloader_storage_flash_input},
};

static bootloader_custom_ota_config_t custom_ota_config;
static bootloader_custom_ota_header_t custom_ota_header;
static const bootloader_custom_ota_engine_t *custom_ota_engines[MAX_ENGINE];

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

#ifndef CONFIG_ENABLE_LEGACY_ESP_BOOTLOADER_PLUS_V2_SUPPORT
    custom_ota_engines[DIFF_ENGINE] = bootloader_custom_ota_engine_get(custom_ota_header.delta_type,
                                                                       diff_engines, sizeof(diff_engines) / sizeof(diff_engines[0]));
#else
    custom_ota_engines[DIFF_ENGINE] = bootloader_custom_ota_engine_get(DIFF_NONE,
                                                                       diff_engines, sizeof(diff_engines) / sizeof(diff_engines[0]));
#endif
    if (!custom_ota_engines[DIFF_ENGINE]) {
        ESP_LOGE(TAG, "No supported diff type!");
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

#ifndef CONFIG_ENABLE_LEGACY_ESP_BOOTLOADER_PLUS_V2_SUPPORT
static int bootloader_custom_previous_ota_index(const bootloader_state_t *bs)
{
    esp_ota_select_entry_t otadata[2];
    if (bootloader_custom_utility_read_otadata(&bs->ota_info, otadata) != ESP_OK) {
        return 0;
    }
    int active = bootloader_common_get_active_otadata(otadata);
    if (active < 0) {
        return 0;
    }
    int inactive = active ^ 1;
    uint32_t seq = otadata[inactive].ota_seq;
    if (seq == UINT32_MAX || seq == 0) {
        return 0;
    }
    if (bootloader_common_ota_select_crc(&otadata[inactive]) != otadata[inactive].crc) {
        return 0;
    }
    return (int)((seq - 1) % bs->app_count);
}

static int bootloader_custom_other_slot(int app_count, int base_index, int patch_index)
{
    for (int i = 0; i < app_count; i++) {
        if (i != base_index && i != patch_index) {
            return i;
        }
    }
    return -1;
}

static bool bootloader_custom_base_crc_ok(void)
{
    uint32_t base_len = 0;
    uint32_t expect = 0;
    uint32_t crc = 0;
    uint8_t buf[256];
    bool allow_decrypt = bootloader_custom_flash_encryption_enabled();
#ifdef CONFIG_BOOTLOADER_WDT_ENABLE
    wdt_hal_context_t rtc_wdt_ctx = RWDT_HAL_CONTEXT_DEFAULT();
    uint32_t feed = 0;
#endif

    memcpy(&base_len, custom_ota_header.reserved, sizeof(base_len));
    memcpy(&expect, custom_ota_header.reserved + sizeof(base_len), sizeof(expect));
    if (base_len == 0 || base_len > custom_ota_config.base_size) {
        ESP_LOGE(TAG, "base check length invalid");
        return false;
    }

    for (uint32_t off = 0; off < base_len;) {
#ifdef CONFIG_BOOTLOADER_WDT_ENABLE
        if ((feed++ & 15U) == 0) {
            wdt_hal_write_protect_disable(&rtc_wdt_ctx);
            wdt_hal_feed(&rtc_wdt_ctx);
            wdt_hal_write_protect_enable(&rtc_wdt_ctx);
        }
#endif
        uint32_t n = base_len - off;
        if (n > sizeof(buf)) {
            n = sizeof(buf);
        }
        if ((n & 3U) != 0 && n > 3U) {
            n &= ~3U;
        }
        uint32_t addr = custom_ota_config.base_addr + off;
        if ((n & 3U) == 0) {
            if (bootloader_flash_read(addr, buf, n, allow_decrypt) != ESP_OK) {
                return false;
            }
            crc = esp_rom_crc32_le(crc, buf, n);
            off += n;
        } else {
            uint8_t tmp[4] = {0};
            if (bootloader_flash_read(addr, tmp, 4, allow_decrypt) != ESP_OK) {
                return false;
            }
            crc = esp_rom_crc32_le(crc, tmp, n);
            off += n;
        }
    }
    if (crc != expect) {
        ESP_LOGE(TAG, "base app check error");
        return false;
    }
    return true;
}
#endif

static int bootloader_custom_ota_engines_start()
{
    /*
    * Decompress engine return 0 if no errors; When an error occurs, it returns -1 if the original firmware is not damaged,
    * and returns -2 if the original firmware is damaged.
    * TODO: Unify the parameters used by the function.
    */
    int ret = custom_ota_engines[DECOMPRESS_ENGINE]->input(NULL, 0);
    if (ret < 0) {
        return ret;
    }

#if defined(CONFIG_BOOTLOADER_DIFF_DDELTA) && !defined(CONFIG_ENABLE_LEGACY_ESP_BOOTLOADER_PLUS_V2_SUPPORT)
    /* XZ success only means the compressed stream ended. A truncated ddelta
     * payload still returns need-more-input during flush; require the zero
     * terminator and bytes_written == new_file_size before updating otadata. */
    if (custom_ota_header.delta_type == DIFF_DDELTA && !bootloader_diff_ddelta_is_complete()) {
        ESP_LOGE(TAG, "ddelta incomplete after decompress");
        return -1;
    }
#endif
    return ret;
}

bootloader_custom_ota_config_t *bootloader_custom_ota_get_config_param(void)
{
    return &custom_ota_config;
}

bootloader_custom_ota_header_t *bootloader_custom_ota_get_header_param(void)
{
    return &custom_ota_header;
}

#if CONFIG_ENABLE_LEGACY_ESP_BOOTLOADER_PLUS_V2_SUPPORT
static esp_err_t bootloader_custom_ota_clear_storage_header()
{
#define SEC_SIZE    (1024 * 4)
    return bootloader_flash_erase_range(custom_ota_config.src_addr, SEC_SIZE);
}

static bool bootloader_custom_search_partition_pos(uint8_t type, uint8_t subtype, esp_partition_pos_t *pos)
{
    const esp_partition_info_t *partitions;
    esp_err_t err;
    int num_partitions;
    bool ret = false;

    partitions = bootloader_mmap(ESP_PARTITION_TABLE_OFFSET, ESP_PARTITION_TABLE_MAX_LEN);
    if (!partitions) {
        ESP_LOGE(TAG, "bootloader_mmap(0x%x, 0x%x) failed", ESP_PARTITION_TABLE_OFFSET, ESP_PARTITION_TABLE_MAX_LEN);
        return ret;
    }
    ESP_LOGD(TAG, "mapped partition table 0x%x at 0x%x", ESP_PARTITION_TABLE_OFFSET, (intptr_t)partitions);

    err = esp_partition_table_verify(partitions, true, &num_partitions);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to verify partition table");
        return ret;
    }

    for (int i = 0; i < num_partitions; i++) {
        const esp_partition_info_t *partition = &partitions[i];

        if (type == partition->type && subtype == partition->subtype) {
            ESP_LOGD(TAG, "load partition table entry 0x%x", (intptr_t)partition);
            ESP_LOGD(TAG, "find type=%x subtype=%x", partition->type, partition->subtype);
            *pos = partition->pos;
            ret = true;
            break;
        }
    }

    bootloader_munmap(partitions);

    return ret;
}
#endif

int bootloader_custom_ota_main(bootloader_state_t *bs, int boot_index)
{
    int ota_index = 0; // By default, the extracted data is always placed in partition app_0
    esp_partition_pos_t pos;
#ifndef CONFIG_ENABLE_LEGACY_ESP_BOOTLOADER_PLUS_V2_SUPPORT
    pos = bs->ota[boot_index]; // Get the partition pos of the current partition

    // Check if this is standard image compiled from IDF
    if (bootloader_custom_ota_get_header_type(&pos) == OTA_IMAGE_TYPE_1) {
        // If standard IDF image, quick boot from this boot index
        return boot_index;
    }

    const int saved_boot_index = boot_index;
    // If not boot from the last partition, just do the quick boot from this boot index
    if ((boot_index + 1) != bs->app_count) {
        return boot_index;
    }
    // If not returned above, the partition `pos` is of the downloaded compressed image.
    // Resolve the previously running slot before any path that may goto exit, so a
    // failed package check restores that slot instead of always falling back to ota_0.
    int last_run_index = bootloader_custom_previous_ota_index(bs);
#else
    if (boot_index <= FACTORY_INDEX) {
        ota_index = 0;
    } else {
        ota_index = (boot_index + 1) == bs->app_count ? 0 : (boot_index + 1);
    }

    if (!bootloader_custom_search_partition_pos(PART_TYPE_DATA, BOOTLOADER_CUSTOM_OTA_PARTITION_SUBTYPE, &pos)) {
        ESP_LOGE(TAG, "no custom OTA storage partition!");
        return boot_index;
    }
#endif // CONFIG_ENABLE_LEGACY_ESP_BOOTLOADER_PLUS_V2_SUPPORT

    ESP_LOGI(TAG, "bootloader plus version: %d.%d.%d", BOOTLOADER_SUPPORT_PLUS_VER_MAJOR, BOOTLOADER_SUPPORT_PLUS_VER_MINOR, BOOTLOADER_SUPPORT_PLUS_VER_PATCH);

    custom_ota_config.src_addr = pos.offset;
    custom_ota_config.src_size = pos.size;
    custom_ota_config.src_offset = 0;

    esp_err_t err_ret = esp_image_verify(ESP_IMAGE_VERIFY_SILENT, &pos, NULL);
#ifndef CONFIG_ENABLE_LEGACY_ESP_BOOTLOADER_PLUS_V2_SUPPORT
    if (err_ret != ESP_OK) {
        ESP_LOGE(TAG, "image verify err!");
        goto exit;
    }
#else
    if (err_ret == CUSTOM_OTA_IMAGE_TYPE_INVALID) {
        return boot_index;
    } else if (err_ret == ESP_FAIL) {
        ESP_LOGE(TAG, "image verify err!");
        goto exit;
    }
#endif

#ifndef CONFIG_ENABLE_LEGACY_ESP_BOOTLOADER_PLUS_V2_SUPPORT
    if (custom_ota_header.delta_type == DIFF_DDELTA) {
        if (bs->app_count < 3) {
            ESP_LOGE(TAG, "ddelta needs three app partitions");
            goto exit;
        }
        int base_index = last_run_index;
        int dst_index = bootloader_custom_other_slot((int)bs->app_count, base_index, saved_boot_index);
        if (base_index == saved_boot_index || dst_index < 0) {
            ESP_LOGE(TAG, "ddelta slot select failed, base %d patch %d", base_index, saved_boot_index);
            goto exit;
        }
        ota_index = dst_index;
        custom_ota_config.base_addr = bs->ota[base_index].offset;
        custom_ota_config.base_size = bs->ota[base_index].size;
        custom_ota_config.base_offset = 0;
        if (!bootloader_custom_base_crc_ok()) {
            goto exit;
        }
#if defined(CONFIG_BOOTLOADER_DIFF_DDELTA) && !defined(CONFIG_SKIP_VALIDATE_FULL_BASE_IMAGE)
        /* The header CRC already covers the whole base file and runs before
         * this. This pass checks image structure and, with secure boot, the
         * signature. esp_image_verify() rejects a NULL metadata pointer. */
        {
            esp_image_metadata_t base_meta = {0};
            if (esp_image_verify(ESP_IMAGE_VERIFY_SILENT, &bs->ota[base_index], &base_meta) != ESP_OK) {
                ESP_LOGE(TAG, "base image verify err");
                goto exit;
            }
        }
#endif
        ESP_LOGI(TAG, "ddelta base slot %d, write slot %d", base_index, dst_index);
    }
#endif

    custom_ota_config.dst_addr = bs->ota[ota_index].offset;
    custom_ota_config.dst_size = bs->ota[ota_index].size;
    custom_ota_config.dst_offset = 0;

    ESP_LOGD(TAG, "boot from slot %d, OTA to slot: %d", boot_index, ota_index);

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
    Erase and apply failures use the same exit path as a failed package check, so otadata
    is restored instead of staying on the patch slot.
    */
    int ret = 0;
#ifdef CONFIG_BOOTLOADER_WDT_ENABLE
    wdt_hal_context_t rtc_wdt_ctx = RWDT_HAL_CONTEXT_DEFAULT();

    if ((custom_ota_config.dst_addr % FLASH_SECTOR_SIZE) != 0 || (custom_ota_config.dst_size % FLASH_SECTOR_SIZE != 0)) {
        return ESP_ERR_INVALID_SIZE;
    }
    uint32_t end = custom_ota_config.dst_addr + custom_ota_config.dst_size;
    for (uint32_t start = custom_ota_config.dst_addr; start < end;) {
        wdt_hal_write_protect_disable(&rtc_wdt_ctx);
        wdt_hal_feed(&rtc_wdt_ctx);
        wdt_hal_write_protect_enable(&rtc_wdt_ctx);
        uint32_t erase_len = FLASH_SECTOR_SIZE;
        if (start % FLASH_BLOCK_SIZE == 0 && end - start >= FLASH_BLOCK_SIZE) {
            erase_len = FLASH_BLOCK_SIZE;
        }
        if (bootloader_flash_erase_range(start, erase_len) != ESP_OK) {
            ESP_LOGE(TAG, "erase OTA slot error");
            goto exit;
        }
        start += erase_len;
    }
#else
    bootloader_flash_erase_range(custom_ota_config.dst_addr, custom_ota_config.dst_size);
#endif

    ret = bootloader_custom_ota_engines_start();

    if (ret < 0) {
        ESP_LOGE(TAG, "OTA failed!");
        goto exit;
    }

#if defined(CONFIG_BOOTLOADER_DIFF_DDELTA) && !defined(CONFIG_SKIP_VALIDATE_FULL_NEW_IMAGE)
    if (custom_ota_header.delta_type == DIFF_DDELTA) {
        /* esp_image_verify() rejects a NULL metadata pointer for a standard app image. */
        esp_image_metadata_t new_meta = {0};
        if (esp_image_verify(ESP_IMAGE_VERIFY_SILENT, &bs->ota[ota_index], &new_meta) != ESP_OK) {
            ESP_LOGE(TAG, "new image verify err");
            goto exit;
        }
    }
#endif

    if (bootloader_custom_utility_updata_ota_data(bs, ota_index) == ESP_OK) {
        ESP_LOGI(TAG, "OTA success, boot from slot %d", ota_index);
    } else {
        ESP_LOGE(TAG, "update OTA data failed!");
    }

#ifndef CONFIG_ENABLE_LEGACY_ESP_BOOTLOADER_PLUS_V2_SUPPORT
    return ota_index;

exit:
    if (bootloader_custom_utility_updata_ota_data(bs, last_run_index) != ESP_OK) {
        ESP_LOGE(TAG, "restore OTA data failed");
        /* otadata still selects the patch slot. Erase the first sector of that
         * partition so the next boot sees an invalid header and skips the
         * erase/apply cycle. Use pos.offset, not src_addr: a type-2 image
         * (--add_app_header) moves src_addr forward by APP_IMAGE_HEADER_LEN_DEFAULT
         * (0x120), which is not sector-aligned and bootloader_flash_erase_range()
         * rejects it. */
        if (pos.size < FLASH_SECTOR_SIZE ||
                bootloader_flash_erase_range(pos.offset, FLASH_SECTOR_SIZE) != ESP_OK) {
            ESP_LOGE(TAG, "erase patch header error");
        }
    }
    return last_run_index;
#else // NOT CONFIG_ENABLE_LEGACY_ESP_BOOTLOADER_PLUS_V2_SUPPORT
    boot_index = ota_index;

exit:
    // Erase custom OTA binary header to avoid doing OTA again in the next reboot.
    if (bootloader_custom_ota_clear_storage_header() != ESP_OK) {
        ESP_LOGE(TAG, "erase compressed OTA header error!");
    }
    return boot_index;
#endif // CONFIG_ENABLE_LEGACY_ESP_BOOTLOADER_PLUS_V2_SUPPORT
}
