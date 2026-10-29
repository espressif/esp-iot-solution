/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Apache-2.0
*/

#include "esp_check.h"
#include "usbh_helper.h"

#define TAG "usbh_helper"

const usb_device_match_id_t _g_esp_usb_device_match_id_any[] = {
    {
        .match_flags = USB_DEVICE_ID_MATCH_VENDOR | USB_DEVICE_ID_MATCH_PRODUCT,
        .idVendor = USB_DEVICE_VENDOR_ANY,
        .idProduct = USB_DEVICE_PRODUCT_ANY,
    },
    {0}  // Null-terminated
};

/* returns 0 if no match, 1 if match */
int usbh_match_device(const usb_device_desc_t *dev_desc, const usb_device_match_id_t *id)
{
    ESP_RETURN_ON_FALSE(dev_desc != NULL, 0, TAG, "Invalid device descriptor");
    ESP_RETURN_ON_FALSE(id != NULL, 0, TAG, "Invalid match ID");

    if ((id->match_flags & USB_DEVICE_ID_MATCH_VENDOR) &&
            id->idVendor != USB_DEVICE_VENDOR_ANY &&
            id->idVendor != dev_desc->idVendor) {
        return 0;
    }

    if ((id->match_flags & USB_DEVICE_ID_MATCH_PRODUCT) &&
            id->idProduct != USB_DEVICE_PRODUCT_ANY &&
            id->idProduct != dev_desc->idProduct) {
        return 0;
    }

    /* No need to test id->bcdDevice_lo != 0, since 0 is never
       greater than any unsigned number. */
    if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_BCD) &&
            (id->bcdDevice > dev_desc->bcdDevice)) {
        return 0;
    }

    if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_CLASS) &&
            (id->bDeviceClass != dev_desc->bDeviceClass)) {
        return 0;
    }

    if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_SUBCLASS) &&
            (id->bDeviceSubClass != dev_desc->bDeviceSubClass)) {
        return 0;
    }

    if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_PROTOCOL) &&
            (id->bDeviceProtocol != dev_desc->bDeviceProtocol)) {
        return 0;
    }

    return 1;
}

int usbh_match_interface(const usb_intf_desc_t *intf_desc, const usb_device_match_id_t *id)
{
    ESP_RETURN_ON_FALSE(intf_desc != NULL, 0, TAG, "Invalid interface descriptor");
    ESP_RETURN_ON_FALSE(id != NULL, 0, TAG, "Invalid match ID");

    if ((id->match_flags & USB_DEVICE_ID_MATCH_INT_CLASS) &&
            (id->bInterfaceClass != intf_desc->bInterfaceClass)) {
        return 0;
    }

    if ((id->match_flags & USB_DEVICE_ID_MATCH_INT_SUBCLASS) &&
            (id->bInterfaceSubClass != intf_desc->bInterfaceSubClass)) {
        return 0;
    }

    if ((id->match_flags & USB_DEVICE_ID_MATCH_INT_PROTOCOL) &&
            (id->bInterfaceProtocol != intf_desc->bInterfaceProtocol)) {
        return 0;
    }

    if ((id->match_flags & USB_DEVICE_ID_MATCH_INT_NUMBER) &&
            (id->bInterfaceNumber != intf_desc->bInterfaceNumber)) {
        return 0;
    }

    return 1;
}

int usbh_match_device_from_id_list(const usb_device_desc_t *dev_desc, const usb_device_match_id_t *id_list)
{
    ESP_RETURN_ON_FALSE(dev_desc != NULL, 0, TAG, "Invalid device descriptor");
    ESP_RETURN_ON_FALSE(id_list != NULL, 0, TAG, "Invalid match ID");

    while (id_list->match_flags != 0) {
        if (usbh_match_device(dev_desc, id_list)) {
            return 1; // Match found
        }
        id_list++;
    }
    return 0;
}

int usbh_match_interface_in_configuration(const usb_config_desc_t *config_desc, const usb_device_match_id_t *id, int *matched_intf_num)
{
    ESP_RETURN_ON_FALSE(config_desc != NULL, 0, TAG, "Invalid configuration descriptor");
    ESP_RETURN_ON_FALSE(id != NULL, 0, TAG, "Invalid match ID");

    size_t total_length = config_desc->wTotalLength;
    int desc_offset = 0;
    // Get first Interface descriptor
    const usb_standard_desc_t *current_desc = (const usb_standard_desc_t *)config_desc;
    while ((current_desc = usb_parse_next_descriptor_of_type(current_desc, total_length, USB_B_DESCRIPTOR_TYPE_INTERFACE, &desc_offset))) {
        const usb_intf_desc_t *intf_desc = (const usb_intf_desc_t *)current_desc;
        if (usbh_match_interface(intf_desc, id)) {
            if (matched_intf_num) {
                *matched_intf_num = intf_desc->bInterfaceNumber; // Match found
            }
            return 1;
        }
    }
    return 0; // No match found
}

int usbh_match_id_from_list(const usb_device_desc_t *dev_desc, const usb_config_desc_t *config_desc, const usb_device_match_id_t *id_list, int *matched_intf_num)
{
    ESP_RETURN_ON_FALSE(dev_desc != NULL, 0, TAG, "Invalid device descriptor");
    ESP_RETURN_ON_FALSE(config_desc != NULL, 0, TAG, "Invalid configuration descriptor");
    ESP_RETURN_ON_FALSE(id_list != NULL, 0, TAG, "Invalid match ID");

    while (id_list->match_flags != 0) {
        if (0 == usbh_match_device(dev_desc, id_list)) {
            goto _skip; // Device does not match, skip to next ID
        }
        if ((USB_DEVICE_ID_MATCH_INTF & id_list->match_flags) && 0 == usbh_match_interface_in_configuration(config_desc, id_list, matched_intf_num)) {
            goto _skip; // Interface does not match, skip to next ID
        }
        return 1; // Match found
_skip:
        id_list++;
    }
    return 0; // No match found
}
