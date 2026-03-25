/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_xiaozhi_transport.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_XIAOZHI_WEBSOCKET_MAX_STRING_SIZE 256
#define ESP_XIAOZHI_WEBSOCKET_MAX_PAYLOAD_SIZE 2048
#define ESP_XIAOZHI_WEBSOCKET_MAX_HEADER_SIZE 768

#define WS_OPCODE_TEXT       0x1
#define WS_OPCODE_BINARY    0x2
#define WS_OPCODE_CONTINUATION 0x0

typedef uint32_t esp_xiaozhi_websocket_handle_t;

/**
 * @brief  Initialize the websocket application
 *
 * @param[out] handle  Output websocket handle
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle pointer
 *       - ESP_ERR_NO_MEM        Failed to allocate websocket context
 */
esp_err_t esp_xiaozhi_websocket_init(esp_xiaozhi_websocket_handle_t *handle);

/**
 * @brief  Deinitialize the websocket application
 *
 * @param[in] handle  Websocket handle from init
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle
 */
esp_err_t esp_xiaozhi_websocket_deinit(esp_xiaozhi_websocket_handle_t handle);

/**
 * @brief  Start the websocket application
 *
 * @param[in] handle  Websocket handle
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle
 *       - ESP_ERR_NOT_FOUND     WebSocket configuration is missing (namespace or URL)
 *       - ESP_ERR_NO_MEM        Failed to init client or keystore error
 *       - Other                 Error from keystore, client init/register/start
 */
esp_err_t esp_xiaozhi_websocket_start(esp_xiaozhi_websocket_handle_t handle);

/**
 * @brief  Stop the websocket application
 *
 * @param[in] handle  Websocket handle
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle
 *       - Other                 Error from esp_websocket_client_stop
 */
esp_err_t esp_xiaozhi_websocket_stop(esp_xiaozhi_websocket_handle_t handle);

/**
 * @brief  Send text to the websocket application
 *
 * @param[in] handle  Websocket handle
 * @param[in] text    Null-terminated text to send
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle or text
 *       - ESP_ERR_INVALID_STATE   Client not initialized or not connected
 *       - ESP_FAIL   Send failed (e.g. connection lost)
 */
esp_err_t esp_xiaozhi_websocket_send_text(esp_xiaozhi_websocket_handle_t handle, const char *text);

/**
 * @brief  Send binary data to the websocket application
 *
 * @param[in] handle    Websocket handle
 * @param[in] data      Binary data
 * @param[in] data_len  Length in bytes
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle or data or data_len is 0
 *       - ESP_ERR_INVALID_STATE   Client not initialized or not connected
 *       - ESP_FAIL   Send failed (e.g. connection lost)
 */
esp_err_t esp_xiaozhi_websocket_send_binary(esp_xiaozhi_websocket_handle_t handle, const uint8_t *data, size_t data_len);

/**
 * @brief  Set the callbacks for the websocket application
 *
 * @param[in] handle           Websocket handle
 * @param[in] transport_handle  Transport handle to pass to callbacks
 * @param[in] callbacks       Callbacks (must not be NULL)
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle or callbacks
 */
esp_err_t esp_xiaozhi_websocket_set_callbacks(esp_xiaozhi_websocket_handle_t handle, esp_xiaozhi_transport_handle_t transport_handle, const esp_xiaozhi_transport_callbacks_t *callbacks);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
