/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <esp_err.h>
#include <sys/queue.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_mcp_data.h"
#include "esp_mcp_property.h"
#include "esp_mcp_tool.h"

/**
 * @brief MCP property structure
 *
 * Structure representing a property definition for tool input parameters.
 * Properties define the expected type, default values, and value constraints.
 */
typedef struct esp_mcp_property_s {
    char* name;                         /*!< Property name */
    esp_mcp_property_type_t type;       /*!< Property type (boolean, integer, float, string, array, object) */
    esp_mcp_value_data_t default_value; /*!< Default value (valid only if has_default_value is true) */
    bool has_default_value;             /*!< Whether a default value is set */
    int min_value;                      /*!< Minimum value for numeric properties (valid only if has_range is true) */
    int max_value;                      /*!< Maximum value for numeric properties (valid only if has_range is true) */
    bool has_range;                     /*!< Whether value range constraints are set */
} esp_mcp_property_t;

/**
 * @brief Property list item structure
 *
 * Linked list node containing a property pointer.
 */
typedef struct esp_mcp_property_item_t {
    SLIST_ENTRY(esp_mcp_property_item_t) next;      /*!< Next item in the list */
    esp_mcp_property_t* property;                   /*!< Pointer to property structure */
} esp_mcp_property_item_t;
SLIST_HEAD(esp_mcp_property_list, esp_mcp_property_item_t);

/**
 * @brief MCP tool structure
 *
 * Structure representing an MCP tool with name, description, input properties,
 * and execution callback function.
 */
typedef struct esp_mcp_tool_s {
    char* name;                                      /*!< Tool name */
    char* description;                             /*!< Tool description */
    esp_mcp_property_list_t *properties;           /*!< List of input properties */
    esp_mcp_tool_callback_t callback;              /*!< Callback function executed when tool is called */
} esp_mcp_tool_t;

/**
 * @brief Tool list item structure
 *
 * Linked list node containing a tool pointer.
 */
typedef struct esp_mcp_tool_item_t {
    SLIST_ENTRY(esp_mcp_tool_item_t) next;        /*!< Next item in the list */
    esp_mcp_tool_t* tools;                        /*!< Pointer to tool structure */
} esp_mcp_tool_item_t;
SLIST_HEAD(esp_mcp_tool_list, esp_mcp_tool_item_t);
typedef struct esp_mcp_tool_list esp_mcp_tool_list_t;

/**
 * @brief Message buffer structure
 *
 * Structure mapping message ID to response buffer. Used to store responses
 * for pending requests identified by their message ID.
 */
typedef struct esp_mcp_mbuf_t {
    SLIST_ENTRY(esp_mcp_mbuf_t) next;     /*!< Next message buffer in the list */
    uint8_t *outbuf;                      /*!< Response data buffer */
    uint16_t outlen;                      /*!< Response data length (bytes) */
    uint16_t id;                          /*!< Message ID to identify the request */
} esp_mcp_mbuf_t;
SLIST_HEAD(esp_mcp_mbuf_list, esp_mcp_mbuf_t);
typedef struct esp_mcp_mbuf_list esp_mcp_mbuf_list_t;

/**
 * @brief MCP server structure
 *
 * Structure managing MCP server instance state, including registered tools
 * and message buffers for pending responses.
 */
typedef struct esp_mcp_server_s {
    esp_mcp_tool_list_t *tools;       /*!< List of registered tools */
    esp_mcp_mbuf_list_t mbuf;         /*!< List of message buffers for responses */
} esp_mcp_server_t;

#ifdef __cplusplus
}
#endif
