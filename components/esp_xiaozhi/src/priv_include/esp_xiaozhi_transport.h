/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Transport handle type, shared by MQTT and WebSocket
 */
typedef uint32_t esp_xiaozhi_transport_handle_t;

/**
 * @brief  Unified transport callbacks for MQTT and WebSocket
 *         Used by esp_xiaozhi_mcp_config_t; MQTT uses on_message, on_connected, on_disconnected (on_binary may be NULL).
 */
typedef struct {
    esp_err_t (*on_message_callback)(esp_xiaozhi_transport_handle_t handle, const char *topic, size_t topic_len, const char *data, size_t data_len);
    esp_err_t (*on_binary_callback)(esp_xiaozhi_transport_handle_t handle, const uint8_t *data, size_t data_len);  /*!< NULL for MQTT */
    esp_err_t (*on_connected_callback)(esp_xiaozhi_transport_handle_t handle);
    esp_err_t (*on_disconnected_callback)(esp_xiaozhi_transport_handle_t handle);
    esp_err_t (*on_error_callback)(esp_xiaozhi_transport_handle_t handle, esp_err_t err);
} esp_xiaozhi_transport_callbacks_t;

/**
 * @brief  Initialize transport layer (MQTT or WebSocket). Call start to choose and start one.
 *
 * @param[out] handle  Output transport handle
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle pointer
 *       - ESP_ERR_NO_MEM        Failed to allocate transport context
 */
esp_err_t esp_xiaozhi_transport_init(esp_xiaozhi_transport_handle_t *handle);

/**
 * @brief  Deinitialize transport and free resources.
 *
 * @param[in] handle  Transport handle from init
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle
 */
esp_err_t esp_xiaozhi_transport_deinit(esp_xiaozhi_transport_handle_t handle);

/**
 * @brief  Start transport with MQTT or WebSocket according to use_mqtt.
 *
 * @param[in] handle     Transport handle from init
 * @param[in] use_mqtt   True to use MQTT, false to use WebSocket
 * @param[in] callbacks  Callbacks for message/connected/disconnected (and on_binary for WebSocket)
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle or callbacks
 *       - ESP_ERR_NOT_FOUND     Required MQTT/WebSocket configuration is missing
 *       - Other                 Error from MQTT or WebSocket init/start
 */
esp_err_t esp_xiaozhi_transport_start(esp_xiaozhi_transport_handle_t handle, bool use_mqtt,
                                      const esp_xiaozhi_transport_callbacks_t *callbacks);

/**
 * @brief  Stop transport (stop and deinit underlying MQTT or WebSocket).
 *
 * @param[in] handle  Transport handle
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle
 */
esp_err_t esp_xiaozhi_transport_stop(esp_xiaozhi_transport_handle_t handle);

/**
 * @brief  Send text over the active transport.
 *
 * @param[in] handle  Transport handle
 * @param[in] text    Null-terminated text to send
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle or text
 *       - Other                 Error from underlying MQTT or WebSocket send
 */
esp_err_t esp_xiaozhi_transport_send_text(esp_xiaozhi_transport_handle_t handle, const char *text);

/**
 * @brief  Send binary data. Supported only when transport is WebSocket; returns ESP_ERR_NOT_SUPPORTED for MQTT.
 *
 * @param[in] handle    Transport handle
 * @param[in] data      Binary data
 * @param[in] data_len  Length in bytes
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle or data
 *       - ESP_ERR_NOT_SUPPORTED   Transport is MQTT (binary not supported)
 *       - Other    Error from WebSocket send_binary
 */
esp_err_t esp_xiaozhi_transport_send_binary(esp_xiaozhi_transport_handle_t handle, const uint8_t *data, size_t data_len);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
