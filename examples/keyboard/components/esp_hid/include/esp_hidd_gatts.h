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

#if CONFIG_GATTS_ENABLE

#include "esp_gatts_api.h" //for the callback

/**
 * @brief HID BLE GATTS System Callback. Attach it in your code
 *        or call it from your gatts event handler to allow the HID stack to function
 * @param event     : Event type
 * @param gatts_if  : GATTS Interface ID
 * @param param     : Point to callback parameter, currently is union type
 */
void esp_hidd_gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

#endif /* CONFIG_GATTS_ENABLE */

#ifdef __cplusplus
}
#endif
