/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_err.h"
#include "esp_log.h"
#include "usb/usb_host.h"
#include "usb/usb_helpers.h"
#include "iot_usbh_cdc_type.h"
#include "iot_usbh_ecm_descriptor.h"

typedef struct {
    uint8_t bFunctionLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint16_t bcdCDC;
} USB_DESC_ATTR usb_class_specific_desc_header_t;

esp_err_t usbh_ecm_interface_check(const usb_device_desc_t *device_desc, const usb_config_desc_t *config_desc, int *itf_num)
{
    size_t total_length = config_desc->wTotalLength;
    int desc_offset = 0;

    // Get first Interface descriptor
    const usb_standard_desc_t *current_desc = (const usb_standard_desc_t *)config_desc;
    while ((current_desc = usb_parse_next_descriptor_of_type(current_desc, total_length, USB_B_DESCRIPTOR_TYPE_INTERFACE, &desc_offset))) {
        const usb_intf_desc_t *intf_desc = (const usb_intf_desc_t *)current_desc;
        if ((intf_desc->bInterfaceClass == USB_COMMUNICATIONS_INTERFACE_CLASS) &&
                (intf_desc->bInterfaceSubClass == USB_COMMUNICATIONS_INTERFACE_SUBCLASS_ETHERNET_NETWORKING_CONTROL_MODEL) &&
                (intf_desc->bInterfaceProtocol == 0x00)) {

            *itf_num = intf_desc->bInterfaceNumber;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t usbh_get_ecm_function_desc(const usb_device_desc_t *device_desc, const usb_config_desc_t *config_desc, const usb_ecm_function_desc_t **ret_desc)
{
    size_t total_length = config_desc->wTotalLength;
    int desc_offset = 0;

    // Get first Interface descriptor
    const usb_standard_desc_t *current_desc = (const usb_standard_desc_t *)config_desc;
    while ((current_desc = usb_parse_next_descriptor_of_type(current_desc, total_length, 0x24, &desc_offset))) {
        const usb_class_specific_desc_header_t *desc = (const usb_class_specific_desc_header_t *)current_desc;
        if (desc->bDescriptorSubtype == USB_CS_DESCRIPTOR_SUBTYPE_ETHERNET_NETWORKING) {
            *ret_desc = (const usb_ecm_function_desc_t *)(current_desc);
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}
