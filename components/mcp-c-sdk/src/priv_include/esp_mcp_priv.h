/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_mcp_engine.h"
#include "esp_mcp_tool_priv.h"
#include "esp_mcp_item.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Add a tool to the MCP engine (internal API)
 *
 * Registers a tool with the engine. The engine takes ownership of the tool only on success.
 * If the function returns an error, the tool ownership remains with the caller, who is
 * responsible for destroying it.
 *
 * @note This is an internal header for the MCP engine/manager glue.
 *       These APIs are intended to be called by the manager/router layer (`esp_mcp_mgr.c`)
 *       and transport adapters, not by applications directly.
 *       The stable public engine API is declared in `esp_mcp_engine.h`.
 *
 * @param[in] mcp Pointer to the MCP instance (must not be NULL)
 * @param[in] tool Pointer to the tool to add (must not be NULL)
 * @return
 *      - ESP_OK: Tool added successfully (server takes ownership)
 *      - ESP_ERR_INVALID_ARG: Invalid parameter (tool ownership remains with caller)
 *      - ESP_ERR_INVALID_STATE: Tool with the same name already exists (tool ownership remains with caller)
 *      - ESP_ERR_NO_MEM: Memory allocation failed (tool ownership remains with caller)
 */
esp_err_t esp_mcp_add_tool(esp_mcp_t *mcp, esp_mcp_tool_t *tool);

/**
 * @brief Parse an MCP message string (internal API)
 *
 * Parses the incoming MCP protocol message and executes the appropriate tool
 * if the message is a tool call request.
 *
 * @param[in] mcp Pointer to the MCP engine instance (must not be NULL)
 * @param[in] message MCP message string in JSON format (must not be NULL)
 * @return
 *      - ESP_OK: Message parsed and processed successfully
 *      - ESP_ERR_INVALID_ARG: Invalid parameter
 *      - ESP_FAIL: Message parsing or tool execution failed
 */
esp_err_t esp_mcp_parse_message(esp_mcp_t *mcp, const char *message);

/**
 * @brief Get MCP message buffer (internal API)
 *
 * Retrieves the response message buffer for the specified message ID.
 * The buffer contains the JSON response to be sent to the client.
 *
 * @param[in] mcp Pointer to the MCP engine instance (must not be NULL)
 * @param[out] outbuf Pointer to store the output buffer pointer (must not be NULL)
 * @param[out] outlen Pointer to store the output buffer length (must not be NULL)
 * @return
 *      - ESP_OK: Buffer retrieved successfully
 *      - ESP_ERR_INVALID_ARG: Invalid parameter
 *      - ESP_ERR_NOT_FOUND: Message ID not found
 */
esp_err_t esp_mcp_get_mbuf(esp_mcp_t *mcp, uint8_t **outbuf, uint16_t *outlen);

/**
 * @brief Remove and free MCP message buffer (internal API)
 *
 * Removes the message buffer for the specified message ID from the server's
 * mbuf list and frees the associated memory. This should be called after
 * the outbuf has been used and is no longer needed.
 *
 * @param[in] mcp Pointer to the MCP engine instance (must not be NULL)
 * @param[in] outbuf Output buffer
 * @return
 *      - ESP_OK: Buffer removed and freed successfully
 *      - ESP_ERR_INVALID_ARG: Invalid parameter
 *      - ESP_ERR_NOT_FOUND: Message ID not found
 */
esp_err_t esp_mcp_remove_mbuf(esp_mcp_t *mcp, const uint8_t *outbuf);

/**
 * @brief Build initialize request
 *
 * @param[in] mcp Pointer to the MCP engine instance (must not be NULL)
 * @param[in] info Info
 * @param[out] output Output buffer
 * @param[out] out_id Output ID
 * @return ESP_OK on success; ESP_ERR_INVALID_ARG on parse/field errors
 */
esp_err_t esp_mcp_info_init(esp_mcp_t *mcp, const esp_mcp_info_t *info, char **output, uint16_t *out_id);

/**
 * @brief Build tools/list request
 *
 * @param[in] mcp Pointer to the MCP engine instance (must not be NULL)
 * @param[in] info Info
 * @param[out] output Output buffer
 * @param[out] out_id Output ID
 * @return ESP_OK on success; ESP_ERR_INVALID_ARG on parse/field errors
 */
esp_err_t esp_mcp_tools_list(esp_mcp_t *mcp, const esp_mcp_info_t *info, char **output, uint16_t *out_id);

/**
 * @brief Build tools/call request
 *
 * @param[in] mcp Pointer to the MCP engine instance (must not be NULL)
 * @param[in] info Info
 * @param[out] output Output buffer
 * @param[out] out_id Output ID
 * @return ESP_OK on success; ESP_ERR_INVALID_ARG on parse/field errors
 */
esp_err_t esp_mcp_tools_call(esp_mcp_t *mcp, const esp_mcp_info_t *info, char **output, uint16_t *out_id);

/**
 * @brief Free response buffer
 *
 * @param[in] buf Response buffer
 */
void esp_mcp_resp_free(char *buf);

/**
 * @brief Parse MCP response, extract id and result/error JSON
 *
 * @param[in]  mcp        MCP engine handle (unused for now, kept for future use)
 * @param[in]  input      Raw response JSON string
 * @param[out] result     Parsed response result structure
 *
 * @return
 *      - ESP_OK: Parse successful, result contains application-level data (success or error), should notify application layer
 *      - ESP_ERR_INVALID_RESPONSE: Parse successful, but result contains protocol-level error (JSON-RPC error field),
 *                                  error_code and error_message are set, should notify application layer with is_error=true
 *      - ESP_ERR_INVALID_ARG: Parse failed (invalid JSON or missing fields)
 *      - ESP_ERR_NO_MEM: Memory allocation failed
 */
esp_err_t esp_mcp_resp_parse(esp_mcp_t *mcp, const char *input, esp_mcp_resp_t *result);

#ifdef __cplusplus
}
#endif
