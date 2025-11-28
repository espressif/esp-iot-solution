/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_mcp_property.h"
#include "esp_mcp_tool.h"

/**
 * @brief MCP server structure
 *
 * Opaque structure representing a MCP server instance that handles
 * MCP protocol messages and tool execution.
 */
typedef struct esp_mcp_server_s esp_mcp_server_t;

/**
 * @brief Create a MCP server instance
 *
 * Allocates and initializes a new MCP server instance.
 *
 * @param[out] server Pointer to store the created server instance (must not be NULL)
 * @return
 *      - ESP_OK: Server created successfully
 *      - ESP_ERR_NO_MEM: Memory allocation failed
 */
esp_err_t esp_mcp_server_create(esp_mcp_server_t **server);

/**
 * @brief Destroy a MCP server instance
 *
 * Frees all resources associated with the server, including all registered tools
 * and message buffers. The function will attempt to clean up all resources even
 * if some cleanup operations fail, and will log any errors encountered during cleanup.
 *
 * @param[in] server Pointer to the server instance to destroy (must not be NULL)
 * @return
 *      - ESP_OK: Cleanup completed (resources have been released, even if some errors occurred during cleanup)
 *      - ESP_ERR_INVALID_ARG: Invalid parameter (server is NULL)
 *
 * @note This function always completes the cleanup process and returns ESP_OK unless
 *       the server parameter is invalid. Any errors during resource cleanup are logged
 *       but do not affect the return value, as the caller cannot meaningfully recover
 *       from such errors anyway.
 */
esp_err_t esp_mcp_server_destroy(esp_mcp_server_t *server);

/**
 * @brief Add a tool to the MCP server
 *
 * Adds a tool to the server.
 *
 * @param[in] server Pointer to the server instance (must not be NULL)
 * @param[in] tool Pointer to the tool to add (must not be NULL)
 * @return ESP_OK if successful, otherwise an error code
 */
esp_err_t esp_mcp_server_add_tool(esp_mcp_server_t *server, esp_mcp_tool_t *tool);

/**
 * @brief Remove a tool from the MCP server
 *
 * Removes a tool from the server.
 *
 * @param[in] server Pointer to the server instance (must not be NULL)
 * @param[in] tool Pointer to the tool to remove (must not be NULL)
 * @return ESP_OK if successful, otherwise an error code
 */
esp_err_t esp_mcp_server_remove_tool(esp_mcp_server_t *server, esp_mcp_tool_t *tool);

#ifdef __cplusplus
}
#endif
