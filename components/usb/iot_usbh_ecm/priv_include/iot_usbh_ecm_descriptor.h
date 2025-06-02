/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "usb/usb_host.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Check for the presence of an ECM interface descriptor.
 *
 * This function scans through the USB configuration descriptor to find an
 * Interface Association Descriptor (IAD) that matches the criteria for an
 * ECM device. Specifically, it looks for an IAD with two interfaces and
 * a function class of USB_CLASS_WIRELESS_CONTROLLER.
 *
 * @param device_desc Pointer to the USB device descriptor.
 * @param config_desc Pointer to the USB configuration descriptor.
 * @param itf_num Pointer to an integer where the first interface number will be stored if found.
 *
 * @return
 *     - ESP_OK: ECM interface descriptor found
 *     - ESP_ERR_NOT_FOUND: ECM interface descriptor not found
 */
esp_err_t usbh_ecm_interface_check(const usb_device_desc_t *device_desc, const usb_config_desc_t *config_desc, int *itf_num);

/**
 * @brief Check and get MAC address string index from ECM device descriptor
 *
 * This function scans through the USB configuration descriptor to find the
 * Ethernet Networking Functional Descriptor and extracts the MAC address string index.
 *
 * @param[in] device_desc Pointer to USB device descriptor
 * @param[in] config_desc Pointer to USB configuration descriptor
 * @param[out] mac_str_index Pointer to store the MAC address string index
 *
 * @return
 *      - ESP_OK: MAC address string index found and stored successfully
 *      - ESP_ERR_NOT_FOUND: MAC address string index not found
 */
esp_err_t usbh_ecm_mac_str_index_check(const usb_device_desc_t *device_desc, const usb_config_desc_t *config_desc, uint8_t *mac_str_index);

#ifdef __cplusplus
}
#endif
