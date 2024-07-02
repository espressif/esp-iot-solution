/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_flash_partitions.h"
#include "esp_image_format.h"
#include "esp_rom_md5.h"
#include "esp_rom_crc.h"

#include "bootloader_custom_utility.h"
#include "bootloader_flash_priv.h"
#include "bootloader_custom_image_format.h"

#ifdef CONFIG_SECURE_BOOT_V2_ENABLED
#include "esp_secure_boot.h"
#define ALIGN_UP(num, align) (((num) + ((align) - 1)) & ~((align) - 1))
#endif

static const char *TAG = "custom_image";

static bootloader_custom_ota_header_t *custom_ota_header;

bootloader_custom_ota_image_type_t bootloader_custom_ota_get_header_type(const esp_partition_pos_t *part)
{
    uint8_t buf[4];

    /* Read the first 4 bytes to check the image type:
     *  custom image with esp original image headers: 0xE9 0x00
     *  custom image with header: "ESP"
     */
    if (bootloader_flash_read(part->offset, buf, 4, true) != ESP_OK) {
        ESP_LOGE(TAG, "get img type failed");
        return OTA_IMAGE_TYPE_MAX;
    }

    if (buf[0] == ESP_IMAGE_HEADER_MAGIC && buf[1] == 0x00) {
        return OTA_IMAGE_TYPE_2;
    } else if (!strcmp((char *)buf, BOOTLOADER_CUSTOM_OTA_HEADER_MAGIC)) {
        return OTA_IMAGE_TYPE_3;
    } else if (buf[0] == ESP_IMAGE_HEADER_MAGIC && buf[1] != 0x00)  {
        return OTA_IMAGE_TYPE_1;
    } else {
        return OTA_IMAGE_TYPE_MAX;
    }
}

static bool bootloader_custom_ota_header_check(uint32_t src_addr)
{
    uint32_t header_crc = 0;
    uint32_t bootloader_custom_ota_header_length = sizeof(bootloader_custom_ota_header_t);
    custom_ota_header = bootloader_custom_ota_get_header_param();

    // Read custom image header
    bootloader_flash_read(src_addr, custom_ota_header, sizeof(bootloader_custom_ota_header_t), true);

    // check OTA version
    if (custom_ota_header->version > BOOTLOADER_CUSTOM_OTA_HEADER_VERSION) {
        ESP_LOGW(TAG, "custom OTA version check error!");
        return false;
    }

    header_crc = custom_ota_header->crc32;
    // check CRC32
    if (esp_rom_crc32_le(0, (const uint8_t *)custom_ota_header, bootloader_custom_ota_header_length - sizeof(custom_ota_header->crc32)) != header_crc) {
        ESP_LOGW(TAG, "custom OTA CRC32 check error!");
        return false;
    }

    return true;
}

#if defined(BOOTLOADER_BUILD) && defined(CONFIG_SECURE_SIGNED_APPS_RSA_SCHEME) && defined(CONFIG_SECURE_BOOT_V2_ENABLED) && !defined(CONFIG_SKIP_VALIDATE_CUSTOM_COMPRESSED_DATA)
static bool bootloader_custom_ota_verify_signature(uint32_t src_addr)
{
    uint32_t bootloader_custom_ota_header_length = sizeof(bootloader_custom_ota_header_t);
    uint32_t packed_image_len = ALIGN_UP((custom_ota_header->length + bootloader_custom_ota_header_length), FLASH_SECTOR_SIZE);

#ifdef  CONFIG_BOOTLOADER_CUSTOM_DEBUG_ON
    ESP_LOGI(TAG, "sign_length is %0xu, src_addr is %0xu, packed_image_len is %0xu", custom_ota_header->length, src_addr, packed_image_len);
#endif

    //check signature
    esp_err_t verify_err = esp_secure_boot_verify_signature(src_addr, packed_image_len);
    if (verify_err != ESP_OK) {
        ESP_LOGE(TAG, "verify signature fail, err:%0xu", verify_err);
        return false;
    }

    ESP_LOGI(TAG, "verify signature success when decompressor");
    return true;
}
#endif

#if defined(BOOTLOADER_BUILD) && !defined(CONFIG_SKIP_VALIDATE_CUSTOM_COMPRESSED_DATA)
static bool bootloader_custom_ota_md5_check(uint32_t src_addr)
{
#define MD5_BUF_SIZE    (1024 * 4)

    // check MD5
    md5_context_t md5_context;
    uint8_t digest[16];
    uint8_t data[MD5_BUF_SIZE];
    uint32_t len = 0;
    uint32_t bootloader_custom_ota_header_length = sizeof(bootloader_custom_ota_header_t);

    esp_rom_md5_init(&md5_context);
    for (uint32_t loop = 0; loop < custom_ota_header->length;) {
        len = (custom_ota_header->length - loop > MD5_BUF_SIZE) ? MD5_BUF_SIZE : (custom_ota_header->length - loop);
        bootloader_flash_read(src_addr + bootloader_custom_ota_header_length + loop, data, len, true);
        esp_rom_md5_update(&md5_context, data, len);
        loop += len;
    }
    esp_rom_md5_final(digest, &md5_context);

    if (memcmp(custom_ota_header->md5, digest, sizeof(digest))) {
        ESP_LOGE(TAG, "custom OTA MD5 check error!");
        return false;
    }

    return true;
}
#endif

#ifndef BOOTLOADER_BUILD
#include "esp_partition.h"
#include "esp_ota_ops.h"
#endif

int __real_esp_image_verify(esp_image_load_mode_t mode, const esp_partition_pos_t *part, esp_image_metadata_t *data);
int __wrap_esp_image_verify(esp_image_load_mode_t mode, const esp_partition_pos_t *part, esp_image_metadata_t *data);

esp_err_t __wrap_esp_image_verify(esp_image_load_mode_t mode, const esp_partition_pos_t *part, esp_image_metadata_t *data)
{
    bootloader_custom_ota_image_type_t image_type = bootloader_custom_ota_get_header_type(part);

    if (image_type == OTA_IMAGE_TYPE_1) {
#ifndef BOOTLOADER_BUILD
        ESP_LOGW(TAG, "OTA into standard image, even when custom bootloder is enabled");
#else //in BOOTLOADER_BUILD
        return __real_esp_image_verify(mode, part, data);
#endif
    } else if (image_type == OTA_IMAGE_TYPE_2 || image_type == OTA_IMAGE_TYPE_3) {
        bootloader_custom_ota_config_t *custom_ota_config = bootloader_custom_ota_get_config_param();

#ifndef BOOTLOADER_BUILD
        custom_ota_config->src_addr = part->offset;
#endif

#if defined(BOOTLOADER_BUILD)
        if (image_type == OTA_IMAGE_TYPE_2) {
            custom_ota_config->src_addr += APP_IMAGE_HEADER_LEN_DEFAULT;   // hard code here, the length of esp original image headers is 0x120
        }

        if (!bootloader_custom_ota_header_check(custom_ota_config->src_addr)) {
            return ESP_FAIL;
        }

#ifndef CONFIG_SKIP_VALIDATE_CUSTOM_COMPRESSED_DATA
        if (!bootloader_custom_ota_md5_check(custom_ota_config->src_addr)) {
            return ESP_FAIL;
        }

#if defined(CONFIG_SECURE_SIGNED_APPS_RSA_SCHEME) && defined(CONFIG_SECURE_BOOT_V2_ENABLED)
        if (!bootloader_custom_ota_verify_signature(custom_ota_config->src_addr)) {
            return ESP_FAIL;
        }
#endif // END CONFIG_SECURE_SIGNED_APPS_RSA_SCHEME && CONFIG_SECURE_BOOT_V2_ENABLED
#endif // END CONFIG_SKIP_VALIDATE_CUSTOM_COMPRESSED_DATA
#elif !defined(CONFIG_SKIP_VALIDATE_CUSTOM_COMPRESSED_HEADER) // NOT BOOTLOADER_BUILD
        if (image_type == OTA_IMAGE_TYPE_2) {
            custom_ota_config->src_addr += APP_IMAGE_HEADER_LEN_DEFAULT;   // hard code here, the length of esp original image headers is 0x120
        }

        if (!bootloader_custom_ota_header_check(custom_ota_config->src_addr)) {
            return ESP_FAIL;
        }
#endif
    } else {
        return CUSTOM_OTA_IMAGE_TYPE_INVALID;
    }

    return ESP_OK;
}

#ifndef BOOTLOADER_BUILD

#include "esp_partition.h"

const esp_partition_t* __real_esp_ota_get_next_update_partition(const esp_partition_t *start_from);
const esp_partition_t* __wrap_esp_ota_get_next_update_partition(const esp_partition_t *start_from);

const esp_partition_t* __wrap_esp_ota_get_next_update_partition(const esp_partition_t *start_from)
{
    const esp_partition_t *default_ota = NULL;

    /* We use the last OTA partition as the OTA partition, both for compressed and compressed + diff.

       This loop iterates subtypes instead of using esp_partition_find, so we
       get all OTA partitions in a known order (low slot to high slot).
    */

    for (esp_partition_subtype_t t = ESP_PARTITION_SUBTYPE_APP_OTA_0;
            t != ESP_PARTITION_SUBTYPE_APP_OTA_MAX;
            t++) {
        const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_APP, t, NULL);
        if (p == NULL) {
            continue;
        }

        default_ota = p;
    }

    ESP_LOGI(TAG, "OTA to %s", default_ota->label);

    return default_ota;
}
#endif
