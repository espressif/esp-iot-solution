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
    ML302_USB_MODE_ECM = 5, // from AT Commands Reference Guide
    ML302_USB_MODE_RNDIS = 3,
} ml302_usb_mode_t;

esp_err_t at_cmd_ml302_power_down(at_handle_t ath, void *param, void *result);

esp_err_t at_cmd_ml302_set_usb(at_handle_t ath, void *param, void *result);

#ifdef __cplusplus
}
#endif
