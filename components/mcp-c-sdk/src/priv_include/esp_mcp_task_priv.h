/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Internal MCP task instance
 */
typedef struct esp_mcp_task_s esp_mcp_task_t;

/**
 * @brief Internal MCP task store
 */
typedef struct esp_mcp_task_store_s esp_mcp_task_store_t;

/**
 * @brief Task execution status values used by MCP task flow
 */
typedef enum {
    ESP_MCP_TASK_WORKING = 0,
    ESP_MCP_TASK_COMPLETED,
    ESP_MCP_TASK_FAILED,
    ESP_MCP_TASK_CANCELLED,
    ESP_MCP_TASK_INPUT_REQUIRED,
    ESP_MCP_TASK_CANCELLING,
} esp_mcp_task_status_t;

/**
 * @brief Task status change callback
 *
 * @param[in] task_id Task ID
 * @param[in] status New status
 * @param[in] status_message Status message (nullable)
 * @param[in] user_ctx User context
 */
typedef void (*esp_mcp_task_status_cb_t)(const char *task_id,
                                         esp_mcp_task_status_t status,
                                         const char *status_message,
                                         const char *origin_session_id,
                                         void *user_ctx);

/**
 * @brief Create task store
 *
 * @return Task store handle, or NULL on failure
 */
esp_mcp_task_store_t *esp_mcp_task_store_create(void);

/**
 * @brief Set task status change callback
 *
 * @param[in] store Task store handle
 * @param[in] cb Callback function
 * @param[in] user_ctx User context passed to callback
 */
void esp_mcp_task_store_set_status_cb(esp_mcp_task_store_t *store,
                                      esp_mcp_task_status_cb_t cb,
                                      void *user_ctx);

/**
 * @brief Destroy task store and all contained tasks
 *
 * @param[in] store Task store handle
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_task_store_destroy(esp_mcp_task_store_t *store);

/** Remove all tasks from the store (used on session re-initialize). */
esp_err_t esp_mcp_task_store_clear(esp_mcp_task_store_t *store);

/**
 * @brief Iterate all tasks in store
 *
 * @param[in] store Task store handle
 * @param[in] callback Callback invoked for each task
 * @param[in] arg User context passed to callback
 * @return ESP_OK on success, or callback error
 */
esp_err_t esp_mcp_task_store_foreach(const esp_mcp_task_store_t *store,
                                     esp_err_t (*callback)(esp_mcp_task_t *task, void *arg),
                                     void *arg);

/**
 * @brief Create a task and insert into store
 *
 * @param[in] store Task store handle
 * @param[in] task_id Unique task id string
 * @return Task handle on success, NULL on failure
 */
esp_mcp_task_t *esp_mcp_task_create(esp_mcp_task_store_t *store, const char *task_id);

/**
 * @brief Find task by id
 *
 * @param[in] store Task store handle
 * @param[in] task_id Task id string
 * @return Task handle if found, otherwise NULL
 */
esp_mcp_task_t *esp_mcp_task_find(const esp_mcp_task_store_t *store, const char *task_id);

/**
 * @brief Mark an asynchronous worker as started for a task
 *
 * @param[in] task Task handle
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_task_mark_worker_starting(esp_mcp_task_t *task);

/**
 * @brief Mark an asynchronous worker as finished for a task
 *
 * @param[in] task Task handle
 */
void esp_mcp_task_mark_worker_finished(esp_mcp_task_t *task);

/**
 * @brief Set task origin session id
 *
 * @param[in] task Task handle
 * @param[in] session_id Origin MCP session id (nullable)
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_task_set_origin_session(esp_mcp_task_t *task, const char *session_id);

/**
 * @brief Get task origin session id
 *
 * @param[in] task Task handle
 * @return Origin session id string or NULL
 */
const char *esp_mcp_task_origin_session(const esp_mcp_task_t *task);

/**
 * @brief Get task id
 *
 * @param[in] task Task handle
 * @return Task id string
 */
const char *esp_mcp_task_id(const esp_mcp_task_t *task);

/**
 * @brief Get current task status
 *
 * @param[in] task Task handle
 * @return Current status value
 */
esp_mcp_task_status_t esp_mcp_task_get_status(const esp_mcp_task_t *task);

/**
 * @brief Get task status message
 *
 * @param[in] task Task handle
 * @return Status message string (nullable)
 */
char *esp_mcp_task_status_message(const esp_mcp_task_t *task);

/**
 * @brief Get task result JSON payload
 *
 * @param[in] task Task handle
 * @return Result JSON object string (nullable)
 */
char *esp_mcp_task_result_json(const esp_mcp_task_t *task);

/**
 * @brief Check whether cancellation was requested
 *
 * @param[in] task Task handle
 * @return true if cancellation requested
 */
bool esp_mcp_task_get_cancel_requested(const esp_mcp_task_t *task);

/**
 * @brief Check cancellation point and finalize status if needed
 *
 * @param[in] task Task handle
 * @param[in] checkpoint Optional checkpoint label for diagnostics
 * @return true if task is cancelled and caller should stop work
 */
bool esp_mcp_task_checkpoint_cancelled(esp_mcp_task_t *task, const char *checkpoint);

/**
 * @brief Set task status and message
 *
 * @param[in] task Task handle
 * @param[in] status Target status
 * @param[in] status_message Optional status message
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_task_set_status(esp_mcp_task_t *task, esp_mcp_task_status_t status, const char *status_message);

/**
 * @brief Set task result JSON
 *
 * @param[in] task Task handle
 * @param[in] result_json JSON object string
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_task_set_result_json(esp_mcp_task_t *task, const char *result_json, bool is_error_response);

/**
 * @brief Check whether stored task payload represents a JSON-RPC error object
 *
 * @param[in] task Task handle
 * @return true when result_json should be wrapped as an error response
 */
bool esp_mcp_task_result_is_error_response(const esp_mcp_task_t *task);

/**
 * @brief Request task cancellation
 *
 * @param[in] task Task handle
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_task_request_cancel(esp_mcp_task_t *task);

/**
 * @brief Set task TTL (time-to-live)
 *
 * @param[in] task Task handle
 * @param[in] ttl_ms TTL in milliseconds, negative to clear
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_task_set_ttl(esp_mcp_task_t *task, int64_t ttl_ms);

/**
 * @brief Get task created timestamp
 *
 * @param[in] task Task handle
 * @return Created timestamp (epoch seconds), or 0 if unavailable
 */
int64_t esp_mcp_task_get_created_at(const esp_mcp_task_t *task);

/**
 * @brief Get task last updated timestamp
 *
 * @param[in] task Task handle
 * @return Last updated timestamp (epoch seconds), or 0 if unavailable
 */
int64_t esp_mcp_task_get_last_updated_at(const esp_mcp_task_t *task);

/**
 * @brief Check if task has TTL set
 *
 * @param[in] task Task handle
 * @return true if TTL is set, false otherwise
 */
bool esp_mcp_task_has_ttl(const esp_mcp_task_t *task);

/**
 * @brief Get task TTL
 *
 * @param[in] task Task handle
 * @return TTL in milliseconds, or 0 if not set
 */
int64_t esp_mcp_task_get_ttl(const esp_mcp_task_t *task);

/**
 * @brief Clean up expired tasks from store
 *
 * Removes tasks that have exceeded their TTL.
 *
 * @param[in] store Task store handle
 * @return Number of tasks removed
 */
size_t esp_mcp_task_store_cleanup_expired(esp_mcp_task_store_t *store);

#ifdef __cplusplus
}
#endif
