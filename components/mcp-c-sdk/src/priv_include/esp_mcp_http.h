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

/**
 * @brief Initialize MCP HTTP transport
 *
 * Initializes the built-in HTTP transport for the specified MCP instance.
 * This function sets up the HTTP server/client infrastructure required for
 * MCP communication over HTTP.
 *
 * @param[in] handle MCP handle from esp_mcp_init()
 *
 * @return
 *      - ESP_OK: HTTP transport initialized successfully
 *      - ESP_ERR_INVALID_ARG: Invalid handle
 */
esp_err_t esp_mcp_http_init(uint32_t handle);

#ifdef __cplusplus
}
#endif
