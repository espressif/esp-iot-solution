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

#include "esp_mcp_data.h"
#include "esp_mcp_property.h"

typedef struct esp_mcp_tool_s esp_mcp_tool_t;

/**
 * @brief Tool callback function type
 *
 * Callback function called when a tool is executed. Receives the input properties
 * and returns the execution result as a MCP value.
 *
 * @param[in] properties Tool input properties (can be NULL)
 * @return MCP value containing the tool execution result
 */
typedef esp_mcp_value_t (*esp_mcp_tool_callback_t)(const esp_mcp_property_list_t *properties);

/**
 * @brief Create a new MCP tool
 *
 * @param[in] name Tool name (must not be NULL)
 * @param[in] description Tool description (must not be NULL)
 * @param[in] callback Tool callback function (must not be NULL)
 * @return Pointer to the created tool, or NULL on failure
 */
esp_mcp_tool_t *esp_mcp_tool_create(const char *name, const char *description, esp_mcp_tool_callback_t callback);

/**
 * @brief Destroy a tool
 *
 * @param[in] tool Pointer to the tool (must not be NULL)
 * @return ESP_OK if successful, otherwise an error code
 *      - ESP_ERR_INVALID_ARG: Invalid parameter
 *      - ESP_ERR_INVALID_STATE: Tool is not initialized
 *      - ESP_ERR_NO_MEM: Memory allocation failed
 *      - ESP_ERR_INTERNAL: Internal error
 */
esp_err_t esp_mcp_tool_destroy(esp_mcp_tool_t *tool);

/**
 * @brief Add a property to a tool
 *
 * @param[in] tool Pointer to the tool (must not be NULL)
 * @param[in] property Pointer to the property (must not be NULL)
 * @return ESP_OK if successful, otherwise an error code
 */
esp_err_t esp_mcp_tool_add_property(esp_mcp_tool_t *tool, esp_mcp_property_t *property);

/**
 * @brief Remove a property from a tool
 *
 * @param[in] tool Pointer to the tool (must not be NULL)
 * @param[in] property Pointer to the property (must not be NULL)
 * @return ESP_OK if successful, otherwise an error code
 */
esp_err_t esp_mcp_tool_remove_property(esp_mcp_tool_t *tool, esp_mcp_property_t *property);

#ifdef __cplusplus
}
#endif
