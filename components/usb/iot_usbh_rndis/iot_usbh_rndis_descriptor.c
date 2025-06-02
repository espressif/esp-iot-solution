/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_err.h"
#include "esp_log.h"
#include "usb/usb_host.h"
#include "usb/usb_helpers.h"

esp_err_t usbh_rndis_interface_check(const usb_device_desc_t *device_desc, const usb_config_desc_t *config_desc, int *itf_num)
{
    size_t total_length = config_desc->wTotalLength;
    int desc_offset = 0;

    // Get first Interface descriptor
    const usb_standard_desc_t *current_desc = (const usb_standard_desc_t *)config_desc;
    while ((current_desc = usb_parse_next_descriptor_of_type(current_desc, total_length, USB_B_DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION, &desc_offset))) {
        const usb_iad_desc_t *iad_desc = (const usb_iad_desc_t *)current_desc;
        if ((iad_desc->bInterfaceCount == 2) && (iad_desc->bFunctionClass == USB_CLASS_WIRELESS_CONTROLLER)) {
            *itf_num = iad_desc->bFirstInterface;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}
