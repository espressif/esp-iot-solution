/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <esp_log.h>
#include <esp_check.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cJSON.h>

#include "esp_mcp_server.h"
#include "esp_mcp_http.h"
#include "esp_mcp.h"
#include "esp_mcp_server_priv.h"

static const char *TAG = "esp_mcp";

typedef struct esp_mcp_item_s {
    esp_mcp_server_t*           server;     /*!< MCP server instance (for server role) */
    esp_mcp_config_t            config;     /*!< MCP configuration */
    esp_mcp_transport_funcs_t   funcs;      /*!< Transport callback functions */
    char*                       mbuf;       /*!< MCP message buffer */
    uint16_t                    mbuf_size;  /*!< MCP message buffer size */
    bool                        run;        /*!< Flag to indicate if the instance is running */
} esp_mcp_item_t;

static int esp_mcp_open(esp_mcp_handle_t t, esp_mcp_transport_config_t *config)
{
    esp_mcp_item_t *mcp = (esp_mcp_item_t *)t;
    if (mcp && mcp->funcs.open) {
        return mcp->funcs.open(mcp->funcs.transport, config);
    }
    return -1;
}

static int esp_mcp_read(esp_mcp_handle_t t, char *buffer, int len, int timeout_ms)
{
    esp_mcp_item_t *mcp = (esp_mcp_item_t *)t;
    if (mcp && mcp->funcs.read) {
        return mcp->funcs.read(mcp->funcs.transport, buffer, len, timeout_ms);
    }
    return -1;
}

static int esp_mcp_write(esp_mcp_handle_t t, const char *buffer, int len, int timeout_ms)
{
    esp_mcp_item_t *mcp = (esp_mcp_item_t *)t;
    if (mcp && mcp->funcs.write) {
        return mcp->funcs.write(mcp->funcs.transport, buffer, len, timeout_ms);
    }
    return -1;
}

static int esp_mcp_close(esp_mcp_handle_t t)
{
    esp_mcp_item_t *mcp = (esp_mcp_item_t *)t;
    if (mcp && mcp->funcs.close) {
        return mcp->funcs.close(mcp->funcs.transport);
    }
    return -1;
}

static void esp_mcp_task(void *pvParameters)
{
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    int recv_len = 0;
    esp_mcp_item_t *mcp = (esp_mcp_item_t *)pvParameters;

    esp_mcp_transport_config_t config = {
        .host = mcp->config.host,
        .port = mcp->config.port,
        .uri = mcp->config.uri,
        .timeout_ms = mcp->config.timeout_ms
    };
    esp_mcp_open((esp_mcp_handle_t)mcp, &config);

    while (mcp->run) {
        recv_len = esp_mcp_read((esp_mcp_handle_t)mcp, mcp->mbuf, mcp->config.mbuf_size, portMAX_DELAY);
        if (recv_len < 0 || recv_len == 0) {
            continue;
        }

        ESP_LOGI(TAG, "content: %s", (char*)mcp->mbuf);
        esp_mcp_server_parse_message(mcp->server, (char*)mcp->mbuf);
        esp_mcp_server_get_mbuf(mcp->server, 0, &outlen, &outbuf);
        if (outbuf) {
            ESP_LOGI(TAG, "outbuf: %s", (char*)outbuf);
        }
        esp_mcp_write((esp_mcp_handle_t)mcp, (char*)outbuf, outlen, portMAX_DELAY);
    }

    esp_mcp_close((esp_mcp_handle_t)mcp);

    vTaskDelete(NULL);
}

esp_err_t esp_mcp_init(const esp_mcp_config_t* config, esp_mcp_handle_t* handle)
{
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Invalid config");
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_mcp_item_t *mcp_item = NULL;
    mcp_item = (esp_mcp_item_t *)calloc(1, sizeof(esp_mcp_item_t));
    ESP_RETURN_ON_FALSE(mcp_item, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for MCP instance");

    mcp_item->config.task_priority = config->task_priority;
    mcp_item->config.stack_size = config->stack_size;
    mcp_item->config.core_id = config->core_id;
    mcp_item->config.task_caps = config->task_caps;
    mcp_item->config.type = config->type;
    mcp_item->config.host = config->host;
    mcp_item->config.port = config->port;
    mcp_item->config.timeout_ms = config->timeout_ms;
    mcp_item->config.uri = config->uri;
    mcp_item->config.mbuf_size = config->mbuf_size;

    mcp_item->mbuf = (char*)calloc(1, mcp_item->config.mbuf_size);
    ESP_RETURN_ON_FALSE(mcp_item->mbuf, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for MCP message buffer");

    mcp_item->server = config->instance;
    mcp_item->run = false;

    *handle = (esp_mcp_handle_t)mcp_item;

    switch (config->type) {
#if CONFIG_MCP_TRANSPORT_HTTP
    case ESP_MCP_TRANSPORT_TYPE_HTTP:
        ESP_ERROR_CHECK(esp_mcp_http_init((esp_mcp_handle_t)mcp_item));
        break;
#endif
    case ESP_MCP_TRANSPORT_TYPE_CUSTOM:
        ESP_LOGI(TAG, "Custom transport mode - call esp_mcp_transport_set_funcs() before esp_mcp_start()");
        break;
    default:
        ESP_LOGE(TAG, "Invalid or unsupported transport type: %d", config->type);
        free(mcp_item->mbuf);
        free(mcp_item);
        return ESP_ERR_NOT_SUPPORTED;
    }

    return ESP_OK;
}

esp_err_t esp_mcp_start(esp_mcp_handle_t handle)
{
    esp_mcp_item_t *mcp = (esp_mcp_item_t *)handle;
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP handle");
    ESP_RETURN_ON_FALSE(!mcp->run, ESP_OK, TAG, "MCP instance is already running");

    // Validate custom transport callbacks if using custom transport
    if (mcp->config.type == ESP_MCP_TRANSPORT_TYPE_CUSTOM) {
        ESP_RETURN_ON_FALSE(mcp->funcs.open && mcp->funcs.read && 
                           mcp->funcs.write && mcp->funcs.close, 
                           ESP_ERR_INVALID_STATE, TAG, 
                           "Custom transport functions not set. Call esp_mcp_transport_set_funcs() first");
    }

    mcp->run = true;
    BaseType_t ret = xTaskCreate(esp_mcp_task, "esp_mcp_task", mcp->config.stack_size, mcp, mcp->config.task_priority, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create MCP task");
        mcp->run = false;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "MCP instance started successfully");
    return ESP_OK;
}

esp_err_t esp_mcp_stop(esp_mcp_handle_t handle)
{
    esp_mcp_item_t *mcp = (esp_mcp_item_t *)handle;
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP handle");
    ESP_RETURN_ON_FALSE(mcp->run, ESP_OK, TAG, "MCP instance is not running");

    mcp->run = false;

    ESP_LOGI(TAG, "MCP instance stopped successfully");
    return ESP_OK;
}

esp_err_t esp_mcp_deinit(esp_mcp_handle_t handle)
{
    esp_mcp_item_t *mcp = (esp_mcp_item_t *)handle;
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP handle");

    if (mcp->run) {
        esp_mcp_stop(handle);
    }

    if (mcp->mbuf) {
        free(mcp->mbuf);
    }

    free(mcp);

    return ESP_OK;
}

esp_err_t esp_mcp_transport_set_funcs(esp_mcp_handle_t handle, esp_mcp_transport_funcs_t funcs)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(funcs.open && funcs.read && funcs.write && funcs.close, 
                       ESP_ERR_INVALID_ARG, TAG, 
                       "All transport callback functions (open, read, write, close) must be provided");

    esp_mcp_item_t *mcp = (esp_mcp_item_t *)handle;

    mcp->funcs = funcs;

    ESP_LOGI(TAG, "Custom transport callbacks registered successfully");
    return ESP_OK;
}
