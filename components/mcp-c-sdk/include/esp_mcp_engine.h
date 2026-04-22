/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_mcp_property.h"
#include "esp_mcp_tool.h"
#include "esp_mcp_resource.h"
#include "esp_mcp_prompt.h"
#include "esp_mcp_completion.h"

/**
 * @brief MCP engine instance (protocol semantics + tool dispatch)
 *
 * Opaque structure representing an MCP engine instance that handles MCP protocol semantics
 * (e.g., tools/list, tools/call, ping, initialize) and tool execution.
 *
 * @note Naming/layering:
 *       - Public manager/router APIs use `esp_mcp_mgr_*` (see `esp_mcp_mgr.h`)
 *       - Transport implementations expose `esp_mcp_transport_*` tables (e.g., `esp_mcp_transport_http`)
 *       - This header defines the engine public API. For simplicity, the engine public APIs keep
 *         the short prefix `esp_mcp_*` (e.g., esp_mcp_create()).
 */
typedef struct esp_mcp_s esp_mcp_t;

/**
 * @brief Create an MCP instance
 *
 * Allocates and initializes a new MCP instance.
 *
 * @param[out] mcp Pointer to store the created MCP instance (must not be NULL)
 * @return
 *      - ESP_OK: Server created successfully
 *      - ESP_ERR_NO_MEM: Memory allocation failed
 */
esp_err_t esp_mcp_create(esp_mcp_t **mcp);

/**
 * @brief Destroy an MCP instance
 *
 * Frees all resources associated with the MCP instance, including all registered tools
 * and message buffers. The function will attempt to clean up all resources even
 * if some cleanup operations fail, and will log any errors encountered during cleanup.
 *
 * @param[in] mcp Pointer to the MCP instance to destroy (must not be NULL)
 * @return
 *      - ESP_OK: Cleanup completed (resources have been released, even if some errors occurred during cleanup)
 *      - ESP_ERR_INVALID_ARG: Invalid parameter (server is NULL)
 *
 * @note This function always completes the cleanup process and returns ESP_OK unless
 *       the server parameter is invalid. Any errors during resource cleanup are logged
 *       but do not affect the return value, as the caller cannot meaningfully recover
 *       from such errors anyway.
 */
esp_err_t esp_mcp_destroy(esp_mcp_t *mcp);

/**
 * @brief Add a tool to the MCP instance
 *
 * Adds a tool to the MCP instance.
 *
 * @param[in] mcp Pointer to the MCP instance (must not be NULL)
 * @param[in] tool Pointer to the tool to add (must not be NULL)
 * @return ESP_OK if successful, otherwise an error code
 */
esp_err_t esp_mcp_add_tool(esp_mcp_t *mcp, esp_mcp_tool_t *tool);

/**
 * @brief Remove a tool from the MCP instance
 *
 * Removes a tool from the MCP instance.
 *
 * @param[in] mcp Pointer to the MCP instance (must not be NULL)
 * @param[in] tool Pointer to the tool to remove (must not be NULL)
 * @return ESP_OK if successful, otherwise an error code
 */
esp_err_t esp_mcp_remove_tool(esp_mcp_t *mcp, esp_mcp_tool_t *tool);

/**
 * @brief Add a resource to the MCP instance
 *
 * @param[in] mcp MCP instance
 * @param[in] res Resource to add
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_add_resource(esp_mcp_t *mcp, esp_mcp_resource_t *res);

/**
 * @brief Remove a resource from the MCP instance
 *
 * @param[in] mcp MCP instance
 * @param[in] res Resource to remove
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_remove_resource(esp_mcp_t *mcp, esp_mcp_resource_t *res);

/**
 * @brief Add a prompt to the MCP instance
 *
 * @param[in] mcp MCP instance
 * @param[in] prompt Prompt to add
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_add_prompt(esp_mcp_t *mcp, esp_mcp_prompt_t *prompt);

/**
 * @brief Remove a prompt from the MCP instance
 *
 * @param[in] mcp MCP instance
 * @param[in] prompt Prompt to remove
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_remove_prompt(esp_mcp_t *mcp, esp_mcp_prompt_t *prompt);

/**
 * @brief Set completion provider for completion/complete support
 *
 * @param[in] mcp MCP instance
 * @param[in] provider Completion provider, or NULL to clear
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_set_completion_provider(esp_mcp_t *mcp, esp_mcp_completion_provider_t *provider);

/**
 * @brief Set server metadata for initialize response
 *
 * Sets optional server metadata fields that will be included in the
 * serverInfo object of the initialize response per MCP 2025-11-25 spec.
 *
 * @param[in] mcp MCP instance
 * @param[in] title Human-readable server title (nullable)
 * @param[in] description Server description (nullable)
 * @param[in] icons_json Server icons JSON object string (nullable)
 * @param[in] website_url Server website URL (nullable)
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_set_server_info(esp_mcp_t *mcp, const char *title, const char *description, const char *icons_json, const char *website_url);

/**
 * @brief Set instructions for the client
 *
 * Sets the instructions field that will be included in the initialize
 * response per MCP 2025-11-25 spec.
 *
 * @param[in] mcp MCP instance
 * @param[in] instructions Instructions for the client (nullable)
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_set_instructions(esp_mcp_t *mcp, const char *instructions);

/**
 * @brief Emit notifications/message to a client session
 *
 * @param[in] mcp MCP instance
 * @param[in] level Log level string (e.g. "info", "warning", "error")
 * @param[in] logger Logger name (nullable)
 * @param[in] data_json_object JSON object string payload
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_notify_log_message(esp_mcp_t *mcp, const char *level, const char *logger, const char *data_json_object);

/**
 * @brief Emit notifications/progress to a client session
 *
 * @param[in] mcp MCP instance
 * @param[in] progress_token Client-provided progress token
 * @param[in] progress Current progress value
 * @param[in] total Optional total value (set negative to omit)
 * @param[in] message Optional human-readable status text
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_notify_progress(esp_mcp_t *mcp, const char *progress_token, double progress, double total, const char *message);

/**
 * @brief Generic handler for inbound MCP requests
 *
 * This callback is primarily useful when the SDK acts as an MCP client and
 * receives server-initiated requests such as `roots/list`,
 * `sampling/createMessage`, `elicitation/create`, or the `tasks/` request family.
 * Applications should allocate `*out_result_json` with `malloc/calloc/strdup`.
 * When the callback returns `ESP_OK` and `*out_result_json` is NULL, the SDK
 * replies with an empty result object `{}`.
 *
 * Return value mapping:
 * - `ESP_OK`: request handled successfully, reply with `*out_result_json`
 * - `ESP_ERR_NOT_SUPPORTED`: request not handled, continue normal dispatch
 * - `ESP_ERR_INVALID_ARG`: reply with JSON-RPC invalid params
 * - other errors: reply with JSON-RPC internal error
 *
 * @param[in] method Request method string
 * @param[in] params_json Request params JSON string, or NULL when params absent
 * @param[out] out_result_json Result JSON string allocated by callback
 * @param[in] user_ctx User context pointer
 * @return esp_err_t status as described above
 */
typedef esp_err_t (*esp_mcp_incoming_request_handler_t)(const char *method,
                                                        const char *params_json,
                                                        char **out_result_json,
                                                        void *user_ctx);

/**
 * @brief Generic handler for inbound MCP notifications
 *
 * This callback is useful for observing notifications that the SDK does not
 * otherwise expose directly, for example `notifications/roots/list_changed`,
 * `notifications/message`, `notifications/progress`, or
 * `notifications/tasks/status`.
 *
 * @param[in] method Notification method string
 * @param[in] params_json Notification params JSON string, or NULL when params absent
 * @param[in] user_ctx User context pointer
 * @return ESP_OK on success
 */
typedef esp_err_t (*esp_mcp_incoming_notification_handler_t)(const char *method,
                                                             const char *params_json,
                                                             void *user_ctx);

/**
 * @brief Set a generic handler for inbound MCP requests
 *
 * @param[in] mcp MCP instance
 * @param[in] handler Handler function, or NULL to clear
 * @param[in] user_ctx User context pointer passed to handler
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_set_incoming_request_handler(esp_mcp_t *mcp,
                                               esp_mcp_incoming_request_handler_t handler,
                                               void *user_ctx);

/**
 * @brief Set a generic handler for inbound MCP notifications
 *
 * @param[in] mcp MCP instance
 * @param[in] handler Handler function, or NULL to clear
 * @param[in] user_ctx User context pointer passed to handler
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_set_incoming_notification_handler(esp_mcp_t *mcp,
                                                    esp_mcp_incoming_notification_handler_t handler,
                                                    void *user_ctx);

/**
 * @brief Callback for server-initiated request responses
 *
 * @param[in] error_code 0/1 for application-level result, negative for JSON-RPC
 *            protocol error, positive esp_err_t for local/transport failure
 * @param[in] request_name Logical request name/method
 * @param[in] resp_json Response JSON payload
 * @param[in] user_ctx User context pointer provided when request was sent
 * @return ESP_OK on success
 */
typedef esp_err_t (*esp_mcp_server_req_cb_t)(int error_code, const char *request_name, const char *resp_json, void *user_ctx,
                                             uint32_t jsonrpc_request_id);

/**
 * @brief Set current request context (transport-provided)
 *
 * @param[in] mcp MCP instance
 * @param[in] session_id Session id from transport layer (nullable)
 * @param[in] auth_subject Authenticated principal/subject (nullable)
 * @param[in] is_authenticated True when current request has passed auth checks
 * @return ESP_OK on success
 *
 * @note This context is best-effort and is overwritten for each request.
 */
esp_err_t esp_mcp_set_request_context(esp_mcp_t *mcp,
                                      const char *session_id,
                                      const char *auth_subject,
                                      bool is_authenticated);

/**
 * @brief Get session id from current request context
 *
 * @param[in] mcp MCP instance
 * @return Session id string, or NULL if unavailable
 */
const char *esp_mcp_get_request_session_id(const esp_mcp_t *mcp);

/**
 * @brief Clear session-scoped runtime state
 *
 * @param[in] mcp MCP instance
 * @param[in] session_id Target session id (nullable)
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_clear_session_state(esp_mcp_t *mcp, const char *session_id);

/**
 * @brief Emit server-initiated roots/list request to one session
 *
 * @param[in] mcp MCP instance
 * @param[in] session_id Target MCP session id (nullable)
 * @param[in] cb Callback invoked when response arrives or times out
 * @param[in] user_ctx User context pointer passed to callback
 * @param[in] timeout_ms Request timeout in milliseconds (0 means default timeout)
 * @param[out] out_request_id Output buffer for generated request id (nullable)
 * @param[in] out_request_id_len Length of out_request_id buffer in bytes
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_request_roots_list(esp_mcp_t *mcp,
                                     const char *session_id,
                                     esp_mcp_server_req_cb_t cb,
                                     void *user_ctx,
                                     uint32_t timeout_ms,
                                     char *out_request_id,
                                     size_t out_request_id_len);

/**
 * @brief Emit server-initiated sampling/createMessage request to one session
 *
 * @param[in] mcp MCP instance
 * @param[in] session_id Target MCP session id (nullable)
 * @param[in] params_json JSON object string for method params
 * @param[in] cb Callback invoked when response arrives or times out
 * @param[in] user_ctx User context pointer passed to callback
 * @param[in] timeout_ms Request timeout in milliseconds (0 means default timeout)
 * @param[out] out_request_id Output buffer for generated request id (nullable)
 * @param[in] out_request_id_len Length of out_request_id buffer in bytes
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_request_sampling_create(esp_mcp_t *mcp,
                                          const char *session_id,
                                          const char *params_json,
                                          esp_mcp_server_req_cb_t cb,
                                          void *user_ctx,
                                          uint32_t timeout_ms,
                                          char *out_request_id,
                                          size_t out_request_id_len);

/**
 * @brief Emit server-initiated sampling/createMessage with tools support
 *
 * This function sends a sampling request with tool definitions and toolChoice
 * per MCP 2025-11-25 spec.
 *
 * @param[in] mcp MCP instance
 * @param[in] session_id Target MCP session id (nullable)
 * @param[in] params_json JSON object string for method params (excluding tools/toolChoice)
 * @param[in] tools_json JSON array string of tool definitions (nullable)
 * @param[in] tool_choice Tool choice mode: "auto", "none", or "required" (nullable)
 * @param[in] cb Callback invoked when response arrives or times out
 * @param[in] user_ctx User context pointer passed to callback
 * @param[in] timeout_ms Request timeout in milliseconds (0 means default timeout)
 * @param[out] out_request_id Output buffer for generated request id (nullable)
 * @param[in] out_request_id_len Length of out_request_id buffer in bytes
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_request_sampling_with_tools(esp_mcp_t *mcp,
                                              const char *session_id,
                                              const char *params_json,
                                              const char *tools_json,
                                              const char *tool_choice,
                                              esp_mcp_server_req_cb_t cb,
                                              void *user_ctx,
                                              uint32_t timeout_ms,
                                              char *out_request_id,
                                              size_t out_request_id_len);

/**
 * @brief Emit server-initiated elicitation/create to one session
 *
 * @param[in] mcp MCP instance
 * @param[in] session_id Target MCP session id (nullable)
 * @param[in] params_json JSON object string for method params
 * @param[in] cb Callback invoked when response arrives or times out
 * @param[in] user_ctx User context pointer passed to callback
 * @param[in] timeout_ms Request timeout in milliseconds (0 means default timeout)
 * @param[out] out_request_id Output buffer for generated request id (nullable)
 * @param[in] out_request_id_len Length of out_request_id buffer in bytes
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_request_elicitation(esp_mcp_t *mcp,
                                      const char *session_id,
                                      const char *params_json,
                                      esp_mcp_server_req_cb_t cb,
                                      void *user_ctx,
                                      uint32_t timeout_ms,
                                      char *out_request_id,
                                      size_t out_request_id_len);

/**
 * @brief Emit server-initiated elicitation/create with URL mode
 *
 * This function sends an elicitation request with a URL that the client
 * should open for user interaction per MCP 2025-11-25 spec.
 *
 * @param[in] mcp MCP instance
 * @param[in] session_id Target MCP session id (nullable)
 * @param[in] params_json JSON object string for method params (excluding url)
 * @param[in] url URL for the elicitation interaction
 * @param[in] cb Callback invoked when response arrives or times out
 * @param[in] user_ctx User context pointer passed to callback
 * @param[in] timeout_ms Request timeout in milliseconds (0 means default timeout)
 * @param[out] out_request_id Output buffer for generated request id (nullable)
 * @param[in] out_request_id_len Length of out_request_id buffer in bytes
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_request_elicitation_url(esp_mcp_t *mcp,
                                          const char *session_id,
                                          const char *params_json,
                                          const char *url,
                                          esp_mcp_server_req_cb_t cb,
                                          void *user_ctx,
                                          uint32_t timeout_ms,
                                          char *out_request_id,
                                          size_t out_request_id_len);

/**
 * @brief Emit server-initiated tasks/get request to one session
 *
 * @param[in] mcp MCP instance
 * @param[in] session_id Target MCP session id (nullable)
 * @param[in] task_id Target task id
 * @param[in] cb Callback invoked when response arrives or times out
 * @param[in] user_ctx User context pointer passed to callback
 * @param[in] timeout_ms Request timeout in milliseconds (0 means default timeout)
 * @param[out] out_request_id Output buffer for generated request id (nullable)
 * @param[in] out_request_id_len Length of out_request_id buffer in bytes
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_request_tasks_get(esp_mcp_t *mcp,
                                    const char *session_id,
                                    const char *task_id,
                                    esp_mcp_server_req_cb_t cb,
                                    void *user_ctx,
                                    uint32_t timeout_ms,
                                    char *out_request_id,
                                    size_t out_request_id_len);

/**
 * @brief Emit server-initiated tasks/result request to one session
 *
 * @param[in] mcp MCP instance
 * @param[in] session_id Target MCP session id (nullable)
 * @param[in] task_id Target task id
 * @param[in] cb Callback invoked when response arrives or times out
 * @param[in] user_ctx User context pointer passed to callback
 * @param[in] timeout_ms Request timeout in milliseconds (0 means default timeout)
 * @param[out] out_request_id Output buffer for generated request id (nullable)
 * @param[in] out_request_id_len Length of out_request_id buffer in bytes
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_request_tasks_result(esp_mcp_t *mcp,
                                       const char *session_id,
                                       const char *task_id,
                                       esp_mcp_server_req_cb_t cb,
                                       void *user_ctx,
                                       uint32_t timeout_ms,
                                       char *out_request_id,
                                       size_t out_request_id_len);

/**
 * @brief Emit server-initiated tasks/cancel request to one session
 *
 * Requires the peer to advertise `capabilities.tasks.cancel`.
 *
 * @param[in] mcp MCP instance
 * @param[in] session_id Target MCP session id (nullable)
 * @param[in] task_id Target task id
 * @param[in] cb Callback invoked when response arrives or times out
 * @param[in] user_ctx User context pointer passed to callback
 * @param[in] timeout_ms Request timeout in milliseconds (0 means default timeout)
 * @param[out] out_request_id Output buffer for generated request id (nullable)
 * @param[in] out_request_id_len Length of out_request_id buffer in bytes
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_request_tasks_cancel(esp_mcp_t *mcp,
                                       const char *session_id,
                                       const char *task_id,
                                       esp_mcp_server_req_cb_t cb,
                                       void *user_ctx,
                                       uint32_t timeout_ms,
                                       char *out_request_id,
                                       size_t out_request_id_len);

/**
 * @brief Emit server-initiated tasks/list request to one session
 *
 * Requires the peer to advertise `capabilities.tasks.list`.
 *
 * @param[in] mcp MCP instance
 * @param[in] session_id Target MCP session id (nullable)
 * @param[in] cursor Pagination cursor to continue from (nullable)
 * @param[in] cb Callback invoked when response arrives or times out
 * @param[in] user_ctx User context pointer passed to callback
 * @param[in] timeout_ms Request timeout in milliseconds (0 means default timeout)
 * @param[out] out_request_id Output buffer for generated request id (nullable)
 * @param[in] out_request_id_len Length of out_request_id buffer in bytes
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_request_tasks_list(esp_mcp_t *mcp,
                                     const char *session_id,
                                     const char *cursor,
                                     esp_mcp_server_req_cb_t cb,
                                     void *user_ctx,
                                     uint32_t timeout_ms,
                                     char *out_request_id,
                                     size_t out_request_id_len);

/**
 * @brief Handle one MCP JSON-RPC message and produce a response buffer
 *
 * This is a low-level helper that processes a single incoming MCP JSON-RPC message.
 * If the input is a request, it produces a JSON-RPC response (UTF-8 JSON).
 * If the input is a notification or response, it produces no output (outbuf==NULL).
 *
 * @param[in]  mcp     MCP instance
 * @param[in]  inbuf   Input buffer (UTF-8 JSON)
 * @param[in]  inlen   Input length in bytes
 * @param[out] outbuf  Output buffer pointer (allocated by the engine, free with esp_mcp_free_response)
 * @param[out] outlen  Output length in bytes
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_handle_message(esp_mcp_t *mcp, const uint8_t *inbuf, uint16_t inlen, uint8_t **outbuf, uint16_t *outlen);

/**
 * @brief Free a response buffer returned by esp_mcp_handle_message
 *
 * @param[in] mcp MCP instance
 * @param[in] outbuf Output buffer to free (may be NULL)
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_free_response(esp_mcp_t *mcp, uint8_t *outbuf);

#ifdef __cplusplus
}
#endif
