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
 * @brief MCP handle type
 *
 * Opaque handle representing an initialized MCP instance.
 * Created by esp_mcp_init() and used for all subsequent MCP operations.
 */
typedef uint32_t esp_mcp_handle_t;

/**
 * @brief MCP transport type enumeration
 *
 * Supported transport types for MCP communication:
 * - Built-in: HTTP transport (enable via menuconfig)
 * - Custom: User-defined transport via callback functions
 */
typedef enum esp_mcp_transport_type_e {
    ESP_MCP_TRANSPORT_TYPE_HTTP = 0,          /*!< Built-in HTTP transport */
    ESP_MCP_TRANSPORT_TYPE_CUSTOM = 0x80      /*!< Custom transport (user-defined) */
} esp_mcp_transport_type_t;

/**
 * @brief MCP transport configuration structure
 *
 * Configuration parameters passed to the transport's open() callback.
 * Used for both built-in and custom transports.
 */
typedef struct esp_mcp_transport_config_s {
    const char *host;               /*!< Host address or name */
    uint16_t port;                  /*!< Port number */
    const char *uri;                /*!< URI path */
    const char *method;             /*!< HTTP method (for HTTP transport) */
    uint16_t timeout_ms;            /*!< Operation timeout (milliseconds) */
} esp_mcp_transport_config_t;

/**
 * @brief Transport callback functions structure
 *
 * Structure containing callback functions for custom transport implementation.
 * Allows supporting any protocol not included in the built-in transport types.
 *
 * @note Usage for custom transport:
 *       1. Set config->type = ESP_MCP_TRANSPORT_TYPE_CUSTOM
 *       2. Implement all four callback functions (none can be NULL)
 *       3. Call esp_mcp_transport_set_funcs() before esp_mcp_start()
 *
 * @note The open() callback can implement either:
 *       - Server mode: listen and accept incoming connections
 *       - Client mode: connect to a remote server
 */
typedef struct {
    uint32_t transport;                                                              /*!< User-defined transport handle/context */
    int (*open)(esp_mcp_handle_t t, esp_mcp_transport_config_t *config);             /*!< Open/Connect callback */
    int (*read)(esp_mcp_handle_t t, char *buffer, int len, int timeout_ms);          /*!< Read callback */
    int (*write)(esp_mcp_handle_t t, const char *buffer, int len, int timeout_ms);   /*!< Write callback */
    int (*close)(esp_mcp_handle_t t);                                                /*!< Close callback */
} esp_mcp_transport_funcs_t;

/**
 * @brief MCP instance configuration structure
 *
 * Configuration parameters for initializing a MCP instance.
 *
 * @note The MCP role (client/server) is determined by the transport's open() implementation:
 *       - Client: open() connects to a remote server
 *       - Server: open() listens and accepts incoming connections
 */
typedef struct esp_mcp_config_s {
    uint16_t    task_priority;      /*!< FreeRTOS task priority for MCP instance */
    uint16_t    stack_size;         /*!< Stack size (bytes) for MCP task */
    int16_t     core_id;            /*!< CPU core ID to run MCP task on (0 or 1) */
    uint32_t    task_caps;          /*!< Memory capabilities for task stack allocation */

    esp_mcp_transport_type_t type;  /*!< Transport type (HTTP or custom) */
    const char *host;               /*!< Transport host address or name */
    const char *uri;                /*!< Transport URI path */
    uint16_t port;                  /*!< Transport port number */
    uint16_t timeout_ms;            /*!< Transport operation timeout (milliseconds) */

    uint16_t mbuf_size;             /*!< Message buffer size (bytes) */

    void *instance;                 /*!< Instance context (e.g., esp_mcp_server_t* for server role) */
} esp_mcp_config_t;

/**
 * @brief Initialize MCP instance
 *
 * Allocates and initializes a MCP instance with the provided configuration.
 * The instance must be initialized before calling esp_mcp_start().
 *
 * @param[in] config Pointer to MCP configuration structure (must not be NULL)
 * @param[out] handle Pointer to store the initialized MCP handle (must not be NULL)
 *
 * @return
 *      - ESP_OK: Instance initialized successfully
 *      - ESP_ERR_INVALID_ARG: Invalid parameter
 *      - ESP_ERR_NO_MEM: Memory allocation failed
 *      - ESP_ERR_NOT_SUPPORTED: Unsupported transport type
 *
 * @note For custom transport, call esp_mcp_transport_set_funcs() after this function
 *       and before esp_mcp_start().
 */
esp_err_t esp_mcp_init(const esp_mcp_config_t* config, esp_mcp_handle_t* handle);

/**
 * @brief Start MCP instance
 *
 * Creates a FreeRTOS task to handle MCP communication. The task continuously
 * processes incoming messages and executes tool calls.
 *
 * @param[in] handle MCP handle from esp_mcp_init()
 *
 * @return
 *      - ESP_OK: Instance started successfully
 *      - ESP_ERR_INVALID_ARG: Invalid handle
 *      - ESP_ERR_INVALID_STATE: Custom transport functions not set (for CUSTOM transport)
 *      - ESP_FAIL: Failed to create FreeRTOS task
 *
 * @note Must be called after esp_mcp_init().
 * @note For custom transport, esp_mcp_transport_set_funcs() must be called first.
 */
esp_err_t esp_mcp_start(esp_mcp_handle_t handle);

/**
 * @brief Stop MCP instance
 *
 * Signals the MCP communication task to exit. The task finishes processing
 * the current message and then terminates.
 *
 * @param[in] handle MCP handle from esp_mcp_init()
 *
 * @return
 *      - ESP_OK: Instance stopped successfully or was not running
 *      - ESP_ERR_INVALID_ARG: Invalid handle
 *
 * @note The instance can be restarted by calling esp_mcp_start() again.
 * @note Resources are freed only when esp_mcp_deinit() is called.
 */
esp_err_t esp_mcp_stop(esp_mcp_handle_t handle);

/**
 * @brief Deinitialize MCP instance
 *
 * Stops the instance (if running) and frees all allocated resources, including
 * the message buffer and instance structure.
 *
 * @param[in] handle MCP handle from esp_mcp_init()
 *
 * @return
 *      - ESP_OK: Instance deinitialized successfully
 *      - ESP_ERR_INVALID_ARG: Invalid handle
 *
 * @note The handle becomes invalid after successful return.
 * @note If running, the instance is stopped automatically before deinitialization.
 */
esp_err_t esp_mcp_deinit(esp_mcp_handle_t handle);

/**
 * @brief Set transport callback functions for custom transport
 *
 * Registers callback functions required for custom transport implementation.
 * Must be called after esp_mcp_init() and before esp_mcp_start() when using
 * ESP_MCP_TRANSPORT_TYPE_CUSTOM.
 *
 * @param[in] handle MCP handle from esp_mcp_init()
 * @param[in] funcs Transport callback functions structure:
 *                  - transport: User-defined transport handle/context
 *                  - open: Open/connect callback (must not be NULL)
 *                  - read: Read data callback (must not be NULL)
 *                  - write: Write data callback (must not be NULL)
 *                  - close: Close callback (must not be NULL)
 *
 * @return
 *      - ESP_OK: Callbacks registered successfully
 *      - ESP_ERR_INVALID_ARG: Invalid handle or any callback is NULL
 */
esp_err_t esp_mcp_transport_set_funcs(esp_mcp_handle_t handle, esp_mcp_transport_funcs_t funcs);

#ifdef __cplusplus
}
#endif
