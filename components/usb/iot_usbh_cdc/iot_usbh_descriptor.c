/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_err.h"
#include "esp_check.h"
#include "usb/usb_helpers.h"
#include "iot_usbh_descriptor.h"

static const char *TAG = "cdc_descriptor";

esp_err_t cdc_parse_interface_descriptor(const usb_device_desc_t *device_desc, const usb_config_desc_t *config_desc, uint8_t intf_idx, usb_intf_desc_t **intf_desc, cdc_parsed_info_t *info_ret)
{
    int desc_offset = 0;

    *intf_desc = (usb_intf_desc_t *)usb_parse_interface_descriptor(config_desc, intf_idx, 0, &desc_offset);
    ESP_RETURN_ON_FALSE(
        *intf_desc,
        ESP_ERR_NOT_FOUND, TAG, "Required interface no %d was not found.", intf_idx);

    int temp_offset = desc_offset;
    for (int i = 0; i < (*intf_desc)->bNumEndpoints; i++) {
        const usb_ep_desc_t *this_ep = usb_parse_endpoint_descriptor_by_index(*intf_desc, i, config_desc->wTotalLength, &desc_offset);
        assert(this_ep);

        if (USB_EP_DESC_GET_XFERTYPE(this_ep) == USB_TRANSFER_TYPE_BULK) {
            info_ret->data_intf = *intf_desc;
            if (USB_EP_DESC_GET_EP_DIR(this_ep)) {
                info_ret->in_ep = this_ep;
                printf("Found IN endpoint: %d\n", this_ep->bEndpointAddress);
            } else {
                info_ret->out_ep = this_ep;
                printf("Found OUT endpoint: %d\n", this_ep->bEndpointAddress);
            }
        }
        desc_offset = temp_offset;
    }

    // If we did not find IN and OUT data endpoints, the device cannot be used
    return (info_ret->in_ep && info_ret->out_ep) ? ESP_OK : ESP_ERR_NOT_FOUND;
}
