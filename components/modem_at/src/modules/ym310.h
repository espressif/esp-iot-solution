/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../include/modem_at_parser.h"

typedef enum {
    YM310_USB_MODE_ECM = 1, // from AT Commands Reference
    YM310_USB_MODE_RNDIS = 0,
} ym310_usb_mode_t;

esp_err_t at_cmd_ym310_set_usb(at_handle_t ath, void *param, void *result);

#ifdef __cplusplus
}
#endif
