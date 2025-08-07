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

/**
 * @brief Tool callback function type
 *
 * Callback function called when a tool is executed. Receives the input properties
 * and returns the execution result as a MCP value.
 *
 * @param[in] properties Tool input properties (can be NULL)
 * @return MCP value containing the tool execution result
 */
typedef esp_mcp_value_t (*esp_mcp_tool_callback_t)(const esp_mcp_property_list_t* properties);

#ifdef __cplusplus
}
#endif
