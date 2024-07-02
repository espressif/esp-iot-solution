/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_flash_partitions.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OTA_IMAGE_TYPE_1,           // standard image compiled from IDF
    OTA_IMAGE_TYPE_2,           // image with standard image header and custom ota header
    OTA_IMAGE_TYPE_3,           // image with custom ota header
    OTA_IMAGE_TYPE_MAX
} bootloader_custom_ota_image_type_t;

/**
 * @brief Get the type of image according to the header for the given partition
 *
 * @param part Pointer to the partition
 *
 * @return Type of image
 */
bootloader_custom_ota_image_type_t bootloader_custom_ota_get_header_type(const esp_partition_pos_t *part);

#ifdef __cplusplus
}
#endif
