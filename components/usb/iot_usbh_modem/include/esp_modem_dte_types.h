/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "iot_usbh_cdc.h"

#ifdef __cplusplus
extern "C" {
#endif

/** USB Modem ID */
typedef struct {
    usb_device_match_id_t match_id; /*!< USB device match ID */

    int modem_itf_num;  /*!< USB modem interface number, (Required) */
    int at_itf_num;  /*!< USB AT interface number, (Optional, set -1 if not use) */
    const char *name;  /*!< USB Modem name, can be NULL */
} usb_modem_id_t;

#ifdef __cplusplus
}
#endif
