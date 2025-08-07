/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_mcp_server.h"
#include "esp_mcp_tool.h"
#include "esp_mcp_property.h"
#include "esp_mcp_data.h"
#include "esp_mcp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Default MCP configuration
 *
 * This macro defines the default configuration for an MCP instance using HTTP transport.
 * For server role, set .instance to your esp_mcp_server_t* pointer.
 * For custom transport, set .type to ESP_MCP_TRANSPORT_TYPE_CUSTOM and call
 * esp_mcp_transport_set_funcs() before esp_mcp_start().
 */
#define MCP_SERVER_DEFAULT_CONFIG() {                                     \
    .task_priority      = tskIDLE_PRIORITY+5,                          \
    .stack_size         = CONFIG_MCP_TOOLCALL_STACK_SIZE,              \
    .core_id            = tskNO_AFFINITY,                              \
    .task_caps          = (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),     \
    .type               = ESP_MCP_TRANSPORT_TYPE_HTTP,                 \
    .host               = CONFIG_MCP_HOST,                             \
    .uri                = CONFIG_MCP_URI,                              \
    .port               = CONFIG_MCP_PORT,                             \
    .timeout_ms         = CONFIG_MCP_TRANSPORT_TIME,                   \
    .mbuf_size          = 2048,                                        \
    .instance           = NULL                                         \
}

#ifdef __cplusplus
}
#endif
