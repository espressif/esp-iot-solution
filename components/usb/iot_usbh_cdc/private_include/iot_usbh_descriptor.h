/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "usb/usb_types_ch9.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const usb_ep_desc_t *notif_ep;
    const usb_ep_desc_t *in_ep;
    const usb_ep_desc_t *out_ep;
    const usb_intf_desc_t *notif_intf;
    const usb_intf_desc_t *data_intf;
} cdc_parsed_info_t;

esp_err_t cdc_parse_interface_descriptor(const usb_device_desc_t *device_desc, const usb_config_desc_t *config_desc, uint8_t intf_idx, cdc_parsed_info_t *info_ret);

#ifdef __cplusplus
}
#endif
