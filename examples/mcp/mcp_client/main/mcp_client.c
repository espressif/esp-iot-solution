/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdatomic.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "protocol_examples_common.h"

#include "esp_mcp_engine.h"
#include "esp_mcp_mgr.h"
#include "esp_http_client.h"
#include "cJSON.h"

static const char *TAG = "mcp_client";
static const uint32_t MCP_CLIENT_TASK_STACK_SIZE = 10240;

/** True if URL authority already contains :port (handles https://[::1]:8080/). */
static bool mcp_origin_authority_has_port(const char *url)
{
    const char *auth = strstr(url, "://");
    if (!auth) {
        return false;
    }
    auth += 3;
    const char *end = auth;
    while (*end && *end != '/') {
        end++;
    }
    if (auth == end) {
        return false;
    }
    if (*auth == '[') {
        const char *close = memchr(auth, ']', (size_t)(end - auth));
        if (!close || close >= end) {
            return false;
        }
        return memchr(close, ':', (size_t)(end - close)) != NULL;
    }
    return memchr(auth, ':', (size_t)(end - auth)) != NULL;
}

/** Plain host -> http://host:port; origin with http(s):// keeps scheme, appends :port only when needed. */
static void mcp_client_build_base_url(char *out, size_t out_len)
{
    const char *host = CONFIG_MCP_REMOTE_HOST;
    const int port = CONFIG_MCP_REMOTE_PORT;

    if (strncmp(host, "https://", 8) == 0) {
        if (mcp_origin_authority_has_port(host) || port == 443) {
            snprintf(out, out_len, "%s", host);
        } else {
            snprintf(out, out_len, "%s:%d", host, port);
        }
    } else if (strncmp(host, "http://", 7) == 0) {
        if (mcp_origin_authority_has_port(host) || port == 80) {
            snprintf(out, out_len, "%s", host);
        } else {
            snprintf(out, out_len, "%s:%d", host, port);
        }
    } else {
        snprintf(out, out_len, "http://%s:%d", host, port);
    }
}

/**
 * @brief Response synchronization context
 */
typedef struct {
    SemaphoreHandle_t resp_sem;      /*!< Semaphore for response synchronization */
    atomic_int pending_responses;    /*!< Counter for pending responses */
    bool has_next_cursor;
    char next_cursor[128];
    bool has_first_tool_name;
    char first_tool_name[96];
} resp_sync_ctx_t;

typedef esp_err_t (*mcp_post_fn_t)(esp_mcp_mgr_handle_t mgr, const esp_mcp_mgr_req_t *req);

static esp_err_t resp_cb(int error_code, const char *ep_name, const char *resp_json, void *user_ctx,
                         uint32_t jsonrpc_request_id)
{
    (void)jsonrpc_request_id;
    resp_sync_ctx_t *ctx = (resp_sync_ctx_t *)user_ctx;
    if (!ctx) {
        ESP_LOGW(TAG, "Response callback: user_ctx is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    atomic_fetch_sub(&ctx->pending_responses, 1);
    if (ctx->resp_sem) {
        xSemaphoreGive(ctx->resp_sem);
    }

    // Parse failure: resp_json is NULL, error_code is esp_err_t
    if (resp_json == NULL) {
        ESP_LOGE(TAG, "[Parse Failure] Endpoint: %s, err=%d", ep_name ? ep_name : "NULL", error_code);
        return ESP_OK;
    }

    // Protocol-level error: error_code is negative JSON-RPC error code
    if (error_code < 0) {
        ESP_LOGE(TAG, "[Protocol Error] Endpoint: %s, code=%d", ep_name ? ep_name : "NULL", error_code);
        return ESP_OK;
    }

    // Application-level response
    if (error_code == 0) {
        ESP_LOGI(TAG, "[Success] Endpoint: %s", ep_name ? ep_name : "NULL");
    } else if (error_code == 1) {
        ESP_LOGW(TAG, "[Application Error] Endpoint: %s", ep_name ? ep_name : "NULL");
    } else {
        ESP_LOGW(TAG, "[Unexpected] Endpoint: %s, code=%d", ep_name ? ep_name : "NULL", error_code);
    }

    if (error_code == 0 && ep_name && resp_json &&
            (strcmp(ep_name, "tools/list") == 0 || strcmp(ep_name, "toolslist") == 0)) {
        cJSON *root = cJSON_Parse(resp_json);
        if (root) {
            cJSON *next_cursor = cJSON_GetObjectItem(root, "nextCursor");
            if (next_cursor && cJSON_IsString(next_cursor) && next_cursor->valuestring && next_cursor->valuestring[0]) {
                snprintf(ctx->next_cursor, sizeof(ctx->next_cursor), "%s", next_cursor->valuestring);
                ctx->has_next_cursor = true;
            } else {
                ctx->has_next_cursor = false;
                ctx->next_cursor[0] = '\0';
            }

            cJSON *tools = cJSON_GetObjectItem(root, "tools");
            if (!ctx->has_first_tool_name && tools && cJSON_IsArray(tools)) {
                cJSON *first = cJSON_GetArrayItem(tools, 0);
                cJSON *name = first ? cJSON_GetObjectItem(first, "name") : NULL;
                if (name && cJSON_IsString(name) && name->valuestring && name->valuestring[0]) {
                    snprintf(ctx->first_tool_name, sizeof(ctx->first_tool_name), "%s", name->valuestring);
                    ctx->has_first_tool_name = true;
                    ESP_LOGI(TAG, "Discovered first tool from tools/list: %s", ctx->first_tool_name);
                }
            }
            cJSON_Delete(root);
        }
    }

    return ESP_OK;
}

/** Block until all in-flight responses have invoked @ref resp_cb (pending_responses == 0). */
static esp_err_t mcp_client_wait_pending_zero(resp_sync_ctx_t *sync_ctx, int timeout_ms)
{
    if (!sync_ctx) {
        return ESP_ERR_INVALID_ARG;
    }
    TickType_t start_tick = xTaskGetTickCount();
    while (atomic_load(&sync_ctx->pending_responses) > 0) {
        TickType_t elapsed_ticks = xTaskGetTickCount() - start_tick;
        int elapsed_ms = (int)(elapsed_ticks * portTICK_PERIOD_MS);
        if (elapsed_ms >= timeout_ms) {
            return ESP_ERR_TIMEOUT;
        }
        int remaining_ms = timeout_ms - elapsed_ms;
        int wait_ms = (remaining_ms > 1000) ? 1000 : remaining_ms;
        xSemaphoreTake(sync_ctx->resp_sem, pdMS_TO_TICKS(wait_ms));
    }
    return ESP_OK;
}

static esp_err_t mcp_client_post_with_tracking(resp_sync_ctx_t *sync_ctx,
                                               esp_mcp_mgr_handle_t mgr,
                                               const esp_mcp_mgr_req_t *req,
                                               mcp_post_fn_t post_fn,
                                               const char *req_name,
                                               int required)
{
    if (!(sync_ctx && req && post_fn)) {
        return ESP_ERR_INVALID_ARG;
    }

    atomic_fetch_add(&sync_ctx->pending_responses, 1);
    esp_err_t ret = post_fn(mgr, req);
    if (ret != ESP_OK) {
        atomic_fetch_sub(&sync_ctx->pending_responses, 1);
        ESP_LOGE(TAG, "%s failed: %s", req_name ? req_name : "request", esp_err_to_name(ret));
        if (required) {
            return ret;
        }
    }

    return ESP_OK;
}

static esp_err_t mcp_client_auth_callback(const esp_mcp_mgr_auth_context_t *context,
                                          char *out_bearer_token,
                                          size_t out_bearer_token_len,
                                          void *user_ctx)
{
    (void)context;
    (void)user_ctx;

    if (out_bearer_token && out_bearer_token_len > 0 && CONFIG_MCP_HTTP_AUTH_BEARER_TOKEN[0] != '\0') {
        strncpy(out_bearer_token, CONFIG_MCP_HTTP_AUTH_BEARER_TOKEN, out_bearer_token_len - 1);
        out_bearer_token[out_bearer_token_len - 1] = '\0';
    }

    return ESP_OK;
}

static void mcp_client_task(void *arg)
{
    (void)arg;
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    char base_url[128] = {0};
    mcp_client_build_base_url(base_url, sizeof(base_url));
    ESP_LOGI(TAG, "Remote base_url=%s, endpoint=%s", base_url, CONFIG_MCP_REMOTE_EP);

    esp_http_client_config_t httpc_cfg = {
        .url = base_url,
        .timeout_ms = CONFIG_MCP_TRANSPORT_TIME,
    };

    esp_mcp_mgr_config_t mgr_cfg = {
        .transport = esp_mcp_transport_http_client,
        .config = &httpc_cfg,
        .instance = mcp,
    };

    esp_mcp_mgr_handle_t mgr = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(mgr_cfg, &mgr));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(mgr));
    ESP_ERROR_CHECK(esp_mcp_mgr_set_bearer_token(mgr, CONFIG_MCP_HTTP_AUTH_BEARER_TOKEN));
    ESP_ERROR_CHECK(esp_mcp_mgr_set_auth_callback(mgr, mcp_client_auth_callback, NULL));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(mgr, CONFIG_MCP_REMOTE_EP, NULL));

    resp_sync_ctx_t *sync_ctx = calloc(1, sizeof(resp_sync_ctx_t));
    ESP_ERROR_CHECK(sync_ctx ? ESP_OK : ESP_ERR_NO_MEM);

    sync_ctx->resp_sem = xSemaphoreCreateCounting(10, 0);
    ESP_ERROR_CHECK(sync_ctx->resp_sem ? ESP_OK : ESP_ERR_NO_MEM);
    sync_ctx->pending_responses = 0;

    esp_mcp_mgr_req_t req = {
        .ep_name = CONFIG_MCP_REMOTE_EP,
        .cb = resp_cb,
        .user_ctx = sync_ctx,
        .u.init = {
            /* Server may negotiate a different protocolVersion in the initialize result. */
            .protocol_version = "2025-11-25",
            .name = "mcp_client",
            .version = "0.1.0"
        },
    };
    ret = mcp_client_post_with_tracking(sync_ctx, mgr, &req, esp_mcp_mgr_post_info_init, "initialize", 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MCP initialization request failed, skip follow-up requests");
        goto cleanup;
    }
    ret = mcp_client_wait_pending_zero(sync_ctx, 30000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Timeout waiting for initialize (and optional notifications/initialized)");
        goto cleanup;
    }

    sync_ctx->has_first_tool_name = false;
    sync_ctx->first_tool_name[0] = '\0';
    sync_ctx->has_next_cursor = false;
    sync_ctx->next_cursor[0] = '\0';

    req.u.list.cursor = NULL;
    req.u.list.limit = -1;
    (void)mcp_client_post_with_tracking(sync_ctx, mgr, &req, esp_mcp_mgr_post_tools_list, "tools/list page-1", 0);
    (void)mcp_client_wait_pending_zero(sync_ctx, 30000);

#if CONFIG_MCP_TOOLS_LIST_PAGINATION
    for (int page = 1; page < CONFIG_MCP_TOOLS_LIST_MAX_PAGES; page++) {
        if (!sync_ctx->has_next_cursor) {
            break;
        }
        req.u.list.cursor = sync_ctx->next_cursor;
        req.u.list.limit = -1;
        ESP_LOGI(TAG, "Requesting tools/list page-%d with cursor=%s", page + 1, req.u.list.cursor);
        sync_ctx->has_next_cursor = false;
        sync_ctx->next_cursor[0] = '\0';
        (void)mcp_client_post_with_tracking(sync_ctx, mgr, &req, esp_mcp_mgr_post_tools_list, "tools/list next-page", 0);
        (void)mcp_client_wait_pending_zero(sync_ctx, 30000);
    }
#endif

#if CONFIG_MCP_DYNAMIC_TOOL_CALL
    if (sync_ctx->has_first_tool_name) {
        req.u.call.tool_name = sync_ctx->first_tool_name;
        req.u.call.args_json = "{}";
        ESP_LOGI(TAG, "Calling first discovered tool dynamically: %s", req.u.call.tool_name);
        (void)mcp_client_post_with_tracking(sync_ctx, mgr, &req, esp_mcp_mgr_post_tools_call, "tools/call dynamic-first", 0);
    } else {
        ESP_LOGW(TAG, "No tool discovered from tools/list, skip dynamic tools/call");
    }
#endif

    // Example tool call (works with the mcp_server example tools)
    req.u.call.tool_name = "self.get_device_status";
    req.u.call.args_json = "{}";
    (void)mcp_client_post_with_tracking(sync_ctx, mgr, &req, esp_mcp_mgr_post_tools_call, "tools/call self.get_device_status", 0);

    req.u.call.tool_name = "self.audio_speaker.set_volume";
    req.u.call.args_json = "{\"volume\": 50}";
    (void)mcp_client_post_with_tracking(sync_ctx, mgr, &req, esp_mcp_mgr_post_tools_call, "tools/call self.audio_speaker.set_volume", 0);

    req.u.call.tool_name = "self.screen.set_brightness";
    req.u.call.args_json = "{\"brightness\": 50}";
    (void)mcp_client_post_with_tracking(sync_ctx, mgr, &req, esp_mcp_mgr_post_tools_call, "tools/call self.screen.set_brightness", 0);

    req.u.call.tool_name = "self.screen.set_theme";
    req.u.call.args_json = "{\"theme\": \"dark\"}";
    (void)mcp_client_post_with_tracking(sync_ctx, mgr, &req, esp_mcp_mgr_post_tools_call, "tools/call self.screen.set_theme", 0);

    req.u.call.tool_name = "self.screen.set_hsv";
    req.u.call.args_json = "{\"HSV\": [180, 50, 80]}";
    (void)mcp_client_post_with_tracking(sync_ctx, mgr, &req, esp_mcp_mgr_post_tools_call, "tools/call self.screen.set_hsv", 0);

    req.u.call.tool_name = "self.screen.set_rgb";
    req.u.call.args_json = "{\"RGB\": {\"red\": 255, \"green\": 128, \"blue\": 0}}";
    (void)mcp_client_post_with_tracking(sync_ctx, mgr, &req, esp_mcp_mgr_post_tools_call, "tools/call self.screen.set_rgb", 0);

#if CONFIG_MCP_DEMO_RESOURCES
    ESP_LOGI(TAG, "Running optional resources/* demo requests");
    atomic_fetch_add(&sync_ctx->pending_responses, 1);
    ret = esp_mcp_mgr_post_resources_list(mgr, CONFIG_MCP_REMOTE_EP, NULL, -1, resp_cb, sync_ctx);
    if (ret != ESP_OK) {
        atomic_fetch_sub(&sync_ctx->pending_responses, 1);
        ESP_LOGW(TAG, "resources/list skipped: %s", esp_err_to_name(ret));
    }

    atomic_fetch_add(&sync_ctx->pending_responses, 1);
    ret = esp_mcp_mgr_post_resources_templates_list(mgr, CONFIG_MCP_REMOTE_EP, NULL, -1, resp_cb, sync_ctx);
    if (ret != ESP_OK) {
        atomic_fetch_sub(&sync_ctx->pending_responses, 1);
        ESP_LOGW(TAG, "resources/templates/list skipped: %s", esp_err_to_name(ret));
    }

    atomic_fetch_add(&sync_ctx->pending_responses, 1);
    ret = esp_mcp_mgr_post_resources_read(mgr,
                                          CONFIG_MCP_REMOTE_EP,
                                          CONFIG_MCP_DEMO_RESOURCE_URI,
                                          resp_cb,
                                          sync_ctx);
    if (ret != ESP_OK) {
        atomic_fetch_sub(&sync_ctx->pending_responses, 1);
        ESP_LOGW(TAG, "resources/read skipped: %s", esp_err_to_name(ret));
    }
#endif

#if CONFIG_MCP_DEMO_PROMPTS
    ESP_LOGI(TAG, "Running optional prompts/* demo requests");
    atomic_fetch_add(&sync_ctx->pending_responses, 1);
    ret = esp_mcp_mgr_post_prompts_list(mgr, CONFIG_MCP_REMOTE_EP, NULL, -1, resp_cb, sync_ctx);
    if (ret != ESP_OK) {
        atomic_fetch_sub(&sync_ctx->pending_responses, 1);
        ESP_LOGW(TAG, "prompts/list skipped: %s", esp_err_to_name(ret));
    }

    atomic_fetch_add(&sync_ctx->pending_responses, 1);
    ret = esp_mcp_mgr_post_prompts_get(mgr,
                                       CONFIG_MCP_REMOTE_EP,
                                       CONFIG_MCP_DEMO_PROMPT_NAME,
                                       "{}",
                                       resp_cb,
                                       sync_ctx);
    if (ret != ESP_OK) {
        atomic_fetch_sub(&sync_ctx->pending_responses, 1);
        ESP_LOGW(TAG, "prompts/get skipped: %s", esp_err_to_name(ret));
    }
#endif

    ret = mcp_client_wait_pending_zero(sync_ctx, 30000);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Timeout waiting for %d pending responses", atomic_load(&sync_ctx->pending_responses));
        vTaskDelay(pdMS_TO_TICKS(100));
    } else {
        ESP_LOGI(TAG, "All responses received");
    }

cleanup:
    if (atomic_load(&sync_ctx->pending_responses) > 0) {
        (void)mcp_client_wait_pending_zero(sync_ctx, 5000);
    }

    if (mgr) {
        ret = esp_mcp_mgr_stop(mgr);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "esp_mcp_mgr_stop failed: %s", esp_err_to_name(ret));
        }
        ret = esp_mcp_mgr_deinit(mgr);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "esp_mcp_mgr_deinit failed: %s", esp_err_to_name(ret));
        }
    }

    ret = esp_mcp_destroy(mcp);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "esp_mcp_destroy failed: %s", esp_err_to_name(ret));
    }

    if (sync_ctx->resp_sem) {
        vSemaphoreDelete(sync_ctx->resp_sem);
        sync_ctx->resp_sem = NULL;
    }
    free(sync_ctx);
    sync_ctx = NULL;

    vTaskDelete(NULL);
}

void app_main(void)
{
    BaseType_t ok = xTaskCreate(mcp_client_task, "mcp_client_task", MCP_CLIENT_TASK_STACK_SIZE, NULL, 5, NULL);
    ESP_ERROR_CHECK(ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
