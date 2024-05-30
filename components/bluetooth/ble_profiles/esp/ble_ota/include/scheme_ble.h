/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifndef _SCHEME_BLE_H_
#define _SCHEME_BLE_H_

#ifdef CONFIG_OTA_WITH_PROTOCOMM
#include <protocomm.h>
#include <protocomm_ble.h>

#include "manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Scheme that can be used by manager for ota
 *          over BLE transport with GATT server
 */
extern const esp_ble_ota_scheme_t esp_ble_ota_scheme_ble;

/* This scheme specific event handler is to be used when application
 * doesn't require BT and BLE after ota has finished */
#define ESP_BLE_OTA_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM {    \
    .event_cb  = esp_ble_ota_scheme_ble_event_cb_free_btdm, \
    .user_data = NULL                                     \
}

/* This scheme specific event handler is to be used when application
 * doesn't require BLE to be active after ota has finished */
#define ESP_BLE_OTA_SCHEME_BLE_EVENT_HANDLER_FREE_BLE {    \
    .event_cb  = esp_ble_ota_scheme_ble_event_cb_free_ble, \
    .user_data = NULL                                    \
}

/* This scheme specific event handler is to be used when application
 * doesn't require BT to be active after ota has finished */
#define ESP_BLE_OTA_SCHEME_BLE_EVENT_HANDLER_FREE_BT {    \
    .event_cb  = esp_ble_ota_scheme_ble_event_cb_free_bt, \
    .user_data = NULL                                   \
}

void esp_ble_ota_scheme_ble_event_cb_free_btdm(void *user_data, esp_ble_ota_cb_event_t event, void *event_data);
void esp_ble_ota_scheme_ble_event_cb_free_ble(void *user_data, esp_ble_ota_cb_event_t event, void *event_data);
void esp_ble_ota_scheme_ble_event_cb_free_bt(void *user_data, esp_ble_ota_cb_event_t event, void *event_data);

/**
 * @brief   Set the 128 bit GATT service UUID used for ota
 *
 * This API is used to override the default 128 bit ota
 * service UUID, which is 0000ffff-0000-1000-8000-00805f9b34fb.
 *
 * This must be called before starting ota, i.e. before
 * making a call to esp_ble_ota_start(), otherwise
 * the default UUID will be used.
 *
 * @note    The data being pointed to by the argument must be valid
 *          at least till ota is started. Upon start, the
 *          manager will store an internal copy of this UUID, and
 *          this data can be freed or invalidated afterwards.
 *
 * @param[in] uuid128  A custom 128 bit UUID
 *
 * @return
 *  - ESP_OK              : Success
 *  - ESP_ERR_INVALID_ARG : Null argument
 */
esp_err_t esp_ble_ota_scheme_ble_set_service_uuid(uint8_t *uuid128);

/**
 * @brief   Set manufacturer specific data in scan response
 *
 * This must be called before starting ota, i.e. before
 * making a call to esp_ble_ota_start().
 *
 * @note    It is important to understand that length of custom manufacturer
 *          data should be within limits. The manufacturer data goes into scan
 *          response along with BLE device name. By default, BLE device name
 *          length is of 11 Bytes, however it can vary as per application use
 *          case. So, one has to honour the scan response data size limits i.e.
 *          (mfg_data_len + 2) < 31 - (device_name_length + 2 ). If the
 *          mfg_data length exceeds this limit, the length will be truncated.
 *
 * @param[in] mfg_data      Custom manufacturer data
 * @param[in] mfg_data_len  Manufacturer data length
 *
 * @return
 *  - ESP_OK              : Success
 *  - ESP_ERR_INVALID_ARG : Null argument
 */
esp_err_t esp_ble_ota_scheme_ble_set_mfg_data(uint8_t *mfg_data, ssize_t mfg_data_len);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_OTA_WITH_PROTOCOMM */
#endif
