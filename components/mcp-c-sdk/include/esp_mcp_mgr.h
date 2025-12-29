/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MCP transport manager handle type
 *
 * Opaque handle representing an initialized MCP (Model Context Protocol) transport manager instance.
 * The transport manager handles communication between ESP32 devices and AI applications using
 * the MCP protocol over various transport layers (HTTP, BLE, etc.).
 * Created by esp_mcp_mgr_init() and used for all subsequent MCP transport operations.
 */
typedef uint32_t esp_mcp_mgr_handle_t;

/**
 * @brief MCP transport handle type
 *
 * Opaque handle representing an initialized MCP transport instance.
 * A transport defines how MCP protocol messages are transmitted (e.g., HTTP, BLE).
 * Created by the transport's new() function and used for transport-specific operations.
 */
typedef uint32_t esp_mcp_transport_handle_t;

/**
 * @brief Response callback type for outbound requests
 *
 * This callback is invoked when a response is received for an outbound MCP request.
 * The meaning of parameters depends on the response type:
 *
 * @param error_code Error indicator or error code, meaning depends on resp_json:
 *                   - If resp_json is not NULL and contains application result:
 *                     * 0: Application-level success (result.isError = false or not present)
 *                     * 1: Application-level error (result.isError = true)
 *                   - If resp_json is not NULL and contains protocol error:
 *                     * JSON-RPC error code (negative integer, e.g., -32601 for "Method not found")
 *                     * See MCP_ERROR_CODE_* constants in esp_mcp_error.h
 *                   - If resp_json is NULL (parse failure):
 *                     * esp_err_t error code (e.g., ESP_ERR_INVALID_ARG, ESP_ERR_NO_MEM)
 *
 * @param ep_name    Endpoint name or tool name (may be NULL);
 *                   valid only during callback execution, do NOT free
 *
 * @param resp_json  Response JSON body, meaning depends on error_code:
 *                   - If error_code is 0 or 1: Contains result object JSON (application-level response)
 *                   - If error_code is negative (JSON-RPC error code): Contains error message string (protocol-level error)
 *                   - If error_code is positive esp_err_t: NULL (parse failure)
 *                   Valid only during callback execution, do NOT free
 *
 * @param user_ctx   User context provided when sending the request
 *
 * @return ESP_OK on success, or error code if callback processing failed
 *
 * @note The callback is called synchronously. The ep_name and resp_json pointers are valid
 *       only during the callback execution. The framework will free these resources after
 *       the callback returns. The callback should NOT attempt to free these strings.
 */
typedef esp_err_t (*esp_mcp_mgr_resp_cb_t)(int error_code, const char *ep_name, const char *resp_json, void *user_ctx);

/**
 * @brief MCP request structure
 *
 * Structure for transmitting MCP requests.
 */
typedef struct {
    const char *ep_name;                         /*!< Endpoint name */
    esp_mcp_mgr_resp_cb_t cb;                    /*!< Response callback */
    void *user_ctx;                              /*!< User context */
    union {
        struct {
            const char *protocol_version;   /*!< Protocol version string, default "2024-11-05" if NULL */
            const char *name;               /*!< Client name */
            const char *version;            /*!< Client version */
        } init;                             /*!< Parameters for initialize request */
        struct {
            const char *cursor;                  /*!< Cursor */
        } list;                             /*!< Parameters for tools/list request */
        struct {
            const char *tool_name;               /*!< Tool name */
            const char *args_json;               /*!< Tool arguments JSON */
        } call;                             /*!< Parameters for tools/call request */
    } u;                                     /*!< Union containing request-specific parameters */
} esp_mcp_mgr_req_t;

/**
 * @brief MCP transport function table
 *
 * Structure defining the function callbacks for a specific MCP transport implementation.
 * Each transport (HTTP, BLE, etc.) provides its own implementation of these functions
 * to handle protocol-specific operations like starting/stopping the transport layer and
 * managing endpoints for MCP message routing.
 */
typedef struct esp_mcp_transport_s {
    esp_err_t (*init)(esp_mcp_mgr_handle_t handle, esp_mcp_transport_handle_t *transport_handle);   /*!< Create a new transport instance */
    esp_err_t (*deinit)(esp_mcp_transport_handle_t handle);                                    /*!< Destroy a transport instance */
    esp_err_t (*start)(esp_mcp_transport_handle_t handle, void *config);              /*!< Start the transport layer (e.g., start HTTP server) */
    esp_err_t (*stop)(esp_mcp_transport_handle_t handle);                              /*!< Stop the transport layer (e.g., stop HTTP server) */
    esp_err_t (*create_config)(const void *config, void **config_out);                        /*!< Create transport-specific configuration */
    esp_err_t (*delete_config)(void *config);                                              /*!< Destroy transport-specific configuration */
    esp_err_t (*register_endpoint)(esp_mcp_transport_handle_t handle, const char *ep_name, void *priv_data);   /*!< Register an MCP endpoint for message routing */
    esp_err_t (*unregister_endpoint)(esp_mcp_transport_handle_t handle, const char *ep_name);     /*!< Unregister an MCP endpoint */
    esp_err_t (*request)(esp_mcp_transport_handle_t handle, const uint8_t *inbuf, uint16_t inlen); /*!< Perform an outbound MCP request (client-side), optional, fire-and-forget */
} esp_mcp_transport_t;

/**
 * @brief MCP transport manager configuration structure
 *
 * Configuration structure for initializing an MCP transport manager instance.
 * Specifies the transport to use (HTTP, BLE, etc.) and associated configuration.
 */
typedef struct {
    esp_mcp_transport_t transport;  /*!< Transport function table (e.g., esp_mcp_transport_http) */
    void *config;                   /*!< Transport-specific configuration (e.g., HTTP server port, BLE service UUID) */
    void *instance;                 /*!< MCP instance context (esp_mcp_t *) for handling MCP protocol messages */
} esp_mcp_mgr_config_t;

extern const esp_mcp_transport_t esp_mcp_transport_http_server;
extern const esp_mcp_transport_t esp_mcp_transport_http_client;

/**
 * @brief Deprecated: Use esp_mcp_transport_http_server or esp_mcp_transport_http_client instead
 *
 * @deprecated This symbol is deprecated and will be removed in a future release.
 *             Use esp_mcp_transport_http_server for server mode or esp_mcp_transport_http_client for client mode.
 *             This alias currently maps to esp_mcp_transport_http_server for backward compatibility.
 */
__attribute__((deprecated("Use esp_mcp_transport_http_server or esp_mcp_transport_http_client instead")))
extern const esp_mcp_transport_t esp_mcp_transport_http;

/**
 * @brief Initialize MCP transport manager
 *
 * Initializes an MCP (Model Context Protocol) transport manager with the specified
 * transport (HTTP, BLE, etc.). The transport manager handles communication
 * between ESP32 devices and AI applications using the MCP protocol.
 *
 * @param[in] config MCP transport manager configuration (transport, config, server instance)
 * @param[out] handle Pointer to store the created MCP transport manager handle
 *
 * @return
 *      - ESP_OK: MCP transport manager initialized successfully
 *      - ESP_ERR_INVALID_ARG: Invalid arguments (NULL handle or invalid transport functions)
 *      - ESP_ERR_NO_MEM: Out of memory
 */
esp_err_t esp_mcp_mgr_init(esp_mcp_mgr_config_t config, esp_mcp_mgr_handle_t *handle);

/**
 * @brief Deinitialize MCP transport manager
 *
 * Deinitializes the MCP transport manager and releases all associated resources,
 * including transport instances and configurations. The transport must
 * be stopped (via esp_mcp_mgr_stop()) before deinitialization.
 *
 * @param[in] handle MCP transport manager handle
 *
 * @return
 *      - ESP_OK: MCP transport manager deinitialized successfully
 *      - ESP_ERR_INVALID_ARG: Invalid handle
 */
esp_err_t esp_mcp_mgr_deinit(esp_mcp_mgr_handle_t handle);

/**
 * @brief Start MCP transport layer
 *
 * Starts the underlying transport layer (e.g., HTTP server, BLE service) to begin
 * accepting MCP protocol messages from AI applications. The transport manager must
 * be initialized before calling this function.
 *
 * @param[in] handle MCP transport manager handle
 *
 * @return
 *      - ESP_OK: Transport layer started successfully
 *      - ESP_ERR_INVALID_ARG: Invalid handle
 *      - ESP_ERR_FAIL: Failed to start transport layer (e.g., port already in use)
 */
esp_err_t esp_mcp_mgr_start(esp_mcp_mgr_handle_t handle);

/**
 * @brief Stop MCP transport layer
 *
 * Stops the underlying transport layer (e.g., HTTP server, BLE service) and stops
 * accepting new MCP protocol messages. Existing connections may be gracefully closed.
 *
 * @param[in] handle MCP transport manager handle
 *
 * @return
 *      - ESP_OK: Transport layer stopped successfully
 *      - ESP_ERR_INVALID_ARG: Invalid handle
 */
esp_err_t esp_mcp_mgr_stop(esp_mcp_mgr_handle_t handle);

/**
 * @brief Register an MCP endpoint
 *
 * Registers an endpoint (e.g., "tools/list", "tools/call") for routing MCP protocol
 * messages. Endpoints are used to organize different types of MCP operations.
 * The endpoint will be accessible via the transport layer (e.g., HTTP path).
 *
 * @param[in] handle MCP transport manager handle
 * @param[in] ep_name Endpoint name (e.g., "mcp_server", "tools/list")
 * @param[in] priv_data Private data passed to the endpoint handler (typically NULL or MCP engine instance)
 * @return
 *      - ESP_OK: Endpoint registered successfully
 *      - ESP_ERR_INVALID_ARG: Invalid arguments
 *      - ESP_ERR_INVALID_STATE: Endpoint already exists
 *      - ESP_ERR_NO_MEM: Memory allocation failed
 */
esp_err_t esp_mcp_mgr_register_endpoint(esp_mcp_mgr_handle_t handle, const char *ep_name, void *priv_data);

/**
 * @brief Unregister an MCP endpoint
 *
 * Removes an endpoint from the MCP transport manager. The endpoint will no longer
 * accept MCP protocol messages. All associated resources are freed.
 *
 * @param[in] handle MCP transport manager handle
 * @param[in] ep_name Endpoint name to remove
 * @return
 *      - ESP_OK: Endpoint removed successfully
 *      - ESP_ERR_INVALID_ARG: Invalid arguments
 *      - ESP_ERR_NOT_FOUND: Endpoint not found
 */
esp_err_t esp_mcp_mgr_unregister_endpoint(esp_mcp_mgr_handle_t handle, const char *ep_name);

/**
 * @brief Handle an MCP protocol request
 *
 * Processes an incoming MCP protocol message (JSON-RPC 2.0 format) and routes it
 * to the appropriate endpoint handler. The handler parses the message, executes
 * the requested MCP operation (e.g., initialize, tools/list, tools/call), and returns
 * the response. This function is typically called by transport layer implementations
 * (e.g., HTTP handler) when receiving MCP protocol messages from AI applications.
 *
 * @param[in] handle MCP transport manager handle
 * @param[in] ep_name Endpoint name to route the request to (e.g., "mcp_server")
 * @param[in] inbuf Input buffer containing the MCP protocol message (JSON-RPC 2.0 format, null-terminated)
 * @param[in] inlen Length of the input buffer in bytes
 * @param[out] outbuf Pointer to store the output buffer containing the MCP response JSON (caller must free with cJSON_free after use)
 * @param[out] outlen Pointer to store the output buffer length in bytes
 * @return
 *      - ESP_OK: Request handled successfully, response available in outbuf
 *      - ESP_ERR_INVALID_ARG: Invalid arguments (NULL pointer or empty endpoint name)
 *      - ESP_ERR_NOT_FOUND: Endpoint not found
 *      - ESP_FAIL: Failed to process MCP request (parse error, tool execution error, etc.)
 *
 * @note The caller is responsible for freeing the outbuf using cJSON_free() after sending the response.
 * @note If the function returns an error, outbuf may be NULL or contain an error response.
 */
esp_err_t esp_mcp_mgr_req_handle(esp_mcp_mgr_handle_t handle, const char *ep_name, const uint8_t *inbuf, uint16_t inlen, uint8_t **outbuf, uint16_t *outlen);

/**
 * @brief Perform an outbound MCP request via the configured transport (client-side)
 *
 * Sends a JSON-RPC request to a remote MCP server endpoint over the configured transport.
 * The transport must implement the optional `transport.request` callback.
 *
 * @param[in] handle MCP transport manager handle
 * @param[in] inbuf Input buffer containing the MCP protocol message (JSON-RPC request)
 * @param[in] inlen Length of the input buffer in bytes
 * @return
 *      - ESP_OK: Request performed successfully (fire-and-forget)
 *      - ESP_ERR_INVALID_ARG: Invalid arguments
 *      - ESP_ERR_NOT_SUPPORTED: Transport does not support outbound requests
 *      - Other: Transport-specific failure
 */
esp_err_t esp_mcp_mgr_perform_handle(esp_mcp_mgr_handle_t handle, const uint8_t *inbuf, uint16_t inlen);

/**
 * @brief Build and send initialize request to remote MCP server
 *
 * Constructs and sends an MCP initialize request to establish a session with a remote MCP server.
 * The response will be delivered via the callback specified in the request structure.
 *
 * @param[in] handle MCP transport manager handle
 * @param[in] req Request structure containing endpoint name, callback, and initialization parameters
 *                 (protocol_version, name, version)
 * @return
 *      - ESP_OK: Request sent successfully
 *      - ESP_ERR_INVALID_ARG: Invalid arguments (NULL handle or request)
 *      - ESP_ERR_NOT_SUPPORTED: Transport does not support outbound requests
 *      - Other: Transport-specific failure
 */
esp_err_t esp_mcp_mgr_post_info_init(esp_mcp_mgr_handle_t handle, const esp_mcp_mgr_req_t *req);

/**
 * @brief Build and send tools/list request to remote MCP server
 *
 * Constructs and sends an MCP tools/list request to retrieve available tools from a remote MCP server.
 * Supports cursor-based pagination. The response will be delivered via the callback specified in the request structure.
 *
 * @param[in] handle MCP transport manager handle
 * @param[in] req Request structure containing endpoint name, callback, and optional cursor for pagination
 * @return
 *      - ESP_OK: Request sent successfully
 *      - ESP_ERR_INVALID_ARG: Invalid arguments (NULL handle or request)
 *      - ESP_ERR_NOT_SUPPORTED: Transport does not support outbound requests
 *      - Other: Transport-specific failure
 */
esp_err_t esp_mcp_mgr_post_tools_list(esp_mcp_mgr_handle_t handle, const esp_mcp_mgr_req_t *req);

/**
 * @brief Build and send tools/call request to remote MCP server
 *
 * Constructs and sends an MCP tools/call request to execute a tool on a remote MCP server.
 * The tool name and arguments (as JSON string) must be provided. The response will be delivered
 * via the callback specified in the request structure.
 *
 * @param[in] handle MCP transport manager handle
 * @param[in] req Request structure containing endpoint name, callback, tool name, and arguments JSON
 * @return
 *      - ESP_OK: Request sent successfully
 *      - ESP_ERR_INVALID_ARG: Invalid arguments (NULL handle, request, or missing tool_name)
 *      - ESP_ERR_NOT_SUPPORTED: Transport does not support outbound requests
 *      - Other: Transport-specific failure
 */
esp_err_t esp_mcp_mgr_post_tools_call(esp_mcp_mgr_handle_t handle, const esp_mcp_mgr_req_t *req);

/**
 * @brief Destroy a MCP response buffer
 *
 * Destroys a MCP response buffer and frees the memory. This function uses
 * the message ID from the last request processed by the handler.
 *
 * @param[in] handle MCP transport manager handle
 * @param[in] response_buffer MCP response buffer to destroy (currently unused, kept for API compatibility)
 * @return
 *      - ESP_OK: Response buffer destroyed successfully
 *      - ESP_ERR_INVALID_ARG: Invalid arguments
 *      - ESP_ERR_NOT_FOUND: Message ID not found
 *
 * @note This function uses the msg_id from the last request. For more precise control,
 *       use esp_mcp_mgr_req_destroy_response_by_id() instead.
 */
esp_err_t esp_mcp_mgr_req_destroy_response(esp_mcp_mgr_handle_t handle, uint8_t *response_buffer);

/**
 * @brief Destroy a MCP response buffer by message ID
 *
 * Destroys a MCP response buffer for the specified message ID and frees the memory.
 * This is the recommended way to free response buffers when you know the message ID.
 *
 * @param[in] handle MCP transport manager handle
 * @param[in] id Message ID of the response to destroy
 * @return
 *      - ESP_OK: Response buffer destroyed successfully
 *      - ESP_ERR_INVALID_ARG: Invalid arguments
 *      - ESP_ERR_NOT_FOUND: Message ID not found
 *
 * @note This function should be called after sending the response to prevent memory leaks.
 *       Each handled request may allocate a response buffer that must be freed
 *       using this function or esp_mcp_mgr_req_destroy_response().
 */
esp_err_t esp_mcp_mgr_req_destroy_response_by_id(esp_mcp_mgr_handle_t handle, uint16_t id);

#ifdef __cplusplus
}
#endif
