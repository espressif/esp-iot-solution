/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#ifdef BOOTLOADER_BUILD
#include "bootloader_utility.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define BOOTLOADER_CUSTOM_OTA_HEADER_MAGIC      "ESP"
#define APP_IMAGE_HEADER_LEN_DEFAULT            288 // sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t). See esp_app_format.

#if CONFIG_ENABLE_LEGACY_ESP_BOOTLOADER_PLUS_V2_SUPPORT
#define BOOTLOADER_CUSTOM_OTA_PARTITION_SUBTYPE 0x22
#define BOOTLOADER_CUSTOM_OTA_HEADER_VERSION    2 // don't use v3 header in legacy esp-bootloader-plus
typedef struct {
    uint8_t  magic[4];             /*!< The magic num of compressed firmware, default is BOOTLOADER_CUSTOM_OTA_HEADER_MAGIC */
    uint8_t  version;              /*!< Custom ota header version */
    uint8_t  compress_type: 4;     /*!< The compression algorithm used by the compression firmware */
    uint8_t  delta_type: 4;        /*!< The diff algorithm used to generate original patch file */
    uint8_t  encryption_type;      /*!< The Encryption algorithm for pre-encrypted compressed firmware(or compressed patch) */
    uint8_t  reserved;             /*!< Reserved */
    uint8_t  firmware_version[32]; /*!< The compressed firmware version, it can be same as the new firmware */
    uint32_t length;               /*!< The length of compressed firmware(or compressed patch) */
    uint8_t  md5[32];              /*!< The MD5 of compressed firmware(or compressed patch) */
    uint32_t base_len_for_crc32;   /*!< The length of data used for CRC check in base_app(have to be 4-byte aligned.) */
    uint32_t base_crc32;           /*!< The CRC32 of the base_app[0~base_len_for_crc32] */
    uint32_t crc32;                /*!< The CRC32 of the header */
} __attribute__((packed)) bootloader_custom_ota_header_t; // Coyied from bootloader_custom_ota_header_v2_t

#else
#define BOOTLOADER_CUSTOM_OTA_HEADER_VERSION    3

typedef struct {
    uint8_t  magic[4];             /*!< The magic num of compressed firmware, default is BOOTLOADER_CUSTOM_OTA_HEADER_MAGIC */
    uint8_t  version;              /*!< Custom ota header version */
    uint8_t  compress_type: 4;     /*!< The compression algorithm used by the compression firmware */
    uint8_t  reserved_4b: 4;       /*!< reserved 4 bits */
    uint8_t  reserved[10];         /*!< Reserved */
    uint32_t length;               /*!< The length of compressed firmware(or compressed patch) */
    uint8_t  md5[16];              /*!< The MD5 of compressed firmware(or compressed patch) */
    uint32_t crc32;                /*!< The CRC32 of the header */
} __attribute__((packed)) bootloader_custom_ota_header_t;
#endif

#ifdef BOOTLOADER_BUILD
/**
 * @brief Get current post field information
 *
 * @param boot_status Pointer to bootloader state
 * @param boot_index  Boot partition index
 *
 * @return The actual boot partition index
 */
int bootloader_custom_ota_main(bootloader_state_t *boot_status, int boot_index);
#endif

#ifdef __cplusplus
}
#endif
