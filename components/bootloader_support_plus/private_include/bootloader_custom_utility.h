/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_flash_partitions.h"
#include "bootloader_utility.h"
#include "bootloader_custom_ota.h"

#ifdef CONFIG_BOOTLOADER_WDT_ENABLE
#include "hal/wdt_hal.h"
#if !defined(RWDT_HAL_CONTEXT_DEFAULT)
#if CONFIG_IDF_TARGET_ESP32H2
#define RWDT_HAL_CONTEXT_DEFAULT()     {.inst = WDT_RWDT, .rwdt_dev = &LP_WDT}
#else
#define RWDT_HAL_CONTEXT_DEFAULT()     {.inst = WDT_RWDT, .rwdt_dev = &RTCCNTL}
#endif
#endif // RWDT_HAL_CONTEXT_DEFAULT
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_ERR_CUSTOM_OTA_BASE        0x2100
#define CUSTOM_OTA_IMAGE_TYPE_INVALID (ESP_ERR_CUSTOM_OTA_BASE + 1)

typedef enum {
    DECOMPRESS_ENGINE,
    STORAGE_ENGINE,
    MAX_ENGINE
} bootloader_custom_ota_engine_type_t;

typedef enum {
    COMPRESS_NONE,
    COMPRESS_XZ,
} bootloader_custom_ota_compress_type_t;

typedef enum {
    STORAGE_FLASH,
} bootloader_custom_ota_storage_type_t;

typedef struct {
    uint32_t src_addr;
    uint32_t src_size;
    uint32_t src_offset;

    uint32_t dst_addr;
    uint32_t dst_size;
    uint32_t dst_offset;
} bootloader_custom_ota_config_t;

typedef struct {
    bootloader_custom_ota_config_t *config;
    bootloader_custom_ota_header_t *header;
    void                           **engines;
} bootloader_custom_ota_params_t;

typedef struct {
    int type;
    int (*init)(bootloader_custom_ota_params_t *params);                  // init OK return 0, others return -1, -2, ...
    int (*input)(void *addr, int size); // the input function just recv data and use data, and then put data to next engine; return 0 if no errors.
} bootloader_custom_ota_engine_t;

/**
 * @brief Update OTA data for a new boot partition.
 *
 * @param boot_status Pointer to bootloader state
 * @param boot_index  Boot partition index for partition containing app image to boot.
 *
 * @return
 *    - ESP_OK: OTA data updated, next reboot will use specified partition.
 *    - ESP_ERR_INVALID_ARG: partition argument was NULL or didn't point to a valid OTA partition of type "app".
 *    - ESP_ERR_OTA_VALIDATE_FAILED: Partition contained invalid app image. Also returned if secure boot is enabled and signature validation failed.
 *    - ESP_ERR_NOT_FOUND: OTA data partition not found.
 *    - ESP_ERR_FLASH_OP_TIMEOUT or ESP_ERR_FLASH_OP_FAIL: Flash erase or write failed.
 */
esp_err_t bootloader_custom_utility_updata_ota_data(const bootloader_state_t *bs, int boot_index);

/**
 * @brief Get partition info of currently configured nwe_app partition and compressed_app partition.
 *
 * @return Pointer to info for custom ota structure.
 */
bootloader_custom_ota_config_t *bootloader_custom_ota_get_config_param(void);

/**
 * @brief Get file header information stored in compressed firmware.
 *
 * @return Pointer to info for custom ota header structure.
 */
bootloader_custom_ota_header_t *bootloader_custom_ota_get_header_param(void);

#ifdef __cplusplus
}
#endif
