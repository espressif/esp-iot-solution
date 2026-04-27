/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
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
 * @brief MCP response parse result structure
 */
typedef struct esp_mcp_resp_s {
    char *output;              /*!< Result or error object JSON string (allocated, free with esp_mcp_resp_free) */
    uint16_t id;               /*!< Parsed numeric id field (legacy, 0 when non-numeric) */
    char *id_str;              /*!< Canonical id string for both number/string ids (allocated) */
    bool is_error;             /*!< Set to true if error payload is present (application-level error),
                                    false if result payload is present without error */
    int error_code;            /*!< Error code (only valid when is_error is true or return value is ESP_ERR_INVALID_RESPONSE) */
    char *error_message;       /*!< Error message string (allocated, free with esp_mcp_resp_free, only valid when error_code is set) */
} esp_mcp_resp_t;

/**
 * @brief MCP info structure (internal build params)
 */
typedef struct esp_mcp_info_s {
    const char *protocol_version;   /*!< Protocol version string, default "2025-11-25" if NULL */
    const char *name;               /*!< Client name */
    const char *version;            /*!< Client version */
    const char *title;              /*!< Optional client title */
    const char *description;        /*!< Optional client description */
    const char *icons_json;         /*!< Optional client icons JSON array */
    const char *website_url;        /*!< Optional client website URL */
    const char *capabilities_json;  /*!< Optional client capabilities JSON object */
    const char *cursor;             /*!< Cursor */
    int tools_list_limit;           /*!< Outbound tools/list: >=0 include params.limit; <0 omit */
    const char *tool_name;          /*!< Tool name */
    const char *args_json;          /*!< Arguments JSON */
} esp_mcp_info_t;

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
    char *title;                                     /*!< Optional display title */
    char *description;                             /*!< Tool description */
    char *icons_json;                                /*!< Optional JSON array string for icons */
    char *output_schema_json;                        /*!< Optional JSON object string for output schema */
    char *annotations_json;                          /*!< Optional JSON object string for annotations */
    char task_support[12];                           /*!< execution.taskSupport: required|optional|forbidden|empty */
    esp_mcp_property_list_t *properties;           /*!< List of input properties */
    esp_mcp_tool_callback_t callback;              /*!< Callback function executed when tool is called */
    void *callback_ex;                               /*!< Optional extended callback */
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

#define ESP_MCP_MAX_SESSION_STATES 64

typedef struct {
    bool used;
    char session_id[32];
    bool received_initialize_request;
    bool require_initialized_notification;
    bool received_initialized_notification;
    bool has_log_level;
    char log_level[12];
    bool client_cap_roots_list;
    bool client_cap_sampling_create;
    bool client_cap_sampling_context;
    bool client_cap_sampling_tools;
    bool client_cap_elicitation_form;
    bool client_cap_elicitation_url;
    bool client_cap_tasks_list;
    bool client_cap_tasks_cancel;
    bool client_cap_tasks_sampling_create;
    bool client_cap_tasks_elicitation_create;
} esp_mcp_session_state_t;

/**
 * @brief MCP structure (server-side MCP instance)
 *
 * Structure managing MCP instance state, including registered tools
 * and message buffers for pending responses.
 */
typedef struct esp_mcp_s {
    esp_mcp_tool_list_t *tools;       /*!< List of registered tools */
    void *resources;                 /*!< Resource registry (opaque) */
    void *prompts;                   /*!< Prompt registry (opaque) */
    void *completion;                /*!< Completion provider (opaque) */
    void *tasks;                     /*!< Task store (opaque) */
    void *mgr_ctx;                   /*!< Back-pointer to manager context (opaque) */
    esp_mcp_mbuf_t mbuf;              /*!< Message buffer for responses */
    SemaphoreHandle_t mutex;          /*!< Mutex to serialize request handling */
    SemaphoreHandle_t resource_sub_mutex; /*!< Protects resource_subscriptions[] and count */
    SemaphoreHandle_t req_id_mutex;       /*!< Protects recent_request_ids[] and cursor */
    esp_mcp_session_state_t default_session_state; /*!< Lifecycle/capabilities for transports without session ids */
    esp_mcp_session_state_t session_states[ESP_MCP_MAX_SESSION_STATES]; /*!< Lifecycle/capabilities keyed by MCP-Session-Id */
    // Server metadata for initialize response (GAP-03, GAP-04)
    char *server_title;               /*!< Human-readable server title (nullable) */
    char *server_description;         /*!< Server description (nullable) */
    char *server_icons_json;          /*!< Server icons JSON object string (nullable) */
    char *server_website_url;         /*!< Server website URL (nullable) */
    char *instructions;               /*!< Instructions for the client (nullable) */
    bool client_initialized;          /*!< True after a successful client-side initialize response */
    bool client_requires_initialized_notification; /*!< True when negotiated client lifecycle requires notifications/initialized */
    bool client_sent_initialized_notification; /*!< True after client-side notifications/initialized is sent */
    esp_err_t (*incoming_request_handler)(const char *method,
                                          const char *params_json,
                                          char **out_result_json,
                                          void *user_ctx);
    void *incoming_request_ctx;
    esp_err_t (*incoming_notification_handler)(const char *method,
                                               const char *params_json,
                                               void *user_ctx);
    void *incoming_notification_ctx;
    // Per-request metadata (best-effort; overwritten each request)
    bool has_progress_token;
    char progress_token[32];
    bool has_related_task;
    char related_task_id[80];
    // Logging level for logging/setLevel (syslog strings)
    char log_level[12];

    // Last received cancellation notification (best-effort)
    bool has_cancel_request;
    char cancelled_request_id[32];
    char cancel_reason[64];

    // Per-request transport context
    bool request_authenticated;
    char request_session_id[32];
    char request_auth_subject[64];

    volatile bool destroy_requested;     /*!< True once destroy starts */
    volatile uint32_t tool_worker_count; /*!< Number of running async tool workers */

    // Session-scoped resource subscriptions
    size_t resource_subscriptions_count;
    struct {
        char session_id[32];
        char uri[128];
    } resource_subscriptions[CONFIG_MCP_RESOURCE_SUBSCRIPTIONS_MAX];
    // Recent request-id cache for session-level duplicate detection.
    size_t recent_request_id_cursor;
    struct {
        bool used;
        char session_id[32];
        char request_id[64];
    } recent_request_ids[64];

    // Generic _meta storage for unknown fields
    char *request_meta_json;
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
