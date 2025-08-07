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
esp_err_t esp_mcp_server_create(esp_mcp_server_t** server);

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
esp_err_t esp_mcp_server_destroy(esp_mcp_server_t* server);

/**
 * @brief Add a tool with callback function to the MCP server
 *
 * Creates a tool with the specified parameters and registers it with the server.
 *
 * @note The function takes ownership of the properties list. The caller must not
 *       destroy or reuse the properties list after calling this function, as it
 *       will be destroyed when the tool is destroyed (via server destruction or
 *       tool removal).
 *
 * @param[in] server Pointer to the server instance (must not be NULL)
 * @param[in] name Tool name (must not be NULL)
 * @param[in] description Tool description (must not be NULL)
 * @param[in] properties Tool input properties list (must not be NULL, ownership transferred to tool)
 * @param[in] callback Tool execution callback function (must not be NULL)
 * @return
 *      - ESP_OK: Tool added successfully
 *      - ESP_ERR_INVALID_ARG: Invalid parameter
 *      - ESP_ERR_NO_MEM: Memory allocation failed
 */
esp_err_t esp_mcp_server_add_tool_with_callback(esp_mcp_server_t* server, const char* name, const char* description, const esp_mcp_property_list_t* properties, esp_mcp_tool_callback_t callback);

#ifdef __cplusplus
}
#endif
