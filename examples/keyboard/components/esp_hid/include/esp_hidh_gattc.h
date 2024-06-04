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

#if CONFIG_GATTC_ENABLE

#include "esp_gattc_api.h" //for the callback

/**
 * @brief HID BLE GATTC System Callback. Attach it in your code
 *        or call it from your gattc event handler to allow the HID stack to function
 * @param event     : Event type
 * @param gattc_if  : GATTC Interface ID
 * @param param     : Point to callback parameter, currently is union type
 */
void esp_hidh_gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

#endif /* CONFIG_GATTC_ENABLE */

#ifdef __cplusplus
}
#endif
