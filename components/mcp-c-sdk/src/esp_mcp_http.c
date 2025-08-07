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
#include <esp_http_server.h>

#include "esp_mcp.h"

static const char *TAG = "esp_mcp_http";

typedef struct esp_mcp_http_server {
    httpd_handle_t handle;
    httpd_req_t    *req;
    EventGroupHandle_t event_group;
    SemaphoreHandle_t mutex;
} esp_mcp_http_server_t;

#define MCP_HTTP_SERVER_READ_EVENT BIT0
#define MCP_HTTP_SERVER_WRITE_EVENT BIT1

static esp_err_t mcp_message_handler(httpd_req_t *req)
{
    esp_mcp_http_server_t *server = (esp_mcp_http_server_t *)req->user_ctx;
    if (!server) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "MCP server not initialized");
        ESP_RETURN_ON_ERROR(ESP_FAIL, TAG, "MCP server not initialized");
    }

    if (xSemaphoreTake(server->mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Server busy");
        ESP_LOGE(TAG, "Failed to acquire mutex, server busy");
        return ESP_FAIL;
    }

    if (server->req != NULL) {
        xSemaphoreGive(server->mutex);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Server busy");
        ESP_LOGE(TAG, "Previous request still in progress");
        return ESP_FAIL;
    }

    server->req = req;
    xSemaphoreGive(server->mutex);

    xEventGroupSetBits(server->event_group, MCP_HTTP_SERVER_READ_EVENT);

    EventBits_t bits = xEventGroupWaitBits(
                           server->event_group,
                           MCP_HTTP_SERVER_WRITE_EVENT,
                           pdTRUE,
                           pdFALSE,
                           pdMS_TO_TICKS(30000)
                       );

    if ((bits & MCP_HTTP_SERVER_WRITE_EVENT) == 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Request timeout");
        ESP_LOGE(TAG, "Timeout waiting for write completion");

        if (xSemaphoreTake(server->mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            server->req = NULL;
            xSemaphoreGive(server->mutex);
        } else {
            ESP_LOGE(TAG, "Failed to acquire mutex during timeout cleanup");
        }

        xEventGroupClearBits(server->event_group, MCP_HTTP_SERVER_READ_EVENT);
        
        return ESP_FAIL;
    }

    if (xSemaphoreTake(server->mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        server->req = NULL;
        xSemaphoreGive(server->mutex);
    } else {
        ESP_LOGE(TAG, "Failed to acquire mutex during cleanup");
    }

    return ESP_OK;
}

static int esp_mcp_http_open(esp_mcp_handle_t handle, esp_mcp_transport_config_t *config)
{
    ESP_RETURN_ON_FALSE(handle, ESP_FAIL, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(config, ESP_FAIL, TAG, "Invalid configuration");
    ESP_RETURN_ON_FALSE(config->uri, ESP_FAIL, TAG, "Invalid URI");
    ESP_RETURN_ON_FALSE(config->port > 0 && config->port < 65536, ESP_FAIL, TAG, "Invalid port: %d", config->port);

    esp_mcp_http_server_t *http_server = (esp_mcp_http_server_t *)handle;

    if (http_server->handle) {
        ESP_LOGW(TAG, "HTTP server already running");
        return ESP_OK;
    }

    httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();
    httpd_config.server_port = config->port;

    esp_err_t ret = httpd_start(&http_server->handle, &httpd_config);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_FAIL, TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));

    httpd_uri_t mcp_root = {
        .uri       = config->uri,
        .method    = HTTP_POST,
        .handler   = mcp_message_handler,
        .user_ctx  = http_server
    };

    ret = httpd_register_uri_handler(http_server->handle, &mcp_root);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register URI handler: %s", esp_err_to_name(ret));
        httpd_stop(http_server->handle);
        http_server->handle = NULL;
        return ESP_FAIL;
    }

    return ESP_OK;
}

static int esp_mcp_http_read(esp_mcp_handle_t handle, char *buffer, int len, int timeout_ms)
{
    ESP_RETURN_ON_FALSE(handle, ESP_FAIL, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(buffer, ESP_FAIL, TAG, "Invalid buffer");
    ESP_RETURN_ON_FALSE(len > 0, ESP_FAIL, TAG, "Invalid length");

    esp_mcp_http_server_t *http_server = (esp_mcp_http_server_t *)handle;

    EventBits_t bits = xEventGroupWaitBits(
                           http_server->event_group,
                           MCP_HTTP_SERVER_READ_EVENT,
                           pdTRUE,
                           pdFALSE,
                           pdMS_TO_TICKS(timeout_ms)
                       );

    ESP_RETURN_ON_FALSE(bits & MCP_HTTP_SERVER_READ_EVENT, ESP_FAIL, TAG, "Timeout waiting for read event");

    if (xSemaphoreTake(http_server->mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire mutex for read");
        return ESP_FAIL;
    }

    httpd_req_t *req = http_server->req;
    xSemaphoreGive(http_server->mutex);

    ESP_RETURN_ON_FALSE(req, ESP_FAIL, TAG, "No request available");

    int recv_len = httpd_req_recv(req, buffer, len);
    ESP_RETURN_ON_FALSE(recv_len >= 0, ESP_FAIL, TAG, "Failed to receive data, error: %d", recv_len);

    return recv_len;
}

static int esp_mcp_http_write(esp_mcp_handle_t handle, const char *buffer, int len, int timeout_ms)
{
    ESP_RETURN_ON_FALSE(handle, ESP_FAIL, TAG, "Invalid handle");

    esp_mcp_http_server_t *http_server = (esp_mcp_http_server_t *)handle;
    if (xSemaphoreTake(http_server->mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire mutex for write");
        return ESP_FAIL;
    }

    httpd_req_t *req = http_server->req;
    xSemaphoreGive(http_server->mutex);

    ESP_RETURN_ON_FALSE(req, ESP_FAIL, TAG, "No request available");

    esp_err_t ret = ESP_OK;

    if (buffer && len > 0) {
        ret = httpd_resp_set_hdr(req, "Connection", "keep-alive");
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_FAIL, TAG, "Failed to set Connection header %s", esp_err_to_name(ret));

        ret = httpd_resp_set_type(req, "application/json");
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_FAIL, TAG, "Failed to set Content-Type header %s", esp_err_to_name(ret));

        ret = httpd_resp_send(req, buffer, len);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_FAIL, TAG, "Failed to send response %s", esp_err_to_name(ret));
    } else {
        ret = httpd_resp_set_status(req, HTTPD_204);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_FAIL, TAG, "Failed to set status code %s", esp_err_to_name(ret));

        ret = httpd_resp_set_type(req, "application/json");
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_FAIL, TAG, "Failed to set Content-Type header %s", esp_err_to_name(ret));

        ret = httpd_resp_send(req, NULL, 0);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_FAIL, TAG, "Failed to send empty response %s", esp_err_to_name(ret));
    }

    xEventGroupSetBits(http_server->event_group, MCP_HTTP_SERVER_WRITE_EVENT);

    return len;
}

static int esp_mcp_http_close(esp_mcp_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_mcp_http_server_t *http_server = (esp_mcp_http_server_t *)handle;
    if (http_server->handle) {
        httpd_stop(http_server->handle);
    }

    if (http_server->event_group) {
        vEventGroupDelete(http_server->event_group);
    }
    
    if (http_server->mutex) {
        vSemaphoreDelete(http_server->mutex);
    }
    
    free(http_server);

    return ESP_OK;
}

esp_err_t esp_mcp_http_init(esp_mcp_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_mcp_http_server_t *http_server = calloc(1, sizeof(esp_mcp_http_server_t));
    ESP_RETURN_ON_FALSE(http_server, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for HTTP server");

    http_server->event_group = xEventGroupCreate();
    if (!http_server->event_group) {
        free(http_server);
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }

    http_server->mutex = xSemaphoreCreateMutex();
    if (!http_server->mutex) {
        vEventGroupDelete(http_server->event_group);
        free(http_server);
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    esp_mcp_transport_funcs_t funcs = {
        .transport = (uint32_t)http_server,
        .open = esp_mcp_http_open,
        .read = esp_mcp_http_read,
        .write = esp_mcp_http_write,
        .close = esp_mcp_http_close
    };
    esp_mcp_transport_set_funcs(handle, funcs);

    return ESP_OK;
}
