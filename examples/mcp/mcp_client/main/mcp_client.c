/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>
#include <stdatomic.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "protocol_examples_common.h"

#include "esp_mcp_engine.h"
#include "esp_mcp_mgr.h"
#include "esp_http_client.h"

static const char *TAG = "mcp_client";

/**
 * @brief Response synchronization context
 */
typedef struct {
    SemaphoreHandle_t resp_sem;      /*!< Semaphore for response synchronization */
    atomic_int pending_responses;    /*!< Counter for pending responses */
} resp_sync_ctx_t;

static esp_err_t resp_cb(int error_code, const char *ep_name, const char *resp_json, void *user_ctx)
{
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
        ESP_LOGE(TAG, "[Parse Failure] Endpoint: %s, Error: %s", ep_name ? ep_name : "NULL", esp_err_to_name(error_code));
        return ESP_OK;
    }

    // Protocol-level error: error_code is negative JSON-RPC error code
    if (error_code < 0) {
        ESP_LOGE(TAG, "[Protocol Error] Endpoint: %s, Code: %d, Message: %s", ep_name ? ep_name : "NULL", error_code, resp_json);
        return ESP_OK;
    }

    // Application-level response
    if (error_code == 0) {
        ESP_LOGI(TAG, "[Success] Endpoint: %s, Response: %s", ep_name ? ep_name : "NULL", resp_json);
    } else if (error_code == 1) {
        ESP_LOGW(TAG, "[Application Error] Endpoint: %s, Response: %s",  ep_name ? ep_name : "NULL", resp_json);
    } else {
        ESP_LOGW(TAG, "[Unexpected] Endpoint: %s, Error code: %d, Response: %s", ep_name ? ep_name : "NULL", error_code, resp_json);
    }

    return ESP_OK;
}

void app_main(void)
{
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
    snprintf(base_url, sizeof(base_url), "http://%s:%d", CONFIG_MCP_REMOTE_HOST, CONFIG_MCP_REMOTE_PORT);
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
            .protocol_version = "2024-11-05",
            .name = "mcp_client",
            .version = "0.1.0"
        },
    };
    atomic_fetch_add(&sync_ctx->pending_responses, 1);
    ESP_ERROR_CHECK(esp_mcp_mgr_post_info_init(mgr, &req));

    req.u.list.cursor = NULL;
    atomic_fetch_add(&sync_ctx->pending_responses, 1);
    ESP_ERROR_CHECK(esp_mcp_mgr_post_tools_list(mgr, &req));

    // Example tool call (works with the mcp_server example tools)
    req.u.call.tool_name = "self.get_device_status";
    req.u.call.args_json = "{}";
    atomic_fetch_add(&sync_ctx->pending_responses, 1);
    ESP_ERROR_CHECK(esp_mcp_mgr_post_tools_call(mgr, &req));

    req.u.call.tool_name = "self.audio_speaker.set_volume";
    req.u.call.args_json = "{\"volume\": 50}";
    atomic_fetch_add(&sync_ctx->pending_responses, 1);
    ESP_ERROR_CHECK(esp_mcp_mgr_post_tools_call(mgr, &req));

    req.u.call.tool_name = "self.screen.set_brightness";
    req.u.call.args_json = "{\"brightness\": 50}";
    atomic_fetch_add(&sync_ctx->pending_responses, 1);
    ESP_ERROR_CHECK(esp_mcp_mgr_post_tools_call(mgr, &req));

    req.u.call.tool_name = "self.screen.set_theme";
    req.u.call.args_json = "{\"theme\": \"dark\"}";
    atomic_fetch_add(&sync_ctx->pending_responses, 1);
    ESP_ERROR_CHECK(esp_mcp_mgr_post_tools_call(mgr, &req));

    req.u.call.tool_name = "self.screen.set_hsv";
    req.u.call.args_json = "{\"HSV\": [180, 50, 80]}";
    atomic_fetch_add(&sync_ctx->pending_responses, 1);
    ESP_ERROR_CHECK(esp_mcp_mgr_post_tools_call(mgr, &req));

    req.u.call.tool_name = "self.screen.set_rgb";
    req.u.call.args_json = "{\"RGB\": {\"red\": 255, \"green\": 128, \"blue\": 0}}";
    atomic_fetch_add(&sync_ctx->pending_responses, 1);
    ESP_ERROR_CHECK(esp_mcp_mgr_post_tools_call(mgr, &req));

    const int timeout_ms = 30000;
    TickType_t start_tick = xTaskGetTickCount();
    while (atomic_load(&sync_ctx->pending_responses) > 0) {
        TickType_t elapsed_ticks = xTaskGetTickCount() - start_tick;
        int elapsed_ms = (int)(elapsed_ticks * portTICK_PERIOD_MS);
        if (elapsed_ms >= timeout_ms) {
            break;
        }
        int remaining_ms = timeout_ms - elapsed_ms;
        int wait_ms = (remaining_ms > 1000) ? 1000 : remaining_ms;
        xSemaphoreTake(sync_ctx->resp_sem, pdMS_TO_TICKS(wait_ms));
    }

    int remaining = atomic_load(&sync_ctx->pending_responses);
    if (remaining > 0) {
        ESP_LOGW(TAG, "Timeout waiting for %d pending responses", remaining);
        vTaskDelay(pdMS_TO_TICKS(100));
    } else {
        ESP_LOGI(TAG, "All responses received");
    }

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(mgr));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(mgr));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));

    if (sync_ctx->resp_sem) {
        vSemaphoreDelete(sync_ctx->resp_sem);
        sync_ctx->resp_sem = NULL;
    }
    free(sync_ctx);
    sync_ctx = NULL;
}
