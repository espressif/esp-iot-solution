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
 * @brief MCP engine instance (protocol semantics + tool dispatch)
 *
 * Opaque structure representing an MCP engine instance that handles MCP protocol semantics
 * (e.g., tools/list, tools/call, ping, initialize) and tool execution.
 *
 * @note Naming/layering:
 *       - Public manager/router APIs use `esp_mcp_mgr_*` (see `esp_mcp_mgr.h`)
 *       - Transport implementations expose `esp_mcp_transport_*` tables (e.g., `esp_mcp_transport_http`)
 *       - This header defines the engine public API. For simplicity, the engine public APIs keep
 *         the short prefix `esp_mcp_*` (e.g., esp_mcp_create()).
 */
typedef struct esp_mcp_s esp_mcp_t;

/**
 * @brief Create an MCP instance
 *
 * Allocates and initializes a new MCP instance.
 *
 * @param[out] mcp Pointer to store the created MCP instance (must not be NULL)
 * @return
 *      - ESP_OK: Server created successfully
 *      - ESP_ERR_NO_MEM: Memory allocation failed
 */
esp_err_t esp_mcp_create(esp_mcp_t **mcp);

/**
 * @brief Destroy an MCP instance
 *
 * Frees all resources associated with the MCP instance, including all registered tools
 * and message buffers. The function will attempt to clean up all resources even
 * if some cleanup operations fail, and will log any errors encountered during cleanup.
 *
 * @param[in] mcp Pointer to the MCP instance to destroy (must not be NULL)
 * @return
 *      - ESP_OK: Cleanup completed (resources have been released, even if some errors occurred during cleanup)
 *      - ESP_ERR_INVALID_ARG: Invalid parameter (server is NULL)
 *
 * @note This function always completes the cleanup process and returns ESP_OK unless
 *       the server parameter is invalid. Any errors during resource cleanup are logged
 *       but do not affect the return value, as the caller cannot meaningfully recover
 *       from such errors anyway.
 */
esp_err_t esp_mcp_destroy(esp_mcp_t *mcp);

/**
 * @brief Add a tool to the MCP instance
 *
 * Adds a tool to the MCP instance.
 *
 * @param[in] mcp Pointer to the MCP instance (must not be NULL)
 * @param[in] tool Pointer to the tool to add (must not be NULL)
 * @return ESP_OK if successful, otherwise an error code
 */
esp_err_t esp_mcp_add_tool(esp_mcp_t *mcp, esp_mcp_tool_t *tool);

/**
 * @brief Remove a tool from the MCP instance
 *
 * Removes a tool from the MCP instance.
 *
 * @param[in] mcp Pointer to the MCP instance (must not be NULL)
 * @param[in] tool Pointer to the tool to remove (must not be NULL)
 * @return ESP_OK if successful, otherwise an error code
 */
esp_err_t esp_mcp_remove_tool(esp_mcp_t *mcp, esp_mcp_tool_t *tool);

#ifdef __cplusplus
}
#endif
