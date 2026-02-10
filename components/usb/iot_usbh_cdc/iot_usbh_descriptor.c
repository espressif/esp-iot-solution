/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_check.h"
#include "usb/usb_helpers.h"
#include "iot_usbh_descriptor.h"
#include "usb/usb_types_ch9.h"
#include "iot_usbh_cdc_type.h"

static const char *TAG = "cdc_descriptor";

#define USB_SUBCLASS_NULL          0x00
#define USB_SUBCLASS_COMMON        0x02
#define USB_PROTOCOL_NULL          0x00
#define USB_DEVICE_PROTOCOL_IAD    0x01

/**
 * @brief Searches interface by index and verifies its CDC-compliance
 *
 * @param[in] device_desc Pointer to Device descriptor
 * @param[in] config_desc Pointer do Configuration descriptor
 * @param[in] intf_idx    Index of the required interface
 * @return true  The required interface is CDC compliant
 * @return false The required interface is NOT CDC compliant
 */
static bool cdc_parse_is_cdc_compliant(const usb_device_desc_t *device_desc, const usb_config_desc_t *config_desc, uint8_t intf_idx)
{
    int desc_offset = 0;

    if (device_desc->bDeviceClass == USB_CLASS_PER_INTERFACE || device_desc->bDeviceClass == USB_CLASS_COMM) {
        const usb_intf_desc_t *intf_desc = usb_parse_interface_descriptor(config_desc, intf_idx, 0, NULL);
        if (intf_desc->bInterfaceClass == USB_CLASS_COMM) {
            // 1. This is a Communication Device Class: Class defined in Interface descriptor
            return true;
        }
    }

    if (((device_desc->bDeviceClass == USB_CLASS_MISC) && (device_desc->bDeviceSubClass == USB_SUBCLASS_COMMON) &&
            (device_desc->bDeviceProtocol == USB_DEVICE_PROTOCOL_IAD)) ||
            ((device_desc->bDeviceClass == USB_CLASS_PER_INTERFACE) && (device_desc->bDeviceSubClass == USB_SUBCLASS_NULL) &&
             (device_desc->bDeviceProtocol == USB_PROTOCOL_NULL))) {
        const usb_standard_desc_t *this_desc = (const usb_standard_desc_t *)config_desc;
        while ((this_desc = usb_parse_next_descriptor_of_type(this_desc, config_desc->wTotalLength, USB_B_DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION, &desc_offset))) {
            const usb_iad_desc_t *iad_desc = (const usb_iad_desc_t *)this_desc;
            if ((iad_desc->bFirstInterface == intf_idx) &&
                    (iad_desc->bInterfaceCount == 2)) {
                // 2. This is a composite device, that uses Interface Association Descriptor
                return true;
            }
        };
    }
    return false;
}

/**
 * @brief Parse CDC interface descriptor and return interface information.
 *        this function not only parse the standard CDC interface descriptor, but also parse the vendor specific CDC interface descriptor.
 *        If the interface is a CDC Notification interface, return both Notification and Data interfaces.
 *        If the interface is a CDC Data interface, return only the Data interface.
 *
 * There are several types of CDC interface:
 *   1. Standard CDC-> [
 *                      IAD Descriptor,
 *                      notification interface: [interrupt endpoint],
 *                      Some class specific descriptor,
 *                      data interface: [in bulk endpoint, out bulk endpoint]
 *                      ]
 *   2. Vendor specific CDC type1 -> [
 *                                      specific interface: [
 *                                          Some class specific descriptor,
 *                                          interrupt endpoint, // for notification
 *                                          in bulk endpoint,
 *                                          out bulk endpoint
 *                                       ]
 *                                    ]
 *   3. Vendor specific CDC type2 -> [
 *                                      specific interface: [
 *                                          in bulk endpoint,
 *                                          out bulk endpoint
 *                                       ]
 *                                    ]
 *
 * @param device_desc Pointer to USB device descriptor
 * @param config_desc Pointer to USB configuration descriptor
 * @param intf_idx Index of the interface to parse
 * @param info_ret Pointer to store the parsed interface information
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t cdc_parse_interface_descriptor(const usb_device_desc_t *device_desc, const usb_config_desc_t *config_desc, uint8_t intf_idx, usbh_cdc_parsed_info_t *info_ret)
{
    int desc_offset = 0;

    const usb_intf_desc_t *first_intf_desc = usb_parse_interface_descriptor(config_desc, intf_idx, 0, &desc_offset);
    ESP_RETURN_ON_FALSE(first_intf_desc, ESP_ERR_NOT_FOUND, TAG, "Required interface no %d was not found.", intf_idx);

    int temp_offset = desc_offset;
    if (first_intf_desc->bInterfaceClass == USB_CLASS_VENDOR_SPEC) {
        if (3 == first_intf_desc->bNumEndpoints) {
            // if there are 3 endpoints, check if the first one is a CDC Header descriptor. It is a relatively strict inspection
            const usb_standard_desc_t *this_desc = (const usb_standard_desc_t *)first_intf_desc;
            this_desc = usb_parse_next_descriptor_of_type(this_desc, config_desc->wTotalLength, USB_CS_DESCRIPTOR_TYPE_INTERFACE, &desc_offset);
            if (this_desc) {
                const usb_cdc_header_func_desc_t *cdc_header = (const usb_cdc_header_func_desc_t *)this_desc;
                ESP_RETURN_ON_FALSE(cdc_header->bDescriptorSubtype == USB_CS_DESCRIPTOR_SUBTYPE_HEADER, ESP_ERR_NOT_FOUND, TAG,
                                    "interface no %d is not a supported CDC interface.", intf_idx);
                ESP_RETURN_ON_FALSE(cdc_header->bcdCDC == 0x0110, ESP_ERR_NOT_FOUND, TAG, "interface no %d is not a supported CDC interface.", intf_idx);
            }
        }

        for (int i = 0; i < (first_intf_desc)->bNumEndpoints; i++) {
            const usb_ep_desc_t *this_ep = usb_parse_endpoint_descriptor_by_index(first_intf_desc, i, config_desc->wTotalLength, &desc_offset);
            assert(this_ep);
            if (USB_EP_DESC_GET_XFERTYPE(this_ep) == USB_TRANSFER_TYPE_INTR) {
                info_ret->notif_intf = first_intf_desc;
                info_ret->notif_ep = this_ep;
            } else if (USB_EP_DESC_GET_XFERTYPE(this_ep) == USB_TRANSFER_TYPE_BULK) {
                info_ret->data_intf = first_intf_desc;
                if (USB_EP_DESC_GET_EP_DIR(this_ep)) {
                    info_ret->in_ep = this_ep;
                } else {
                    info_ret->out_ep = this_ep;
                }
            }
            desc_offset = temp_offset;
        }
    } else {
        const bool cdc_compliant = cdc_parse_is_cdc_compliant(device_desc, config_desc, intf_idx);
        if (cdc_compliant) {
            info_ret->notif_intf = first_intf_desc; // We make sure that intf_desc is set for CDC compliant devices that use EP0 as notification element
            const usb_ep_desc_t *this_ep = usb_parse_endpoint_descriptor_by_index(first_intf_desc, 0, config_desc->wTotalLength, &desc_offset);
            assert(this_ep);
            assert(USB_EP_DESC_GET_XFERTYPE(this_ep) == USB_TRANSFER_TYPE_INTR);
            info_ret->notif_ep = this_ep;

            const int num_of_alternate = usb_parse_interface_number_of_alternate(config_desc, intf_idx + 1);
            for (int i = 0; i < num_of_alternate + 1; i++) {
                const usb_intf_desc_t *second_intf_desc = usb_parse_interface_descriptor(config_desc, intf_idx + 1, i, &desc_offset);
                temp_offset = desc_offset;
                if (second_intf_desc && second_intf_desc->bNumEndpoints == 2) {
                    for (int i = 0; i < second_intf_desc->bNumEndpoints; i++) {
                        const usb_ep_desc_t *this_ep = usb_parse_endpoint_descriptor_by_index(second_intf_desc, i, config_desc->wTotalLength, &desc_offset);
                        assert(this_ep);
                        if (USB_EP_DESC_GET_XFERTYPE(this_ep) == USB_TRANSFER_TYPE_BULK) {
                            info_ret->data_intf = second_intf_desc;
                            if (USB_EP_DESC_GET_EP_DIR(this_ep)) {
                                info_ret->in_ep = this_ep;
                            } else {
                                info_ret->out_ep = this_ep;
                            }
                        }
                        desc_offset = temp_offset;
                    }
                    break;
                }
            }
        }
    }
    if (info_ret->notif_ep) {
        ESP_LOGI(TAG, "Found NOTIF endpoint: %d", USB_EP_DESC_GET_EP_NUM(info_ret->notif_ep));
    }
    if (info_ret->out_ep) {
        ESP_LOGI(TAG, "Found OUT endpoint: %d", USB_EP_DESC_GET_EP_NUM(info_ret->out_ep));
    }
    if (info_ret->in_ep) {
        ESP_LOGI(TAG, "Found IN endpoint: %d", USB_EP_DESC_GET_EP_NUM(info_ret->in_ep));
    }
    // If we did not find IN and OUT data endpoints, the device cannot be used
    return (info_ret->in_ep && info_ret->out_ep) ? ESP_OK : ESP_ERR_NOT_FOUND;
}
