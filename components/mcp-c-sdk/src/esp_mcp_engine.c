/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>

#include <esp_log.h>
#include <esp_app_desc.h>
#include <esp_check.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <esp_random.h>
#include <cJSON.h>

#include "esp_mcp_mgr.h"
#include "esp_mcp_item.h"
#include "esp_mcp_error.h"
#include "esp_mcp_property_priv.h"
#include "esp_mcp_resource_priv.h"
#include "esp_mcp_tool_priv.h"
#include "esp_mcp_prompt_priv.h"
#include "esp_mcp_completion_priv.h"
#include "esp_mcp_task_priv.h"
#include "esp_mcp_priv.h"

static const char *TAG = "esp_mcp_engine";

#define MCP_PROTO_BASELINE "2024-11-05"
#define MCP_PROTO_LATEST_KNOWN "2025-11-25"
#define MCP_PROTO_DEFAULT "2025-03-26"

static esp_err_t esp_mcp_reply_result(esp_mcp_t *mcp, const cJSON *id, const char *result);
static esp_err_t esp_mcp_reply_invalid_params(esp_mcp_t *mcp,
                                              const cJSON *id,
                                              const char *method,
                                              const char *expected,
                                              const char *param);
static esp_err_t esp_mcp_reply_internal_error_with_method(esp_mcp_t *mcp,
                                                          const cJSON *request_id,
                                                          const char *method,
                                                          const char *reason);
static esp_err_t esp_mcp_emit_log_message(esp_mcp_t *mcp,
                                          const char *level,
                                          const char *jsonrpc_message);

static void esp_mcp_add_rfc3339_time_if_valid(cJSON *obj, const char *key, int64_t epoch_seconds)
{
    if (!obj || !key || epoch_seconds <= 0) {
        return;
    }

    time_t ts = (time_t)epoch_seconds;
    struct tm tm_utc = {0};
    if (!gmtime_r(&ts, &tm_utc)) {
        return;
    }

    char buf[32] = {0};
    if (strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_utc) == 0) {
        return;
    }

    cJSON_AddStringToObject(obj, key, buf);
}

static void esp_mcp_copy_str(char *dst, size_t dst_sz, const char *src)
{
    if (!dst || dst_sz == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, dst_sz, "%s", src);
}

/* resource_sub_mutex: protects subscription array for read snapshots (notifications)
 * and write paths (subscribe / unsubscribe / session clear). See GitLab #2578318. */
static void esp_mcp_resource_sub_lock(esp_mcp_t *mcp)
{
    if (mcp && mcp->resource_sub_mutex) {
        (void)xSemaphoreTake(mcp->resource_sub_mutex, portMAX_DELAY);
    }
}

static void esp_mcp_resource_sub_unlock(esp_mcp_t *mcp)
{
    if (mcp && mcp->resource_sub_mutex) {
        (void)xSemaphoreGive(mcp->resource_sub_mutex);
    }
}

static void esp_mcp_req_id_lock(esp_mcp_t *mcp)
{
    if (mcp && mcp->req_id_mutex) {
        (void)xSemaphoreTake(mcp->req_id_mutex, portMAX_DELAY);
    }
}

static void esp_mcp_req_id_unlock(esp_mcp_t *mcp)
{
    if (mcp && mcp->req_id_mutex) {
        (void)xSemaphoreGive(mcp->req_id_mutex);
    }
}

static esp_err_t esp_mcp_emit_message(esp_mcp_t *mcp, const char *session_id, const char *jsonrpc_message)
{
    if (!mcp || !mcp->mgr_ctx || !jsonrpc_message) {
        return ESP_ERR_INVALID_ARG;
    }
    return esp_mcp_mgr_emit_message((esp_mcp_mgr_handle_t)mcp->mgr_ctx, session_id, jsonrpc_message);
}

static int esp_mcp_log_level_rank(const char *level)
{
    if (!level) {
        return -1;
    }
    if (strcmp(level, "debug") == 0) {
        return 0;
    }
    if (strcmp(level, "info") == 0) {
        return 1;
    }
    if (strcmp(level, "notice") == 0) {
        return 2;
    }
    if (strcmp(level, "warning") == 0 || strcmp(level, "warn") == 0) {
        return 3;
    }
    if (strcmp(level, "error") == 0) {
        return 4;
    }
    if (strcmp(level, "critical") == 0) {
        return 5;
    }
    if (strcmp(level, "alert") == 0) {
        return 6;
    }
    if (strcmp(level, "emergency") == 0) {
        return 7;
    }
    return -1;
}

static const char *esp_mcp_normalize_log_level(const char *level)
{
    int rank = esp_mcp_log_level_rank(level);
    if (rank < 0) {
        return NULL;
    }

    static const char * const s_levels[] = {
        "debug",
        "info",
        "notice",
        "warning",
        "error",
        "critical",
        "alert",
        "emergency"
    };
    return s_levels[rank];
}

static void esp_mcp_handle_elicitation_complete_notification(esp_mcp_t *mcp, const cJSON *params)
{
    if (!mcp || !params || !cJSON_IsObject(params) || !mcp->mgr_ctx) {
        return;
    }

    const cJSON *elicitation_id = cJSON_GetObjectItem((cJSON *)params, "elicitationId");
    if (!(elicitation_id && (cJSON_IsString(elicitation_id) || cJSON_IsNumber(elicitation_id)))) {
        elicitation_id = cJSON_GetObjectItem((cJSON *)params, "requestId");
    }
    if (!(elicitation_id && (cJSON_IsString(elicitation_id) || cJSON_IsNumber(elicitation_id)))) {
        ESP_LOGW(TAG, "notifications/elicitation/complete missing elicitationId/requestId");
        return;
    }

    cJSON *bridged = cJSON_CreateObject();
    if (!bridged) {
        return;
    }

    cJSON *id_dup = cJSON_Duplicate((cJSON *)elicitation_id, 1);
    cJSON *result_dup = cJSON_Duplicate((cJSON *)params, 1);
    if (!id_dup || !result_dup) {
        if (id_dup) {
            cJSON_Delete(id_dup);
        }
        if (result_dup) {
            cJSON_Delete(result_dup);
        }
        cJSON_Delete(bridged);
        return;
    }

    cJSON_AddStringToObject(bridged, "jsonrpc", "2.0");
    cJSON_AddItemToObject(bridged, "id", id_dup);
    cJSON_AddItemToObject(bridged, "result", result_dup);

    char *bridged_json = cJSON_PrintUnformatted(bridged);
    cJSON_Delete(bridged);
    if (!bridged_json) {
        return;
    }

    size_t bridged_len = strlen(bridged_json);
    if (bridged_len > UINT16_MAX) {
        ESP_LOGE(TAG, "Elicitation bridged JSON exceeds %" PRIu16 " bytes", (uint16_t)UINT16_MAX);
        cJSON_free(bridged_json);
        return;
    }

    (void)esp_mcp_mgr_perform_handle((esp_mcp_mgr_handle_t)mcp->mgr_ctx,
                                     (const uint8_t *)bridged_json,
                                     (uint16_t)bridged_len);
    cJSON_free(bridged_json);
}

static void esp_mcp_emit_notification_no_params(esp_mcp_t *mcp, const char *method)
{
    if (!mcp || !method) {
        return;
    }
    cJSON *n = cJSON_CreateObject();
    if (!n) {
        return;
    }
    cJSON_AddStringToObject(n, "jsonrpc", "2.0");
    cJSON_AddStringToObject(n, "method", method);
    char *s = cJSON_PrintUnformatted(n);
    cJSON_Delete(n);
    if (!s) {
        return;
    }
    (void)esp_mcp_emit_message(mcp, NULL, s);
    cJSON_free(s);
}

static void esp_mcp_emit_resource_updated(esp_mcp_t *mcp, const char *uri)
{
    if (!mcp || !uri) {
        return;
    }
    cJSON *n = cJSON_CreateObject();
    cJSON *params = cJSON_CreateObject();
    if (!n || !params) {
        if (n) {
            cJSON_Delete(n);
        }
        if (params) {
            cJSON_Delete(params);
        }
        return;
    }
    cJSON_AddStringToObject(n, "jsonrpc", "2.0");
    cJSON_AddStringToObject(n, "method", "notifications/resources/updated");
    cJSON_AddItemToObject(n, "params", params);
    cJSON_AddStringToObject(params, "uri", uri);
    char *s = cJSON_PrintUnformatted(n);
    cJSON_Delete(n);
    if (!s) {
        return;
    }
    bool matched_any = false;
    bool broadcast_default = false;
    size_t sid_cap = sizeof(mcp->resource_subscriptions) / sizeof(mcp->resource_subscriptions[0]);
    size_t sid_size = sizeof(mcp->resource_subscriptions[0].session_id);
    char *sid_snapshot = calloc(sid_cap, sid_size);
    if (!sid_snapshot) {
        cJSON_free(s);
        return;
    }
    size_t nsid = 0;
    esp_mcp_resource_sub_lock(mcp);
    for (size_t i = 0; i < mcp->resource_subscriptions_count; ++i) {
        if (strcmp(mcp->resource_subscriptions[i].uri, uri) != 0) {
            continue;
        }
        matched_any = true;
        const char *sid = mcp->resource_subscriptions[i].session_id;
        if (!sid[0]) {
            broadcast_default = true;
            continue;
        }
        bool duplicated = false;
        for (size_t j = 0; j < i; ++j) {
            if (strcmp(mcp->resource_subscriptions[j].uri, uri) == 0 &&
                    strcmp(mcp->resource_subscriptions[j].session_id, sid) == 0) {
                duplicated = true;
                break;
            }
        }
        if (!duplicated && nsid < sid_cap) {
            char *snapshot_sid = sid_snapshot + (nsid * sid_size);
            esp_mcp_copy_str(snapshot_sid, sid_size, sid);
            nsid++;
        }
    }
    esp_mcp_resource_sub_unlock(mcp);

    for (size_t k = 0; k < nsid; k++) {
        const char *snapshot_sid = sid_snapshot + (k * sid_size);
        (void)esp_mcp_emit_message(mcp, snapshot_sid, s);
    }
    if (broadcast_default) {
        (void)esp_mcp_emit_message(mcp, NULL, s);
    }
    if (!matched_any) {
        ESP_LOGD(TAG, "Skip resources/updated without subscribers: %s", uri);
    }
    free(sid_snapshot);
    cJSON_free(s);
}

static const char *esp_mcp_task_status_to_str(esp_mcp_task_status_t st);

static bool esp_mcp_task_status_is_terminal(esp_mcp_task_status_t st)
{
    return st == ESP_MCP_TASK_COMPLETED ||
           st == ESP_MCP_TASK_FAILED ||
           st == ESP_MCP_TASK_CANCELLED;
}

static esp_err_t esp_mcp_task_add_json(cJSON *obj, const esp_mcp_task_t *task)
{
    ESP_RETURN_ON_FALSE(obj && task, ESP_ERR_INVALID_ARG, TAG, "Invalid task json args");

    const char *task_id = esp_mcp_task_id(task);
    const char *status = esp_mcp_task_status_to_str(esp_mcp_task_get_status(task));
    char *status_message = esp_mcp_task_status_message(task);

    if (!(task_id && status)) {
        if (status_message) {
            free(status_message);
            status_message = NULL;
        }
        ESP_RETURN_ON_FALSE(false, ESP_ERR_INVALID_ARG, TAG, "Invalid task snapshot");
    }

    cJSON_AddStringToObject(obj, "taskId", task_id);
    cJSON_AddStringToObject(obj, "status", status);
    if (status_message && status_message[0]) {
        cJSON_AddStringToObject(obj, "statusMessage", status_message);
    }
    if (status_message) {
        free(status_message);
        status_message = NULL;
    }

    int64_t created = esp_mcp_task_get_created_at(task);
    int64_t updated = esp_mcp_task_get_last_updated_at(task);
    esp_mcp_add_rfc3339_time_if_valid(obj, "createdAt", created);
    esp_mcp_add_rfc3339_time_if_valid(obj, "lastUpdatedAt", updated);

    if (esp_mcp_task_has_ttl(task)) {
        cJSON_AddNumberToObject(obj, "ttl", esp_mcp_task_get_ttl(task));
    } else {
        cJSON_AddNullToObject(obj, "ttl");
    }
    cJSON_AddNumberToObject(obj, "pollInterval", CONFIG_MCP_TASK_POLL_INTERVAL_MS);

    return ESP_OK;
}

static const char *esp_mcp_tasks_get_param_id(const cJSON *params)
{
    if (!(params && cJSON_IsObject(params))) {
        return NULL;
    }
    cJSON *task_id = cJSON_GetObjectItem((cJSON *)params, "taskId");
    if (task_id && cJSON_IsString(task_id) && task_id->valuestring && task_id->valuestring[0]) {
        return task_id->valuestring;
    }
    // Backward compatibility for a transient non-schema field name.
    cJSON *legacy_id = cJSON_GetObjectItem((cJSON *)params, "id");
    if (legacy_id && cJSON_IsString(legacy_id) &&
            legacy_id->valuestring && legacy_id->valuestring[0]) {
        return legacy_id->valuestring;
    }
    return NULL;
}

static void esp_mcp_task_status_changed_cb(const char *task_id,
                                           esp_mcp_task_status_t status,
                                           const char *status_message,
                                           const char *origin_session_id,
                                           void *user_ctx)
{
    esp_mcp_t *mcp = (esp_mcp_t *)user_ctx;
    if (!mcp || !task_id) {
        return;
    }

    cJSON *n = cJSON_CreateObject();
    cJSON *params = cJSON_CreateObject();
    if (!n || !params) {
        if (n) {
            cJSON_Delete(n);
        }
        if (params) {
            cJSON_Delete(params);
        }
        return;
    }
    cJSON_AddStringToObject(n, "jsonrpc", "2.0");
    cJSON_AddStringToObject(n, "method", "notifications/tasks/status");
    cJSON_AddItemToObject(n, "params", params);

    esp_mcp_task_store_t *store = (esp_mcp_task_store_t *)mcp->tasks;
    esp_mcp_task_t *task = store ? esp_mcp_task_find(store, task_id) : NULL;
    if (task) {
        (void)esp_mcp_task_add_json(params, task);
    } else {
        cJSON_AddStringToObject(params, "taskId", task_id);
        cJSON_AddStringToObject(params, "status", esp_mcp_task_status_to_str(status));
        if (status_message && status_message[0]) {
            cJSON_AddStringToObject(params, "statusMessage", status_message);
        }
        cJSON_AddNullToObject(params, "ttl");
        cJSON_AddNumberToObject(params, "pollInterval", CONFIG_MCP_TASK_POLL_INTERVAL_MS);
    }
    char *s = cJSON_PrintUnformatted(n);
    cJSON_Delete(n);
    if (!s) {
        return;
    }
    if (origin_session_id && origin_session_id[0]) {
        (void)esp_mcp_emit_message(mcp, origin_session_id, s);
    } else {
        ESP_LOGW(TAG, "Skip tasks/status without origin session: %s", task_id);
    }
    cJSON_free(s);
}

esp_err_t esp_mcp_notify_log_message(esp_mcp_t *mcp, const char *level, const char *logger, const char *data_json_object)
{
    ESP_RETURN_ON_FALSE(mcp && level, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    const char *normalized_level = esp_mcp_normalize_log_level(level);
    ESP_RETURN_ON_FALSE(normalized_level, ESP_ERR_INVALID_ARG, TAG, "Invalid log level");

    cJSON *root = cJSON_CreateObject();
    cJSON *params = cJSON_CreateObject();
    if (!root || !params) {
        if (root) {
            cJSON_Delete(root);
        }
        if (params) {
            cJSON_Delete(params);
        }
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddStringToObject(root, "method", "notifications/message");
    cJSON_AddItemToObject(root, "params", params);

    cJSON_AddStringToObject(params, "level", normalized_level);
    if (logger) {
        cJSON_AddStringToObject(params, "logger", logger);
    }
    if (data_json_object) {
        cJSON *data = cJSON_Parse(data_json_object);
        if (data) {
            cJSON_AddItemToObject(params, "data", data);
        } else {
            cJSON_AddStringToObject(params, "data", data_json_object);
        }
    } else {
        cJSON_AddItemToObject(params, "data", cJSON_CreateObject());
    }

    char *s = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    ESP_RETURN_ON_FALSE(s, ESP_ERR_NO_MEM, TAG, "Print failed");
    esp_err_t r = esp_mcp_emit_log_message(mcp, normalized_level, s);
    cJSON_free(s);
    return r;
}

esp_err_t esp_mcp_notify_progress(esp_mcp_t *mcp, const char *progress_token, double progress, double total, const char *message)
{
    ESP_RETURN_ON_FALSE(mcp && progress_token, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    cJSON *root = cJSON_CreateObject();
    cJSON *params = cJSON_CreateObject();
    if (!root || !params) {
        if (root) {
            cJSON_Delete(root);
        }
        if (params) {
            cJSON_Delete(params);
        }
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddStringToObject(root, "method", "notifications/progress");
    cJSON_AddItemToObject(root, "params", params);

    // progressToken can be string or number; use string for simplicity
    cJSON_AddStringToObject(params, "progressToken", progress_token);
    cJSON_AddNumberToObject(params, "progress", progress);
    if (total >= 0) {
        cJSON_AddNumberToObject(params, "total", total);
    }
    if (message) {
        cJSON_AddStringToObject(params, "message", message);
    }

    char *s = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    ESP_RETURN_ON_FALSE(s, ESP_ERR_NO_MEM, TAG, "Print failed");
    const char *target_session = mcp->request_session_id[0] ? mcp->request_session_id : NULL;
    esp_err_t r = esp_mcp_emit_message(mcp, target_session, s);
    cJSON_free(s);
    return r;
}

esp_err_t esp_mcp_set_incoming_request_handler(esp_mcp_t *mcp,
                                               esp_mcp_incoming_request_handler_t handler,
                                               void *user_ctx)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP instance");
    mcp->incoming_request_handler = handler;
    mcp->incoming_request_ctx = user_ctx;
    return ESP_OK;
}

esp_err_t esp_mcp_set_incoming_notification_handler(esp_mcp_t *mcp,
                                                    esp_mcp_incoming_notification_handler_t handler,
                                                    void *user_ctx)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP instance");
    mcp->incoming_notification_handler = handler;
    mcp->incoming_notification_ctx = user_ctx;
    return ESP_OK;
}

static char *esp_mcp_print_json_item(const cJSON *item)
{
    if (!item) {
        return NULL;
    }
    return cJSON_PrintUnformatted((cJSON *)item);
}

static void esp_mcp_dispatch_incoming_notification(esp_mcp_t *mcp,
                                                   const char *method,
                                                   const cJSON *params)
{
    if (!(mcp && mcp->incoming_notification_handler && method)) {
        return;
    }

    char *params_json = esp_mcp_print_json_item(params);
    esp_err_t ret = mcp->incoming_notification_handler(method, params_json, mcp->incoming_notification_ctx);
    if (params_json) {
        cJSON_free(params_json);
    }
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Incoming notification handler failed for %s: %s", method, esp_err_to_name(ret));
    }
}

static esp_err_t esp_mcp_dispatch_incoming_request(esp_mcp_t *mcp,
                                                   const char *method,
                                                   const cJSON *params,
                                                   const cJSON *id)
{
    if (!(mcp && mcp->incoming_request_handler && method && id)) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    char *params_json = esp_mcp_print_json_item(params);
    char *result_json = NULL;
    esp_err_t cb_ret = mcp->incoming_request_handler(method, params_json, &result_json, mcp->incoming_request_ctx);
    if (params_json) {
        cJSON_free(params_json);
    }

    if (cb_ret == ESP_ERR_NOT_SUPPORTED) {
        free(result_json);
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (cb_ret == ESP_OK) {
        const char *payload = result_json ? result_json : "{}";
        esp_err_t reply_ret = esp_mcp_reply_result(mcp, id, payload);
        free(result_json);
        return reply_ret;
    }
    free(result_json);
    if (cb_ret == ESP_ERR_INVALID_ARG) {
        return esp_mcp_reply_invalid_params(mcp, id, method, "callback-specific params", "params");
    }
    return esp_mcp_reply_internal_error_with_method(mcp, id, method, "incoming request callback failed");
}

static uint16_t esp_mcp_mbuf_id_from_json_id(const cJSON *id)
{
    if (!id || !cJSON_IsNumber(id)) {
        return 0;
    }
    double val = id->valuedouble;
    if (val != (double)(int64_t)val) {
        ESP_LOGW(TAG, "Numeric request id is not an integer; mbuf id cleared");
        return 0;
    }
    int64_t iv = (int64_t)val;
    if (iv <= 0 || iv > (int64_t)UINT16_MAX) {
        return 0;
    }
    return (uint16_t)iv;
}

static esp_err_t esp_mcp_request_id_to_str(const cJSON *id, char *out, size_t out_len)
{
    ESP_RETURN_ON_FALSE(id && out && out_len > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    if (cJSON_IsString(id) && id->valuestring) {
        snprintf(out, out_len, "%s", id->valuestring);
        return ESP_OK;
    }
    if (cJSON_IsNumber(id)) {
        double val = id->valuedouble;
        if ((double)((int64_t)val) == val) {
            snprintf(out, out_len, "%" PRId64, (int64_t)val);
        } else {
            snprintf(out, out_len, "%.17g", val);
        }
        return ESP_OK;
    }
    return ESP_ERR_INVALID_ARG;
}

static const char *esp_mcp_request_session_or_default(const esp_mcp_t *mcp)
{
    if (!mcp || !mcp->request_session_id[0]) {
        return "__default";
    }
    return mcp->request_session_id;
}

static void esp_mcp_session_state_reset_caps(esp_mcp_session_state_t *state)
{
    if (!state) {
        return;
    }
    state->client_cap_roots_list = false;
    state->client_cap_sampling_create = false;
    state->client_cap_sampling_context = false;
    state->client_cap_sampling_tools = false;
    state->client_cap_elicitation_form = false;
    state->client_cap_elicitation_url = false;
    state->client_cap_tasks_list = false;
    state->client_cap_tasks_cancel = false;
    state->client_cap_tasks_sampling_create = false;
    state->client_cap_tasks_elicitation_create = false;
}

static void esp_mcp_session_state_reset_lifecycle(esp_mcp_session_state_t *state)
{
    if (!state) {
        return;
    }
    state->received_initialize_request = false;
    state->require_initialized_notification = false;
    state->received_initialized_notification = false;
    state->has_log_level = false;
    state->log_level[0] = '\0';
    esp_mcp_session_state_reset_caps(state);
}

static esp_mcp_session_state_t *esp_mcp_session_state_find(esp_mcp_t *mcp, const char *session_id)
{
    if (!(mcp && session_id && session_id[0])) {
        return NULL;
    }
    for (size_t i = 0; i < ESP_MCP_MAX_SESSION_STATES; ++i) {
        if (mcp->session_states[i].used &&
                strcmp(mcp->session_states[i].session_id, session_id) == 0) {
            return &mcp->session_states[i];
        }
    }
    return NULL;
}

static const esp_mcp_session_state_t *esp_mcp_session_state_find_const(const esp_mcp_t *mcp,
                                                                       const char *session_id)
{
    if (!(mcp && session_id && session_id[0])) {
        return NULL;
    }
    for (size_t i = 0; i < ESP_MCP_MAX_SESSION_STATES; ++i) {
        if (mcp->session_states[i].used &&
                strcmp(mcp->session_states[i].session_id, session_id) == 0) {
            return &mcp->session_states[i];
        }
    }
    return NULL;
}

static esp_mcp_session_state_t *esp_mcp_get_session_state(esp_mcp_t *mcp,
                                                          const char *session_id,
                                                          bool create)
{
    ESP_RETURN_ON_FALSE(mcp, NULL, TAG, "Invalid server");

    if (!(session_id && session_id[0])) {
        return &mcp->default_session_state;
    }

    esp_mcp_session_state_t *state = esp_mcp_session_state_find(mcp, session_id);
    if (state || !create) {
        return state;
    }

    for (size_t i = 0; i < ESP_MCP_MAX_SESSION_STATES; ++i) {
        if (mcp->session_states[i].used) {
            continue;
        }
        memset(&mcp->session_states[i], 0, sizeof(mcp->session_states[i]));
        mcp->session_states[i].used = true;
        snprintf(mcp->session_states[i].session_id,
                 sizeof(mcp->session_states[i].session_id),
                 "%s",
                 session_id);
        return &mcp->session_states[i];
    }
    return NULL;
}

static const esp_mcp_session_state_t *esp_mcp_get_session_state_const(const esp_mcp_t *mcp,
                                                                      const char *session_id)
{
    if (!mcp) {
        return NULL;
    }
    if (!(session_id && session_id[0])) {
        return &mcp->default_session_state;
    }
    return esp_mcp_session_state_find_const(mcp, session_id);
}

static esp_mcp_session_state_t *esp_mcp_get_request_session_state(esp_mcp_t *mcp, bool create)
{
    return esp_mcp_get_session_state(mcp,
                                     mcp && mcp->request_session_id[0] ? mcp->request_session_id : NULL,
                                     create);
}

static const esp_mcp_session_state_t *esp_mcp_get_request_session_state_const(const esp_mcp_t *mcp)
{
    return esp_mcp_get_session_state_const(mcp,
                                           mcp && mcp->request_session_id[0] ? mcp->request_session_id : NULL);
}

static bool esp_mcp_session_allows_log_level(const esp_mcp_session_state_t *state, const char *level)
{
    if (!level) {
        return false;
    }
    if (!(state && state->has_log_level)) {
        return true;
    }

    int message_rank = esp_mcp_log_level_rank(level);
    int min_rank = esp_mcp_log_level_rank(state->log_level);
    if (message_rank < 0 || min_rank < 0) {
        return false;
    }
    return message_rank >= min_rank;
}

static esp_err_t esp_mcp_emit_log_message(esp_mcp_t *mcp,
                                          const char *level,
                                          const char *jsonrpc_message)
{
    ESP_RETURN_ON_FALSE(mcp && level && jsonrpc_message, ESP_ERR_INVALID_ARG, TAG, "Invalid args");

    if (mcp->request_session_id[0]) {
        const esp_mcp_session_state_t *state =
            esp_mcp_get_session_state_const(mcp, mcp->request_session_id);
        if (!esp_mcp_session_allows_log_level(state, level)) {
            return ESP_OK;
        }
        return esp_mcp_emit_message(mcp, mcp->request_session_id, jsonrpc_message);
    }

    bool has_named_sessions = false;
    esp_err_t last_ret = ESP_OK;
    for (size_t i = 0; i < ESP_MCP_MAX_SESSION_STATES; ++i) {
        const esp_mcp_session_state_t *state = &mcp->session_states[i];
        if (!(state->used && state->received_initialize_request)) {
            continue;
        }
        has_named_sessions = true;
        if (!esp_mcp_session_allows_log_level(state, level)) {
            continue;
        }
        esp_err_t emit_ret = esp_mcp_emit_message(mcp, state->session_id, jsonrpc_message);
        if (emit_ret != ESP_OK) {
            last_ret = emit_ret;
        }
    }

    if (has_named_sessions) {
        return last_ret;
    }

    if (!esp_mcp_session_allows_log_level(&mcp->default_session_state, level)) {
        return ESP_OK;
    }
    return esp_mcp_emit_message(mcp, NULL, jsonrpc_message);
}

static bool esp_mcp_task_visible_to_request_session(const esp_mcp_t *mcp, const esp_mcp_task_t *task)
{
    if (!task) {
        return false;
    }
    const char *origin_session_id = esp_mcp_task_origin_session(task);
    if (!origin_session_id || !origin_session_id[0]) {
        return true;
    }
    if (!mcp || !mcp->request_session_id[0]) {
        return false;
    }
    return strcmp(origin_session_id, mcp->request_session_id) == 0;
}

static bool esp_mcp_is_duplicate_request_id(esp_mcp_t *mcp, const char *session_id, const char *request_id)
{
    if (!mcp || !session_id || !request_id) {
        return false;
    }
    bool found = false;
    esp_mcp_req_id_lock(mcp);
    for (size_t i = 0; i < (sizeof(mcp->recent_request_ids) / sizeof(mcp->recent_request_ids[0])); ++i) {
        if (!mcp->recent_request_ids[i].used) {
            continue;
        }
        if (strcmp(mcp->recent_request_ids[i].session_id, session_id) == 0 &&
                strcmp(mcp->recent_request_ids[i].request_id, request_id) == 0) {
            found = true;
            break;
        }
    }
    esp_mcp_req_id_unlock(mcp);
    return found;
}

static void esp_mcp_remember_request_id(esp_mcp_t *mcp, const char *session_id, const char *request_id)
{
    if (!mcp || !session_id || !request_id) {
        return;
    }
    size_t cap = sizeof(mcp->recent_request_ids) / sizeof(mcp->recent_request_ids[0]);
    esp_mcp_req_id_lock(mcp);
    size_t idx = mcp->recent_request_id_cursor % cap;
    mcp->recent_request_id_cursor = (idx + 1) % cap;
    mcp->recent_request_ids[idx].used = true;
    snprintf(mcp->recent_request_ids[idx].session_id, sizeof(mcp->recent_request_ids[idx].session_id), "%s", session_id);
    snprintf(mcp->recent_request_ids[idx].request_id, sizeof(mcp->recent_request_ids[idx].request_id), "%s", request_id);
    esp_mcp_req_id_unlock(mcp);
}

static esp_err_t esp_mcp_add_mbuf(esp_mcp_t *mcp, uint16_t mbuf_id, uint8_t *outbuf, uint16_t outlen)
{
    if (mcp->mbuf.outbuf) {
        cJSON_free(mcp->mbuf.outbuf);
        mcp->mbuf.outbuf = NULL;
        mcp->mbuf.outlen = 0;
        mcp->mbuf.id = 0;
    }

    mcp->mbuf.id = mbuf_id;
    mcp->mbuf.outbuf = outbuf;
    mcp->mbuf.outlen = outlen;

    return ESP_OK;
}

static esp_err_t esp_mcp_reply_result(esp_mcp_t *mcp, const cJSON *id, const char *result)
{
    ESP_RETURN_ON_FALSE(mcp && result, ESP_ERR_INVALID_ARG, TAG, "Invalid server or result");

    cJSON *payload = cJSON_CreateObject();
    if (!payload) {
        ESP_LOGE(TAG, "Failed to allocate memory for payload");
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(payload, "jsonrpc", "2.0");
    if (!(id && (cJSON_IsNumber(id) || cJSON_IsString(id)))) {
        cJSON_Delete(payload);
        return ESP_ERR_INVALID_ARG;
    }
    cJSON *id_dup = cJSON_Duplicate(id, 1);
    if (!id_dup) {
        ESP_LOGE(TAG, "Failed to duplicate request id");
        cJSON_Delete(payload);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddItemToObject(payload, "id", id_dup);

    cJSON *result_obj = cJSON_Parse(result);
    if (result_obj) {
        if ((mcp->has_related_task || mcp->request_meta_json) && cJSON_IsObject(result_obj)) {
            cJSON *meta = cJSON_CreateObject();
            if (meta) {
                // Add related-task if present
                if (mcp->has_related_task) {
                    cJSON *rt = cJSON_CreateObject();
                    if (rt) {
                        cJSON_AddStringToObject(rt, "taskId", mcp->related_task_id);
                        cJSON_AddItemToObject(meta, "io.modelcontextprotocol/related-task", rt);
                    }
                }
                // Add any other _meta fields from the request
                if (mcp->request_meta_json) {
                    cJSON *other_meta = cJSON_Parse(mcp->request_meta_json);
                    if (other_meta) {
                        cJSON *item = NULL;
                        cJSON_ArrayForEach(item, other_meta) {
                            cJSON_AddItemToObject(meta, item->string, cJSON_Duplicate(item, 1));
                        }
                        cJSON_Delete(other_meta);
                    }
                }
                cJSON_AddItemToObject(result_obj, "_meta", meta);
            }
        }
        cJSON_AddItemToObject(payload, "result", result_obj);
    } else {
        cJSON_AddStringToObject(payload, "result", result);
    }

    char *payload_str = cJSON_PrintUnformatted(payload);
    if (!payload_str) {
        ESP_LOGE(TAG, "Failed to print result payload");
        cJSON_Delete(payload);
        return ESP_ERR_NO_MEM;
    }
    size_t payload_len = strlen(payload_str);
    if (payload_len > UINT16_MAX) {
        ESP_LOGE(TAG, "Result payload too large: %u bytes", (unsigned)payload_len);
        cJSON_free(payload_str);
        cJSON_Delete(payload);
        return ESP_ERR_INVALID_SIZE;
    }
    esp_mcp_add_mbuf(mcp, esp_mcp_mbuf_id_from_json_id(id), (uint8_t *)payload_str, (uint16_t)payload_len);

    cJSON_Delete(payload);

    return ESP_OK;
}

static esp_err_t esp_mcp_reply_result_with_related_task(esp_mcp_t *mcp,
                                                        const cJSON *id,
                                                        const char *result,
                                                        const char *task_id)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid server");

    bool saved_has_related_task = mcp->has_related_task;
    char saved_related_task_id[sizeof(mcp->related_task_id)] = {0};
    if (saved_has_related_task) {
        strncpy(saved_related_task_id, mcp->related_task_id, sizeof(saved_related_task_id) - 1);
    }

    if (task_id && task_id[0]) {
        mcp->has_related_task = true;
        strncpy(mcp->related_task_id, task_id, sizeof(mcp->related_task_id) - 1);
        mcp->related_task_id[sizeof(mcp->related_task_id) - 1] = '\0';
    }

    esp_err_t ret = esp_mcp_reply_result(mcp, id, result);

    mcp->has_related_task = saved_has_related_task;
    if (saved_has_related_task) {
        strncpy(mcp->related_task_id, saved_related_task_id, sizeof(mcp->related_task_id) - 1);
        mcp->related_task_id[sizeof(mcp->related_task_id) - 1] = '\0';
    } else {
        mcp->related_task_id[0] = '\0';
    }

    return ret;
}

static esp_err_t esp_mcp_reply_error_ex(esp_mcp_t *mcp,
                                        int error_code,
                                        const char *message,
                                        const cJSON *request_id,
                                        const cJSON *error_data)
{
    ESP_RETURN_ON_FALSE(mcp && message, ESP_ERR_INVALID_ARG, TAG, "Invalid server or message");

    cJSON *payload = cJSON_CreateObject();
    if (!payload) {
        ESP_LOGE(TAG, "Failed to allocate memory for payload");
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(payload, "jsonrpc", "2.0");

    // Only omit/set null id when it is unknown/unparsable.
    if (request_id && (cJSON_IsNumber(request_id) || cJSON_IsString(request_id))) {
        cJSON_AddItemToObject(payload, "id", cJSON_Duplicate(request_id, 1));
    } else {
        cJSON_AddNullToObject(payload, "id");
    }

    cJSON *error = cJSON_Parse(message);
    if (!error) {
        error = cJSON_CreateObject();
        if (!error) {
            ESP_LOGE(TAG, "Failed to allocate memory for error");
            cJSON_Delete(payload);
            return ESP_ERR_NO_MEM;
        }
        cJSON_AddNumberToObject(error, "code", error_code);
        cJSON_AddStringToObject(error, "message", message);
    }
    if (error_data && cJSON_IsObject(error_data)) {
        cJSON_AddItemToObject(error, "data", cJSON_Duplicate(error_data, 1));
    }
    cJSON_AddItemToObject(payload, "error", error);

    char *payload_str = cJSON_PrintUnformatted(payload);
    if (payload_str) {
        size_t payload_len = strlen(payload_str);
        if (payload_len > UINT16_MAX) {
            ESP_LOGE(TAG, "Error payload too large: %u bytes", (unsigned)payload_len);
            cJSON_free(payload_str);
            cJSON_Delete(payload);
            return ESP_ERR_INVALID_SIZE;
        }
        esp_mcp_add_mbuf(mcp, esp_mcp_mbuf_id_from_json_id(request_id), (uint8_t *)payload_str, (uint16_t)payload_len);
    }
    cJSON_Delete(payload);

    return ESP_OK;
}

static esp_err_t esp_mcp_reply_error_with_kv(esp_mcp_t *mcp,
                                             int error_code,
                                             const char *message,
                                             const cJSON *request_id,
                                             const char *key,
                                             const char *value)
{
    cJSON *data = cJSON_CreateObject();
    if (data && key && value) {
        cJSON_AddStringToObject(data, key, value);
    }
    esp_err_t ret = esp_mcp_reply_error_ex(mcp, error_code, message, request_id, data);
    if (data) {
        cJSON_Delete(data);
    }
    return ret;
}

static esp_err_t esp_mcp_reply_invalid_params(esp_mcp_t *mcp,
                                              const cJSON *request_id,
                                              const char *method,
                                              const char *expected,
                                              const char *field)
{
    cJSON *data = cJSON_CreateObject();
    if (data) {
        if (method) {
            cJSON_AddStringToObject(data, "method", method);
        }
        if (expected) {
            cJSON_AddStringToObject(data, "expected", expected);
        }
        if (field) {
            cJSON_AddStringToObject(data, "field", field);
        }
    }
    esp_err_t ret = esp_mcp_reply_error_ex(mcp, MCP_ERROR_CODE_INVALID_PARAMS, MCP_ERROR_MESSAGE_INVALID_PARAMS, request_id, data);
    if (data) {
        cJSON_Delete(data);
    }
    return ret;
}

static esp_err_t esp_mcp_reply_invalid_params_and_fail(esp_mcp_t *mcp,
                                                       const cJSON *request_id,
                                                       const char *method,
                                                       const char *expected,
                                                       const char *field)
{
    esp_err_t ret = esp_mcp_reply_invalid_params(mcp, request_id, method, expected, field);
    return ret == ESP_OK ? ESP_ERR_INVALID_ARG : ret;
}

static esp_err_t esp_mcp_reply_not_found(esp_mcp_t *mcp,
                                         const cJSON *request_id,
                                         const char *method,
                                         const char *key,
                                         const char *value,
                                         const char *message)
{
    cJSON *data = cJSON_CreateObject();
    if (data) {
        if (method) {
            cJSON_AddStringToObject(data, "method", method);
        }
        if (key && value) {
            cJSON_AddStringToObject(data, key, value);
        }
    }
    esp_err_t ret = esp_mcp_reply_error_ex(mcp, -32002, message ? message : "Not found", request_id, data);
    if (data) {
        cJSON_Delete(data);
    }
    return ret;
}

static esp_err_t esp_mcp_reply_invalid_request_with_reason(esp_mcp_t *mcp,
                                                           const cJSON *request_id,
                                                           const char *method,
                                                           const char *reason)
{
    cJSON *data = cJSON_CreateObject();
    if (data) {
        if (method) {
            cJSON_AddStringToObject(data, "method", method);
        }
        if (reason) {
            cJSON_AddStringToObject(data, "reason", reason);
        }
    }
    esp_err_t ret = esp_mcp_reply_error_ex(mcp, MCP_ERROR_CODE_INVALID_REQUEST, MCP_ERROR_MESSAGE_INVALID_REQUEST, request_id, data);
    if (data) {
        cJSON_Delete(data);
    }
    return ret;
}

static esp_err_t esp_mcp_reply_internal_error_with_method(esp_mcp_t *mcp,
                                                          const cJSON *request_id,
                                                          const char *method,
                                                          const char *reason)
{
    cJSON *data = cJSON_CreateObject();
    if (data) {
        if (method) {
            cJSON_AddStringToObject(data, "method", method);
        }
        if (reason) {
            cJSON_AddStringToObject(data, "reason", reason);
        }
    }
    esp_err_t ret = esp_mcp_reply_error_ex(mcp, MCP_ERROR_CODE_INTERNAL_ERROR, MCP_ERROR_MESSAGE_INTERNAL_ERROR, request_id, data);
    if (data) {
        cJSON_Delete(data);
    }
    return ret;
}

static esp_err_t esp_mcp_reply_error_with_method_reason(esp_mcp_t *mcp,
                                                        int error_code,
                                                        const char *message,
                                                        const cJSON *request_id,
                                                        const char *method,
                                                        const char *reason)
{
    cJSON *data = cJSON_CreateObject();
    if (data) {
        if (method) {
            cJSON_AddStringToObject(data, "method", method);
        }
        if (reason) {
            cJSON_AddStringToObject(data, "reason", reason);
        }
    }
    esp_err_t ret = esp_mcp_reply_error_ex(mcp, error_code, message, request_id, data);
    if (data) {
        cJSON_Delete(data);
    }
    return ret;
}

static esp_err_t esp_mcp_parse_capabilities(esp_mcp_session_state_t *state, const cJSON *capabilities)
{
    ESP_RETURN_ON_FALSE(state, ESP_ERR_INVALID_ARG, TAG, "Invalid state");
    ESP_RETURN_ON_FALSE(capabilities, ESP_ERR_INVALID_ARG, TAG, "Invalid capabilities");

    cJSON *vision = cJSON_GetObjectItem(capabilities, "vision");
    if (cJSON_IsObject(vision)) {
        cJSON *url = cJSON_GetObjectItem(vision, "url");
        cJSON *token = cJSON_GetObjectItem(vision, "token");
        if (cJSON_IsString(url)) {
            ESP_LOGI(TAG, "Vision URL: %s", url->valuestring);
        }

        if (cJSON_IsString(token)) {
            const char *token_str = token->valuestring;
            int token_len = strlen(token_str);
            if (token_len > 8) {
                char masked_token[16] = {0};
                snprintf(masked_token, sizeof(masked_token), "%.*s***%.*s",
                         4, token_str, 4, token_str + token_len - 4);
                ESP_LOGI(TAG, "Vision Token: %s", masked_token);
            } else {
                ESP_LOGI(TAG, "Vision Token: ***");
            }
        }
    }

    // Strict gate for server-initiated requests: only emit if client declared capability.
    cJSON *roots = cJSON_GetObjectItem(capabilities, "roots");
    cJSON *sampling = cJSON_GetObjectItem(capabilities, "sampling");
    cJSON *elicitation = cJSON_GetObjectItem(capabilities, "elicitation");
    cJSON *tasks = cJSON_GetObjectItem(capabilities, "tasks");
    state->client_cap_roots_list = cJSON_IsObject(roots) || cJSON_IsTrue(roots);
    state->client_cap_sampling_create = cJSON_IsObject(sampling) || cJSON_IsTrue(sampling);
    state->client_cap_sampling_context = false;
    state->client_cap_sampling_tools = false;
    if (cJSON_IsObject(sampling)) {
        cJSON *context = cJSON_GetObjectItem(sampling, "context");
        cJSON *tools = cJSON_GetObjectItem(sampling, "tools");
        state->client_cap_sampling_context = cJSON_IsObject(context) || cJSON_IsTrue(context);
        state->client_cap_sampling_tools = cJSON_IsObject(tools) || cJSON_IsTrue(tools);
    }

    state->client_cap_elicitation_form = false;
    state->client_cap_elicitation_url = false;
    state->client_cap_tasks_list = false;
    state->client_cap_tasks_cancel = false;
    state->client_cap_tasks_sampling_create = false;
    state->client_cap_tasks_elicitation_create = false;
    if (cJSON_IsTrue(elicitation)) {
        state->client_cap_elicitation_form = true;
    } else if (cJSON_IsObject(elicitation)) {
        cJSON *form = cJSON_GetObjectItem(elicitation, "form");
        cJSON *url = cJSON_GetObjectItem(elicitation, "url");
        if (!form && !url) {
            // Per 2025-11-25, {"elicitation": {}} is equivalent to {"form": {}}.
            state->client_cap_elicitation_form = true;
        } else {
            state->client_cap_elicitation_form = cJSON_IsObject(form) || cJSON_IsTrue(form);
            state->client_cap_elicitation_url = cJSON_IsObject(url) || cJSON_IsTrue(url);
        }
    }

    if (cJSON_IsTrue(tasks)) {
        state->client_cap_tasks_list = true;
        state->client_cap_tasks_cancel = true;
        state->client_cap_tasks_sampling_create = true;
        state->client_cap_tasks_elicitation_create = true;
    } else if (cJSON_IsObject(tasks)) {
        cJSON *supported = cJSON_GetObjectItem(tasks, "supported");
        if (supported && cJSON_IsBool(supported)) {
            bool enabled = cJSON_IsTrue(supported);
            state->client_cap_tasks_list = enabled;
            state->client_cap_tasks_cancel = enabled;
            state->client_cap_tasks_sampling_create = enabled;
            state->client_cap_tasks_elicitation_create = enabled;
        } else {
            cJSON *list = cJSON_GetObjectItem(tasks, "list");
            cJSON *cancel = cJSON_GetObjectItem(tasks, "cancel");
            cJSON *requests = cJSON_GetObjectItem(tasks, "requests");
            cJSON *task_sampling = requests ? cJSON_GetObjectItem(requests, "sampling") : NULL;
            cJSON *task_elicitation = requests ? cJSON_GetObjectItem(requests, "elicitation") : NULL;
            cJSON *create_message = task_sampling ? cJSON_GetObjectItem(task_sampling, "createMessage") : NULL;
            cJSON *create = task_elicitation ? cJSON_GetObjectItem(task_elicitation, "create") : NULL;

            state->client_cap_tasks_list = cJSON_IsObject(list) || cJSON_IsTrue(list);
            state->client_cap_tasks_cancel = cJSON_IsObject(cancel) || cJSON_IsTrue(cancel);
            state->client_cap_tasks_sampling_create = cJSON_IsObject(create_message) || cJSON_IsTrue(create_message);
            state->client_cap_tasks_elicitation_create = cJSON_IsObject(create) || cJSON_IsTrue(create);
        }
    }

    return ESP_OK;
}

static bool esp_mcp_is_protocol_version_date(const char *version)
{
    if (!version || strlen(version) != 10) {
        return false;
    }
    // Expected format: YYYY-MM-DD
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) {
            if (version[i] != '-') {
                return false;
            }
            continue;
        }
        if (version[i] < '0' || version[i] > '9') {
            return false;
        }
    }
    return true;
}

static const char * const s_supported_protocol_versions[] = {
    MCP_PROTO_BASELINE,
    MCP_PROTO_DEFAULT,
    "2025-06-18",
    MCP_PROTO_LATEST_KNOWN,
    NULL
};

static bool esp_mcp_is_known_protocol_version(const char *version)
{
    if (!version) {
        return false;
    }
    for (int i = 0; s_supported_protocol_versions[i]; i++) {
        if (strcmp(version, s_supported_protocol_versions[i]) == 0) {
            return true;
        }
    }
    return false;
}

static esp_err_t esp_mcp_validate_initialize_params(esp_mcp_t *mcp,
                                                    const cJSON *params,
                                                    const cJSON *id,
                                                    const char **out_protocol_version)
{
    if (!(params && cJSON_IsObject(params))) {
        (void)esp_mcp_reply_invalid_params(mcp,
                                           id,
                                           "initialize",
                                           "params.protocolVersion+params.capabilities+params.clientInfo",
                                           "params");
        return ESP_ERR_INVALID_ARG;
    }

    const cJSON *protocol_version = cJSON_GetObjectItem((cJSON *)params, "protocolVersion");
    if (!(protocol_version && cJSON_IsString(protocol_version) &&
            protocol_version->valuestring && protocol_version->valuestring[0])) {
        (void)esp_mcp_reply_invalid_params(mcp, id, "initialize", "params.protocolVersion", "params.protocolVersion");
        return ESP_ERR_INVALID_ARG;
    }
    if (!esp_mcp_is_protocol_version_date(protocol_version->valuestring)) {
        (void)esp_mcp_reply_invalid_params(mcp, id, "initialize", "YYYY-MM-DD", "params.protocolVersion");
        return ESP_ERR_INVALID_ARG;
    }
    if (!esp_mcp_is_known_protocol_version(protocol_version->valuestring)) {
        (void)esp_mcp_reply_invalid_params(mcp,
                                           id,
                                           "initialize",
                                           "supported params.protocolVersion",
                                           "params.protocolVersion");
        return ESP_ERR_INVALID_ARG;
    }

    const cJSON *capabilities = cJSON_GetObjectItem((cJSON *)params, "capabilities");
    if (!(capabilities && cJSON_IsObject(capabilities))) {
        (void)esp_mcp_reply_invalid_params(mcp, id, "initialize", "params.capabilities", "params.capabilities");
        return ESP_ERR_INVALID_ARG;
    }

    const cJSON *client_info = cJSON_GetObjectItem((cJSON *)params, "clientInfo");
    if (!(client_info && cJSON_IsObject(client_info))) {
        (void)esp_mcp_reply_invalid_params(mcp, id, "initialize", "params.clientInfo", "params.clientInfo");
        return ESP_ERR_INVALID_ARG;
    }

    const cJSON *client_name = cJSON_GetObjectItem((cJSON *)client_info, "name");
    if (!(client_name && cJSON_IsString(client_name) &&
            client_name->valuestring && client_name->valuestring[0])) {
        (void)esp_mcp_reply_invalid_params(mcp,
                                           id,
                                           "initialize",
                                           "params.clientInfo.name",
                                           "params.clientInfo.name");
        return ESP_ERR_INVALID_ARG;
    }

    const cJSON *client_version = cJSON_GetObjectItem((cJSON *)client_info, "version");
    if (!(client_version && cJSON_IsString(client_version) &&
            client_version->valuestring && client_version->valuestring[0])) {
        (void)esp_mcp_reply_invalid_params(mcp,
                                           id,
                                           "initialize",
                                           "params.clientInfo.version",
                                           "params.clientInfo.version");
        return ESP_ERR_INVALID_ARG;
    }

    if (out_protocol_version) {
        *out_protocol_version = protocol_version->valuestring;
    }
    return ESP_OK;
}

static const char *esp_mcp_negotiate_protocol_version(const char *requested_version)
{
    if (!requested_version || requested_version[0] == '\0') {
        return MCP_PROTO_DEFAULT;
    }

    if (esp_mcp_is_known_protocol_version(requested_version)) {
        return requested_version;
    }
    return NULL;
}

static bool esp_mcp_protocol_requires_initialized(const char *version)
{
    if (!version || !esp_mcp_is_protocol_version_date(version)) {
        return true;
    }
    // MCP protocol versions are date-formatted; lexical compare matches chronological order.
    return strcmp(version, MCP_PROTO_BASELINE) > 0;
}

static esp_err_t esp_mcp_handle_initialize(esp_mcp_t *mcp, const cJSON *params, const cJSON *id)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid server");

    esp_mcp_session_state_t *session_state = esp_mcp_get_request_session_state(mcp, true);
    if (!session_state) {
        return esp_mcp_reply_internal_error_with_method(mcp, id, "initialize", "session state alloc failed");
    }

    if (session_state->received_initialize_request) {
        // Some clients may re-send initialize on reconnect/session reset.
        // Treat it as session re-initialization for compatibility.
        ESP_LOGW(TAG, "Re-initialize received; resetting MCP session state");
        if (mcp->tasks) {
            (void)esp_mcp_task_store_clear((esp_mcp_task_store_t *)mcp->tasks);
        }
        esp_mcp_resource_sub_lock(mcp);
        mcp->resource_subscriptions_count = 0;
        memset(mcp->resource_subscriptions, 0, sizeof(mcp->resource_subscriptions));
        esp_mcp_resource_sub_unlock(mcp);
        esp_mcp_req_id_lock(mcp);
        memset(mcp->recent_request_ids, 0, sizeof(mcp->recent_request_ids));
        mcp->recent_request_id_cursor = 0;
        esp_mcp_req_id_unlock(mcp);
        session_state->require_initialized_notification = false;
        session_state->received_initialized_notification = false;
        session_state->received_initialize_request = false;
    }

    mcp->client_initialized = false;

    const char *requested_protocol_version = NULL;
    esp_err_t validate_ret = esp_mcp_validate_initialize_params(mcp, params, id, &requested_protocol_version);
    if (validate_ret != ESP_OK) {
        return validate_ret;
    }

    const char *negotiated_protocol_version = esp_mcp_negotiate_protocol_version(requested_protocol_version);
    if (!negotiated_protocol_version) {
        return esp_mcp_reply_invalid_params(mcp,
                                            id,
                                            "initialize",
                                            "supported params.protocolVersion",
                                            "params.protocolVersion");
    }

    esp_mcp_session_state_reset_lifecycle(session_state);
    if (strcmp(negotiated_protocol_version, MCP_PROTO_LATEST_KNOWN) > 0) {
        ESP_LOGW(TAG,
                 "Client requested newer protocol %s; using compatibility behavior for %s",
                 negotiated_protocol_version,
                 MCP_PROTO_LATEST_KNOWN);
    }
    session_state->require_initialized_notification =
        esp_mcp_protocol_requires_initialized(negotiated_protocol_version);
    session_state->received_initialized_notification = false;

    cJSON *capabilities_param = cJSON_GetObjectItem(params, "capabilities");
    if (capabilities_param && cJSON_IsObject(capabilities_param)) {
        esp_mcp_parse_capabilities(session_state, capabilities_param);
    }

    const esp_app_desc_t *app_desc = esp_app_get_description();
    cJSON *message = cJSON_CreateObject();
    if (!message) {
        ESP_LOGE(TAG, "Failed to allocate memory for message");
        return ESP_ERR_NO_MEM;
    }

    cJSON *tools = cJSON_CreateObject();
    cJSON *experimental = cJSON_CreateObject();
    cJSON *resources = cJSON_CreateObject();
    cJSON *prompts = cJSON_CreateObject();
    cJSON *logging = cJSON_CreateObject();
    cJSON *server_info = cJSON_CreateObject();
    cJSON *capabilities = cJSON_CreateObject();
    cJSON *completions = NULL;
    if (mcp->completion) {
        completions = cJSON_CreateObject();
    }

    if (!tools || !experimental || !resources || !prompts || !logging || !server_info || !capabilities ||
            (mcp->completion && !completions)) {
        ESP_LOGE(TAG, "Failed to allocate memory for message components");
        if (tools) {
            cJSON_Delete(tools);
        }

        if (experimental) {
            cJSON_Delete(experimental);
        }
        if (resources) {
            cJSON_Delete(resources);
        }
        if (prompts) {
            cJSON_Delete(prompts);
        }
        cJSON_Delete(completions);
        if (logging) {
            cJSON_Delete(logging);
        }

        if (server_info) {
            cJSON_Delete(server_info);
        }

        if (capabilities) {
            cJSON_Delete(capabilities);
        }

        cJSON_Delete(message);
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddBoolToObject(tools, "listChanged", true);
    cJSON_AddBoolToObject(resources, "subscribe", true);
    cJSON_AddBoolToObject(resources, "listChanged", true);
    cJSON_AddBoolToObject(prompts, "listChanged", true);
    cJSON *tasks_cap = cJSON_CreateObject();
    if (tasks_cap) {
        cJSON_AddItemToObject(tasks_cap, "list", cJSON_CreateObject());
        cJSON_AddItemToObject(tasks_cap, "cancel", cJSON_CreateObject());
        cJSON *task_requests = cJSON_CreateObject();
        if (task_requests) {
            cJSON *tools_requests = cJSON_CreateObject();
            if (tools_requests) {
                cJSON_AddItemToObject(tools_requests, "call", cJSON_CreateObject());
                cJSON_AddItemToObject(task_requests, "tools", tools_requests);
            }
            cJSON_AddItemToObject(tasks_cap, "requests", task_requests);
        }
        cJSON_AddItemToObject(capabilities, "tasks", tasks_cap);
    }
    cJSON_AddItemToObject(capabilities, "experimental", experimental);
    cJSON_AddItemToObject(capabilities, "tools", tools);
    cJSON_AddItemToObject(capabilities, "resources", resources);
    cJSON_AddItemToObject(capabilities, "prompts", prompts);
    if (completions) {
        cJSON_AddItemToObject(capabilities, "completions", completions);
    }
    cJSON_AddItemToObject(capabilities, "logging", logging);

    cJSON_AddStringToObject(server_info, "name", app_desc->project_name);
    cJSON_AddStringToObject(server_info, "version", app_desc->version);
    if (mcp->server_title) {
        cJSON_AddStringToObject(server_info, "title", mcp->server_title);
    }
    if (mcp->server_description) {
        cJSON_AddStringToObject(server_info, "description", mcp->server_description);
    }
    if (mcp->server_icons_json) {
        cJSON *icons = cJSON_Parse(mcp->server_icons_json);
        if (icons) {
            cJSON_AddItemToObject(server_info, "icons", icons);
        }
    }
    if (mcp->server_website_url) {
        cJSON_AddStringToObject(server_info, "websiteUrl", mcp->server_website_url);
    }

    cJSON_AddStringToObject(message, "protocolVersion", negotiated_protocol_version);
    cJSON_AddItemToObject(message, "capabilities", capabilities);
    cJSON_AddItemToObject(message, "serverInfo", server_info);
    if (mcp->instructions) {
        cJSON_AddStringToObject(message, "instructions", mcp->instructions);
    }

    char *message_str = cJSON_PrintUnformatted(message);
    if (message_str) {
        esp_mcp_reply_result(mcp, id, message_str);
        cJSON_free(message_str);
    }
    cJSON_Delete(message);
    session_state->received_initialize_request = true;

    return ESP_OK;
}

static esp_err_t esp_mcp_tools_foreach_cb(esp_mcp_tool_t *tool, void *arg)
{
    esp_mcp_tool_foreach_ctx_t *ctx = (esp_mcp_tool_foreach_ctx_t *)arg;

    if (!(*ctx->found_cursor)) {
        if (!strcmp(tool->name, ctx->cursor_str)) {
            *ctx->found_cursor = true;
        } else {
            return ESP_OK;
        }
    }

    size_t nc_cap = ctx->next_cursor_cap ? ctx->next_cursor_cap : 256;
    if (ctx->page_limit > 0 && ctx->emitted >= ctx->page_limit) {
        snprintf(ctx->next_cursor, nc_cap, "%s", tool->name);
        ctx->should_break = true;
        return ESP_ERR_INVALID_SIZE;
    }

    char *tool_json = esp_mcp_tool_to_json(tool);
    if (tool_json) {
        char *tmp_payload = cJSON_PrintUnformatted(ctx->tools);
        if (!tmp_payload) {
            cJSON_free(tool_json);
            return ESP_OK;
        }

        if (strlen(tmp_payload) > CONFIG_MCP_TOOLLIST_MAX_SIZE) {
            esp_mcp_copy_str(ctx->next_cursor, nc_cap, tool->name);
            cJSON_free(tool_json);
            cJSON_free(tmp_payload);
            ctx->should_break = true;
            return ESP_ERR_INVALID_SIZE;
        }

        cJSON *tool_obj = cJSON_Parse(tool_json);
        if (tool_obj) {
            cJSON_AddItemToArray(ctx->tools_array, tool_obj);
            ctx->emitted++;
        } else {
            ESP_LOGE(TAG, "Failed to parse tool JSON: %s", tool_json);
        }
        cJSON_free(tool_json);
        cJSON_free(tmp_payload);
    }
    return ESP_OK;
}

static esp_err_t esp_mcp_parse_paginated_request(const cJSON *params,
                                                 const cJSON *id,
                                                 esp_mcp_t *mcp,
                                                 const char *method,
                                                 bool allow_limit,
                                                 const char **out_cursor,
                                                 size_t *out_limit)
{
    ESP_RETURN_ON_FALSE(mcp && method && out_cursor, ESP_ERR_INVALID_ARG, TAG, "Invalid args");

    *out_cursor = NULL;
    if (out_limit) {
        *out_limit = 32;
    }

    if (!params) {
        return ESP_OK;
    }

    cJSON *cursor_item = cJSON_GetObjectItem((cJSON *)params, "cursor");
    if (cursor_item) {
        if (!cJSON_IsString(cursor_item) || !cursor_item->valuestring) {
            return esp_mcp_reply_invalid_params_and_fail(mcp, id, method, "params.cursor", "params.cursor");
        }
        if (cursor_item->valuestring[0] != '\0') {
            *out_cursor = cursor_item->valuestring;
        }
    }

    if (allow_limit && out_limit) {
        cJSON *limit_item = cJSON_GetObjectItem((cJSON *)params, "limit");
        if (limit_item) {
            if (!cJSON_IsNumber(limit_item) || limit_item->valuedouble <= 0) {
                return esp_mcp_reply_invalid_params_and_fail(mcp, id, method, "params.limit", "params.limit");
            }
            *out_limit = (size_t)limit_item->valuedouble;
            if (*out_limit > 128) {
                *out_limit = 128;
            }
        }
    }

    return ESP_OK;
}

static esp_err_t esp_mcp_handle_tools_list(esp_mcp_t *mcp, const cJSON *params, const cJSON *id)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid server");

    const char *cursor = NULL;
    char cursor_str[256] = {0};
    char next_cursor[256] = {0};
    bool found_cursor = true;
    esp_err_t ret = esp_mcp_parse_paginated_request(params, id, mcp, "tools/list", false, &cursor, NULL);
    if (ret != ESP_OK) {
        return ret;
    }
    size_t page_limit = 0;
    if (params) {
        cJSON *limit_item = cJSON_GetObjectItem((cJSON *)params, "limit");
        if (limit_item) {
            if (!cJSON_IsNumber(limit_item) || limit_item->valuedouble <= 0) {
                return esp_mcp_reply_invalid_params_and_fail(mcp, id, "tools/list", "params.limit", "params.limit");
            }
            page_limit = (size_t)limit_item->valuedouble;
            if (page_limit > 128) {
                page_limit = 128;
            }
        }
    }
    if (cursor) {
        esp_mcp_copy_str(cursor_str, sizeof(cursor_str), cursor);
        found_cursor = false;
    }

    char *payload_str = NULL;

    cJSON *tools = cJSON_CreateObject();
    if (!tools) {
        ESP_LOGE(TAG, "Failed to allocate memory for tools");
        return ESP_ERR_NO_MEM;
    }

    cJSON *tools_array = cJSON_CreateArray();
    if (!tools_array) {
        ESP_LOGE(TAG, "Failed to allocate memory for tools array");
        cJSON_Delete(tools);
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddItemToObject(tools, "tools", tools_array);

    esp_mcp_tool_foreach_ctx_t ctx = {
        .tools = (void *)tools,
        .tools_array = (void *)tools_array,
        .cursor_str = cursor_str,
        .next_cursor = next_cursor,
        .next_cursor_cap = sizeof(next_cursor),
        .found_cursor = &found_cursor,
        .should_break = false,
        .page_limit = page_limit,
        .emitted = 0,
    };

    esp_mcp_tool_list_foreach(mcp->tools, esp_mcp_tools_foreach_cb, &ctx);

    if (cursor && !found_cursor) {
        cJSON_Delete(tools);
        return esp_mcp_reply_invalid_params(mcp, id, "tools/list", "known params.cursor", "params.cursor");
    }

    if (strlen(next_cursor)) {
        cJSON_AddStringToObject(tools, "nextCursor", next_cursor);
    }

    payload_str = cJSON_PrintUnformatted(tools);
    if (payload_str) {
        bool list_not_empty = !esp_mcp_tool_list_is_empty(mcp->tools);
        if (strlen(payload_str) == 12 && list_not_empty) {
            ESP_LOGE(TAG, "tools/list: Failed to add tool %s because of payload size limit", next_cursor);
            esp_mcp_reply_error_with_method_reason(mcp,
                                                   MCP_ERROR_CODE_FAILED_TO_ADD_TOOL,
                                                   MCP_ERROR_MESSAGE_FAILED_TO_ADD_TOOL,
                                                   id,
                                                   "tools/list",
                                                   "response page truncated");
            ret = ESP_ERR_NO_MEM;
        } else {
            esp_mcp_reply_result(mcp, id, payload_str);
        }
        cJSON_free(payload_str);
    }
    cJSON_Delete(tools);

    return ret;
}

static esp_err_t esp_mcp_tool_args_cb(const esp_mcp_property_t *property, void *arg)
{
    esp_mcp_property_foreach_ctx_t *ctx = (esp_mcp_property_foreach_ctx_t *)arg;
    const char *name = property->name;
    cJSON *value = cJSON_GetObjectItem(ctx->arg, name);
    if (!value) {
        ctx->has_error = true;
        ESP_LOGE(TAG, "tools/call: Missing argument: %s", name);
        return ESP_ERR_INVALID_ARG;
    }

    esp_mcp_property_t *new_prop = NULL;
    if (property->type == ESP_MCP_PROPERTY_TYPE_BOOLEAN) {
        new_prop = esp_mcp_property_create_with_bool(name, value->valueint == 1);
    } else if (property->type == ESP_MCP_PROPERTY_TYPE_INTEGER) {
        int int_value = value->valueint;
        if (property->has_range) {
            if (int_value < property->min_value || int_value > property->max_value) {
                ESP_LOGE(TAG, "tools/call: Integer argument %s (%d) is out of range [%d, %d]",
                         name, int_value, property->min_value, property->max_value);
                ctx->has_error = true;
                return ESP_ERR_INVALID_ARG;
            }
        }
        new_prop = esp_mcp_property_create_with_int(name, int_value);
    } else if (property->type == ESP_MCP_PROPERTY_TYPE_FLOAT) {
        float float_value = (float)value->valuedouble;
        new_prop = esp_mcp_property_create_with_float(name, float_value);
    } else if (property->type == ESP_MCP_PROPERTY_TYPE_STRING) {
        if (!value->valuestring) {
            ctx->has_error = true;
            ESP_LOGE(TAG, "tools/call: String argument is null: %s", name);
            return ESP_ERR_INVALID_ARG;
        }
        new_prop = esp_mcp_property_create_with_string(name, value->valuestring);
    } else if (property->type == ESP_MCP_PROPERTY_TYPE_ARRAY) {
        char *value_str = cJSON_PrintUnformatted(value);
        if (!value_str) {
            ctx->has_error = true;
            ESP_LOGE(TAG, "tools/call: Failed to print array argument: %s", name);
            return ESP_ERR_INVALID_ARG;
        }
        new_prop = esp_mcp_property_create_with_array(name, value_str);
        cJSON_free(value_str);
    } else if (property->type == ESP_MCP_PROPERTY_TYPE_OBJECT) {
        char *value_str = cJSON_PrintUnformatted(value);
        if (!value_str) {
            ctx->has_error = true;
            ESP_LOGE(TAG, "tools/call: Failed to print object argument: %s", name);
            return ESP_ERR_INVALID_ARG;
        }
        new_prop = esp_mcp_property_create_with_object(name, value_str);
        cJSON_free(value_str);
    }

    if (!new_prop) {
        ctx->has_error = true;
        ESP_LOGE(TAG, "tools/call: Failed to create property for argument: %s", name);
        return ESP_ERR_NO_MEM;
    }
    esp_err_t add_ret = esp_mcp_property_list_add_property(ctx->list, new_prop);
    if (add_ret != ESP_OK) {
        esp_mcp_property_destroy(new_prop);
        ctx->has_error = true;
        ESP_LOGE(TAG, "tools/call: Failed to add argument property: %s", name);
        return add_ret;
    }

    return ESP_OK;
}

static void esp_mcp_make_task_id(char out[48])
{
    uint8_t rnd[16];
    esp_fill_random(rnd, sizeof(rnd));
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < sizeof(rnd); i++) {
        out[i * 2] = hex[(rnd[i] >> 4) & 0xF];
        out[i * 2 + 1] = hex[rnd[i] & 0xF];
    }
    out[32] = '\0';
}

static bool esp_mcp_task_simulate_checkpoint_work(esp_mcp_task_t *task, const cJSON *args_obj)
{
    if (!task || !args_obj || !cJSON_IsObject(args_obj)) {
        return false;
    }
    cJSON *simulate_ms = cJSON_GetObjectItem((cJSON *)args_obj, "__simulateWorkMs");
    if (!simulate_ms || !cJSON_IsNumber(simulate_ms) || simulate_ms->valuedouble <= 0) {
        return false;
    }
    uint32_t remaining_ms = (uint32_t)simulate_ms->valuedouble;
    while (remaining_ms > 0) {
        if (esp_mcp_task_checkpoint_cancelled(task, "simulated-work")) {
            return true;
        }
        uint32_t step_ms = remaining_ms > 100 ? 100 : remaining_ms;
        vTaskDelay(pdMS_TO_TICKS(step_ms));
        if (remaining_ms >= step_ms) {
            remaining_ms -= step_ms;
        } else {
            remaining_ms = 0;
        }
    }
    return esp_mcp_task_checkpoint_cancelled(task, "simulated-work-finish");
}

typedef struct {
    esp_mcp_t *mcp;
    esp_mcp_task_t *task;
    char tool_name[64];
    char *args_json; // JSON object string or NULL
} esp_mcp_tool_task_ctx_t;

static char *esp_mcp_build_error_json(int error_code,
                                      const char *message,
                                      const char *method,
                                      const char *expected,
                                      const char *field,
                                      const char *reason)
{
    cJSON *error = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    if (!error || !data) {
        if (error) {
            cJSON_Delete(error);
        }
        if (data) {
            cJSON_Delete(data);
        }
        return NULL;
    }

    cJSON_AddNumberToObject(error, "code", error_code);
    cJSON_AddStringToObject(error, "message", message ? message : MCP_ERROR_MESSAGE_INTERNAL_ERROR);
    if (method) {
        cJSON_AddStringToObject(data, "method", method);
    }
    if (expected) {
        cJSON_AddStringToObject(data, "expected", expected);
    }
    if (field) {
        cJSON_AddStringToObject(data, "field", field);
    }
    if (reason) {
        cJSON_AddStringToObject(data, "reason", reason);
    }
    if (data->child) {
        cJSON_AddItemToObject(error, "data", data);
    } else {
        cJSON_Delete(data);
    }

    char *error_json = cJSON_PrintUnformatted(error);
    cJSON_Delete(error);
    return error_json;
}

static void esp_mcp_tool_task_worker_atomic_dec(esp_mcp_t *mcp)
{
    if (!mcp) {
        return;
    }
    uint32_t prev;
    do {
        prev = __atomic_load_n(&mcp->tool_worker_count, __ATOMIC_SEQ_CST);
        if (prev == 0) {
            return;
        }
    } while (!__atomic_compare_exchange_n(&mcp->tool_worker_count, &prev, prev - 1,
                                          false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
}

static void esp_mcp_tool_task_worker(void *arg)
{
    esp_mcp_tool_task_ctx_t *ctx = (esp_mcp_tool_task_ctx_t *)arg;
    cJSON *args_obj = NULL;
    esp_mcp_property_list_t *arguments = NULL;
    char *result = NULL;

    if (!ctx || !ctx->mcp || !ctx->task) {
        if (ctx) {
            free(ctx->args_json);
            free(ctx);
        }
        vTaskDelete(NULL);
        return;
    }

    if (esp_mcp_task_checkpoint_cancelled(ctx->task, "worker-enter")) {
        goto cleanup;
    }

    if (ctx->mcp->destroy_requested) {
        (void)esp_mcp_task_set_status(ctx->task, ESP_MCP_TASK_FAILED, "Engine destroying");
        goto cleanup;
    }

    esp_mcp_tool_t *tool = esp_mcp_tool_list_find_tool(ctx->mcp->tools, ctx->tool_name);
    if (!tool) {
        result = esp_mcp_build_error_json(MCP_ERROR_CODE_TOOL_CALL_FAILED,
                                          MCP_ERROR_MESSAGE_TOOL_CALL_FAILED,
                                          "tools/call",
                                          NULL,
                                          NULL,
                                          "tool disappeared before execution");
        if (result) {
            (void)esp_mcp_task_set_result_json(ctx->task, result, true);
        }
        (void)esp_mcp_task_set_status(ctx->task, ESP_MCP_TASK_FAILED, "Unknown tool");
        goto cleanup;
    }

    if (ctx->args_json) {
        args_obj = cJSON_Parse(ctx->args_json);
        if (!args_obj) {
            (void)esp_mcp_task_set_status(ctx->task, ESP_MCP_TASK_FAILED, "Invalid arguments");
            goto cleanup;
        }
    }

    arguments = esp_mcp_property_list_create();
    if (!arguments) {
        result = esp_mcp_build_error_json(MCP_ERROR_CODE_FAILED_TO_CREATE_ARGUMENTS,
                                          MCP_ERROR_MESSAGE_FAILED_TO_CREATE_ARGUMENTS,
                                          "tools/call",
                                          NULL,
                                          NULL,
                                          "argument list alloc failed");
        if (result) {
            (void)esp_mcp_task_set_result_json(ctx->task, result, true);
        }
        (void)esp_mcp_task_set_status(ctx->task, ESP_MCP_TASK_FAILED, "No mem");
        goto cleanup;
    }

    if (args_obj && cJSON_IsObject(args_obj)) {
        if (!tool->properties) {
            ESP_LOGW(TAG, "tools/call: tool %s has no property schema", ctx->tool_name);
            if (cJSON_GetArraySize(args_obj) > 0) {
                ESP_LOGE(TAG, "tools/call: arguments provided but tool has no properties: %s", ctx->tool_name);
                result = esp_mcp_build_error_json(MCP_ERROR_CODE_INVALID_PARAMS,
                                                  MCP_ERROR_MESSAGE_INVALID_PARAMS,
                                                  "tools/call",
                                                  "params.arguments",
                                                  "params.arguments",
                                                  NULL);
                if (result) {
                    (void)esp_mcp_task_set_result_json(ctx->task, result, true);
                }
                (void)esp_mcp_task_set_status(ctx->task, ESP_MCP_TASK_FAILED, "Invalid arguments");
                goto cleanup;
            }
        } else {
            esp_mcp_property_foreach_ctx_t args_ctx = {
                .list = arguments,
                .callback = esp_mcp_tool_args_cb,
                .arg = (void *)args_obj,
                .has_error = false
            };

            esp_err_t foreach_ret = esp_mcp_property_list_foreach(tool->properties, &args_ctx);
            if (foreach_ret != ESP_OK || args_ctx.has_error) {
                result = esp_mcp_build_error_json(MCP_ERROR_CODE_INVALID_PARAMS,
                                                  MCP_ERROR_MESSAGE_INVALID_PARAMS,
                                                  "tools/call",
                                                  "params.arguments",
                                                  "params.arguments",
                                                  NULL);
                if (result) {
                    (void)esp_mcp_task_set_result_json(ctx->task, result, true);
                }
                (void)esp_mcp_task_set_status(ctx->task, ESP_MCP_TASK_FAILED, "Invalid arguments");
                goto cleanup;
            }
        }
    }
    if (esp_mcp_task_simulate_checkpoint_work(ctx->task, args_obj)) {
        goto cleanup;
    }

    if (esp_mcp_task_checkpoint_cancelled(ctx->task, "after-args-parse")) {
        goto cleanup;
    }

    if (esp_mcp_task_checkpoint_cancelled(ctx->task, "before-tool-call")) {
        goto cleanup;
    }

    result = esp_mcp_tool_call(tool, arguments);

    if (!result) {
        result = esp_mcp_build_error_json(MCP_ERROR_CODE_TOOL_CALL_FAILED,
                                          MCP_ERROR_MESSAGE_TOOL_CALL_FAILED,
                                          "tools/call",
                                          NULL,
                                          NULL,
                                          "tool callback returned null");
        if (result) {
            (void)esp_mcp_task_set_result_json(ctx->task, result, true);
        }
        (void)esp_mcp_task_set_status(ctx->task, ESP_MCP_TASK_FAILED, "Tool failed");
        goto cleanup;
    }

    // Determine success based on CallToolResult.isError
    bool is_error = false;
    cJSON *rj = cJSON_Parse(result);
    if (rj && cJSON_IsObject(rj)) {
        cJSON *ie = cJSON_GetObjectItem(rj, "isError");
        if (ie && cJSON_IsBool(ie)) {
            is_error = cJSON_IsTrue(ie);
        }
    }
    if (rj) {
        cJSON_Delete(rj);
    }

    (void)esp_mcp_task_set_result_json(ctx->task, result, false);
    cJSON_free(result);
    result = NULL;

    if (esp_mcp_task_checkpoint_cancelled(ctx->task, "after-tool-call")) {
    } else if (is_error) {
        (void)esp_mcp_task_set_status(ctx->task, ESP_MCP_TASK_FAILED, "Failed");
    } else {
        (void)esp_mcp_task_set_status(ctx->task, ESP_MCP_TASK_COMPLETED, "Completed");
    }

cleanup:
    if (arguments) {
        esp_mcp_property_list_destroy(arguments);
    }
    if (args_obj) {
        cJSON_Delete(args_obj);
    }
    if (result) {
        cJSON_free(result);
    }
    esp_mcp_task_mark_worker_finished(ctx->task);
    esp_mcp_tool_task_worker_atomic_dec(ctx->mcp);
    free(ctx->args_json);
    free(ctx);
    vTaskDelete(NULL);
}

static esp_err_t esp_mcp_tools_call_create_task(esp_mcp_t *mcp,
                                                const cJSON *id,
                                                const char *tool_name,
                                                const cJSON *tool_arguments,
                                                const cJSON *task_meta,
                                                int stack_size)
{
    ESP_RETURN_ON_FALSE(mcp && tool_name, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    ESP_RETURN_ON_FALSE(mcp->tasks, ESP_ERR_INVALID_STATE, TAG, "Tasks not initialized");

    bool has_ttl_override = false;
    int64_t ttl_ms = -1;
    if (task_meta) {
        cJSON *ttl = cJSON_GetObjectItem((cJSON *)task_meta, "ttl");
        if (ttl) {
            if (!cJSON_IsNumber(ttl) || ttl->valuedouble < 0) {
                ESP_LOGE(TAG, "tools/call: Invalid task.ttl");
                return esp_mcp_reply_invalid_params(mcp, id, "tools/call", "params.task.ttl>=0", "params.task.ttl");
            }
            has_ttl_override = true;
            ttl_ms = (int64_t)ttl->valuedouble;
        }
    }

    char task_id[48] = {0};
    esp_mcp_make_task_id(task_id);

    esp_mcp_task_store_t *store = (esp_mcp_task_store_t *)mcp->tasks;
    esp_mcp_task_t *task = esp_mcp_task_create(store, task_id);
    if (!task) {
        return esp_mcp_reply_internal_error_with_method(mcp, id, "tools/call", "task create failed");
    }
    (void)esp_mcp_task_set_origin_session(task, mcp->request_session_id[0] ? mcp->request_session_id : NULL);
    if (has_ttl_override) {
        (void)esp_mcp_task_set_ttl(task, ttl_ms);
    }

    // Prepare async worker context
    esp_mcp_tool_task_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        (void)esp_mcp_task_set_status(task, ESP_MCP_TASK_FAILED, "No mem");
        return esp_mcp_reply_internal_error_with_method(mcp, id, "tools/call", "task context alloc failed");
    }
    ctx->mcp = mcp;
    ctx->task = task;
    if (strlen(tool_name) >= sizeof(ctx->tool_name)) {
        (void)esp_mcp_task_set_status(task, ESP_MCP_TASK_FAILED, "Tool name too long");
        free(ctx);
        return esp_mcp_reply_invalid_params(mcp, id, "tools/call", "params.name <= 63 chars", "params.name");
    }
    esp_mcp_copy_str(ctx->tool_name, sizeof(ctx->tool_name), tool_name);
    if (tool_arguments && cJSON_IsObject(tool_arguments)) {
        ctx->args_json = cJSON_PrintUnformatted((cJSON *)tool_arguments);
        if (!ctx->args_json) {
            (void)esp_mcp_task_set_status(task, ESP_MCP_TASK_FAILED, "Args serialize failed");
            free(ctx);
            return esp_mcp_reply_internal_error_with_method(mcp, id, "tools/call", "task args serialize failed");
        }
    }

    // Spawn worker task; stack_size unit follows FreeRTOS xTaskCreate (stack words).
    int stack_words = stack_size > 0 ? stack_size : CONFIG_MCP_TOOLCALL_STACK_SIZE;
    if (stack_words < 1024) {
        ESP_LOGW(TAG, "tools/call: stackSize too small (%d), clamped to 1024 words", stack_words);
        stack_words = 1024;
    }
    if (stack_words > 16384) {
        ESP_LOGW(TAG, "tools/call: stackSize too large (%d), clamped to 16384 words", stack_words);
        stack_words = 16384;
    }
    (void)__atomic_add_fetch(&mcp->tool_worker_count, 1, __ATOMIC_SEQ_CST);
    if (mcp->destroy_requested) {
        (void)__atomic_sub_fetch(&mcp->tool_worker_count, 1, __ATOMIC_SEQ_CST);
        (void)esp_mcp_task_set_status(task, ESP_MCP_TASK_FAILED, "Engine destroying");
        free(ctx->args_json);
        free(ctx);
        return esp_mcp_reply_internal_error_with_method(mcp, id, "tools/call", "engine destroying");
    }
    (void)esp_mcp_task_mark_worker_starting(task);
    if (xTaskCreate(esp_mcp_tool_task_worker, "mcp_tool_task", stack_words, ctx, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
        (void)__atomic_sub_fetch(&mcp->tool_worker_count, 1, __ATOMIC_SEQ_CST);
        esp_mcp_task_mark_worker_finished(task);
        (void)esp_mcp_task_set_status(task, ESP_MCP_TASK_FAILED, "Spawn failed");
        free(ctx->args_json);
        free(ctx);
        return esp_mcp_reply_internal_error_with_method(mcp, id, "tools/call", "task spawn failed");
    }

    // Reply CreateTaskResult
    cJSON *result = cJSON_CreateObject();
    cJSON *task_obj = cJSON_CreateObject();
    if (!result || !task_obj) {
        if (result) {
            cJSON_Delete(result);
        }
        if (task_obj) {
            cJSON_Delete(task_obj);
        }
        return esp_mcp_reply_internal_error_with_method(mcp, id, "tools/call", "task response alloc failed");
    }
    cJSON_AddItemToObject(result, "task", task_obj);
    (void)esp_mcp_task_add_json(task_obj, task);

    char *s = cJSON_PrintUnformatted(result);
    cJSON_Delete(result);
    ESP_RETURN_ON_FALSE(s, ESP_ERR_NO_MEM, TAG, "Print failed");
    esp_err_t r = esp_mcp_reply_result_with_related_task(mcp, id, s, task_id);
    cJSON_free(s);
    return r;
}

static esp_err_t esp_mcp_do_tool_call(esp_mcp_t *mcp, const cJSON *id, const char *tool_name, const cJSON *tool_arguments, int stack_size)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid server");
    ESP_RETURN_ON_FALSE(tool_name, ESP_ERR_INVALID_ARG, TAG, "Invalid tool name");
    if (tool_arguments != NULL && !cJSON_IsObject(tool_arguments)) {
        ESP_LOGE(TAG, "tools/call: Invalid arguments object");
        return esp_mcp_reply_invalid_params(mcp, id, "tools/call", "params.arguments", "params.arguments");
    }

    if (stack_size <= 0) {
        ESP_LOGE(TAG, "tools/call: Invalid stack size");
        return esp_mcp_reply_invalid_params(mcp, id, "tools/call", "params.stackSize>0", "params.stackSize");
    }

    esp_mcp_tool_t *tool = esp_mcp_tool_list_find_tool(mcp->tools, tool_name);
    if (!tool) {
        ESP_LOGE(TAG, "tools/call: Unknown tool: %s", tool_name);
        return esp_mcp_reply_not_found(mcp, id, "tools/call", "name", tool_name, "Tool not found");
    }

    esp_mcp_property_list_t *arguments = esp_mcp_property_list_create();
    if (arguments) {
        if (tool_arguments && cJSON_IsObject(tool_arguments)) {
            if (!tool->properties) {
                ESP_LOGW(TAG, "tools/call: tool %s has no property schema", tool_name);
                if (cJSON_GetArraySize((cJSON *)tool_arguments) > 0) {
                    ESP_LOGE(TAG, "tools/call: arguments provided but tool has no properties: %s", tool_name);
                    esp_mcp_property_list_destroy(arguments);
                    return esp_mcp_reply_invalid_params(mcp, id, "tools/call", "params.arguments", "params.arguments");
                }
            } else {
                esp_mcp_property_foreach_ctx_t args_ctx = {
                    .list = arguments,
                    .callback = esp_mcp_tool_args_cb,
                    .arg = (void *)tool_arguments,
                    .has_error = false
                };

                esp_err_t foreach_ret = esp_mcp_property_list_foreach(tool->properties, &args_ctx);
                if (foreach_ret != ESP_OK || args_ctx.has_error) {
                    ESP_LOGE(TAG, "tools/call: Failed to process tool arguments");
                    esp_mcp_property_list_destroy(arguments);
                    return esp_mcp_reply_invalid_params(mcp, id, "tools/call", "params.arguments", "params.arguments");
                }
            }
        }

        char *result = esp_mcp_tool_call(tool, arguments);
        if (result) {
            esp_mcp_reply_result(mcp, id, result);
            cJSON_free(result);
        } else {
            esp_mcp_reply_error_with_method_reason(mcp,
                                                   MCP_ERROR_CODE_TOOL_CALL_FAILED,
                                                   MCP_ERROR_MESSAGE_TOOL_CALL_FAILED,
                                                   id,
                                                   "tools/call",
                                                   "tool callback returned null");
        }

        esp_mcp_property_list_destroy(arguments);
    } else {
        esp_mcp_reply_error_with_method_reason(mcp,
                                               MCP_ERROR_CODE_FAILED_TO_CREATE_ARGUMENTS,
                                               MCP_ERROR_MESSAGE_FAILED_TO_CREATE_ARGUMENTS,
                                               id,
                                               "tools/call",
                                               "argument list alloc failed");
    }

    return ESP_OK;
}

static esp_err_t esp_mcp_handle_tools_call(esp_mcp_t *mcp, const cJSON *params, const cJSON *id)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid server");
    ESP_RETURN_ON_FALSE(params, ESP_ERR_INVALID_ARG, TAG, "Invalid params");

    if (!cJSON_IsObject(params)) {
        ESP_LOGE(TAG, "tools/call: Missing params");
        return esp_mcp_reply_invalid_params(mcp, id, "tools/call", "params.name", "params");
    }

    cJSON *tool_name = cJSON_GetObjectItem(params, "name");
    if (!cJSON_IsString(tool_name)) {
        ESP_LOGE(TAG, "tools/call: Missing name");
        return esp_mcp_reply_invalid_params(mcp, id, "tools/call", "params.name", "params.name");
    }

    cJSON *tool_arguments = cJSON_GetObjectItem(params, "arguments");
    if (tool_arguments != NULL && !cJSON_IsObject(tool_arguments)) {
        ESP_LOGE(TAG, "tools/call: Invalid arguments");
        return esp_mcp_reply_invalid_params(mcp, id, "tools/call", "params.arguments", "params.arguments");
    }

    cJSON *stack_size = cJSON_GetObjectItem(params, "stackSize");
    if (stack_size != NULL && !cJSON_IsNumber(stack_size)) {
        ESP_LOGE(TAG, "tools/call: Invalid stackSize");
        return esp_mcp_reply_invalid_params(mcp, id, "tools/call", "params.stackSize", "params.stackSize");
    }

    esp_mcp_tool_t *tool = esp_mcp_tool_list_find_tool(mcp->tools, tool_name->valuestring);
    if (!tool) {
        ESP_LOGE(TAG, "tools/call: Unknown tool: %s", tool_name->valuestring);
        return esp_mcp_reply_not_found(mcp, id, "tools/call", "name", tool_name->valuestring, "Tool not found");
    }

    cJSON *task = cJSON_GetObjectItem(params, "task");
    bool wants_task_mode = false;
    if (task != NULL) {
        if (!cJSON_IsObject(task)) {
            ESP_LOGE(TAG, "tools/call: Invalid task object");
            return esp_mcp_reply_invalid_params(mcp, id, "tools/call", "params.task", "params.task");
        }
        cJSON *ttl = cJSON_GetObjectItem(task, "ttl");
        if (ttl && (!cJSON_IsNumber(ttl) || ttl->valuedouble < 0)) {
            ESP_LOGE(TAG, "tools/call: Invalid task.ttl");
            return esp_mcp_reply_invalid_params(mcp, id, "tools/call", "params.task.ttl>=0", "params.task.ttl");
        }
        wants_task_mode = true;
    }

    bool task_required = (strcmp(tool->task_support, "required") == 0);
    bool task_forbidden = (strcmp(tool->task_support, "forbidden") == 0);

    if (wants_task_mode) {
        if (task_forbidden) {
            return esp_mcp_reply_error_with_method_reason(mcp,
                                                          MCP_ERROR_CODE_METHOD_NOT_FOUND,
                                                          MCP_ERROR_MESSAGE_METHOD_NOT_FOUND,
                                                          id,
                                                          "tools/call",
                                                          "task augmentation forbidden for tool");
        }
        return esp_mcp_tools_call_create_task(mcp, id, tool_name->valuestring, tool_arguments, task,
                                              stack_size ? stack_size->valueint : CONFIG_MCP_TOOLCALL_STACK_SIZE);
    }

    if (task_required) {
        return esp_mcp_reply_error_with_method_reason(mcp,
                                                      MCP_ERROR_CODE_METHOD_NOT_FOUND,
                                                      MCP_ERROR_MESSAGE_METHOD_NOT_FOUND,
                                                      id,
                                                      "tools/call",
                                                      "task augmentation required for tool");
    }

    return esp_mcp_do_tool_call(mcp, id, tool_name->valuestring, tool_arguments,
                                stack_size ? stack_size->valueint : CONFIG_MCP_TOOLCALL_STACK_SIZE);
}

static esp_err_t esp_mcp_handle_ping(esp_mcp_t *mcp, const cJSON *params, const cJSON *id)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid server");

    // MCP 2025-11-25: ping result MUST be an empty object {}
    return esp_mcp_reply_result(mcp, id, "{}");
}

typedef struct {
    cJSON *array;
    const char *cursor;
    bool found_cursor;
    const char *key_name;
    size_t limit;
    size_t emitted;
    bool should_break;
    char next_cursor[128];
} esp_mcp_json_cursor_ctx_t;

static esp_err_t esp_mcp_resources_list_cb(esp_mcp_resource_t *res, void *arg)
{
    esp_mcp_json_cursor_ctx_t *ctx = (esp_mcp_json_cursor_ctx_t *)arg;
    cJSON *o = esp_mcp_resource_to_json(res);
    if (!o) {
        return ESP_OK;
    }

    if (ctx->cursor && !ctx->found_cursor) {
        cJSON *cursor_item = cJSON_GetObjectItem(o, ctx->key_name);
        if (cursor_item && cJSON_IsString(cursor_item) && cursor_item->valuestring &&
                strcmp(cursor_item->valuestring, ctx->cursor) == 0) {
            ctx->found_cursor = true;
        } else {
            cJSON_Delete(o);
            return ESP_OK;
        }
    }

    if (ctx->limit > 0 && ctx->emitted >= ctx->limit) {
        cJSON *cursor_item = cJSON_GetObjectItem(o, ctx->key_name);
        if (cursor_item && cJSON_IsString(cursor_item) && cursor_item->valuestring) {
            snprintf(ctx->next_cursor, sizeof(ctx->next_cursor), "%s", cursor_item->valuestring);
        }
        cJSON_Delete(o);
        return ESP_FAIL;
    }
    cJSON_AddItemToArray(ctx->array, o);
    ctx->emitted++;
    return ESP_OK;
}

static esp_err_t esp_mcp_handle_resources_list(esp_mcp_t *mcp, const cJSON *params, const cJSON *id)
{
    esp_mcp_resource_list_t *list = (esp_mcp_resource_list_t *)mcp->resources;
    const char *cursor = NULL;
    size_t limit = 32;
    esp_err_t parse_ret = esp_mcp_parse_paginated_request(params, id, mcp, "resources/list", true, &cursor, &limit);
    if (parse_ret != ESP_OK) {
        return parse_ret;
    }

    cJSON *result = cJSON_CreateObject();
    cJSON *arr = cJSON_CreateArray();
    if (!result || !arr) {
        if (result) {
            cJSON_Delete(result);
        }
        if (arr) {
            cJSON_Delete(arr);
        }
        ESP_LOGE(TAG, "No mem");
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddItemToObject(result, "resources", arr);
    if (list) {
        esp_mcp_json_cursor_ctx_t ctx = {
            .array = arr,
            .cursor = cursor,
            .found_cursor = (cursor == NULL),
            .key_name = "uri",
            .limit = limit,
            .emitted = 0,
            .should_break = false,
        };
        (void)esp_mcp_resource_list_foreach(list, esp_mcp_resources_list_cb, &ctx);
        if (ctx.cursor && !ctx.found_cursor) {
            cJSON_Delete(result);
            return esp_mcp_reply_invalid_params(mcp, id, "resources/list", "known params.cursor", "params.cursor");
        }
        if (ctx.next_cursor[0]) {
            cJSON_AddStringToObject(result, "nextCursor", ctx.next_cursor);
        }
    }

    char *s = cJSON_PrintUnformatted(result);
    cJSON_Delete(result);
    ESP_RETURN_ON_FALSE(s, ESP_ERR_NO_MEM, TAG, "Print failed");
    esp_err_t r = esp_mcp_reply_result(mcp, id, s);
    cJSON_free(s);
    return r;
}

static esp_err_t esp_mcp_handle_resources_read(esp_mcp_t *mcp, const cJSON *params, const cJSON *id)
{
    if (!(params && cJSON_IsObject(params))) {
        return esp_mcp_reply_invalid_params(mcp, id, "resources/read", "params.uri", "params");
    }
    cJSON *uri = cJSON_GetObjectItem(params, "uri");
    if (!(uri && cJSON_IsString(uri))) {
        return esp_mcp_reply_invalid_params(mcp, id, "resources/read", "params.uri", "params.uri");
    }

    esp_mcp_resource_list_t *list = (esp_mcp_resource_list_t *)mcp->resources;
    esp_mcp_resource_t *res = NULL;
    esp_err_t find_ret = list ? esp_mcp_resource_list_find_ex(list, uri->valuestring, &res) : ESP_ERR_INVALID_STATE;
    if (find_ret == ESP_ERR_INVALID_STATE) {
        return esp_mcp_reply_internal_error_with_method(mcp, id, "resources/read", "resource list unavailable");
    }
    if (find_ret != ESP_OK || !res) {
        return esp_mcp_reply_not_found(mcp, id, "resources/read", "uri", uri->valuestring, "Resource not found");
    }

    char *out_mime = NULL;
    char *out_text = NULL;
    char *out_blob = NULL;
    esp_err_t rr = esp_mcp_resource_read(res, &out_mime, &out_text, &out_blob);
    if (rr != ESP_OK) {
        free(out_mime); free(out_text); free(out_blob);
        return esp_mcp_reply_error_with_method_reason(mcp,
                                                      MCP_ERROR_CODE_INTERNAL_ERROR,
                                                      "Resource read failed",
                                                      id,
                                                      "resources/read",
                                                      esp_err_to_name(rr));
    }

    cJSON *result = cJSON_CreateObject();
    cJSON *contents = cJSON_CreateArray();
    cJSON *c0 = cJSON_CreateObject();
    if (!result || !contents || !c0) {
        if (result) {
            cJSON_Delete(result);
        }
        if (contents) {
            cJSON_Delete(contents);
        }
        if (c0) {
            cJSON_Delete(c0);
        }
        free(out_mime); free(out_text); free(out_blob);
        return esp_mcp_reply_internal_error_with_method(mcp, id, "resources/read", "response alloc failed");
    }

    cJSON_AddItemToObject(result, "contents", contents);
    cJSON_AddItemToArray(contents, c0);
    cJSON_AddStringToObject(c0, "uri", uri->valuestring);
    if (out_mime) {
        cJSON_AddStringToObject(c0, "mimeType", out_mime);
    }
    if (out_text) {
        cJSON_AddStringToObject(c0, "text", out_text);
    }
    if (out_blob) {
        cJSON_AddStringToObject(c0, "blob", out_blob);
    }

    char *s = cJSON_PrintUnformatted(result);
    cJSON_Delete(result);
    free(out_mime); free(out_text); free(out_blob);
    ESP_RETURN_ON_FALSE(s, ESP_ERR_NO_MEM, TAG, "Print failed");
    esp_err_t r = esp_mcp_reply_result(mcp, id, s);
    cJSON_free(s);
    return r;
}

static bool esp_mcp_resource_subscription_contains_unlocked(esp_mcp_t *mcp, const char *session_id, const char *uri)
{
    if (!mcp || !session_id || !uri) {
        return false;
    }
    for (size_t i = 0; i < mcp->resource_subscriptions_count; ++i) {
        if (strcmp(mcp->resource_subscriptions[i].session_id, session_id) == 0 &&
                strcmp(mcp->resource_subscriptions[i].uri, uri) == 0) {
            return true;
        }
    }
    return false;
}

static esp_err_t esp_mcp_resource_subscription_add(esp_mcp_t *mcp, const char *session_id, const char *uri)
{
    if (!mcp || !session_id || !uri) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_mcp_resource_sub_lock(mcp);
    if (esp_mcp_resource_subscription_contains_unlocked(mcp, session_id, uri)) {
        esp_mcp_resource_sub_unlock(mcp);
        return ESP_OK;
    }
    size_t max_subscriptions = sizeof(mcp->resource_subscriptions) / sizeof(mcp->resource_subscriptions[0]);
    if (mcp->resource_subscriptions_count >= max_subscriptions) {
        esp_mcp_resource_sub_unlock(mcp);
        return ESP_ERR_NO_MEM;
    }
    size_t idx = mcp->resource_subscriptions_count++;
    esp_mcp_copy_str(mcp->resource_subscriptions[idx].session_id, sizeof(mcp->resource_subscriptions[idx].session_id), session_id);
    esp_mcp_copy_str(mcp->resource_subscriptions[idx].uri, sizeof(mcp->resource_subscriptions[idx].uri), uri);
    esp_mcp_resource_sub_unlock(mcp);
    return ESP_OK;
}

static esp_err_t esp_mcp_resource_subscription_remove(esp_mcp_t *mcp, const char *session_id, const char *uri)
{
    if (!mcp || !session_id || !uri) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_mcp_resource_sub_lock(mcp);
    for (size_t i = 0; i < mcp->resource_subscriptions_count; ++i) {
        if (strcmp(mcp->resource_subscriptions[i].session_id, session_id) == 0 &&
                strcmp(mcp->resource_subscriptions[i].uri, uri) == 0) {
            for (size_t j = i + 1; j < mcp->resource_subscriptions_count; ++j) {
                memcpy(&mcp->resource_subscriptions[j - 1],
                       &mcp->resource_subscriptions[j],
                       sizeof(mcp->resource_subscriptions[j - 1]));
            }
            mcp->resource_subscriptions_count--;
            esp_mcp_resource_sub_unlock(mcp);
            return ESP_OK;
        }
    }
    esp_mcp_resource_sub_unlock(mcp);
    return ESP_OK; // idempotent
}

static esp_err_t esp_mcp_resources_templates_list_cb(esp_mcp_resource_t *res, void *arg)
{
    esp_mcp_json_cursor_ctx_t *ctx = (esp_mcp_json_cursor_ctx_t *)arg;
    cJSON *r = esp_mcp_resource_to_json(res);
    if (!r) {
        return ESP_OK;
    }

    cJSON *uri = cJSON_GetObjectItem(r, "uri");
    if (!uri || !cJSON_IsString(uri) || !uri->valuestring ||
            !strchr(uri->valuestring, '{') || !strchr(uri->valuestring, '}')) {
        cJSON_Delete(r);
        return ESP_OK;
    }

    if (ctx->cursor && !ctx->found_cursor) {
        if (strcmp(uri->valuestring, ctx->cursor) == 0) {
            ctx->found_cursor = true;
        } else {
            cJSON_Delete(r);
            return ESP_OK;
        }
    }

    if (ctx->limit > 0 && ctx->emitted >= ctx->limit) {
        snprintf(ctx->next_cursor, sizeof(ctx->next_cursor), "%s", uri->valuestring);
        cJSON_Delete(r);
        return ESP_FAIL;
    }

    cJSON *tpl = cJSON_CreateObject();
    if (!tpl) {
        cJSON_Delete(r);
        return ESP_OK;
    }

    cJSON_AddStringToObject(tpl, "uriTemplate", uri->valuestring);
    cJSON *name = cJSON_GetObjectItem(r, "name");
    cJSON *title = cJSON_GetObjectItem(r, "title");
    cJSON *desc = cJSON_GetObjectItem(r, "description");
    cJSON *mime = cJSON_GetObjectItem(r, "mimeType");
    if (name && cJSON_IsString(name)) {
        cJSON_AddStringToObject(tpl, "name", name->valuestring);
    }
    if (title && cJSON_IsString(title)) {
        cJSON_AddStringToObject(tpl, "title", title->valuestring);
    }
    if (desc && cJSON_IsString(desc)) {
        cJSON_AddStringToObject(tpl, "description", desc->valuestring);
    }
    if (mime && cJSON_IsString(mime)) {
        cJSON_AddStringToObject(tpl, "mimeType", mime->valuestring);
    }
    cJSON_AddItemToArray(ctx->array, tpl);
    ctx->emitted++;
    cJSON_Delete(r);
    return ESP_OK;
}

static esp_err_t esp_mcp_handle_resources_templates_list(esp_mcp_t *mcp, const cJSON *params, const cJSON *id)
{
    esp_mcp_resource_list_t *list = (esp_mcp_resource_list_t *)mcp->resources;
    const char *cursor = NULL;
    size_t limit = 32;
    esp_err_t parse_ret = esp_mcp_parse_paginated_request(params, id, mcp, "resources/templates/list", true, &cursor, &limit);
    if (parse_ret != ESP_OK) {
        return parse_ret;
    }

    cJSON *result = cJSON_CreateObject();
    cJSON *arr = cJSON_CreateArray();
    if (!result || !arr) {
        if (result) {
            cJSON_Delete(result);
        }
        if (arr) {
            cJSON_Delete(arr);
        }
        ESP_LOGE(TAG, "No mem");
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddItemToObject(result, "resourceTemplates", arr);
    if (list) {
        esp_mcp_json_cursor_ctx_t ctx = {
            .array = arr,
            .cursor = cursor,
            .found_cursor = (cursor == NULL),
            .key_name = "uriTemplate",
            .limit = limit,
            .emitted = 0,
            .should_break = false,
        };
        (void)esp_mcp_resource_list_foreach(list, esp_mcp_resources_templates_list_cb, &ctx);
        if (ctx.cursor && !ctx.found_cursor) {
            cJSON_Delete(result);
            return esp_mcp_reply_invalid_params(mcp, id, "resources/templates/list", "known params.cursor", "params.cursor");
        }
        if (ctx.next_cursor[0]) {
            cJSON_AddStringToObject(result, "nextCursor", ctx.next_cursor);
        }
    }
    char *s = cJSON_PrintUnformatted(result);
    cJSON_Delete(result);
    ESP_RETURN_ON_FALSE(s, ESP_ERR_NO_MEM, TAG, "Print failed");
    esp_err_t r = esp_mcp_reply_result(mcp, id, s);
    cJSON_free(s);
    return r;
}

static esp_err_t esp_mcp_handle_resources_subscribe(esp_mcp_t *mcp, const cJSON *params, const cJSON *id)
{
    ESP_RETURN_ON_FALSE(params && cJSON_IsObject(params), ESP_ERR_INVALID_ARG, TAG, "Invalid params");
    cJSON *uri = cJSON_GetObjectItem(params, "uri");
    if (!uri || !cJSON_IsString(uri) || !uri->valuestring) {
        return esp_mcp_reply_invalid_params(mcp, id, "resources/subscribe", "params.uri", "params.uri");
    }
    esp_mcp_resource_list_t *list = (esp_mcp_resource_list_t *)mcp->resources;
    esp_mcp_resource_t *res = NULL;
    esp_err_t find_ret = list ? esp_mcp_resource_list_find_ex(list, uri->valuestring, &res) : ESP_ERR_INVALID_STATE;
    if (find_ret == ESP_ERR_INVALID_STATE) {
        return esp_mcp_reply_internal_error_with_method(mcp, id, "resources/subscribe", "resource list unavailable");
    }
    if (find_ret != ESP_OK || !res) {
        return esp_mcp_reply_not_found(mcp, id, "resources/subscribe", "uri", uri->valuestring, "Resource not found");
    }
    if (!mcp->request_session_id[0]) {
        return esp_mcp_reply_error_with_kv(mcp, MCP_ERROR_CODE_INVALID_REQUEST, MCP_ERROR_MESSAGE_INVALID_REQUEST, id, "reason", "missing_session");
    }
    esp_err_t ret = esp_mcp_resource_subscription_add(mcp, mcp->request_session_id, uri->valuestring);
    if (ret == ESP_ERR_NO_MEM) {
        cJSON *data = cJSON_CreateObject();
        if (data) {
            cJSON_AddStringToObject(data, "reason", "subscription_limit_reached");
            cJSON_AddNumberToObject(data,
                                    "limit",
                                    (double)(sizeof(mcp->resource_subscriptions) / sizeof(mcp->resource_subscriptions[0])));
            cJSON_AddStringToObject(data, "uri", uri->valuestring);
        }
        esp_err_t rr = esp_mcp_reply_error_ex(mcp, MCP_ERROR_CODE_INTERNAL_ERROR, "Maximum subscriptions reached", id, data);
        if (data) {
            cJSON_Delete(data);
        }
        return rr;
    }
    if (ret != ESP_OK) {
        return esp_mcp_reply_error_with_kv(mcp, MCP_ERROR_CODE_INTERNAL_ERROR, MCP_ERROR_MESSAGE_INTERNAL_ERROR, id, "uri", uri->valuestring);
    }
    return esp_mcp_reply_result(mcp, id, "{}");
}

static esp_err_t esp_mcp_handle_resources_unsubscribe(esp_mcp_t *mcp, const cJSON *params, const cJSON *id)
{
    ESP_RETURN_ON_FALSE(params && cJSON_IsObject(params), ESP_ERR_INVALID_ARG, TAG, "Invalid params");
    cJSON *uri = cJSON_GetObjectItem(params, "uri");
    if (!uri || !cJSON_IsString(uri) || !uri->valuestring) {
        return esp_mcp_reply_invalid_params(mcp, id, "resources/unsubscribe", "params.uri", "params.uri");
    }
    if (!mcp->request_session_id[0]) {
        return esp_mcp_reply_error_with_kv(mcp, MCP_ERROR_CODE_INVALID_REQUEST, MCP_ERROR_MESSAGE_INVALID_REQUEST, id, "reason", "missing_session");
    }
    esp_err_t ret = esp_mcp_resource_subscription_remove(mcp, mcp->request_session_id, uri->valuestring);
    if (ret != ESP_OK) {
        return esp_mcp_reply_error_with_kv(mcp, MCP_ERROR_CODE_INTERNAL_ERROR, MCP_ERROR_MESSAGE_INTERNAL_ERROR, id, "uri", uri->valuestring);
    }
    return esp_mcp_reply_result(mcp, id, "{}");
}

static esp_err_t esp_mcp_prompts_list_cb(esp_mcp_prompt_t *prompt, void *arg)
{
    esp_mcp_json_cursor_ctx_t *ctx = (esp_mcp_json_cursor_ctx_t *)arg;

    if (ctx->should_break) {
        return ESP_OK;
    }

    cJSON *o = esp_mcp_prompt_to_json(prompt);
    if (!o) {
        return ESP_OK;
    }

    cJSON *cursor_item = cJSON_GetObjectItem(o, ctx->key_name);
    if (ctx->cursor && !ctx->found_cursor) {
        if (cursor_item && cJSON_IsString(cursor_item) && cursor_item->valuestring &&
                strcmp(cursor_item->valuestring, ctx->cursor) == 0) {
            ctx->found_cursor = true;
        } else {
            cJSON_Delete(o);
            return ESP_OK;
        }
    }

    if (ctx->limit > 0 && ctx->emitted >= ctx->limit) {
        if (cursor_item && cJSON_IsString(cursor_item) && cursor_item->valuestring && !ctx->next_cursor[0]) {
            esp_mcp_copy_str(ctx->next_cursor, sizeof(ctx->next_cursor), cursor_item->valuestring);
        }
        cJSON_Delete(o);
        ctx->should_break = true;
        return ESP_OK;
    }

    cJSON_AddItemToArray(ctx->array, o);
    ctx->emitted++;
    return ESP_OK;
}

static esp_err_t esp_mcp_handle_prompts_list(esp_mcp_t *mcp, const cJSON *params, const cJSON *id)
{
    esp_mcp_prompt_list_t *list = (esp_mcp_prompt_list_t *)mcp->prompts;
    const char *cursor = NULL;
    size_t limit = 32;
    esp_err_t parse_ret = esp_mcp_parse_paginated_request(params, id, mcp, "prompts/list", true, &cursor, &limit);
    if (parse_ret != ESP_OK) {
        return parse_ret;
    }

    cJSON *result = cJSON_CreateObject();
    cJSON *arr = cJSON_CreateArray();
    if (!result || !arr) {
        if (result) {
            cJSON_Delete(result);
        }
        if (arr) {
            cJSON_Delete(arr);
        }
        ESP_LOGE(TAG, "No mem");
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddItemToObject(result, "prompts", arr);

    if (list) {
        esp_mcp_json_cursor_ctx_t ctx = {
            .array = arr,
            .cursor = cursor,
            .found_cursor = (cursor == NULL),
            .key_name = "name",
            .limit = limit,
            .emitted = 0,
            .should_break = false,
            .next_cursor = {0},
        };
        (void)esp_mcp_prompt_list_foreach(list, esp_mcp_prompts_list_cb, &ctx);
        if (ctx.cursor && !ctx.found_cursor) {
            cJSON_Delete(result);
            return esp_mcp_reply_invalid_params(mcp, id, "prompts/list", "known params.cursor", "params.cursor");
        }
        if (ctx.next_cursor[0]) {
            cJSON_AddStringToObject(result, "nextCursor", ctx.next_cursor);
        }
    }

    char *s = cJSON_PrintUnformatted(result);
    cJSON_Delete(result);
    ESP_RETURN_ON_FALSE(s, ESP_ERR_NO_MEM, TAG, "Print failed");
    esp_err_t r = esp_mcp_reply_result(mcp, id, s);
    cJSON_free(s);
    return r;
}

static esp_err_t esp_mcp_handle_prompts_get(esp_mcp_t *mcp, const cJSON *params, const cJSON *id)
{
    if (!(params && cJSON_IsObject(params))) {
        return esp_mcp_reply_invalid_params(mcp, id, "prompts/get", "params.name", "params");
    }
    cJSON *name = cJSON_GetObjectItem(params, "name");
    if (!(name && cJSON_IsString(name))) {
        return esp_mcp_reply_invalid_params(mcp, id, "prompts/get", "params.name", "params.name");
    }
    cJSON *arguments = cJSON_GetObjectItem(params, "arguments");
    char *args_json = NULL;
    if (arguments && cJSON_IsObject(arguments)) {
        args_json = cJSON_PrintUnformatted(arguments);
    }

    esp_mcp_prompt_list_t *list = (esp_mcp_prompt_list_t *)mcp->prompts;
    esp_mcp_prompt_t *p = list ? esp_mcp_prompt_list_find(list, name->valuestring) : NULL;
    if (!p) {
        if (args_json) {
            cJSON_free(args_json);
        }
        return esp_mcp_reply_not_found(mcp, id, "prompts/get", "name", name->valuestring, "Prompt not found");
    }

    char *desc = NULL;
    char *messages_json = NULL;
    esp_err_t pr = esp_mcp_prompt_get_messages(p, args_json, &desc, &messages_json);
    if (args_json) {
        cJSON_free(args_json);
    }
    if (pr != ESP_OK) {
        free(desc); free(messages_json);
        return esp_mcp_reply_error_with_method_reason(mcp,
                                                      MCP_ERROR_CODE_INTERNAL_ERROR,
                                                      "Prompt render failed",
                                                      id,
                                                      "prompts/get",
                                                      esp_err_to_name(pr));
    }

    cJSON *result = cJSON_CreateObject();
    if (!result) {
        free(desc); free(messages_json);
        return esp_mcp_reply_internal_error_with_method(mcp, id, "prompts/get", "response alloc failed");
    }
    if (desc) {
        cJSON_AddStringToObject(result, "description", desc);
    }
    if (messages_json) {
        cJSON *msgs = cJSON_Parse(messages_json);
        if (!msgs) {
            ESP_LOGW(TAG, "prompts/get: invalid messages JSON from prompt callback");
            cJSON_AddItemToObject(result, "messages", cJSON_CreateArray());
        } else if (cJSON_IsArray(msgs)) {
            cJSON_AddItemToObject(result, "messages", msgs);
        } else {
            ESP_LOGW(TAG, "prompts/get: prompt callback returned non-array messages");
            cJSON_Delete(msgs);
            cJSON_AddItemToObject(result, "messages", cJSON_CreateArray());
        }
    } else {
        cJSON_AddItemToObject(result, "messages", cJSON_CreateArray());
    }
    free(desc); free(messages_json);

    char *s = cJSON_PrintUnformatted(result);
    cJSON_Delete(result);
    ESP_RETURN_ON_FALSE(s, ESP_ERR_NO_MEM, TAG, "Print failed");
    esp_err_t r = esp_mcp_reply_result(mcp, id, s);
    cJSON_free(s);
    return r;
}

static esp_err_t esp_mcp_handle_completion_complete(esp_mcp_t *mcp, const cJSON *params, const cJSON *id)
{
    if (!(params && cJSON_IsObject(params))) {
        return esp_mcp_reply_invalid_params(mcp, id, "completion/complete", "params.ref+params.argument", "params");
    }

    cJSON *ref = cJSON_GetObjectItem(params, "ref");
    cJSON *argument = cJSON_GetObjectItem(params, "argument");
    if (!(ref && cJSON_IsObject(ref) && argument && cJSON_IsObject(argument))) {
        return esp_mcp_reply_invalid_params(mcp, id, "completion/complete", "params.ref+params.argument", "params");
    }

    cJSON *ref_type = cJSON_GetObjectItem(ref, "type");
    if (!(ref_type && cJSON_IsString(ref_type))) {
        return esp_mcp_reply_invalid_params(mcp, id, "completion/complete", "params.ref.type", "params.ref.type");
    }

    const char *rt = ref_type->valuestring;
    const char *name_or_uri = NULL;
    if (strcmp(rt, "ref/prompt") == 0) {
        cJSON *n = cJSON_GetObjectItem(ref, "name");
        if (!(n && cJSON_IsString(n))) {
            return esp_mcp_reply_invalid_params(mcp, id, "completion/complete", "params.ref.name", "params.ref.name");
        }
        name_or_uri = n->valuestring;
    } else if (strcmp(rt, "ref/resource") == 0) {
        cJSON *u = cJSON_GetObjectItem(ref, "uri");
        if (!(u && cJSON_IsString(u))) {
            return esp_mcp_reply_invalid_params(mcp, id, "completion/complete", "params.ref.uri", "params.ref.uri");
        }
        name_or_uri = u->valuestring;
    } else {
        return esp_mcp_reply_invalid_params(mcp, id, "completion/complete", "ref/prompt|ref/resource", "params.ref.type");
    }

    cJSON *an = cJSON_GetObjectItem(argument, "name");
    cJSON *av = cJSON_GetObjectItem(argument, "value");
    if (!(an && cJSON_IsString(an) && av && cJSON_IsString(av))) {
        return esp_mcp_reply_invalid_params(mcp, id, "completion/complete", "params.argument.name+params.argument.value", "params.argument");
    }

    char *ctx_args_json = NULL;
    cJSON *ctx = cJSON_GetObjectItem(params, "context");
    if (ctx && cJSON_IsObject(ctx)) {
        cJSON *args = cJSON_GetObjectItem(ctx, "arguments");
        if (args && cJSON_IsObject(args)) {
            ctx_args_json = cJSON_PrintUnformatted(args);
        }
    }

    cJSON *result_obj = NULL;
    const esp_mcp_completion_provider_t *p = (const esp_mcp_completion_provider_t *)mcp->completion;
    if (!p) {
        if (ctx_args_json) {
            cJSON_free(ctx_args_json);
        }
        return esp_mcp_reply_error_with_kv(mcp,
                                           MCP_ERROR_CODE_METHOD_NOT_FOUND,
                                           MCP_ERROR_MESSAGE_METHOD_NOT_FOUND,
                                           id,
                                           "method",
                                           "completion/complete");
    }
    esp_err_t cr = esp_mcp_completion_complete(p, rt, name_or_uri, an->valuestring, av->valuestring, ctx_args_json, &result_obj);
    if (ctx_args_json) {
        cJSON_free(ctx_args_json);
    }
    if (cr != ESP_OK || !result_obj) {
        if (result_obj) {
            cJSON_Delete(result_obj);
        }
        return esp_mcp_reply_internal_error_with_method(mcp, id, "completion/complete", "completion callback failed");
    }

    char *s = cJSON_PrintUnformatted(result_obj);
    cJSON_Delete(result_obj);
    ESP_RETURN_ON_FALSE(s, ESP_ERR_NO_MEM, TAG, "Print failed");
    esp_err_t r = esp_mcp_reply_result(mcp, id, s);
    cJSON_free(s);
    return r;
}

static esp_err_t esp_mcp_handle_logging_set_level(esp_mcp_t *mcp, const cJSON *params, const cJSON *id)
{
    if (!(mcp && params && cJSON_IsObject(params))) {
        return esp_mcp_reply_invalid_params(mcp, id, "logging/setLevel", "params.level", "params");
    }
    cJSON *level = cJSON_GetObjectItem(params, "level");
    if (!(level && cJSON_IsString(level) && level->valuestring)) {
        return esp_mcp_reply_invalid_params(mcp, id, "logging/setLevel", "params.level", "params.level");
    }
    const char *normalized_level = esp_mcp_normalize_log_level(level->valuestring);
    if (!normalized_level) {
        return esp_mcp_reply_invalid_params(mcp,
                                            id,
                                            "logging/setLevel",
                                            "debug|info|notice|warning|error|critical|alert|emergency",
                                            "params.level");
    }
    esp_mcp_session_state_t *session_state = esp_mcp_get_request_session_state(mcp, true);
    if (session_state) {
        session_state->has_log_level = true;
        esp_mcp_copy_str(session_state->log_level, sizeof(session_state->log_level), normalized_level);
    }
    esp_mcp_copy_str(mcp->log_level, sizeof(mcp->log_level), normalized_level);
    return esp_mcp_reply_result(mcp, id, "{}");
}

static const char *esp_mcp_task_status_to_str(esp_mcp_task_status_t st)
{
    switch (st) {
    case ESP_MCP_TASK_WORKING: return "working";
    case ESP_MCP_TASK_COMPLETED: return "completed";
    case ESP_MCP_TASK_FAILED: return "failed";
    case ESP_MCP_TASK_CANCELLED: return "cancelled";
    case ESP_MCP_TASK_INPUT_REQUIRED: return "input_required";
    case ESP_MCP_TASK_CANCELLING: return "cancelling";
    default: return "unknown";
    }
}

typedef struct {
    const esp_mcp_t *mcp;
    cJSON *array;
    const char *cursor;
    char *next_cursor;
    bool found_cursor;
    bool should_break;
    size_t count;
    size_t max_count;
} esp_mcp_task_list_ctx_t;

static esp_err_t esp_mcp_tasks_list_cb(esp_mcp_task_t *task, void *arg)
{
    esp_mcp_task_list_ctx_t *ctx = (esp_mcp_task_list_ctx_t *)arg;

    if (ctx->should_break) {
        return ESP_OK;
    }

    if (!esp_mcp_task_visible_to_request_session(ctx->mcp, task)) {
        return ESP_OK;
    }

    const char *task_id = esp_mcp_task_id(task);

    if (ctx->cursor && !ctx->found_cursor) {
        if (strcmp(task_id, ctx->cursor) == 0) {
            ctx->found_cursor = true;
        } else {
            return ESP_OK;
        }
    }

    if (ctx->max_count > 0 && ctx->count >= ctx->max_count) {
        strncpy(ctx->next_cursor, task_id, 255);
        ctx->next_cursor[255] = '\0';
        ctx->should_break = true;
        return ESP_OK;
    }

    cJSON *t = cJSON_CreateObject();
    if (!t) {
        return ESP_OK;
    }
    if (esp_mcp_task_add_json(t, task) != ESP_OK) {
        cJSON_Delete(t);
        return ESP_OK;
    }

    cJSON_AddItemToArray(ctx->array, t);
    ctx->count++;
    return ESP_OK;
}

static esp_err_t esp_mcp_handle_tasks_list(esp_mcp_t *mcp, const cJSON *params, const cJSON *id)
{
    esp_mcp_task_store_t *store = (esp_mcp_task_store_t *)mcp->tasks;

    if (store) {
        (void)esp_mcp_task_store_cleanup_expired(store);
    }

    const char *cursor_value = NULL;
    char cursor_str[256] = {0};
    char next_cursor[256] = {0};
    esp_err_t parse_ret = esp_mcp_parse_paginated_request(params, id, mcp, "tasks/list", false, &cursor_value, NULL);
    if (parse_ret != ESP_OK) {
        return parse_ret;
    }
    if (cursor_value) {
        strncpy(cursor_str, cursor_value, sizeof(cursor_str) - 1);
        cursor_str[sizeof(cursor_str) - 1] = '\0';
    }

    cJSON *result = cJSON_CreateObject();
    cJSON *arr = cJSON_CreateArray();
    if (!result || !arr) {
        if (result) {
            cJSON_Delete(result);
        }
        if (arr) {
            cJSON_Delete(arr);
        }
        return esp_mcp_reply_internal_error_with_method(mcp, id, "tasks/list", "response alloc failed");
    }
    cJSON_AddItemToObject(result, "tasks", arr);
    if (store) {
        esp_mcp_task_list_ctx_t ctx = {
            .mcp = mcp,
            .array = arr,
            .cursor = cursor_str[0] ? cursor_str : NULL,
            .next_cursor = next_cursor,
            .found_cursor = (cursor_str[0] == '\0'),
            .should_break = false,
            .count = 0,
            .max_count = CONFIG_MCP_TASKS_LIST_MAX_COUNT
        };
        esp_err_t foreach_ret = esp_mcp_task_store_foreach(store, esp_mcp_tasks_list_cb, &ctx);
        if (foreach_ret != ESP_OK) {
            cJSON_Delete(result);
            return esp_mcp_reply_internal_error_with_method(mcp, id, "tasks/list", "task enumeration failed");
        }
        if (ctx.cursor && !ctx.found_cursor) {
            cJSON_Delete(result);
            return esp_mcp_reply_invalid_params(mcp, id, "tasks/list", "known params.cursor", "params.cursor");
        }
        if (next_cursor[0]) {
            cJSON_AddStringToObject(result, "nextCursor", next_cursor);
        }
    }
    char *s = cJSON_PrintUnformatted(result);
    cJSON_Delete(result);
    if (!s) {
        return esp_mcp_reply_internal_error_with_method(mcp, id, "tasks/list", "json print failed");
    }
    esp_err_t r = esp_mcp_reply_result(mcp, id, s);
    cJSON_free(s);
    return r;
}

static esp_err_t esp_mcp_handle_tasks_get(esp_mcp_t *mcp, const cJSON *params, const cJSON *id)
{
    ESP_RETURN_ON_FALSE(params && cJSON_IsObject(params), ESP_ERR_INVALID_ARG, TAG, "Invalid params");
    const char *task_id = esp_mcp_tasks_get_param_id(params);
    if (!task_id) {
        return esp_mcp_reply_invalid_params(mcp, id, "tasks/get", "params.taskId", "params.taskId");
    }

    esp_mcp_task_store_t *store = (esp_mcp_task_store_t *)mcp->tasks;
    esp_mcp_task_t *task = store ? esp_mcp_task_find(store, task_id) : NULL;
    if (!task || !esp_mcp_task_visible_to_request_session(mcp, task)) {
        return esp_mcp_reply_invalid_params(mcp, id, "tasks/get", "known params.taskId", "params.taskId");
    }

    cJSON *result = cJSON_CreateObject();
    if (!result) {
        return esp_mcp_reply_internal_error_with_method(mcp, id, "tasks/get", "response alloc failed");
    }
    if (esp_mcp_task_add_json(result, task) != ESP_OK) {
        cJSON_Delete(result);
        return esp_mcp_reply_internal_error_with_method(mcp, id, "tasks/get", "task json failed");
    }

    char *s = cJSON_PrintUnformatted(result);
    cJSON_Delete(result);
    if (!s) {
        return esp_mcp_reply_internal_error_with_method(mcp, id, "tasks/get", "json print failed");
    }
    esp_err_t r = esp_mcp_reply_result(mcp, id, s);
    cJSON_free(s);
    return r;
}

static esp_err_t esp_mcp_handle_tasks_cancel(esp_mcp_t *mcp, const cJSON *params, const cJSON *id)
{
    if (!(params && cJSON_IsObject(params))) {
        return esp_mcp_reply_invalid_params(mcp, id, "tasks/cancel", "params.taskId", "params");
    }
    const char *task_id = esp_mcp_tasks_get_param_id(params);
    if (!task_id) {
        return esp_mcp_reply_invalid_params(mcp, id, "tasks/cancel", "params.taskId", "params.taskId");
    }

    esp_mcp_task_store_t *store = (esp_mcp_task_store_t *)mcp->tasks;
    esp_mcp_task_t *task = store ? esp_mcp_task_find(store, task_id) : NULL;
    if (!task || !esp_mcp_task_visible_to_request_session(mcp, task)) {
        return esp_mcp_reply_invalid_params(mcp, id, "tasks/cancel", "known params.taskId", "params.taskId");
    }

    esp_mcp_task_status_t before = esp_mcp_task_get_status(task);
    if (esp_mcp_task_status_is_terminal(before)) {
        return esp_mcp_reply_invalid_params(mcp, id, "tasks/cancel", "non-terminal params.taskId", "params.taskId");
    }
    if (esp_mcp_task_request_cancel(task) != ESP_OK) {
        return esp_mcp_reply_invalid_params(mcp, id, "tasks/cancel", "non-terminal params.taskId", "params.taskId");
    }
    cJSON *result = cJSON_CreateObject();
    if (!result) {
        return esp_mcp_reply_internal_error_with_method(mcp, id, "tasks/cancel", "response alloc failed");
    }
    if (esp_mcp_task_add_json(result, task) != ESP_OK) {
        cJSON_Delete(result);
        return esp_mcp_reply_internal_error_with_method(mcp, id, "tasks/cancel", "task json failed");
    }

    cJSON_AddBoolToObject(result, "cancelAccepted",
                          before == ESP_MCP_TASK_WORKING || before == ESP_MCP_TASK_CANCELLING);
    char *s = cJSON_PrintUnformatted(result);
    cJSON_Delete(result);
    if (!s) {
        return esp_mcp_reply_internal_error_with_method(mcp, id, "tasks/cancel", "json print failed");
    }
    esp_err_t rr = esp_mcp_reply_result(mcp, id, s);
    cJSON_free(s);
    return rr;
}

static esp_err_t esp_mcp_handle_tasks_result(esp_mcp_t *mcp, const cJSON *params, const cJSON *id)
{
    if (!(params && cJSON_IsObject(params))) {
        return esp_mcp_reply_invalid_params(mcp, id, "tasks/result", "params.taskId", "params");
    }
    const char *task_id = esp_mcp_tasks_get_param_id(params);
    if (!task_id) {
        return esp_mcp_reply_invalid_params(mcp, id, "tasks/result", "params.taskId", "params.taskId");
    }

    esp_mcp_task_store_t *store = (esp_mcp_task_store_t *)mcp->tasks;
    char task_id_buf[64] = {0};
    esp_mcp_copy_str(task_id_buf, sizeof(task_id_buf), task_id);

    esp_mcp_task_t *task = store ? esp_mcp_task_find(store, task_id_buf) : NULL;
    if (!task) {
        return esp_mcp_reply_not_found(mcp, id, "tasks/result", "taskId", task_id_buf, "Task not found");
    }
    if (!esp_mcp_task_visible_to_request_session(mcp, task)) {
        return esp_mcp_reply_invalid_params(mcp, id, "tasks/result", "known params.taskId", "params.taskId");
    }
    esp_mcp_task_status_t st = esp_mcp_task_get_status(task);
    if (!esp_mcp_task_status_is_terminal(st)) {
        return esp_mcp_reply_error_with_method_reason(mcp,
                                                      -32000,
                                                      "Task not complete",
                                                      id,
                                                      "tasks/result",
                                                      "task still in progress (poll using pollInterval)");
    }
    if (st == ESP_MCP_TASK_CANCELLED) {
        return esp_mcp_reply_error_with_method_reason(mcp, -32000, "Task cancelled", id, "tasks/result", "task cancelled");
    }

    char *rj = esp_mcp_task_result_json(task);
    if (!rj) {
        return esp_mcp_reply_internal_error_with_method(mcp, id, "tasks/result", "result missing");
    }
    if (esp_mcp_task_result_is_error_response(task)) {
        esp_err_t err = esp_mcp_reply_error_ex(mcp, 0, rj, id, NULL);
        free(rj);
        return err;
    }
    // Return underlying tools/call result object directly
    esp_err_t rr = esp_mcp_reply_result_with_related_task(mcp, id, rj, esp_mcp_task_id(task));
    free(rj);
    return rr;
}

esp_err_t esp_mcp_get_mbuf(esp_mcp_t *mcp, uint8_t **outbuf, uint16_t *outlen)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid server");
    ESP_RETURN_ON_FALSE(outbuf, ESP_ERR_INVALID_ARG, TAG, "Invalid outbuf");
    ESP_RETURN_ON_FALSE(outlen, ESP_ERR_INVALID_ARG, TAG, "Invalid outlen");

    *outbuf = mcp->mbuf.outbuf;
    *outlen = mcp->mbuf.outlen;

    return ESP_OK;
}

esp_err_t esp_mcp_remove_mbuf(esp_mcp_t *mcp, const uint8_t *outbuf)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid server");

    esp_err_t ret = ESP_ERR_NOT_FOUND;
    if (mcp->mbuf.outbuf == outbuf) {
        cJSON_free(mcp->mbuf.outbuf);
        mcp->mbuf.outbuf = NULL;
        mcp->mbuf.outlen = 0;
        mcp->mbuf.id = 0;
        ret = ESP_OK;
    }

    return ret;
}

esp_err_t esp_mcp_create(esp_mcp_t **mcp)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP engine");

    esp_mcp_t *temp_mcp = (esp_mcp_t *)calloc(1, sizeof(esp_mcp_t));
    ESP_RETURN_ON_FALSE(temp_mcp, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for MCP engine");

    temp_mcp->tools = esp_mcp_tool_list_create();
    if (!temp_mcp->tools) {
        ESP_LOGE(TAG, "Failed to allocate memory for MCP tools");
        free(temp_mcp);
        return ESP_ERR_NO_MEM;
    }

    temp_mcp->resources = esp_mcp_resource_list_create();
    if (!temp_mcp->resources) {
        esp_mcp_tool_list_destroy(temp_mcp->tools);
        free(temp_mcp);
        return ESP_ERR_NO_MEM;
    }
    temp_mcp->prompts = esp_mcp_prompt_list_create();
    if (!temp_mcp->prompts) {
        esp_mcp_resource_list_destroy((esp_mcp_resource_list_t *)temp_mcp->resources);
        esp_mcp_tool_list_destroy(temp_mcp->tools);
        free(temp_mcp);
        return ESP_ERR_NO_MEM;
    }
    temp_mcp->completion = NULL;
    temp_mcp->tasks = esp_mcp_task_store_create();
    if (!temp_mcp->tasks) {
        esp_mcp_prompt_list_destroy((esp_mcp_prompt_list_t *)temp_mcp->prompts);
        esp_mcp_resource_list_destroy((esp_mcp_resource_list_t *)temp_mcp->resources);
        esp_mcp_tool_list_destroy(temp_mcp->tools);
        free(temp_mcp);
        return ESP_ERR_NO_MEM;
    }
    esp_mcp_task_store_set_status_cb((esp_mcp_task_store_t *)temp_mcp->tasks,
                                     esp_mcp_task_status_changed_cb,
                                     temp_mcp);

    temp_mcp->mutex = xSemaphoreCreateMutex();
    if (!temp_mcp->mutex) {
        esp_mcp_prompt_list_destroy((esp_mcp_prompt_list_t *)temp_mcp->prompts);
        esp_mcp_resource_list_destroy((esp_mcp_resource_list_t *)temp_mcp->resources);
        esp_mcp_task_store_destroy((esp_mcp_task_store_t *)temp_mcp->tasks);
        esp_mcp_tool_list_destroy(temp_mcp->tools);
        free(temp_mcp);
        return ESP_ERR_NO_MEM;
    }

    temp_mcp->resource_sub_mutex = xSemaphoreCreateMutex();
    if (!temp_mcp->resource_sub_mutex) {
        vSemaphoreDelete(temp_mcp->mutex);
        esp_mcp_prompt_list_destroy((esp_mcp_prompt_list_t *)temp_mcp->prompts);
        esp_mcp_resource_list_destroy((esp_mcp_resource_list_t *)temp_mcp->resources);
        esp_mcp_task_store_destroy((esp_mcp_task_store_t *)temp_mcp->tasks);
        esp_mcp_tool_list_destroy(temp_mcp->tools);
        free(temp_mcp);
        return ESP_ERR_NO_MEM;
    }

    temp_mcp->req_id_mutex = xSemaphoreCreateMutex();
    if (!temp_mcp->req_id_mutex) {
        vSemaphoreDelete(temp_mcp->resource_sub_mutex);
        vSemaphoreDelete(temp_mcp->mutex);
        esp_mcp_prompt_list_destroy((esp_mcp_prompt_list_t *)temp_mcp->prompts);
        esp_mcp_resource_list_destroy((esp_mcp_resource_list_t *)temp_mcp->resources);
        esp_mcp_task_store_destroy((esp_mcp_task_store_t *)temp_mcp->tasks);
        esp_mcp_tool_list_destroy(temp_mcp->tools);
        free(temp_mcp);
        return ESP_ERR_NO_MEM;
    }

    *mcp = temp_mcp;
    temp_mcp->client_initialized = false;
    temp_mcp->request_authenticated = true;
    temp_mcp->incoming_request_handler = NULL;
    temp_mcp->incoming_request_ctx = NULL;
    temp_mcp->incoming_notification_handler = NULL;
    temp_mcp->incoming_notification_ctx = NULL;
    temp_mcp->destroy_requested = false;
    temp_mcp->tool_worker_count = 0;

    return ESP_OK;
}

esp_err_t esp_mcp_destroy(esp_mcp_t *mcp)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP engine");

    bool mutex_held = false;
    mcp->destroy_requested = true;
    if (mcp->mutex) {
        if (xSemaphoreTake(mcp->mutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
            mutex_held = true;
        } else {
            ESP_LOGW(TAG, "Destroy: mutex busy; proceeding with teardown");
        }
    }

    int wait_loops = 500;
    while (__atomic_load_n(&mcp->tool_worker_count, __ATOMIC_SEQ_CST) > 0 && wait_loops-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (__atomic_load_n(&mcp->tool_worker_count, __ATOMIC_SEQ_CST) > 0) {
        ESP_LOGW(TAG, "Destroy: %" PRIu32 " tool workers still running", (uint32_t)__atomic_load_n(&mcp->tool_worker_count, __ATOMIC_SEQ_CST));
    }

    if (mcp->completion) {
        esp_mcp_completion_provider_destroy((esp_mcp_completion_provider_t *)mcp->completion);
        mcp->completion = NULL;
    }
    if (mcp->tasks) {
        esp_mcp_task_store_destroy((esp_mcp_task_store_t *)mcp->tasks);
        mcp->tasks = NULL;
    }
    if (mcp->prompts) {
        esp_mcp_prompt_list_destroy((esp_mcp_prompt_list_t *)mcp->prompts);
        mcp->prompts = NULL;
    }
    if (mcp->resources) {
        esp_mcp_resource_list_destroy((esp_mcp_resource_list_t *)mcp->resources);
        mcp->resources = NULL;
    }

    esp_err_t ret = esp_mcp_tool_list_destroy(mcp->tools);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error destroying tool list: %s", esp_err_to_name(ret));
    }

    if (mcp->mbuf.outbuf) {
        cJSON_free(mcp->mbuf.outbuf);
        mcp->mbuf.outbuf = NULL;
        mcp->mbuf.outlen = 0;
        mcp->mbuf.id = 0;
    }

    if (mcp->server_title) {
        free(mcp->server_title);
        mcp->server_title = NULL;
    }
    if (mcp->server_description) {
        free(mcp->server_description);
        mcp->server_description = NULL;
    }
    if (mcp->server_icons_json) {
        free(mcp->server_icons_json);
        mcp->server_icons_json = NULL;
    }
    if (mcp->server_website_url) {
        free(mcp->server_website_url);
        mcp->server_website_url = NULL;
    }
    if (mcp->instructions) {
        free(mcp->instructions);
        mcp->instructions = NULL;
    }
    if (mcp->request_meta_json) {
        free(mcp->request_meta_json);
        mcp->request_meta_json = NULL;
    }

    if (mcp->resource_sub_mutex) {
        if (xSemaphoreTake(mcp->resource_sub_mutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
            (void)xSemaphoreGive(mcp->resource_sub_mutex);
        } else {
            ESP_LOGW(TAG, "Destroy: resource subscription mutex busy");
        }
        vSemaphoreDelete(mcp->resource_sub_mutex);
        mcp->resource_sub_mutex = NULL;
    }

    if (mcp->req_id_mutex) {
        if (xSemaphoreTake(mcp->req_id_mutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
            (void)xSemaphoreGive(mcp->req_id_mutex);
        }
        vSemaphoreDelete(mcp->req_id_mutex);
        mcp->req_id_mutex = NULL;
    }

    if (mcp->mutex) {
        if (mutex_held) {
            (void)xSemaphoreGive(mcp->mutex);
        }
        vSemaphoreDelete(mcp->mutex);
        mcp->mutex = NULL;
    }

    free(mcp);

    return ESP_OK;
}

esp_err_t esp_mcp_free_response(esp_mcp_t *mcp, uint8_t *outbuf)
{
    (void)mcp;
    if (!outbuf) {
        return ESP_OK;
    }
    free(outbuf);
    return ESP_OK;
}

esp_err_t esp_mcp_set_server_info(esp_mcp_t *mcp, const char *title, const char *description, const char *icons_json, const char *website_url)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP instance");

    char *new_title = NULL;
    char *new_desc = NULL;
    char *new_icons = NULL;
    char *new_url = NULL;

    if (title) {
        new_title = strdup(title);
        ESP_RETURN_ON_FALSE(new_title, ESP_ERR_NO_MEM, TAG, "Failed to allocate server_title");
    }
    if (description) {
        new_desc = strdup(description);
        if (!new_desc) {
            free(new_title);
            return ESP_ERR_NO_MEM;
        }
    }
    if (icons_json) {
        new_icons = strdup(icons_json);
        if (!new_icons) {
            free(new_title);
            free(new_desc);
            return ESP_ERR_NO_MEM;
        }
    }
    if (website_url) {
        new_url = strdup(website_url);
        if (!new_url) {
            free(new_title);
            free(new_desc);
            free(new_icons);
            return ESP_ERR_NO_MEM;
        }
    }

    free(mcp->server_title);
    free(mcp->server_description);
    free(mcp->server_icons_json);
    free(mcp->server_website_url);
    mcp->server_title = new_title;
    mcp->server_description = new_desc;
    mcp->server_icons_json = new_icons;
    mcp->server_website_url = new_url;

    return ESP_OK;
}

esp_err_t esp_mcp_set_instructions(esp_mcp_t *mcp, const char *instructions)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP instance");

    if (mcp->instructions) {
        free(mcp->instructions);
        mcp->instructions = NULL;
    }

    if (instructions) {
        mcp->instructions = strdup(instructions);
        ESP_RETURN_ON_FALSE(mcp->instructions, ESP_ERR_NO_MEM, TAG, "Failed to allocate instructions");
    }

    return ESP_OK;
}

esp_err_t esp_mcp_handle_message(esp_mcp_t *mcp, const uint8_t *inbuf, uint16_t inlen, uint8_t **outbuf, uint16_t *outlen)
{
    ESP_RETURN_ON_FALSE(mcp && inbuf && inlen > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    ESP_RETURN_ON_FALSE(outbuf && outlen, ESP_ERR_INVALID_ARG, TAG, "Invalid out args");

    esp_err_t ret = ESP_OK;
    bool mutex_held = false;
    *outbuf = NULL;
    *outlen = 0;

    if (mcp->mutex) {
        ESP_GOTO_ON_FALSE(xSemaphoreTake(mcp->mutex, portMAX_DELAY) == pdTRUE,
                          ESP_ERR_INVALID_STATE,
                          cleanup,
                          TAG,
                          "Mutex take failed");
        mutex_held = true;
    }

    // Reuse existing parser which fills legacy mbuf, then copy into independent buffer.
    ret = esp_mcp_parse_message(mcp, (const char *)inbuf);
    uint8_t *tmp = NULL;
    uint16_t tmplen = 0;
    // Even when parser returns an error code, it may have produced a valid JSON-RPC error payload.
    // Always drain mbuf so transport can return protocol-level errors to the caller.
    (void)esp_mcp_get_mbuf(mcp, &tmp, &tmplen);
    if (tmp && tmplen > 0) {
        uint8_t *copy = (uint8_t *)calloc(1, (size_t)tmplen + 1);
        if (copy) {
            memcpy(copy, tmp, tmplen);
            *outbuf = copy;
            *outlen = tmplen;
        } else {
            ret = ESP_ERR_NO_MEM;
        }
        (void)esp_mcp_remove_mbuf(mcp, tmp);
    }

cleanup:
    if (mcp->mutex && mutex_held) {
        xSemaphoreGive(mcp->mutex);
    }
    return ret;
}

esp_err_t esp_mcp_add_tool(esp_mcp_t *mcp, esp_mcp_tool_t *tool)
{
    ESP_RETURN_ON_FALSE(mcp && tool, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP engine or tool");

    esp_mcp_tool_t *existing_tool = esp_mcp_tool_list_find_tool(mcp->tools, tool->name);
    if (existing_tool) {
        ESP_LOGW(TAG, "Tool %s already added", tool->name);
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_mcp_tool_list_add_tool(mcp->tools, tool);
    if (ret == ESP_OK) {
        esp_mcp_emit_notification_no_params(mcp, "notifications/tools/list_changed");
    }
    return ret;
}

esp_err_t esp_mcp_remove_tool(esp_mcp_t *mcp, esp_mcp_tool_t *tool)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP engine");
    ESP_RETURN_ON_FALSE(tool, ESP_ERR_INVALID_ARG, TAG, "Invalid tool");

    esp_err_t ret = esp_mcp_tool_list_remove_tool(mcp->tools, tool);
    if (ret == ESP_OK) {
        esp_mcp_emit_notification_no_params(mcp, "notifications/tools/list_changed");
    }
    return ret;
}

esp_err_t esp_mcp_add_resource(esp_mcp_t *mcp, esp_mcp_resource_t *res)
{
    ESP_RETURN_ON_FALSE(mcp && res, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    esp_mcp_resource_list_t *list = (esp_mcp_resource_list_t *)mcp->resources;
    ESP_RETURN_ON_FALSE(list, ESP_ERR_INVALID_STATE, TAG, "Resource list not initialized");
    esp_err_t ret = esp_mcp_resource_list_add(list, res);
    if (ret == ESP_OK) {
        esp_mcp_emit_notification_no_params(mcp, "notifications/resources/list_changed");
        const char *uri = esp_mcp_resource_get_uri(res);
        if (uri) {
            esp_mcp_emit_resource_updated(mcp, uri);
        }
    }
    return ret;
}

esp_err_t esp_mcp_remove_resource(esp_mcp_t *mcp, esp_mcp_resource_t *res)
{
    ESP_RETURN_ON_FALSE(mcp && res, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    esp_mcp_resource_list_t *list = (esp_mcp_resource_list_t *)mcp->resources;
    ESP_RETURN_ON_FALSE(list, ESP_ERR_INVALID_STATE, TAG, "Resource list not initialized");
    esp_err_t ret = esp_mcp_resource_list_remove(list, res);
    if (ret == ESP_OK) {
        esp_mcp_emit_notification_no_params(mcp, "notifications/resources/list_changed");
        const char *uri = esp_mcp_resource_get_uri(res);
        if (uri) {
            esp_mcp_emit_resource_updated(mcp, uri);
        }
    }
    return ret;
}

esp_err_t esp_mcp_add_prompt(esp_mcp_t *mcp, esp_mcp_prompt_t *prompt)
{
    ESP_RETURN_ON_FALSE(mcp && prompt, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    esp_mcp_prompt_list_t *list = (esp_mcp_prompt_list_t *)mcp->prompts;
    ESP_RETURN_ON_FALSE(list, ESP_ERR_INVALID_STATE, TAG, "Prompt list not initialized");
    esp_err_t ret = esp_mcp_prompt_list_add(list, prompt);
    if (ret == ESP_OK) {
        esp_mcp_emit_notification_no_params(mcp, "notifications/prompts/list_changed");
    }
    return ret;
}

esp_err_t esp_mcp_remove_prompt(esp_mcp_t *mcp, esp_mcp_prompt_t *prompt)
{
    ESP_RETURN_ON_FALSE(mcp && prompt, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    esp_mcp_prompt_list_t *list = (esp_mcp_prompt_list_t *)mcp->prompts;
    ESP_RETURN_ON_FALSE(list, ESP_ERR_INVALID_STATE, TAG, "Prompt list not initialized");
    esp_err_t ret = esp_mcp_prompt_list_remove(list, prompt);
    if (ret == ESP_OK) {
        esp_mcp_emit_notification_no_params(mcp, "notifications/prompts/list_changed");
    }
    return ret;
}

esp_err_t esp_mcp_set_completion_provider(esp_mcp_t *mcp, esp_mcp_completion_provider_t *provider)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid mcp");
    if (mcp->completion) {
        esp_mcp_completion_provider_destroy((esp_mcp_completion_provider_t *)mcp->completion);
        mcp->completion = NULL;
    }
    mcp->completion = provider;
    return ESP_OK;
}

esp_err_t esp_mcp_set_request_context(esp_mcp_t *mcp,
                                      const char *session_id,
                                      const char *auth_subject,
                                      bool is_authenticated)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid mcp");
    mcp->request_authenticated = is_authenticated;
    if (session_id) {
        esp_mcp_copy_str(mcp->request_session_id, sizeof(mcp->request_session_id), session_id);
    } else {
        mcp->request_session_id[0] = '\0';
    }
    if (auth_subject) {
        esp_mcp_copy_str(mcp->request_auth_subject, sizeof(mcp->request_auth_subject), auth_subject);
    } else {
        mcp->request_auth_subject[0] = '\0';
    }
    return ESP_OK;
}

const char *esp_mcp_get_request_session_id(const esp_mcp_t *mcp)
{
    if (!mcp || !mcp->request_session_id[0]) {
        return NULL;
    }
    return mcp->request_session_id;
}

esp_err_t esp_mcp_clear_session_state(esp_mcp_t *mcp, const char *session_id)
{
    ESP_RETURN_ON_FALSE(mcp && session_id && session_id[0], ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    esp_mcp_resource_sub_lock(mcp);
    for (size_t i = 0; i < mcp->resource_subscriptions_count;) {
        if (strcmp(mcp->resource_subscriptions[i].session_id, session_id) == 0) {
            for (size_t j = i + 1; j < mcp->resource_subscriptions_count; ++j) {
                memcpy(&mcp->resource_subscriptions[j - 1],
                       &mcp->resource_subscriptions[j],
                       sizeof(mcp->resource_subscriptions[j - 1]));
            }
            mcp->resource_subscriptions_count--;
            continue;
        }
        i++;
    }
    esp_mcp_resource_sub_unlock(mcp);
    esp_mcp_req_id_lock(mcp);
    for (size_t i = 0; i < (sizeof(mcp->recent_request_ids) / sizeof(mcp->recent_request_ids[0])); ++i) {
        if (mcp->recent_request_ids[i].used &&
                strcmp(mcp->recent_request_ids[i].session_id, session_id) == 0) {
            mcp->recent_request_ids[i].used = false;
            mcp->recent_request_ids[i].session_id[0] = '\0';
            mcp->recent_request_ids[i].request_id[0] = '\0';
        }
    }
    esp_mcp_req_id_unlock(mcp);
    esp_mcp_session_state_t *state = esp_mcp_session_state_find(mcp, session_id);
    if (state) {
        memset(state, 0, sizeof(*state));
    }
    return ESP_OK;
}

static esp_err_t esp_mcp_parse_json_message(esp_mcp_t *mcp, const cJSON *json)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid server");
    ESP_RETURN_ON_FALSE(json, ESP_ERR_INVALID_ARG, TAG, "Invalid JSON");

    cJSON *version = cJSON_GetObjectItem(json, "jsonrpc");
    if (version == NULL || !cJSON_IsString(version) || strcmp(version->valuestring, "2.0") != 0) {
        ESP_LOGE(TAG, "Invalid JSONRPC version: %s", version ? version->valuestring : "null");
        esp_mcp_reply_invalid_request_with_reason(mcp, NULL, NULL, "jsonrpc must be 2.0");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *method = cJSON_GetObjectItem(json, "method");
    if (method == NULL || !cJSON_IsString(method)) {
        cJSON *resp_id = cJSON_GetObjectItem(json, "id");
        cJSON *resp_result = cJSON_GetObjectItem(json, "result");
        cJSON *resp_error = cJSON_GetObjectItem(json, "error");
        if (resp_id && (cJSON_IsNumber(resp_id) || cJSON_IsString(resp_id)) && (resp_result || resp_error)) {
            char *resp_json = cJSON_PrintUnformatted((cJSON *)json);
            if (resp_json) {
                size_t rlen = strlen(resp_json);
                if (rlen > UINT16_MAX) {
                    ESP_LOGE(TAG, "JSON-RPC response exceeds %" PRIu16 " bytes", (uint16_t)UINT16_MAX);
                    cJSON_free(resp_json);
                    return ESP_ERR_INVALID_SIZE;
                }
                if (mcp->mgr_ctx) {
                    (void)esp_mcp_mgr_perform_handle((esp_mcp_mgr_handle_t)mcp->mgr_ctx, (const uint8_t *)resp_json, (uint16_t)rlen);
                }
                cJSON_free(resp_json);
            }
            return ESP_OK;
        }
        ESP_LOGE(TAG, "Missing method");
        esp_mcp_reply_invalid_request_with_reason(mcp, NULL, NULL, "missing method");
        return ESP_ERR_INVALID_ARG;
    }

    const char *method_str = method->valuestring;
    const esp_mcp_session_state_t *request_state = esp_mcp_get_request_session_state_const(mcp);
    bool session_initialized = request_state && request_state->received_initialize_request;
    bool session_requires_initialized_notification = request_state && request_state->require_initialized_notification;
    bool session_received_initialized_notification = request_state && request_state->received_initialized_notification;

    if (!session_initialized &&
            !mcp->client_initialized &&
            strcmp(method_str, "initialize") != 0) {
        if (!strncmp(method_str, "notifications", strlen("notifications"))) {
            ESP_LOGW(TAG, "Notification ignored before initialize: %s", method_str);
            return ESP_ERR_INVALID_STATE;
        }
        cJSON *request_id = cJSON_GetObjectItem(json, "id");
        esp_mcp_reply_invalid_request_with_reason(mcp, request_id, method_str, "initialize must be first interaction");
        return ESP_ERR_INVALID_STATE;
    }

    if (!strncmp(method_str, "notifications", strlen("notifications"))) {
        if (cJSON_GetObjectItem(json, "id") != NULL) {
            ESP_LOGW(TAG, "Notification contains id and will be treated as notification: %s", method_str);
        }
        if (!strcmp(method_str, "notifications/initialized")) {
            esp_mcp_session_state_t *mutable_state = esp_mcp_get_request_session_state(mcp, true);
            if (mutable_state) {
                mutable_state->received_initialized_notification = true;
            }
        } else if (!strcmp(method_str, "notifications/cancelled")) {
            // Best-effort store cancellation info for long-running operations/tasks
            mcp->has_cancel_request = false;
            mcp->cancelled_request_id[0] = '\0';
            mcp->cancel_reason[0] = '\0';
            cJSON *params = cJSON_GetObjectItem(json, "params");
            if (params && cJSON_IsObject(params)) {
                cJSON *rid = cJSON_GetObjectItem(params, "requestId");
                cJSON *reason = cJSON_GetObjectItem(params, "reason");
                if (rid && (cJSON_IsString(rid) || cJSON_IsNumber(rid))) {
                    mcp->has_cancel_request = true;
                    char cancel_req_id[48] = {0};
                    if (cJSON_IsString(rid)) {
                        esp_mcp_copy_str(mcp->cancelled_request_id, sizeof(mcp->cancelled_request_id), rid->valuestring);
                        snprintf(cancel_req_id, sizeof(cancel_req_id), "%s", rid->valuestring);
                    } else {
                        snprintf(mcp->cancelled_request_id, sizeof(mcp->cancelled_request_id), "%d", rid->valueint);
                        snprintf(cancel_req_id, sizeof(cancel_req_id), "%d", rid->valueint);
                    }
                    if (cancel_req_id[0] && mcp->mgr_ctx) {
                        (void)esp_mcp_mgr_cancel_pending_request((esp_mcp_mgr_handle_t)mcp->mgr_ctx,
                                                                 cancel_req_id,
                                                                 mcp->request_session_id[0] ? mcp->request_session_id : NULL);
                    }
                }
                if (reason && cJSON_IsString(reason)) {
                    esp_mcp_copy_str(mcp->cancel_reason, sizeof(mcp->cancel_reason), reason->valuestring);
                }
            }
        } else if (!strcmp(method_str, "notifications/elicitation/complete")) {
            cJSON *params = cJSON_GetObjectItem(json, "params");
            esp_mcp_handle_elicitation_complete_notification(mcp, params);
        }
        esp_mcp_dispatch_incoming_notification(mcp, method_str, cJSON_GetObjectItem(json, "params"));
        ESP_LOGD(TAG, "Received notification (method: %s)", method_str);
        return ESP_OK;
    }

    cJSON *params = cJSON_GetObjectItem(json, "params");
    if (params != NULL && !cJSON_IsObject(params)) {
        ESP_LOGE(TAG, "Invalid params for method: %s", method_str);
        cJSON *early_id = cJSON_GetObjectItem(json, "id");
        return esp_mcp_reply_invalid_params(mcp, early_id, method_str, "params as object", "params");
    }

    // Capture per-request _meta (progressToken / related-task)
    mcp->has_progress_token = false;
    mcp->progress_token[0] = '\0';
    mcp->has_related_task = false;
    mcp->related_task_id[0] = '\0';
    if (mcp->request_meta_json) {
        free(mcp->request_meta_json);
        mcp->request_meta_json = NULL;
    }
    if (params && cJSON_IsObject(params)) {
        cJSON *meta = cJSON_GetObjectItem(params, "_meta");
        if (meta && cJSON_IsObject(meta)) {
            cJSON *pt = cJSON_GetObjectItem(meta, "progressToken");
            if (pt && (cJSON_IsString(pt) || cJSON_IsNumber(pt))) {
                mcp->has_progress_token = true;
                if (cJSON_IsString(pt)) {
                    esp_mcp_copy_str(mcp->progress_token, sizeof(mcp->progress_token), pt->valuestring);
                } else {
                    snprintf(mcp->progress_token, sizeof(mcp->progress_token), "%d", pt->valueint);
                }
            }
            cJSON *rt = cJSON_GetObjectItem(meta, "io.modelcontextprotocol/related-task");
            if (rt && cJSON_IsObject(rt)) {
                cJSON *tid = cJSON_GetObjectItem(rt, "taskId");
                if (tid && cJSON_IsString(tid) && tid->valuestring) {
                    mcp->has_related_task = true;
                    esp_mcp_copy_str(mcp->related_task_id, sizeof(mcp->related_task_id), tid->valuestring);
                }
            }
            // Store any other _meta fields for forwarding
            cJSON *other_meta = cJSON_CreateObject();
            if (other_meta) {
                cJSON *item = NULL;
                cJSON_ArrayForEach(item, meta) {
                    if (strcmp(item->string, "progressToken") != 0 &&
                            strcmp(item->string, "io.modelcontextprotocol/related-task") != 0) {
                        cJSON_AddItemToObject(other_meta, item->string, cJSON_Duplicate(item, 1));
                    }
                }
                if (cJSON_GetArraySize(other_meta) > 0) {
                    mcp->request_meta_json = cJSON_PrintUnformatted(other_meta);
                }
                cJSON_Delete(other_meta);
            }
        }
    }

    cJSON *id = cJSON_GetObjectItem(json, "id");
    if (id == NULL || !(cJSON_IsNumber(id) || cJSON_IsString(id))) {
        ESP_LOGE(TAG, "Invalid id for method: %s", method_str);
        esp_mcp_reply_error_with_kv(mcp, MCP_ERROR_CODE_INVALID_REQUEST, MCP_ERROR_MESSAGE_INVALID_REQUEST, NULL, "method", method_str);
        return ESP_ERR_INVALID_ARG;
    }

    char request_id_str[64] = {0};
    if (esp_mcp_request_id_to_str(id, request_id_str, sizeof(request_id_str)) != ESP_OK) {
        esp_mcp_reply_invalid_request_with_reason(mcp, id, method_str, "invalid request id");
        return ESP_ERR_INVALID_ARG;
    }
    const char *request_session = esp_mcp_request_session_or_default(mcp);
    if (esp_mcp_is_duplicate_request_id(mcp, request_session, request_id_str)) {
        esp_mcp_reply_invalid_request_with_reason(mcp, id, method_str, "duplicate request id in session");
        return ESP_ERR_INVALID_STATE;
    }
    esp_mcp_remember_request_id(mcp, request_session, request_id_str);

    if (!mcp->request_authenticated) {
        return esp_mcp_reply_error_with_kv(mcp, -32001, "Unauthorized", id, "method", method_str);
    }

    // Lifecycle gate (enabled for negotiated protocol versions newer than 2024-11-05)
    if (session_requires_initialized_notification &&
            !session_received_initialized_notification &&
            strcmp(method_str, "initialize") != 0 &&
            strcmp(method_str, "ping") != 0) {
        ESP_LOGW(TAG, "Request rejected before notifications/initialized: %s", method_str);
        esp_mcp_reply_invalid_request_with_reason(mcp, id, method_str, "client not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!strcmp(method_str, "initialize")) {
        return esp_mcp_handle_initialize(mcp, params, id);
    } else if (!strcmp(method_str, "tools/list")) {
        return esp_mcp_handle_tools_list(mcp, params, id);
    } else if (!strcmp(method_str, "tools/call")) {
        return esp_mcp_handle_tools_call(mcp, params, id);
    } else if (!strcmp(method_str, "ping")) {
        return esp_mcp_handle_ping(mcp, params, id);
    } else if (!strcmp(method_str, "resources/list")) {
        return esp_mcp_handle_resources_list(mcp, params, id);
    } else if (!strcmp(method_str, "resources/templates/list")) {
        return esp_mcp_handle_resources_templates_list(mcp, params, id);
    } else if (!strcmp(method_str, "resources/read")) {
        return esp_mcp_handle_resources_read(mcp, params, id);
    } else if (!strcmp(method_str, "resources/subscribe")) {
        return esp_mcp_handle_resources_subscribe(mcp, params, id);
    } else if (!strcmp(method_str, "resources/unsubscribe")) {
        return esp_mcp_handle_resources_unsubscribe(mcp, params, id);
    } else if (!strcmp(method_str, "prompts/list")) {
        return esp_mcp_handle_prompts_list(mcp, params, id);
    } else if (!strcmp(method_str, "prompts/get")) {
        return esp_mcp_handle_prompts_get(mcp, params, id);
    } else if (!strcmp(method_str, "completion/complete")) {
        return esp_mcp_handle_completion_complete(mcp, params, id);
    } else if (!strcmp(method_str, "logging/setLevel")) {
        return esp_mcp_handle_logging_set_level(mcp, params, id);
    } else if (!strcmp(method_str, "tasks/list")) {
        return esp_mcp_handle_tasks_list(mcp, params, id);
    } else if (!strcmp(method_str, "tasks/get")) {
        return esp_mcp_handle_tasks_get(mcp, params, id);
    } else if (!strcmp(method_str, "tasks/result")) {
        return esp_mcp_handle_tasks_result(mcp, params, id);
    } else if (!strcmp(method_str, "tasks/cancel")) {
        return esp_mcp_handle_tasks_cancel(mcp, params, id);
    } else {
        esp_err_t cb_ret = esp_mcp_dispatch_incoming_request(mcp, method_str, params, id);
        if (cb_ret != ESP_ERR_NOT_SUPPORTED) {
            return cb_ret;
        }
        ESP_LOGE(TAG, "Method not implemented: %s", method_str);
        esp_mcp_reply_error_with_kv(mcp, MCP_ERROR_CODE_METHOD_NOT_FOUND, MCP_ERROR_MESSAGE_METHOD_NOT_FOUND, id, "method", method_str);
    }

    return ESP_OK;
}

esp_err_t esp_mcp_parse_message(esp_mcp_t *mcp, const char *message)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid server");
    ESP_RETURN_ON_FALSE(message, ESP_ERR_INVALID_ARG, TAG, "Invalid message");

    cJSON *json = cJSON_Parse(message);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse MCP message: %s", message);
        cJSON *data = cJSON_CreateObject();
        if (data) {
            cJSON_AddStringToObject(data, "reason", "invalid json");
        }
        (void)esp_mcp_reply_error_ex(mcp, MCP_ERROR_CODE_PARSE_ERROR, MCP_ERROR_MESSAGE_PARSE_ERROR, NULL, data);
        if (data) {
            cJSON_Delete(data);
        }
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_OK;
    if (cJSON_IsArray(json)) {
        int n = cJSON_GetArraySize(json);
        if (n <= 0) {
            esp_mcp_reply_invalid_request_with_reason(mcp, NULL, NULL, "empty batch");
            ret = ESP_ERR_INVALID_ARG;
        } else {
            cJSON *responses = cJSON_CreateArray();
            if (!responses) {
                cJSON_Delete(json);
                return ESP_ERR_NO_MEM;
            }

            for (int i = 0; i < n; i++) {
                const cJSON *item = cJSON_GetArrayItem(json, i);
                esp_err_t one_ret = esp_mcp_parse_json_message(mcp, item);
                if (one_ret != ESP_OK && ret == ESP_OK) {
                    ret = one_ret;
                }

                uint8_t *tmp = NULL;
                uint16_t tmplen = 0;
                (void)esp_mcp_get_mbuf(mcp, &tmp, &tmplen);
                if (tmp && tmplen > 0) {
                    cJSON *obj = cJSON_Parse((const char *)tmp);
                    if (obj) {
                        cJSON_AddItemToArray(responses, obj);
                    } else {
                        ESP_LOGW(TAG, "Batch item %d: failed to parse mbuf JSON", i);
                    }
                    (void)esp_mcp_remove_mbuf(mcp, tmp);
                }

                if (one_ret == ESP_ERR_NO_MEM) {
                    ESP_LOGE(TAG, "Batch processing aborted at index %d due to OOM", i);
                    break;
                }
            }

            if (cJSON_GetArraySize(responses) > 0) {
                char *resp_str = cJSON_PrintUnformatted(responses);
                if (!resp_str) {
                    ESP_LOGE(TAG, "Failed to print batch JSON-RPC responses");
                    cJSON_Delete(responses);
                    cJSON_Delete(json);
                    return ESP_ERR_NO_MEM;
                }
                size_t batch_len = strlen(resp_str);
                if (batch_len > UINT16_MAX) {
                    ESP_LOGE(TAG, "batch JSON-RPC response exceeds mbuf length limit");
                    cJSON_free(resp_str);
                    cJSON_Delete(responses);
                    cJSON_Delete(json);
                    return ESP_ERR_NO_MEM;
                }
                esp_mcp_add_mbuf(mcp, 0, (uint8_t *)resp_str, (uint16_t)batch_len);
            }
            cJSON_Delete(responses);
        }
    } else {
        ret = esp_mcp_parse_json_message(mcp, json);
    }
    cJSON_Delete(json);

    return ret;
}

static uint16_t esp_mcp_generate_id(void)
{
    uint16_t id;
    do {
        id = (uint16_t)esp_random();
    } while (id == 0);
    return id;
}

static void esp_mcp_generate_request_id_str(char out[33])
{
    uint8_t rnd[16];
    static const char hex[] = "0123456789abcdef";
    esp_fill_random(rnd, sizeof(rnd));
    for (size_t i = 0; i < sizeof(rnd); ++i) {
        out[i * 2] = hex[(rnd[i] >> 4) & 0xF];
        out[i * 2 + 1] = hex[rnd[i] & 0xF];
    }
    out[32] = '\0';
}

static esp_err_t esp_mcp_req_build(const char *method, cJSON *params, uint16_t id, char **out_json)
{
    if (!method || !out_json) {
        if (params) {
            cJSON_Delete(params);
        }
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        if (params) {
            cJSON_Delete(params);
        }
        ESP_LOGE(TAG, "No mem");
        return ESP_ERR_NO_MEM;
    }

    if (!cJSON_AddStringToObject(root, "jsonrpc", "2.0") ||
            !cJSON_AddStringToObject(root, "method", method) ||
            !cJSON_AddNumberToObject(root, "id", id)) {
        cJSON_Delete(root);
        if (params) {
            cJSON_Delete(params);
        }
        return ESP_ERR_NO_MEM;
    }
    if (params) {
        if (!cJSON_AddItemToObject(root, "params", params)) {
            cJSON_Delete(root);
            cJSON_Delete(params);
            return ESP_ERR_NO_MEM;
        }
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    ESP_RETURN_ON_FALSE(json, ESP_ERR_NO_MEM, TAG, "Print failed");

    *out_json = json;

    return ESP_OK;
}

static esp_err_t esp_mcp_req_build_with_str_id(const char *method, cJSON *params, const char *id, char **out_json)
{
    if (!method || !id || !out_json) {
        if (params) {
            cJSON_Delete(params);
        }
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        if (params) {
            cJSON_Delete(params);
        }
        return ESP_ERR_NO_MEM;
    }
    if (!cJSON_AddStringToObject(root, "jsonrpc", "2.0") ||
            !cJSON_AddStringToObject(root, "method", method) ||
            !cJSON_AddStringToObject(root, "id", id)) {
        cJSON_Delete(root);
        if (params) {
            cJSON_Delete(params);
        }
        return ESP_ERR_NO_MEM;
    }
    if (params) {
        if (!cJSON_AddItemToObject(root, "params", params)) {
            cJSON_Delete(root);
            cJSON_Delete(params);
            return ESP_ERR_NO_MEM;
        }
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    ESP_RETURN_ON_FALSE(json, ESP_ERR_NO_MEM, TAG, "Print failed");
    *out_json = json;
    return ESP_OK;
}

typedef struct {
    bool has_task;
    bool uses_sampling_tools;
    bool needs_sampling_context_cap;
} esp_mcp_request_capability_hints_t;

static esp_err_t esp_mcp_parse_request_capability_hints(const char *params_json,
                                                        esp_mcp_request_capability_hints_t *out_hints)
{
    ESP_RETURN_ON_FALSE(out_hints, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    memset(out_hints, 0, sizeof(*out_hints));

    if (!(params_json && params_json[0])) {
        return ESP_OK;
    }

    cJSON *params = cJSON_Parse(params_json);
    if (!(params && cJSON_IsObject(params))) {
        cJSON_Delete(params);
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *task = cJSON_GetObjectItem(params, "task");
    if (task) {
        if (!cJSON_IsObject(task)) {
            cJSON_Delete(params);
            return ESP_ERR_INVALID_ARG;
        }
        out_hints->has_task = true;
    }

    if (cJSON_GetObjectItem(params, "tools") || cJSON_GetObjectItem(params, "toolChoice")) {
        out_hints->uses_sampling_tools = true;
    }

    cJSON *include_context = cJSON_GetObjectItem(params, "includeContext");
    if (include_context &&
            cJSON_IsString(include_context) &&
            include_context->valuestring &&
            strcmp(include_context->valuestring, "none") != 0) {
        out_hints->needs_sampling_context_cap = true;
    }

    cJSON_Delete(params);
    return ESP_OK;
}

static esp_err_t esp_mcp_build_task_id_params_json(const char *task_id, char **out_json)
{
    ESP_RETURN_ON_FALSE(task_id && task_id[0] && out_json, ESP_ERR_INVALID_ARG, TAG, "Invalid args");

    cJSON *params = cJSON_CreateObject();
    if (!params) {
        return ESP_ERR_NO_MEM;
    }
    if (!cJSON_AddStringToObject(params, "taskId", task_id)) {
        cJSON_Delete(params);
        return ESP_ERR_NO_MEM;
    }

    char *params_json = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    if (!params_json) {
        return ESP_ERR_NO_MEM;
    }

    *out_json = params_json;
    return ESP_OK;
}

static esp_err_t esp_mcp_build_optional_cursor_params_json(const char *cursor, char **out_json)
{
    ESP_RETURN_ON_FALSE(out_json, ESP_ERR_INVALID_ARG, TAG, "Invalid args");

    cJSON *params = cJSON_CreateObject();
    if (!params) {
        return ESP_ERR_NO_MEM;
    }
    if (cursor) {
        cJSON_AddStringToObject(params, "cursor", cursor);
    }

    char *params_json = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    if (!params_json) {
        return ESP_ERR_NO_MEM;
    }

    *out_json = params_json;
    return ESP_OK;
}

static esp_err_t esp_mcp_emit_server_request_with_request_id(esp_mcp_t *mcp,
                                                             const char *session_id,
                                                             const char *method,
                                                             const char *params_json,
                                                             const char *request_id,
                                                             esp_mcp_server_req_cb_t cb,
                                                             void *user_ctx,
                                                             uint32_t timeout_ms,
                                                             char *out_request_id,
                                                             size_t out_request_id_len)
{
    const int max_emit_retries = 2;
    esp_mcp_request_capability_hints_t hints = {0};
    const esp_mcp_session_state_t *session_state = NULL;
    ESP_RETURN_ON_FALSE(mcp && method, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    ESP_RETURN_ON_FALSE(request_id && request_id[0], ESP_ERR_INVALID_ARG, TAG, "Invalid request id");
    ESP_RETURN_ON_FALSE(mcp->mgr_ctx, ESP_ERR_INVALID_STATE, TAG, "Manager context not set");
    ESP_RETURN_ON_ERROR(esp_mcp_parse_request_capability_hints(params_json, &hints), TAG, "Invalid params_json");
    session_state = esp_mcp_get_session_state_const(mcp, session_id);
    if (!(session_state && session_state->received_initialize_request)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (session_state->require_initialized_notification &&
            !session_state->received_initialized_notification) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (strcmp(method, "roots/list") == 0 && !session_state->client_cap_roots_list) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (strcmp(method, "sampling/createMessage") == 0 && !session_state->client_cap_sampling_create) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (strcmp(method, "sampling/createMessage") == 0 &&
            hints.has_task &&
            !session_state->client_cap_tasks_sampling_create) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (strcmp(method, "sampling/createMessage") == 0 &&
            hints.uses_sampling_tools &&
            !session_state->client_cap_sampling_tools) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (strcmp(method, "sampling/createMessage") == 0 &&
            hints.needs_sampling_context_cap &&
            !session_state->client_cap_sampling_context) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (strcmp(method, "elicitation/create") == 0 &&
            !session_state->client_cap_elicitation_form &&
            !session_state->client_cap_elicitation_url) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (strcmp(method, "elicitation/create") == 0 &&
            hints.has_task &&
            !session_state->client_cap_tasks_elicitation_create) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (strcmp(method, "tasks/list") == 0 && !session_state->client_cap_tasks_list) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (strcmp(method, "tasks/cancel") == 0 && !session_state->client_cap_tasks_cancel) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    cJSON *params = NULL;
    if (params_json && params_json[0]) {
        params = cJSON_Parse(params_json);
        if (!params || !cJSON_IsObject(params)) {
            cJSON_Delete(params);
            return ESP_ERR_INVALID_ARG;
        }
    } else {
        params = cJSON_CreateObject();
        ESP_RETURN_ON_FALSE(params, ESP_ERR_NO_MEM, TAG, "No mem");
    }
    char *request_json = NULL;
    esp_err_t ret = esp_mcp_req_build_with_str_id(method, params, request_id, &request_json);
    if (ret != ESP_OK) {
        return ret;
    }
    if (cb) {
        ret = esp_mcp_mgr_track_pending_request((esp_mcp_mgr_handle_t)mcp->mgr_ctx,
                                                request_id,
                                                session_id,
                                                method,
                                                (esp_mcp_mgr_resp_cb_t)cb,
                                                user_ctx,
                                                timeout_ms);
        if (ret != ESP_OK) {
            cJSON_free(request_json);
            return ret;
        }
    }
    ret = ESP_FAIL;
    for (int i = 0; i <= max_emit_retries; ++i) {
        ret = esp_mcp_emit_message(mcp, session_id, request_json);
        if (ret == ESP_OK) {
            break;
        }
        if (i < max_emit_retries) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    if (ret != ESP_OK && cb) {
        (void)esp_mcp_mgr_cancel_pending_request((esp_mcp_mgr_handle_t)mcp->mgr_ctx, request_id, session_id);
    }
    cJSON_free(request_json);
    if (ret == ESP_OK && out_request_id && out_request_id_len > 0) {
        snprintf(out_request_id, out_request_id_len, "%s", request_id);
    }
    return ret;
}

static esp_err_t esp_mcp_emit_server_request(esp_mcp_t *mcp,
                                             const char *session_id,
                                             const char *method,
                                             const char *params_json,
                                             esp_mcp_server_req_cb_t cb,
                                             void *user_ctx,
                                             uint32_t timeout_ms,
                                             char *out_request_id,
                                             size_t out_request_id_len)
{
    char request_id[33] = {0};
    esp_mcp_generate_request_id_str(request_id);
    return esp_mcp_emit_server_request_with_request_id(mcp,
                                                       session_id,
                                                       method,
                                                       params_json,
                                                       request_id,
                                                       cb,
                                                       user_ctx,
                                                       timeout_ms,
                                                       out_request_id,
                                                       out_request_id_len);
}

esp_err_t esp_mcp_request_roots_list(esp_mcp_t *mcp,
                                     const char *session_id,
                                     esp_mcp_server_req_cb_t cb,
                                     void *user_ctx,
                                     uint32_t timeout_ms,
                                     char *out_request_id,
                                     size_t out_request_id_len)
{
    return esp_mcp_emit_server_request(mcp, session_id, "roots/list", "{}", cb, user_ctx, timeout_ms, out_request_id, out_request_id_len);
}

esp_err_t esp_mcp_request_sampling_create(esp_mcp_t *mcp,
                                          const char *session_id,
                                          const char *params_json,
                                          esp_mcp_server_req_cb_t cb,
                                          void *user_ctx,
                                          uint32_t timeout_ms,
                                          char *out_request_id,
                                          size_t out_request_id_len)
{
    return esp_mcp_emit_server_request(mcp, session_id, "sampling/createMessage", params_json, cb, user_ctx, timeout_ms, out_request_id, out_request_id_len);
}

esp_err_t esp_mcp_request_sampling_with_tools(esp_mcp_t *mcp,
                                              const char *session_id,
                                              const char *params_json,
                                              const char *tools_json,
                                              const char *tool_choice,
                                              esp_mcp_server_req_cb_t cb,
                                              void *user_ctx,
                                              uint32_t timeout_ms,
                                              char *out_request_id,
                                              size_t out_request_id_len)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    const esp_mcp_session_state_t *session_state = esp_mcp_get_session_state_const(mcp, session_id);
    ESP_RETURN_ON_FALSE(session_state && session_state->received_initialize_request, ESP_ERR_NOT_SUPPORTED, TAG, "Client session not initialized");

    cJSON *params = NULL;
    if (params_json) {
        params = cJSON_Parse(params_json);
    }
    if (!params) {
        params = cJSON_CreateObject();
    }
    ESP_RETURN_ON_FALSE(params, ESP_ERR_NO_MEM, TAG, "No mem");
    if (!cJSON_IsObject(params)) {
        cJSON_Delete(params);
        return ESP_ERR_INVALID_ARG;
    }

    if (tools_json) {
        if (!session_state->client_cap_sampling_tools) {
            cJSON_Delete(params);
            return ESP_ERR_NOT_SUPPORTED;
        }
        cJSON *tools = cJSON_Parse(tools_json);
        if (!tools) {
            cJSON_Delete(params);
            return ESP_ERR_INVALID_ARG;
        }
        if (cJSON_IsArray(tools)) {
            cJSON_AddItemToObject(params, "tools", tools);
        } else {
            cJSON_Delete(tools);
            cJSON_Delete(params);
            return ESP_ERR_INVALID_ARG;
        }
    }

    if (tool_choice) {
        cJSON *tool_choice_obj = cJSON_CreateObject();
        if (!tool_choice_obj) {
            cJSON_Delete(params);
            return ESP_ERR_NO_MEM;
        }
        if (strcmp(tool_choice, "auto") != 0 &&
                strcmp(tool_choice, "required") != 0 &&
                strcmp(tool_choice, "none") != 0) {
            cJSON_Delete(tool_choice_obj);
            cJSON_Delete(params);
            return ESP_ERR_INVALID_ARG;
        }
        cJSON_AddStringToObject(tool_choice_obj, "mode", tool_choice);
        cJSON_AddItemToObject(params, "toolChoice", tool_choice_obj);
    }

    char *modified_params = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    ESP_RETURN_ON_FALSE(modified_params, ESP_ERR_NO_MEM, TAG, "No mem");

    esp_err_t ret = esp_mcp_emit_server_request(mcp, session_id, "sampling/createMessage", modified_params, cb, user_ctx, timeout_ms, out_request_id, out_request_id_len);
    cJSON_free(modified_params);
    return ret;
}

esp_err_t esp_mcp_request_elicitation(esp_mcp_t *mcp,
                                      const char *session_id,
                                      const char *params_json,
                                      esp_mcp_server_req_cb_t cb,
                                      void *user_ctx,
                                      uint32_t timeout_ms,
                                      char *out_request_id,
                                      size_t out_request_id_len)
{
    const esp_mcp_session_state_t *session_state = esp_mcp_get_session_state_const(mcp, session_id);
    ESP_RETURN_ON_FALSE(session_state && session_state->received_initialize_request, ESP_ERR_NOT_SUPPORTED, TAG, "Client session not initialized");

    cJSON *params = NULL;
    if (params_json && params_json[0]) {
        params = cJSON_Parse(params_json);
        if (!(params && cJSON_IsObject(params))) {
            cJSON_Delete(params);
            return ESP_ERR_INVALID_ARG;
        }
    }
    const cJSON *mode = params ? cJSON_GetObjectItem(params, "mode") : NULL;
    bool wants_url = mode && cJSON_IsString(mode) && strcmp(mode->valuestring, "url") == 0;
    if (params) {
        cJSON_Delete(params);
    }

    if (wants_url) {
        ESP_RETURN_ON_FALSE(session_state->client_cap_elicitation_url, ESP_ERR_NOT_SUPPORTED, TAG, "Client does not support URL elicitation");
    } else {
        ESP_RETURN_ON_FALSE(session_state->client_cap_elicitation_form, ESP_ERR_NOT_SUPPORTED, TAG, "Client does not support form elicitation");
    }

    return esp_mcp_emit_server_request(mcp, session_id, "elicitation/create", params_json, cb, user_ctx, timeout_ms, out_request_id, out_request_id_len);
}

esp_err_t esp_mcp_request_elicitation_url(esp_mcp_t *mcp,
                                          const char *session_id,
                                          const char *params_json,
                                          const char *url,
                                          esp_mcp_server_req_cb_t cb,
                                          void *user_ctx,
                                          uint32_t timeout_ms,
                                          char *out_request_id,
                                          size_t out_request_id_len)
{
    ESP_RETURN_ON_FALSE(mcp && url, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    const esp_mcp_session_state_t *session_state = esp_mcp_get_session_state_const(mcp, session_id);
    ESP_RETURN_ON_FALSE(session_state && session_state->received_initialize_request, ESP_ERR_NOT_SUPPORTED, TAG, "Client session not initialized");
    ESP_RETURN_ON_FALSE(session_state->client_cap_elicitation_url, ESP_ERR_NOT_SUPPORTED, TAG, "Client does not support URL elicitation");
    char request_id[33] = {0};
    esp_mcp_generate_request_id_str(request_id);

    cJSON *params = NULL;
    if (params_json) {
        params = cJSON_Parse(params_json);
    }
    if (!params) {
        params = cJSON_CreateObject();
    }
    ESP_RETURN_ON_FALSE(params, ESP_ERR_NO_MEM, TAG, "No mem");
    if (!cJSON_IsObject(params)) {
        cJSON_Delete(params);
        return ESP_ERR_INVALID_ARG;
    }

    cJSON_DeleteItemFromObject(params, "mode");
    cJSON_DeleteItemFromObject(params, "url");
    cJSON_DeleteItemFromObject(params, "elicitationId");
    cJSON_DeleteItemFromObject(params, "requestId");
    if (!cJSON_AddStringToObject(params, "mode", "url") ||
            !cJSON_AddStringToObject(params, "url", url) ||
            !cJSON_AddStringToObject(params, "elicitationId", request_id)) {
        cJSON_Delete(params);
        return ESP_ERR_NO_MEM;
    }

    char *modified_params = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    ESP_RETURN_ON_FALSE(modified_params, ESP_ERR_NO_MEM, TAG, "No mem");

    esp_err_t ret = esp_mcp_emit_server_request_with_request_id(mcp,
                                                                session_id,
                                                                "elicitation/create",
                                                                modified_params,
                                                                request_id,
                                                                cb,
                                                                user_ctx,
                                                                timeout_ms,
                                                                out_request_id,
                                                                out_request_id_len);
    cJSON_free(modified_params);
    return ret;
}

static esp_err_t esp_mcp_request_task_method_with_task_id(esp_mcp_t *mcp,
                                                          const char *session_id,
                                                          const char *method,
                                                          const char *task_id,
                                                          esp_mcp_server_req_cb_t cb,
                                                          void *user_ctx,
                                                          uint32_t timeout_ms,
                                                          char *out_request_id,
                                                          size_t out_request_id_len)
{
    ESP_RETURN_ON_FALSE(mcp && method && task_id && task_id[0], ESP_ERR_INVALID_ARG, TAG, "Invalid args");

    char *params_json = NULL;
    esp_err_t ret = esp_mcp_build_task_id_params_json(task_id, &params_json);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_mcp_emit_server_request(mcp,
                                      session_id,
                                      method,
                                      params_json,
                                      cb,
                                      user_ctx,
                                      timeout_ms,
                                      out_request_id,
                                      out_request_id_len);
    cJSON_free(params_json);
    return ret;
}

esp_err_t esp_mcp_request_tasks_get(esp_mcp_t *mcp,
                                    const char *session_id,
                                    const char *task_id,
                                    esp_mcp_server_req_cb_t cb,
                                    void *user_ctx,
                                    uint32_t timeout_ms,
                                    char *out_request_id,
                                    size_t out_request_id_len)
{
    return esp_mcp_request_task_method_with_task_id(mcp,
                                                    session_id,
                                                    "tasks/get",
                                                    task_id,
                                                    cb,
                                                    user_ctx,
                                                    timeout_ms,
                                                    out_request_id,
                                                    out_request_id_len);
}

esp_err_t esp_mcp_request_tasks_result(esp_mcp_t *mcp,
                                       const char *session_id,
                                       const char *task_id,
                                       esp_mcp_server_req_cb_t cb,
                                       void *user_ctx,
                                       uint32_t timeout_ms,
                                       char *out_request_id,
                                       size_t out_request_id_len)
{
    return esp_mcp_request_task_method_with_task_id(mcp,
                                                    session_id,
                                                    "tasks/result",
                                                    task_id,
                                                    cb,
                                                    user_ctx,
                                                    timeout_ms,
                                                    out_request_id,
                                                    out_request_id_len);
}

esp_err_t esp_mcp_request_tasks_cancel(esp_mcp_t *mcp,
                                       const char *session_id,
                                       const char *task_id,
                                       esp_mcp_server_req_cb_t cb,
                                       void *user_ctx,
                                       uint32_t timeout_ms,
                                       char *out_request_id,
                                       size_t out_request_id_len)
{
    return esp_mcp_request_task_method_with_task_id(mcp,
                                                    session_id,
                                                    "tasks/cancel",
                                                    task_id,
                                                    cb,
                                                    user_ctx,
                                                    timeout_ms,
                                                    out_request_id,
                                                    out_request_id_len);
}

esp_err_t esp_mcp_request_tasks_list(esp_mcp_t *mcp,
                                     const char *session_id,
                                     const char *cursor,
                                     esp_mcp_server_req_cb_t cb,
                                     void *user_ctx,
                                     uint32_t timeout_ms,
                                     char *out_request_id,
                                     size_t out_request_id_len)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid args");

    char *params_json = NULL;
    esp_err_t ret = esp_mcp_build_optional_cursor_params_json(cursor, &params_json);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_mcp_emit_server_request(mcp,
                                      session_id,
                                      "tasks/list",
                                      params_json,
                                      cb,
                                      user_ctx,
                                      timeout_ms,
                                      out_request_id,
                                      out_request_id_len);
    cJSON_free(params_json);
    return ret;
}

static cJSON *esp_mcp_parse_optional_json_object(const char *json)
{
    if (!(json && json[0])) {
        return cJSON_CreateObject();
    }

    cJSON *parsed = cJSON_Parse(json);
    if (!(parsed && cJSON_IsObject(parsed))) {
        cJSON_Delete(parsed);
        return NULL;
    }
    return parsed;
}

static cJSON *esp_mcp_parse_optional_json_array(const char *json)
{
    if (!(json && json[0])) {
        return NULL;
    }

    cJSON *parsed = cJSON_Parse(json);
    if (!(parsed && cJSON_IsArray(parsed))) {
        cJSON_Delete(parsed);
        return NULL;
    }
    return parsed;
}

esp_err_t esp_mcp_info_init(esp_mcp_t *mcp, const esp_mcp_info_t *info, char **output, uint16_t *out_id)
{
    (void)mcp;
    ESP_RETURN_ON_FALSE(output && out_id, ESP_ERR_INVALID_ARG, TAG, "Invalid args");

    const char *proto = (info && info->protocol_version) ? info->protocol_version : "2025-11-25";
    const char *name = (info && info->name) ? info->name : "esp-mcp-client";
    const char *ver = (info && info->version) ? info->version : "0.1.0";
    const char *title = (info && info->title) ? info->title : NULL;
    const char *description = (info && info->description) ? info->description : NULL;
    const char *website_url = (info && info->website_url) ? info->website_url : NULL;

    cJSON *params = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(params, ESP_ERR_NO_MEM, TAG, "No mem");

    cJSON_AddStringToObject(params, "protocolVersion", proto);
    cJSON *capabilities = esp_mcp_parse_optional_json_object(info ? info->capabilities_json : NULL);
    if (!capabilities) {
        cJSON_Delete(params);
        ESP_LOGE(TAG, "Invalid capabilities_json");
        return (info && info->capabilities_json && info->capabilities_json[0]) ? ESP_ERR_INVALID_ARG : ESP_ERR_NO_MEM;
    }
    cJSON_AddItemToObject(params, "capabilities", capabilities);

    cJSON *client_info = cJSON_CreateObject();
    if (!client_info) {
        cJSON_Delete(params);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(client_info, "name", name);
    cJSON_AddStringToObject(client_info, "version", ver);
    if (title) {
        cJSON_AddStringToObject(client_info, "title", title);
    }
    if (description) {
        cJSON_AddStringToObject(client_info, "description", description);
    }
    if (website_url) {
        cJSON_AddStringToObject(client_info, "websiteUrl", website_url);
    }
    if (info && info->icons_json && info->icons_json[0]) {
        cJSON *icons = esp_mcp_parse_optional_json_array(info->icons_json);
        if (!icons) {
            cJSON_Delete(client_info);
            cJSON_Delete(params);
            ESP_LOGE(TAG, "Invalid icons_json");
            return ESP_ERR_INVALID_ARG;
        }
        cJSON_AddItemToObject(client_info, "icons", icons);
    }
    cJSON_AddItemToObject(params, "clientInfo", client_info);

    uint16_t id = esp_mcp_generate_id();
    char *req_json = NULL;
    esp_err_t ret = esp_mcp_req_build("initialize", params, id, &req_json);
    ESP_RETURN_ON_ERROR(ret, TAG, "Build initialize failed: %s", esp_err_to_name(ret));

    *output = req_json;
    *out_id = id;

    return ret;
}

esp_err_t esp_mcp_tools_list(esp_mcp_t *mcp, const esp_mcp_info_t *info, char **output, uint16_t *out_id)
{
    (void)mcp;
    ESP_RETURN_ON_FALSE(output && out_id, ESP_ERR_INVALID_ARG, TAG, "Invalid args");

    cJSON *params = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(params, ESP_ERR_NO_MEM, TAG, "No mem");
    if (info && info->cursor && info->cursor[0] != '\0') {
        cJSON_AddStringToObject(params, "cursor", info->cursor);
    }
    if (info && info->tools_list_limit >= 0) {
        cJSON_AddNumberToObject(params, "limit", (double)info->tools_list_limit);
    }

    uint16_t id = esp_mcp_generate_id();
    char *req_json = NULL;
    esp_err_t ret = esp_mcp_req_build("tools/list", params, id, &req_json);
    ESP_RETURN_ON_ERROR(ret, TAG, "Build tools/list failed: %s", esp_err_to_name(ret));

    *output = req_json;
    *out_id = id;

    return ret;
}

esp_err_t esp_mcp_tools_call(esp_mcp_t *mcp, const esp_mcp_info_t *info, char **output, uint16_t *out_id)
{
    (void)mcp;
    ESP_RETURN_ON_FALSE(output && out_id, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    ESP_RETURN_ON_FALSE(info, ESP_ERR_INVALID_ARG, TAG, "Invalid info");
    ESP_RETURN_ON_FALSE(info->tool_name, ESP_ERR_INVALID_ARG, TAG, "Invalid tool_name");

    cJSON *params = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(params, ESP_ERR_NO_MEM, TAG, "No mem");
    cJSON_AddStringToObject(params, "name", info->tool_name);

    cJSON *args = NULL;
    if (info->args_json && info->args_json[0] != '\0') {
        args = cJSON_Parse(info->args_json);
        if (!args || !cJSON_IsObject(args)) {
            if (args) {
                cJSON_Delete(args);
            }
            cJSON_Delete(params);
            return ESP_ERR_INVALID_ARG;
        }
    } else {
        args = cJSON_CreateObject();
        if (!args) {
            cJSON_Delete(params);
            return ESP_ERR_NO_MEM;
        }
    }
    cJSON_AddItemToObject(params, "arguments", args);

    uint16_t id = esp_mcp_generate_id();
    char *req_json = NULL;
    esp_err_t ret = esp_mcp_req_build("tools/call", params, id, &req_json);
    ESP_RETURN_ON_ERROR(ret, TAG, "Build tools/call failed: %s", esp_err_to_name(ret));

    *output = req_json;
    *out_id = id;

    return ret;
}

esp_err_t esp_mcp_resp_parse(esp_mcp_t *mcp, const char *input, esp_mcp_resp_t *result)
{
    (void)mcp;
    ESP_RETURN_ON_FALSE(input && result, ESP_ERR_INVALID_ARG, TAG, "Invalid args");

    result->output = NULL;
    result->id = 0;
    result->id_str = NULL;
    result->is_error = false;
    result->error_code = 0;
    result->error_message = NULL;

    cJSON *root = cJSON_Parse(input);
    if (!root) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *id = cJSON_GetObjectItem(root, "id");
    cJSON *result_obj = cJSON_GetObjectItem(root, "result");
    cJSON *error = cJSON_GetObjectItem(root, "error");

    if (id && cJSON_IsNumber(id)) {
        if (id->valueint > 0 && id->valueint <= UINT16_MAX) {
            result->id = (uint16_t)id->valueint;
        }
        char idbuf[32] = {0};
        snprintf(idbuf, sizeof(idbuf), "%lld", (long long)id->valuedouble);
        result->id_str = strdup(idbuf);
        if (!result->id_str) {
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
    } else if (id && cJSON_IsString(id) && id->valuestring) {
        result->id_str = strdup(id->valuestring);
        if (!result->id_str) {
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
    }

    char *tmp = NULL;
    esp_err_t ret = ESP_OK;

    if (error) {
        cJSON *code = cJSON_GetObjectItem(error, "code");
        cJSON *message = cJSON_GetObjectItem(error, "message");

        if (code && cJSON_IsNumber(code)) {
            result->error_code = code->valueint;
        }

        if (message && cJSON_IsString(message)) {
            result->error_message = strdup(message->valuestring);
            if (!result->error_message) {
                if (result->id_str) {
                    free(result->id_str);
                    result->id_str = NULL;
                }
                cJSON_Delete(root);
                return ESP_ERR_NO_MEM;
            }
        }

        tmp = cJSON_PrintUnformatted(error);
        ret = ESP_ERR_INVALID_RESPONSE;
    } else if (result_obj && cJSON_IsObject(result_obj)) {
        cJSON *is_error_obj = cJSON_GetObjectItem(result_obj, "isError");
        if (is_error_obj && cJSON_IsBool(is_error_obj)) {
            result->is_error = cJSON_IsTrue(is_error_obj);
        } else {
            result->is_error = false;
        }

        tmp = cJSON_PrintUnformatted(result_obj);
    } else {
        if (result->id_str) {
            free(result->id_str);
            result->id_str = NULL;
        }
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    if (!tmp) {
        if (result->error_message) {
            free(result->error_message);
            result->error_message = NULL;
        }
        if (result->id_str) {
            free(result->id_str);
            result->id_str = NULL;
        }
        cJSON_Delete(root);
        ESP_LOGE(TAG, "Failed to print JSON");
        return ESP_ERR_NO_MEM;
    }

    result->output = tmp;
    cJSON_Delete(root);
    return ret;
}

void esp_mcp_resp_free(char *buf)
{
    if (buf) {
        cJSON_free(buf);
    }
}
