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
 */
typedef struct {
    void *ctx;                                  /*!< Context for callbacks */
    esp_xiaozhi_transport_handle_t transport_handle; /*!< Borrowed transport owned by chat */
    esp_err_t (*send_message)(void *ctx, const char *routing_session, const char *reply_session, const char *payload);
    esp_err_t (*prepare_request)(void *ctx, const char *routing_session, const char *payload);
} esp_xiaozhi_mcp_config_t;

/**
 * @brief  MCP transport function table
 */
extern const esp_mcp_transport_t esp_mcp_transport_xiaozhi;

/**
 * @brief  Enqueue an incoming Xiaozhi MCP message for worker processing
 *
 * @param[in] mgr_handle           MCP manager handle
 * @param[in] ep_name              Endpoint name
 * @param[in] payload              MCP message payload (JSON string)
 * @param[in] payload_len          Payload length
 * @param[in] session_id           Audio session ID for wrapping response (can be NULL)
 * @param[in] routing_session      Connection routing identifier
 * @param[in] dialogue_generation  Dialogue generation counter for request ID cache isolation
 *
 * @return
 *       - ESP_OK                On success
 *       - ESP_ERR_INVALID_ARG   Invalid arguments
 *       - ESP_ERR_INVALID_STATE Transport stopped
 *       - ESP_ERR_NO_MEM        Queue full or allocation failed
 */
esp_err_t esp_xiaozhi_mcp_handle_message(esp_mcp_mgr_handle_t mgr_handle,
                                         const char *ep_name,
                                         const char *payload,
                                         size_t payload_len,
                                         const char *session_id,
                                         const char *routing_session,
                                         uint32_t dialogue_generation);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
