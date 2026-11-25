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
 * @brief MCP tool definition object
 */
typedef struct esp_mcp_tool_s esp_mcp_tool_t;

/**
 * @brief Builder object for MCP tool call result payload
 */
typedef struct esp_mcp_tool_result_s esp_mcp_tool_result_t;

/**
 * @brief Borrowed context of one tool invocation
 *
 * Strings are valid only during the callback. Copy them before deferring work.
 * Client metadata describes peer capabilities; it is not authentication evidence.
 */
typedef struct {
    const char *protocol_version;          /*!< NULL for legacy requests without negotiated version */
    const char *meta_json;                 /*!< Complete request params._meta, nullable */
    const char *session_id;                /*!< Transport routing context, nullable */
    const char *auth_subject;              /*!< Transport-authenticated subject, nullable */
    bool is_authenticated;                /*!< Authentication result supplied by transport */
} esp_mcp_request_context_t;

/**
 * @brief Tool callback with per-request metadata and application context
 *
 * @param[in] properties Tool input properties (can be NULL)
 * @param[in] context Borrowed request context (valid only during the callback)
 * @param[out] result Tool result builder instance
 * @param[in] user_ctx User context pointer passed to esp_mcp_tool_create_with_context()
 * @return ESP_OK on success
 */
typedef esp_err_t (*esp_mcp_tool_callback_with_context_t)(const esp_mcp_property_list_t *properties,
                                                          const esp_mcp_request_context_t *context,
                                                          esp_mcp_tool_result_t *result,
                                                          void *user_ctx);

/**
 * @brief Validate request metadata before executing a tool
 *
 * Implementations must not execute the tool or produce side effects.
 *
 * @param[in] context Borrowed request context (valid only during the callback)
 * @param[in] user_ctx User context pointer passed to esp_mcp_tool_set_request_validator()
 * @return ESP_OK to allow execution, or an error to reject with JSON-RPC -32602
 */
typedef esp_err_t (*esp_mcp_tool_request_validator_t)(const esp_mcp_request_context_t *context, void *user_ctx);

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
 * @brief Extended tool callback allowing full MCP CallToolResult
 *
 * Implementations can populate a tool result with multiple content blocks,
 * structuredContent, and isError flag.
 *
 * @param[in] properties Tool input properties (can be NULL)
 * @param[out] result Tool result builder instance
 * @return ESP_OK on success
 */
typedef esp_err_t (*esp_mcp_tool_callback_ex_t)(const esp_mcp_property_list_t *properties,
                                                esp_mcp_tool_result_t *result);

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
 * @brief Create a new MCP tool with extended callback
 *
 * @param[in] name Tool name
 * @param[in] title Optional display title (can be NULL)
 * @param[in] description Tool description
 * @param[in] callback Extended callback
 * @return Pointer to tool or NULL
 */
esp_mcp_tool_t *esp_mcp_tool_create_ex(const char *name, const char *title, const char *description, esp_mcp_tool_callback_ex_t callback);

/**
 * @brief Create a tool whose callback receives the current request context
 *
 * @param[in] name Tool name (must not be NULL)
 * @param[in] description Tool description (must not be NULL)
 * @param[in] callback Context-aware tool callback function (must not be NULL)
 * @param[in] user_ctx User context pointer passed to @p callback (nullable)
 * @return Pointer to the created tool, or NULL on failure
 */
esp_mcp_tool_t *esp_mcp_tool_create_with_context(const char *name, const char *description,
                                                 esp_mcp_tool_callback_with_context_t callback,
                                                 void *user_ctx);

/**
 * @brief Set required client capabilities for modern requests
 *
 * JSON object matched recursively against clientCapabilities. Legacy calls are
 * unaffected. A missing requirement returns -32021 before executing the tool.
 *
 * @param[in] tool Tool instance (must not be NULL)
 * @param[in] capabilities_json JSON object string of required capabilities, or NULL to clear
 * @return
 *      - ESP_OK: Required capabilities updated successfully
 *      - ESP_ERR_INVALID_ARG: Invalid tool pointer or non-object JSON string
 *      - ESP_ERR_NO_MEM: Memory allocation failed
 */
esp_err_t esp_mcp_tool_set_required_capabilities_json(esp_mcp_tool_t *tool, const char *capabilities_json);

/**
 * @brief Set an optional metadata validator executed before the tool callback
 *
 * @param[in] tool Tool instance (must not be NULL)
 * @param[in] validator Validator callback, or NULL to clear
 * @param[in] user_ctx User context pointer passed to @p validator (nullable)
 * @return
 *      - ESP_OK: Validator updated successfully
 *      - ESP_ERR_INVALID_ARG: Invalid tool pointer
 */
esp_err_t esp_mcp_tool_set_request_validator(esp_mcp_tool_t *tool,
                                             esp_mcp_tool_request_validator_t validator, void *user_ctx);

/**
 * @brief Mark a tool as user-only (excluded unless tools/list.withUserTools is true)
 *
 * @param[in] tool Tool instance (must not be NULL)
 * @param[in] user_only True to restrict listing to requests with withUserTools=true
 * @return
 *      - ESP_OK: Flag updated successfully
 *      - ESP_ERR_INVALID_ARG: Invalid tool pointer
 */
esp_err_t esp_mcp_tool_set_user_only(esp_mcp_tool_t *tool, bool user_only);

/**
 * @brief Set tool title
 *
 * @param[in] tool Tool instance
 * @param[in] title Tool title (nullable)
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_tool_set_title(esp_mcp_tool_t *tool, const char *title);

/**
 * @brief Set tool icons metadata JSON
 *
 * @param[in] tool Tool instance
 * @param[in] icons_json JSON array/object string for icons (nullable)
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_tool_set_icons_json(esp_mcp_tool_t *tool, const char *icons_json);

/**
 * @brief Set tool output schema JSON
 *
 * @param[in] tool Tool instance
 * @param[in] schema_json JSON schema object string (nullable)
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_tool_set_output_schema_json(esp_mcp_tool_t *tool, const char *schema_json);

/**
 * @brief Set tool annotations JSON
 *
 * @param[in] tool Tool instance
 * @param[in] annotations_json JSON object string (nullable)
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_tool_set_annotations_json(esp_mcp_tool_t *tool, const char *annotations_json);

/**
 * @brief Set task support mode for the tool
 *
 * @param[in] tool Tool instance
 * @param[in] task_support "required", "optional", or "forbidden"
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_tool_set_task_support(esp_mcp_tool_t *tool, const char *task_support); // required|optional|forbidden

/**
 * @brief Create tool result builder
 *
 * @return Tool result builder instance, or NULL on failure
 */
esp_mcp_tool_result_t *esp_mcp_tool_result_create(void);

/**
 * @brief Destroy tool result builder
 *
 * @param[in] result Tool result builder
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_tool_result_destroy(esp_mcp_tool_result_t *result);

/**
 * @brief Set result isError flag
 *
 * @param[in] result Tool result builder
 * @param[in] is_error True for application-level error
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_tool_result_set_is_error(esp_mcp_tool_result_t *result, bool is_error);

/**
 * @brief Add text content block
 *
 * @param[in] result Tool result builder
 * @param[in] text Text content
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_tool_result_add_text(esp_mcp_tool_result_t *result, const char *text);

/**
 * @brief Add base64 image content block
 *
 * @param[in] result Tool result builder
 * @param[in] mime_type Image MIME type
 * @param[in] base64_data Base64-encoded image payload
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_tool_result_add_image_base64(esp_mcp_tool_result_t *result, const char *mime_type, const char *base64_data);

/**
 * @brief Add base64 audio content block
 *
 * @param[in] result Tool result builder
 * @param[in] mime_type Audio MIME type
 * @param[in] base64_data Base64-encoded audio payload
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_tool_result_add_audio_base64(esp_mcp_tool_result_t *result, const char *mime_type, const char *base64_data);

/**
 * @brief Add resource_link content block
 *
 * @param[in] result Tool result builder
 * @param[in] uri Resource URI
 * @param[in] name Resource display name
 * @param[in] description Resource description (nullable)
 * @param[in] mime_type Resource MIME type (nullable)
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_tool_result_add_resource_link(esp_mcp_tool_result_t *result,
                                                const char *uri,
                                                const char *name,
                                                const char *description,
                                                const char *mime_type);

/**
 * @brief Add embedded text resource content block
 *
 * @param[in] result Tool result builder
 * @param[in] uri Resource URI
 * @param[in] mime_type Resource MIME type
 * @param[in] text Text payload
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_tool_result_add_embedded_resource_text(esp_mcp_tool_result_t *result,
                                                         const char *uri,
                                                         const char *mime_type,
                                                         const char *text);

/**
 * @brief Add embedded base64 blob resource content block
 *
 * @param[in] result Tool result builder
 * @param[in] uri Resource URI
 * @param[in] mime_type Resource MIME type
 * @param[in] base64_blob Base64 blob payload
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_tool_result_add_embedded_resource_blob(esp_mcp_tool_result_t *result,
                                                         const char *uri,
                                                         const char *mime_type,
                                                         const char *base64_blob);

/**
 * @brief Set structuredContent JSON object
 *
 * @param[in] result Tool result builder
 * @param[in] structured_json_object JSON object string
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_tool_result_set_structured_json(esp_mcp_tool_result_t *result, const char *structured_json_object);

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
