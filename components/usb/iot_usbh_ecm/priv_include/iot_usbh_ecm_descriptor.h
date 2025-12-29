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

typedef struct {
    uint8_t bFunctionLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t iMACAddress;
    uint32_t bmEthernetStatistics;
    uint16_t wMaxSegmentSize;
    uint16_t wNumberMCFilters;
    uint8_t bNumberPowerFilters;
} USB_DESC_ATTR usb_ecm_function_desc_t;

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
 * @brief Get the ECM function Descriptor.
 *
 * This function scans through the USB configuration descriptor to find the
 * Ethernet Networking Functional Descriptor.
 *
 * @param[in] device_desc Pointer to USB device descriptor
 * @param[in] config_desc Pointer to USB configuration descriptor
 * @param[out] ret_desc Pointer to store the found ECM function descriptor
 *
 * @return
 *      - ESP_OK: ECM function descriptor found and stored successfully
 *      - ESP_ERR_NOT_FOUND: ECM function descriptor not found
 */
esp_err_t usbh_get_ecm_function_desc(const usb_device_desc_t *device_desc, const usb_config_desc_t *config_desc, const usb_ecm_function_desc_t **ret_desc);

#ifdef __cplusplus
}
#endif
