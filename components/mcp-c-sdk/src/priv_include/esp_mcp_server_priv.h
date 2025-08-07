/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_mcp_server.h"
#include "esp_mcp_tool_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Add a tool to the MCP server (internal API)
 *
 * Registers a tool with the server. The server takes ownership of the tool.
 *
 * @note This is an internal API used by esp_mcp_server_add_tool_with_callback().
 *       Users should use esp_mcp_server_add_tool_with_callback() instead.
 *
 * @param[in] server Pointer to the server instance (must not be NULL)
 * @param[in] tool Pointer to the tool to add (must not be NULL)
 * @return
 *      - ESP_OK: Tool added successfully
 *      - ESP_ERR_INVALID_ARG: Invalid parameter
 */
esp_err_t esp_mcp_server_add_tool(esp_mcp_server_t* server, esp_mcp_tool_t* tool);

/**
 * @brief Parse an MCP message string (internal API)
 *
 * Parses the incoming MCP protocol message and executes the appropriate tool
 * if the message is a tool call request.
 *
 * @param[in] server Pointer to the server instance (must not be NULL)
 * @param[in] message MCP message string in JSON format (must not be NULL)
 * @return
 *      - ESP_OK: Message parsed and processed successfully
 *      - ESP_ERR_INVALID_ARG: Invalid parameter
 *      - ESP_FAIL: Message parsing or tool execution failed
 */
esp_err_t esp_mcp_server_parse_message(esp_mcp_server_t* server, const char* message);

/**
 * @brief Get MCP message buffer (internal API)
 *
 * Retrieves the response message buffer for the specified message ID.
 * The buffer contains the JSON response to be sent to the client.
 *
 * @param[in] server Pointer to the server instance (must not be NULL)
 * @param[in] id Message ID
 * @param[out] outlen Pointer to store the output buffer length (must not be NULL)
 * @param[out] outbuf Pointer to store the output buffer pointer (must not be NULL)
 * @return
 *      - ESP_OK: Buffer retrieved successfully
 *      - ESP_ERR_INVALID_ARG: Invalid parameter
 *      - ESP_ERR_NOT_FOUND: Message ID not found
 */
esp_err_t esp_mcp_server_get_mbuf(esp_mcp_server_t* server, uint16_t id, uint16_t *outlen, uint8_t **outbuf);

#ifdef __cplusplus
}
#endif

