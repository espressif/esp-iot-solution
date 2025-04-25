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
 * @brief Check for the presence of an RNDIS interface descriptor.
 *
 * This function scans through the USB configuration descriptor to find an
 * Interface Association Descriptor (IAD) that matches the criteria for an
 * RNDIS device. Specifically, it looks for an IAD with two interfaces and
 * a function class of USB_CLASS_WIRELESS_CONTROLLER.
 *
 * @param device_desc Pointer to the USB device descriptor.
 * @param config_desc Pointer to the USB configuration descriptor.
 * @param itf_num Pointer to an integer where the first interface number will be stored if found.
 *
 * @return
 *     - ESP_OK: RNDIS interface descriptor found
 *     - ESP_ERR_NOT_FOUND: RNDIS interface descriptor not found
 */
esp_err_t usbh_rndis_interface_check(const usb_device_desc_t *device_desc, const usb_config_desc_t *config_desc, int *itf_num);

#ifdef __cplusplus
}
#endif
