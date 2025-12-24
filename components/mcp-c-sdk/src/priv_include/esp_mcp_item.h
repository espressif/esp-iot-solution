/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <esp_err.h>
#include <sys/queue.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

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
    char *name;                         /*!< Property name */
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
typedef struct esp_mcp_property_item_s {
    esp_mcp_property_t *property;                   /*!< Pointer to property structure */
    SLIST_ENTRY(esp_mcp_property_item_s) next;      /*!< Next item in the list */
} esp_mcp_property_item_t;

/**
 * @brief Property list structure
 *
 * Structure representing a list of properties.
 */
typedef struct esp_mcp_property_list_s {
    SLIST_HEAD(property_table_t, esp_mcp_property_item_s) properties;   /*!< List head */
    SemaphoreHandle_t mutex;                                          /*!< Mutex to protect the list */
} esp_mcp_property_list_t;

/**
 * @brief MCP tool structure
 *
 * Structure representing an MCP tool with name, description, input properties,
 * and execution callback function.
 */
typedef struct esp_mcp_tool_s {
    char *name;                                      /*!< Tool name */
    char *description;                             /*!< Tool description */
    esp_mcp_property_list_t *properties;           /*!< List of input properties */
    esp_mcp_tool_callback_t callback;              /*!< Callback function executed when tool is called */
} esp_mcp_tool_t;

/**
 * @brief Tool list item structure
 *
 * Linked list node containing a tool pointer.
 */
typedef struct esp_mcp_tool_item_s {
    esp_mcp_tool_t *tools;                        /*!< Pointer to tool structure */
    SLIST_ENTRY(esp_mcp_tool_item_s) next;        /*!< Next item in the list */
} esp_mcp_tool_item_t;

/**
 * @brief Tool list structure
 *
 * Structure representing a list of tools.
 */
typedef struct esp_mcp_tool_list_s {
    SLIST_HEAD(tool_table_t, esp_mcp_tool_item_s) tools;   /*!< List head */
    SemaphoreHandle_t mutex;                                      /*!< Mutex to protect the list */
} esp_mcp_tool_list_t;

/**
 * @brief Message buffer structure
 *
 * Structure mapping message ID to response buffer. Used to store responses
 * for pending requests identified by their message ID.
 */
typedef struct esp_mcp_mbuf_t {
    uint16_t id;                          /*!< Response message ID */
    uint8_t *outbuf;                      /*!< Response data buffer */
    uint16_t outlen;                      /*!< Response data length (bytes) */
} esp_mcp_mbuf_t;

/**
 * @brief MCP structure (server-side MCP instance)
 *
 * Structure managing MCP instance state, including registered tools
 * and message buffers for pending responses.
 */
typedef struct esp_mcp_s {
    esp_mcp_tool_list_t *tools;       /*!< List of registered tools */
    esp_mcp_mbuf_t mbuf;              /*!< Message buffer for responses */
} esp_mcp_t;

/**
 * @brief MCP endpoint handler function type
 *
 * Function type for handling MCP endpoint requests. The handler processes
 * incoming binary data and generates a response buffer.
 *
 * @param[in] inbuf Input data buffer containing the request payload
 * @param[in] inlen Length of input data in bytes
 * @param[out] outbuf Pointer to output buffer pointer. The handler should allocate
 *                    memory for the response and set this pointer to the allocated buffer.
 *                    Can be set to NULL if no response is needed.
 * @param[out] outlen Pointer to output length. The handler should set this to the
 *                    actual length of the response data in bytes.
 * @param[in] priv_data Private data passed during endpoint registration
 * @return
 *      - ESP_OK: Request handled successfully
 *      - Other: Error code indicating failure
 */
typedef esp_err_t (*esp_mcp_ep_handler_t)(const uint8_t *inbuf, uint16_t inlen, uint8_t **outbuf, uint16_t *outlen, void *priv_data);

#ifdef __cplusplus
}
#endif
