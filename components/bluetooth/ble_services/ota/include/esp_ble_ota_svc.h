/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief Public API for ESP BLE OTA GATT service (0x8018) on esp_ble_conn_mgr.
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name ESP BLE OTA GATT service UUIDs and limits
 *
 * Registers with esp_ble_conn_mgr. Not to be confused with esp_ble_ota_init() in the
 * ble_ota profile component; use @ref esp_ble_ota_svc_init for this GATT service only.
 */
/**@{*/

#define BLE_OTA_SERVICE_UUID16                      0x8018  /*!< Primary service, 16-bit ESP BLE OTA GATT service UUID */

#define BLE_OTA_CHR_UUID16_RECV_FW                  0x8020  /*!< RECV_FW_CHAR: firmware in; Write w/o rsp + Notify (ACK as Notify) */

#define BLE_OTA_CHR_UUID16_PROGRESS_BAR             0x8021  /*!< PROGRESS_BAR_CHAR: 0–100.00; Read + Notify */

#define BLE_OTA_CHR_UUID16_COMMAND                  0x8022  /*!< COMMAND_CHAR: host commands; Write w/o rsp + Notify (ACK as Notify) */

#define BLE_OTA_CHR_UUID16_CUSTOMER                 0x8023  /*!< CUSTOMER_CHAR: vendor data; Write w/o rsp + Notify */

#define BLE_OTA_PROGRESS_BAR_LEN                    2       /*!< Progress value size: integer octet (0–100) then hundredths (0–99) */

#define BLE_OTA_ATT_PAYLOAD_MAX_LEN                 512     /*!< Max ATT write / notify payload for this service */
/**@}*/

/**
 * @brief Weak hook: payload written to RECV_FW_CHAR (0x8020).
 *
 * Invoked after the service validates ATT payload length.
 * Override with a strong symbol in profile/application layer.
 *
 * @param[in] data  ATT write payload (valid for the duration of the call).
 * @param[in] len   Payload length in octets.
 */
void esp_ble_ota_recv_fw_data(const uint8_t *data, uint16_t len);

/**
 * @brief Weak hook: payload written to COMMAND_CHAR (0x8022).
 *
 * @param[in] data  ATT write payload.
 * @param[in] len   Payload length in octets.
 */
void esp_ble_ota_recv_cmd_data(const uint8_t *data, uint16_t len);

/**
 * @brief Weak hook: payload written to CUSTOMER_CHAR (0x8023).
 *
 * @param[in] data  ATT write payload.
 * @param[in] len   Payload length in octets.
 */
void esp_ble_ota_recv_customer_data(const uint8_t *data, uint16_t len);

/**
 * @brief Backward-compatible alias of @ref esp_ble_ota_recv_fw_data.
 *
 * @deprecated Prefer @ref esp_ble_ota_recv_fw_data.
 */
#define esp_ble_ota_on_recv_fw      esp_ble_ota_recv_fw_data

/**
 * @brief Backward-compatible alias of @ref esp_ble_ota_recv_cmd_data.
 *
 * @deprecated Prefer @ref esp_ble_ota_recv_cmd_data.
 */
#define esp_ble_ota_on_command      esp_ble_ota_recv_cmd_data

/**
 * @brief Backward-compatible alias of @ref esp_ble_ota_recv_customer_data.
 *
 * @deprecated Prefer @ref esp_ble_ota_recv_customer_data.
 */
#define esp_ble_ota_on_customer     esp_ble_ota_recv_customer_data

/**
 * @brief Register the 0x8018 OTA GATT service with esp_ble_conn_mgr.
 *
 * @return
 *  - ESP_OK on success
 *  - Error code from esp_ble_conn_add_svc() on failure
 */
esp_err_t esp_ble_ota_svc_init(void);

/**
 * @brief Remove the 0x8018 OTA GATT service from esp_ble_conn_mgr.
 *
 * @return
 *  - ESP_OK on success
 *  - Error code from esp_ble_conn_remove_svc() on failure
 */
esp_err_t esp_ble_ota_svc_deinit(void);

/**
 * @brief Read local PROGRESS_BAR as a single @c uint16_t.
 *
 * Return value matches the two stored octets in native byte order
 * (on little-endian: low 8 bits = integer part, high 8 bits = hundredths).
 *
 * @return Current local PROGRESS_BAR value encoded as one @c uint16_t.
 */
uint16_t esp_ble_ota_progress_bar_local_get(void);

/**
 * @brief Send a Notify on RECV_FW_CHAR (0x8020) with raw payload.
 *
 * @param[in] data  Payload to send; must not be NULL.
 * @param[in] len   Length in octets; must be in range [1, @ref BLE_OTA_ATT_PAYLOAD_MAX_LEN].
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_SIZE if len exceeds @ref BLE_OTA_ATT_PAYLOAD_MAX_LEN
 *  - Error from esp_ble_conn_notify() otherwise
 */
esp_err_t esp_ble_ota_notify_recv_fw_raw(const uint8_t *data, uint16_t len);

/**
 * @brief Notify PROGRESS_BAR_CHAR (0x8021) and update the local value used for GATT Read.
 *
 * Air order: @p low_octet (integer) first, then @p high_octet (hundredths).
 *
 * @param[in] low_octet   Integer part (0–100), sent first on air.
 * @param[in] high_octet  Hundredths (0–99), sent second on air; when @p low_octet is 100, must be 0.
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG if values are out of range
 *  - Error from esp_ble_conn_notify() otherwise
 */
esp_err_t esp_ble_ota_notify_progress_bar_raw(uint8_t low_octet, uint8_t high_octet);

/**
 * @brief Send a Notify on COMMAND_CHAR (0x8022) with raw payload.
 *
 * @param[in] data  Payload to send; must not be NULL.
 * @param[in] len   Length in octets; must be in range [1, @ref BLE_OTA_ATT_PAYLOAD_MAX_LEN].
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_SIZE if len exceeds @ref BLE_OTA_ATT_PAYLOAD_MAX_LEN
 *  - Error from esp_ble_conn_notify() otherwise
 */
esp_err_t esp_ble_ota_notify_command_raw(const uint8_t *data, uint16_t len);

/**
 * @brief Send a Notify on CUSTOMER_CHAR (0x8023) with raw payload.
 *
 * @param[in] data  Payload to send; must not be NULL.
 * @param[in] len   Length in octets; must be in range [1, @ref BLE_OTA_ATT_PAYLOAD_MAX_LEN].
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_SIZE if len exceeds @ref BLE_OTA_ATT_PAYLOAD_MAX_LEN
 *  - Error from esp_ble_conn_notify() otherwise
 */
esp_err_t esp_ble_ota_notify_customer_raw(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif
