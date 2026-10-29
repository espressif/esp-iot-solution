/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief Public API for Bluetooth Immediate Alert Service (0x1802) on esp_ble_conn_mgr.
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name Immediate Alert Service UUIDs and Alert Level values
 *
 * Registers with esp_ble_conn_mgr. Call @ref esp_ble_ias_init after @c esp_ble_conn_init().
 */
/**@{*/

#define BLE_IAS_UUID16                              0x1802  /*!< Primary service: Immediate Alert Service (16-bit UUID) */

#define BLE_IAS_CHR_UUID16_ALERT_LEVEL              0x2A06  /*!< Alert Level characteristic (16-bit UUID) */

#define ESP_BLE_IAS_ALERT_LEVEL_NO_ALERT            0x00    /*!< No alert */

#define ESP_BLE_IAS_ALERT_LEVEL_MILD_ALERT          0x01    /*!< Mild alert */

#define ESP_BLE_IAS_ALERT_LEVEL_HIGH_ALERT          0x02    /*!< High alert */

/**@}*/

/**
 * @brief Callback invoked when a connected client writes the Alert Level characteristic with a valid value.
 *
 * @param[in] alert_level  Value written by the client. One of @ref ESP_BLE_IAS_ALERT_LEVEL_NO_ALERT,
 *                         @ref ESP_BLE_IAS_ALERT_LEVEL_MILD_ALERT, or @ref ESP_BLE_IAS_ALERT_LEVEL_HIGH_ALERT.
 * @param[in] priv         User context registered with @ref esp_ble_ias_register_alert_cb.
 */
typedef void (*esp_ble_ias_alert_cb_t)(uint8_t alert_level, void *priv);

/**
 * @brief Register a callback for Alert Level writes.
 *
 * @param[in] cb   Callback function; pass NULL to unregister.
 * @param[in] priv Opaque pointer passed to @p cb.
 *
 * @return
 *  - ESP_OK on success
 */
esp_err_t esp_ble_ias_register_alert_cb(esp_ble_ias_alert_cb_t cb, void *priv);

/**
 * @brief Read the last Alert Level written by a GATT client.
 *
 * @param[out] level The pointer to store the Alert Level (default @ref ESP_BLE_IAS_ALERT_LEVEL_NO_ALERT).
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG if @p level is NULL
 */
esp_err_t esp_ble_ias_get_alert_level(uint8_t *level);

/**
 * @brief Register the Immediate Alert Service (0x1802) with esp_ble_conn_mgr.
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_FAIL on failure from esp_ble_conn_add_svc()
 */
esp_err_t esp_ble_ias_init(void);

#ifdef __cplusplus
}
#endif
