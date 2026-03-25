/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "esp_mcp_mgr.h"
#include "esp_xiaozhi_transport.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  MCP transport configuration structure
 *         When both MQTT and WebSocket are supported, use_mqtt true means prefer MQTT.
 *         One callbacks struct is used for both transports; on_binary_callback is only used by WebSocket.
 */
typedef struct {
    void *ctx;                                  /*!< Context for callbacks */
    char *session_id;                           /*!< Session ID for wrapping responses */
    size_t session_id_len;                      /*!< Session ID length */
    bool use_mqtt;                              /*!< Use MQTT for this session; when false use WebSocket. When both supported, prefer MQTT if true */
    esp_xiaozhi_transport_callbacks_t callbacks; /*!< Transport callbacks (shared by MQTT and WebSocket) */
} esp_xiaozhi_mcp_config_t;

/**
 * @brief  MCP MQTT transport function table
 */
extern const esp_mcp_transport_t esp_mcp_transport_xiaozhi;

/**
 * @brief  Handle incoming MQTT message for MCP
 *
 *         This function should be called when a MCP message is received via MQTT.
 *         It processes the MCP message and sends the response back via MQTT.
 *
 * @param[in] mgr_handle   MCP manager handle
 * @param[in] ep_name     Endpoint name
 * @param[in] payload     MCP message payload (JSON string)
 * @param[in] payload_len Payload length
 * @param[in] session_id  Session ID for wrapping response (can be NULL)
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid arguments
 *       - ESP_FAIL              Failed to process message
 */
esp_err_t esp_xiaozhi_mcp_handle_message(esp_mcp_mgr_handle_t mgr_handle,
                                         const char *ep_name,
                                         const char *payload,
                                         size_t payload_len,
                                         const char *session_id);

/**
 * @brief  Get MQTT handle from MCP MQTT transport
 *
 * @param[in]  mgr_handle        MCP manager handle
 * @param[out] transport_handle  Pointer to store transport handle
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_STATE   MCP transport not initialized
 */
esp_err_t esp_xiaozhi_mcp_get_handle(esp_mcp_mgr_handle_t mgr_handle, esp_xiaozhi_transport_handle_t *transport_handle);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
