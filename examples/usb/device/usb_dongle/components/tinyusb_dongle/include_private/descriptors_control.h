/*
 * SPDX-FileCopyrightText: 2020-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "tinyusb.h"

#ifdef __cplusplus
extern "C" {
#endif

#define USB_STRING_DESCRIPTOR_ARRAY_SIZE            8 // Max 8 string descriptors for a device. LANGID, Manufacturer, Product, Serial number + 4 user defined

/**
 * @brief Parse tinyusb configuration and prepare the device configuration pointer list to configure tinyusb driver
 *
 * @attention All descriptors passed to this function must exist for the duration of USB device lifetime
 *
 * @param[in] config tinyusb stack specific configuration
 * @retval ESP_ERR_INVALID_ARG Default configuration descriptor is provided only for CDC, MSC and NCM classes
 * @retval ESP_ERR_NO_MEM      Memory allocation error
 * @retval ESP_OK              Descriptors configured without error
 */
esp_err_t tinyusb_set_descriptors(const tinyusb_config_t *config);

/**
 * @brief Set specific string descriptor
 *
 * @attention The descriptor passed to this function must exist for the duration of USB device lifetime
 *
 * @param[in] str     UTF-8 string
 * @param[in] str_idx String descriptor index
 */
void tinyusb_set_str_descriptor(const char *str, int str_idx);

/**
 * @brief Free memory allocated during tinyusb_set_descriptors
 *
 */
void tinyusb_free_descriptors(void);

#ifdef __cplusplus
}
#endif
