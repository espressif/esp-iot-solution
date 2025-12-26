/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MCP error code enumeration
 *
 * Error codes defined by the MCP (Model Context Protocol) specification.
 * Standard JSON-RPC 2.0 error codes range from -32768 to -32000.
 */
typedef enum {
    MCP_ERROR_CODE_PARSE_ERROR                          = -32700,   /*!< Invalid JSON was received */
    MCP_ERROR_CODE_INVALID_REQUEST                      = -32600,   /*!< Request is not a valid Request object */
    MCP_ERROR_CODE_METHOD_NOT_FOUND                     = -32601,   /*!< Method does not exist or is not available */
    MCP_ERROR_CODE_INVALID_PARAMS                       = -32602,   /*!< Invalid method parameter(s) */
    MCP_ERROR_CODE_INTERNAL_ERROR                       = -32603,   /*!< Internal JSON-RPC error */
    MCP_ERROR_CODE_SERVER_ERROR                         = -32000,   /*!< Server error (reserved for implementation-defined errors) */
    MCP_ERROR_CODE_FAILED_TO_ADD_TOOL                   = -32001,   /*!< Failed to add tool to server */
    MCP_ERROR_CODE_TOOL_CALL_FAILED                     = -32002,   /*!< Tool execution callback failed */
    MCP_ERROR_CODE_FAILED_TO_CREATE_ARGUMENTS           = -32003,   /*!< Failed to create tool arguments */
} esp_mcp_error_code_t;

/**
 * @brief Error message strings
 *
 * Human-readable error messages corresponding to MCP error codes.
 */
#define MCP_ERROR_MESSAGE_PARSE_ERROR                   "Parse error"
#define MCP_ERROR_MESSAGE_INVALID_REQUEST               "Invalid request"
#define MCP_ERROR_MESSAGE_METHOD_NOT_FOUND              "Method not found"
#define MCP_ERROR_MESSAGE_INVALID_PARAMS                "Invalid params"
#define MCP_ERROR_MESSAGE_INTERNAL_ERROR                "Internal error"
#define MCP_ERROR_MESSAGE_SERVER_ERROR                  "Server error"
#define MCP_ERROR_MESSAGE_FAILED_TO_ADD_TOOL            "Failed to add tool"
#define MCP_ERROR_MESSAGE_TOOL_CALL_FAILED              "Tool call failed"
#define MCP_ERROR_MESSAGE_FAILED_TO_CREATE_ARGUMENTS    "Failed to create arguments"

#define MCP_ERROR_BUF_MAX_SIZE                          64      /*!< Maximum size (bytes) for error message buffer */

/**
 * @brief MCP error structure
 *
 * Structure containing error code and message for MCP error responses.
 */
typedef struct esp_mcp_error_s {
    int code;                /*!< Error code from esp_mcp_error_code_t */
    const char *message;     /*!< Human-readable error message */
} esp_mcp_error_t;

#ifdef __cplusplus
}
#endif
