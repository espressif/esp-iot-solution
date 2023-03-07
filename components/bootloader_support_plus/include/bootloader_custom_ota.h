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

#define BOOTLOADER_CUSTOM_OTA_HEADER_MAGIC 		"ESP"

#define BOOTLOADER_CUSTOM_OTA_HEADER_VERSION    3

#define APP_IMAGE_HEADER_LEN_DEFAULT            288 // sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t). See esp_app_format.

typedef struct {
    uint8_t  magic[4];             /*!< The magic num of compressed firmware, default is BOOTLOADER_CUSTOM_OTA_HEADER_MAGIC */
    uint8_t  version;              /*!< Custom ota header version */
    uint8_t  compress_type:4;      /*!< The compression algorithm used by the compression firmware */
    uint8_t  reserved_4b:4;        /*!< reserved 4 bits */
    uint8_t  reserved[10];         /*!< Reserved */
    uint32_t length;               /*!< The length of compressed firmware(or compressed patch) */
    uint8_t  md5[16];              /*!< The MD5 of compressed firmware(or compressed patch) */
    uint32_t crc32;                /*!< The CRC32 of the header */
} __attribute__((packed)) bootloader_custom_ota_header_t;

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
