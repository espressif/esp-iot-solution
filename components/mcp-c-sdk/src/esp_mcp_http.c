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
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <sys/queue.h>
#include "esp_mcp_priv.h"
#include "esp_mcp_mgr.h"

static const char *TAG = "esp_mcp_http";

/**
 * @brief MCP HTTP item structure
 *
 * Structure containing MCP handle, HTTP server handle, message buffer, and message buffer size.
 */
typedef struct esp_mcp_http_item_s {
    esp_mcp_mgr_handle_t handle;     /*!< MCP handle */
    httpd_handle_t   httpd;      /*!< HTTP server handle */
} esp_mcp_http_item_t;

static esp_err_t esp_mcp_http_post_handler(httpd_req_t *req)
{
    ESP_RETURN_ON_FALSE(req, ESP_ERR_INVALID_ARG, TAG, "Invalid request");
    ESP_RETURN_ON_FALSE(req->user_ctx, ESP_ERR_INVALID_ARG, TAG, "Invalid user context");

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    int total_len = req->content_len;
    int cur_len = 0;
    int recv_len = 0;
    esp_err_t ret = ESP_OK;
    esp_mcp_http_item_t *mcp_http = (esp_mcp_http_item_t *)req->user_ctx;
    char *mbuf = calloc(1, total_len + 1);
    ESP_RETURN_ON_FALSE(mbuf, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for message buffer");

    while (cur_len < total_len) {
        recv_len = httpd_req_recv(req, mbuf + cur_len, total_len - cur_len);
        if (recv_len <= 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
            free(mbuf);
            ESP_RETURN_ON_FALSE(recv_len == HTTPD_SOCK_ERR_TIMEOUT, ESP_FAIL, TAG, "Failed to receive data, error: %d", recv_len);
        }
        cur_len += recv_len;
    }
    mbuf[total_len] = '\0';

    esp_mcp_mgr_req_handle(mcp_http->handle, req->uri + 1, (const uint8_t *)mbuf, total_len, &outbuf, &outlen);
    free(mbuf);
    if (outbuf && outlen > 0) {
        httpd_resp_set_hdr(req, "Connection", "keep-alive");
        httpd_resp_set_type(req, "application/json");
        ret = httpd_resp_send(req, (char *)outbuf, outlen);
        esp_mcp_mgr_req_destroy_response(mcp_http->handle, outbuf);
    } else {
        httpd_resp_set_status(req, HTTPD_204);
        httpd_resp_set_type(req, "application/json");
        ret = httpd_resp_send(req, NULL, 0);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_FAIL, TAG, "Failed to send empty response %s", esp_err_to_name(ret));
    }

    return ESP_OK;
}

static esp_err_t esp_mcp_http_init(esp_mcp_mgr_handle_t handle, esp_mcp_transport_handle_t *transport_handle)
{
    esp_mcp_http_item_t *mcp_http = calloc(1, sizeof(esp_mcp_http_item_t));
    ESP_RETURN_ON_FALSE(mcp_http, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for HTTP item");

    mcp_http->handle = handle;
    *transport_handle = (esp_mcp_transport_handle_t)mcp_http;

    return ESP_OK;
}

static esp_err_t esp_mcp_http_deinit(esp_mcp_transport_handle_t handle)
{
    esp_mcp_http_item_t *mcp_http = (esp_mcp_http_item_t *)handle;
    ESP_RETURN_ON_FALSE(mcp_http, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    mcp_http->handle = 0;
    free(mcp_http);

    return ESP_OK;
}

static esp_err_t esp_mcp_http_create_config(const void *config, void **config_out)
{
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Invalid configuration");

    httpd_config_t *http_config = (httpd_config_t *)config;
    httpd_config_t *http_new_config = calloc(1, sizeof(httpd_config_t));
    ESP_RETURN_ON_FALSE(http_new_config, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for HTTP configuration");

    *http_new_config = *http_config;
    *config_out = http_new_config;

    return ESP_OK;
}

static esp_err_t esp_mcp_http_delete_config(void *config)
{
    httpd_config_t *http_config = (httpd_config_t *)config;
    ESP_RETURN_ON_FALSE(http_config, ESP_ERR_INVALID_ARG, TAG, "Invalid configuration");

    free(http_config);

    return ESP_OK;
}

static esp_err_t esp_mcp_http_start(esp_mcp_transport_handle_t handle, void *config)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Invalid configuration");

    esp_mcp_http_item_t *mcp_http = (esp_mcp_http_item_t *)handle;
    httpd_config_t *http_config = (httpd_config_t *)config;

    esp_err_t ret = httpd_start(&mcp_http->httpd, http_config);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_FAIL, TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));

    return ESP_OK;
}

static esp_err_t esp_mcp_http_stop(esp_mcp_transport_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_mcp_http_item_t *mcp_http = (esp_mcp_http_item_t *)handle;

    esp_err_t ret = httpd_stop(mcp_http->httpd);
    ESP_RETURN_ON_ERROR(ret, TAG, "HTTP server stop failed: %s", esp_err_to_name(ret));

    return ESP_OK;
}

static esp_err_t esp_mcp_http_register_endpoint(esp_mcp_transport_handle_t handle, const char *ep_name, void *priv_data)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(ep_name, ESP_ERR_INVALID_ARG, TAG, "Invalid endpoint name");

    esp_mcp_http_item_t *mcp_http = (esp_mcp_http_item_t *)handle;

    char *ep_uri = calloc(1, strlen(ep_name) + 2);
    ESP_RETURN_ON_FALSE(ep_uri, ESP_ERR_NO_MEM, TAG, "Malloc failed for ep uri");

    sprintf(ep_uri, "/%s", ep_name);

    httpd_uri_t config_handler = {
        .uri      = ep_uri,
        .method   = HTTP_POST,
        .handler  = esp_mcp_http_post_handler,
        .user_ctx = mcp_http
    };

    esp_err_t ret = httpd_register_uri_handler(mcp_http->httpd, &config_handler);
    free(ep_uri);

    ESP_RETURN_ON_ERROR(ret, TAG, "Uri handler register failed: %s", esp_err_to_name(ret));

    return ESP_OK;
}

static esp_err_t esp_mcp_http_unregister_endpoint(esp_mcp_transport_handle_t handle, const char *ep_name)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(ep_name, ESP_ERR_INVALID_ARG, TAG, "Invalid endpoint name");

    esp_mcp_http_item_t *mcp_http = (esp_mcp_http_item_t *)handle;

    char *ep_uri = calloc(1, strlen(ep_name) + 2);
    ESP_RETURN_ON_FALSE(ep_uri, ESP_ERR_NO_MEM, TAG, "Malloc failed for ep uri");

    sprintf(ep_uri, "/%s", ep_name);

    esp_err_t ret = httpd_unregister_uri(mcp_http->httpd, ep_uri);
    free(ep_uri);

    ESP_RETURN_ON_ERROR(ret, TAG, "Uri handler unregister failed: %s", esp_err_to_name(ret));

    return ESP_OK;
}

const esp_mcp_transport_t esp_mcp_transport_http = {
    .init                = esp_mcp_http_init,
    .deinit              = esp_mcp_http_deinit,
    .create_config       = esp_mcp_http_create_config,
    .delete_config       = esp_mcp_http_delete_config,
    .start               = esp_mcp_http_start,
    .stop                = esp_mcp_http_stop,
    .register_endpoint   = esp_mcp_http_register_endpoint,
    .unregister_endpoint = esp_mcp_http_unregister_endpoint,
};
