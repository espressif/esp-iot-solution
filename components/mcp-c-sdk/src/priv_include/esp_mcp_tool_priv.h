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
#include "esp_mcp_tool.h"

/**
 * @brief Tool structure
 *
 * Opaque structure representing an MCP tool with its name, description,
 * input properties, and execution callback.
 *
 * @note This is an internal type used only within the MCP server implementation.
 *       Users should use esp_mcp_add_tool_with_callback() to add tools.
 */
typedef struct esp_mcp_tool_s esp_mcp_tool_t;

/**
 * @brief Tool list structure
 *
 * Opaque structure representing a collection of MCP tools.
 * This is an internal type used only within the MCP server implementation.
 */
typedef struct esp_mcp_tool_list_s esp_mcp_tool_list_t;

/**
 * @brief Context for tool list foreach callback
 *
 * This context is used to pass data to the tool list foreach callback.
 *
 * @note This is an internal type used only within the MCP server implementation.
 */
typedef struct esp_mcp_tool_foreach_ctx_s {
    void *tools;                                        /*!< Pointer to the tools object (must not be NULL) */
    void *tools_array;                                  /*!< Pointer to the tools array (must not be NULL) */
    const char *cursor_str;                             /*!< Cursor string (must not be NULL) */
    char *next_cursor;                                  /*!< Next cursor string (must not be NULL) */
    bool *found_cursor;                                 /*!< Pointer to the found cursor flag (must not be NULL) */
    bool should_break;                                  /*!< Whether to break the iteration (must not be NULL) */
} esp_mcp_tool_foreach_ctx_t;

/**
 * @brief Create a new MCP tool
 *
 * Creates a tool with the specified name, description, input properties,
 * and execution callback function.
 *
 * @note This is an internal API. Users should use esp_mcp_add_tool_with_callback()
 *       instead.
 *
 * @note The function takes ownership of the properties list. The caller must not
 *       destroy or reuse the properties list after calling this function, as it
 *       will be destroyed when the tool is destroyed via esp_mcp_tool_destroy().
 *
 * @param[in] name Tool name (must not be NULL)
 * @param[in] description Tool description (must not be NULL)
 * @param[in] properties Tool input properties list (must not be NULL, ownership transferred to tool)
 * @param[in] callback Tool execution callback function (must not be NULL)
 * @return Pointer to the created tool, or NULL on failure
 */
esp_mcp_tool_t *esp_mcp_tool_create(const char *name, const char *description, esp_mcp_tool_callback_t callback);

/**
 * @brief Convert a tool to a JSON string
 *
 * Converts the tool definition to its JSON representation for the MCP protocol.
 *
 * @note This is an internal API used by the MCP server implementation.
 *
 * @param[in] tool Pointer to the tool (must not be NULL)
 * @return Pointer to the JSON string (caller must free using free()), or NULL on failure
 */
char *esp_mcp_tool_to_json(const esp_mcp_tool_t *tool);

/**
 * @brief Execute a tool
 *
 * Executes the tool with the provided input properties and returns the
 * result as a JSON string.
 *
 * @note This is an internal API used by the MCP server implementation.
 *
 * @param[in] tool Pointer to the tool (must not be NULL)
 * @param[in] properties Input properties for tool execution (can be NULL)
 * @return Pointer to the result JSON string (caller must free using free()), or NULL on failure
 */
char *esp_mcp_tool_call(const esp_mcp_tool_t *tool, const esp_mcp_property_list_t *properties);

/**
 * @brief Destroy a tool
 *
 * Frees all resources associated with the tool.
 *
 * @note This is an internal API used by the MCP server implementation.
 *
 * @param[in] tool Pointer to the tool to destroy (must not be NULL)
 * @return
 *      - ESP_OK: Tool destroyed successfully
 *      - ESP_ERR_INVALID_ARG: Invalid parameter
 */
esp_err_t esp_mcp_tool_destroy(esp_mcp_tool_t *tool);

/**
 * @brief Create a new tool list
 *
 * Creates a new empty tool list.
 *
 * @note This is an internal API used by the MCP server implementation.
 *
 * @return Pointer to the created tool list, or NULL on failure
 */
esp_mcp_tool_list_t *esp_mcp_tool_list_create(void);

/**
 * @brief Add a tool to the list
 *
 * Adds a tool to the list. The list takes ownership of the tool.
 *
 * @note This is an internal API used by the MCP server implementation.
 *
 * @param[in] list Pointer to the tool list (must not be NULL)
 * @param[in] tool Pointer to the tool to add (must not be NULL)
 * @return
 *      - ESP_OK: Tool added successfully
 *      - ESP_ERR_INVALID_ARG: Invalid parameter
 */
esp_err_t esp_mcp_tool_list_add_tool(esp_mcp_tool_list_t *list, esp_mcp_tool_t *tool);

/**
 * @brief Find a tool in the list by name
 *
 * @note This is an internal API used by the MCP server implementation.
 *
 * @param[in] list Pointer to the tool list (must not be NULL)
 * @param[in] name Tool name to search for (must not be NULL)
 * @return Pointer to the found tool, or NULL if not found
 */
esp_mcp_tool_t *esp_mcp_tool_list_find_tool(const esp_mcp_tool_list_t *list, const char *name);

/**
 * @brief Iterate over all tools in the list
 *
 * Calls the callback function for each tool in the list. The iteration
 * is protected by mutex, so the list cannot be modified during iteration.
 *
 * @note This is an internal API used by the MCP server implementation.
 *
 * @param[in] list Pointer to the tool list (must not be NULL)
 * @param[in] callback Callback function called for each tool (must not be NULL)
 * @param[in] arg User data passed to the callback
 * @return
 *      - ESP_OK: All tools processed successfully
 *      - Other: Error code returned by callback
 */
esp_err_t esp_mcp_tool_list_foreach(const esp_mcp_tool_list_t *list, esp_err_t (*callback)(esp_mcp_tool_t *tool, void *arg), void *arg);

/**
 * @brief Check if the tool list is empty
 *
 * @note This is an internal API used by the MCP server implementation.
 *
 * @param[in] list Pointer to the tool list (must not be NULL)
 * @return true if list is empty, false otherwise
 */
bool esp_mcp_tool_list_is_empty(const esp_mcp_tool_list_t *list);

/**
 * @brief Remove a tool from the list
 *
 * @note This is an internal API used by the MCP server implementation.
 *
 * @param[in] list Pointer to the tool list (must not be NULL)
 * @param[in] tool Pointer to the tool to remove (must not be NULL)
 * @return
 *      - ESP_OK: Tool removed successfully
 *      - ESP_ERR_INVALID_ARG: Invalid parameter
 *      - ESP_ERR_NOT_FOUND: Tool not found
 *      - ESP_ERR_INVALID_STATE: List is not initialized
 *      - ESP_ERR_NO_MEM: Memory allocation failed
 *      - ESP_ERR_INTERNAL: Internal error
 */
esp_err_t esp_mcp_tool_list_remove_tool(esp_mcp_tool_list_t *list, esp_mcp_tool_t *tool);

/**
 * @brief Destroy a tool list
 *
 * Frees all resources, including all tools in the list.
 *
 * @note This is an internal API used by the MCP server implementation.
 *
 * @param[in] list Pointer to the tool list to destroy (must not be NULL)
 * @return
 *      - ESP_OK: Tool list destroyed successfully
 *      - ESP_ERR_INVALID_ARG: Invalid parameter
 */
esp_err_t esp_mcp_tool_list_destroy(esp_mcp_tool_list_t *list);

#ifdef __cplusplus
}
#endif
