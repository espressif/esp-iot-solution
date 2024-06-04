/*
 * SPDX-FileCopyrightText: 2017-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "sdkconfig.h"

#if CONFIG_BLUEDROID_ENABLED

#include "esp_bt_defs.h"

/**
 * @brief Open BlueTooth HID Device using BlueDroid
 * @param bda               : BT Device Address
 * @param transport         : BT Device Protocol (Classic/HID)
 * @param remote_addr_type  : BLE Remote address type
 *
 * @return: ESP_OK on success
 */
esp_hidh_dev_t *esp_hidh_dev_open(esp_bd_addr_t bda, esp_hid_transport_t transport, uint8_t remote_addr_type);

#endif /* CONFIG_BLUEDROID_ENABLED */

#ifdef __cplusplus
}
#endif
