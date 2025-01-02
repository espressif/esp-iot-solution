/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/* Function used to tell the linker to include this file
 * with all its symbols.
 */

#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"
#include "esp_flash_partitions.h"
#include "esp_image_format.h"
#include "bootloader_common.h"
#include "bootloader_config.h"
#include "bootloader_utility.h"
#include "sdkconfig.h"
#if CONFIG_IDF_TARGET_ESP32S3
#include "esp32s3/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32P4
#include "esp32p4/rom/rtc.h"
#endif

static const char *TAG = "bootloader_hooks";

void bootloader_hooks_include(void)
{
}

void bootloader_before_init(void)
{
}

#define UF2_BIN_OFFSET  (0x10000)
#define UF2_BIN_SIZE    (CONFIG_PARTITION_TABLE_OFFSET - UF2_BIN_OFFSET)

static void image_loader_from_uf2(void)
{
    bootloader_state_t bs = {
        .factory = {
            // TODO: make this configurable
            .offset = UF2_BIN_OFFSET,
            .size   = UF2_BIN_SIZE
        },
    };
    bootloader_utility_load_boot_image(&bs, FACTORY_INDEX);
}

/**
 * @brief Check if there is a valid app in the flash
 *
 * @param detailed_check
 *        -true: esp_image_verify will read the whole image and try to verify diginal signature, calculating the sha256 over the whole image.
 *        -false: bootloader_common_get_partition_description will read the first 256 bytes and check a magic value (ESP_APP_DESC_MAGIC_WORD).
 * @return
 *        -true
 *        -false
 */
static bool check_if_valid_app(bool detailed_check)
{
    bootloader_state_t bs = {0};
    if (!bootloader_utility_load_partition_table(&bs)) {
        ESP_LOGE(TAG, "load partition table error!");
        return false;
    }

    esp_partition_pos_t app_array[MAX_OTA_SLOTS + 1 + 1 ] = {0}; // ota, factrory and test
    int indx_app = 0;
    if (bs.factory.offset) {
        app_array[indx_app++] = bs.factory;
    }
    if (bs.factory.offset) {
        app_array[indx_app++] = bs.test;
    }
    for (int i = 0; i < bs.app_count; i++) {
        app_array[indx_app++] = bs.ota[i];
    }

    for (int i = 0; i < indx_app; i++) {
        esp_image_metadata_t image_data = {0};
        if (detailed_check) {
            if (esp_image_verify(ESP_IMAGE_VERIFY_SILENT, &app_array[i], &image_data) == ESP_OK) {
                ESP_LOGI(TAG, "Valid app is 0x%08x\n", app_array[i].offset);
                return true;
            }
        }
        /**
        * @brief A simpler check, not yet enabled.
        *
        * else {
        *     esp_app_desc_t app_desc;
        *     if (bootloader_common_get_partition_description(&app_array[i], &app_desc) == ESP_OK) {
        *         esp_rom_printf("App at 0x%08x is not empty\n", app_array[i].offset);
        *         return true;
        *     }
        * }
        */
    }
    return false;
}

/**
 * @brief Get the restart reason object
 *
 * @return int(esp_reset_reason_t)
 */
static int get_restart_reason(void)
{
#define RST_REASON_BIT  0x80000000
#define RST_REASON_MASK 0x7FFF
#define RST_REASON_SHIFT 16

    uint32_t reset_reason_hint = REG_READ(RTC_RESET_CAUSE_REG);
    uint32_t high = (reset_reason_hint >> RST_REASON_SHIFT) & RST_REASON_MASK;
    uint32_t low = reset_reason_hint & RST_REASON_MASK;
    if ((reset_reason_hint & RST_REASON_BIT) == 0 || high != low) {
        return 0;
    }
    return (int) low;
}

#ifdef CONFIG_BOOTLOADER_UF2_GPIO_LEVEL
#define DETECTED_LEVEL 1
#else
#define DETECTED_LEVEL 0
#endif

#define UF2_RESET_REASON_VALUE (CONFIG_UF2_OTA_RESET_REASON_VALUE)

void bootloader_after_init(void)
{
    uint32_t reset_reason = get_restart_reason();
    if (reset_reason == UF2_RESET_REASON_VALUE) {
        ESP_LOGI(TAG, "Reset reason is UF2, loading UF2 image");
        image_loader_from_uf2();
    }

    esp_comm_gpio_hold_t hold = bootloader_common_check_long_hold_gpio_level(CONFIG_BOOTLOADER_UF2_GPIO_NUM, CONFIG_BOOTLOADER_UF2_GPIO_PULL_TIME_SECONDS, DETECTED_LEVEL);
    if (GPIO_LONG_HOLD == hold) {
        ESP_LOGI(TAG, "Hold detected, loading UF2 image");
        image_loader_from_uf2();
    }

    if (!check_if_valid_app(true)) {
        ESP_LOGI(TAG, "No valid app found, loading UF2 image");
        image_loader_from_uf2();
    }
}
