/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <esp_log.h>
#include <esp_check.h>
#include <cJSON.h>
#include "esp_mcp_mgr.h"
#include "esp_xiaozhi_transport.h"
#include "esp_xiaozhi_mcp.h"

/**
 * @brief MCP transport item structure; uses esp_xiaozhi_transport for MCP.
 */
typedef struct esp_xiaozhi_mcp_item_s {
    esp_xiaozhi_mcp_config_t *config;       /*!< XiaoZhi MCP transport configuration */
    esp_xiaozhi_transport_handle_t transport_handle; /*!< Transport handle (MQTT or WebSocket) */
    esp_mcp_mgr_handle_t handle;            /*!< MCP manager handle */
} esp_xiaozhi_mcp_item_t;

static const char *TAG = "ESP_XIAOZHI_MCP";

static esp_xiaozhi_mcp_item_t *s_xiaozhi_mcp = NULL;

static esp_xiaozhi_mcp_item_t *esp_xiaozhi_mcp_get_item_by_mgr_handle(esp_mcp_mgr_handle_t mgr_handle)
{
    esp_xiaozhi_mcp_item_t *mcp_item = (esp_xiaozhi_mcp_item_t *)s_xiaozhi_mcp;
    if (mcp_item && mcp_item->handle == mgr_handle) {
        return mcp_item;
    }

    return NULL;
}

esp_err_t esp_xiaozhi_mcp_handle_message(esp_mcp_mgr_handle_t mgr_handle,
                                         const char *ep_name,
                                         const char *payload,
                                         size_t payload_len,
                                         const char *session_id)
{
    ESP_RETURN_ON_FALSE(ep_name, ESP_ERR_INVALID_ARG, TAG, "Invalid endpoint name");
    ESP_RETURN_ON_FALSE(payload, ESP_ERR_INVALID_ARG, TAG, "Invalid payload");

    esp_xiaozhi_mcp_item_t *mcp_item = esp_xiaozhi_mcp_get_item_by_mgr_handle(mgr_handle);
    ESP_RETURN_ON_FALSE(mcp_item, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP item or handle mismatch");

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    esp_err_t ret = esp_mcp_mgr_req_handle(mgr_handle, ep_name, (const uint8_t *)payload, payload_len, &outbuf, &outlen);
    if (ret == ESP_OK && outbuf && outlen > 0) {
        cJSON *response_root = cJSON_CreateObject();
        if (session_id) {
            cJSON_AddStringToObject(response_root, "session_id", session_id);
        }
        cJSON_AddStringToObject(response_root, "type", "mcp");
        cJSON *response_payload = cJSON_Parse((char *)outbuf);
        if (response_payload) {
            cJSON_AddItemToObject(response_root, "payload", response_payload);
            char *json_str = cJSON_PrintUnformatted(response_root);
            if (json_str) {
                esp_xiaozhi_transport_send_text(mcp_item->transport_handle, json_str);
                cJSON_free(json_str);
            } else {
                ESP_LOGE(TAG, "Failed to print MCP response");
            }
        } else {
            ESP_LOGE(TAG, "Failed to parse MCP response payload");
        }
        cJSON_Delete(response_root);
        esp_mcp_mgr_req_destroy_response(mgr_handle, outbuf);
    } else if (ret != ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to process MCP message: %s", esp_err_to_name(ret));
    }

    return ESP_OK;
}

esp_err_t esp_xiaozhi_mcp_get_handle(esp_mcp_mgr_handle_t mgr_handle, esp_xiaozhi_transport_handle_t *transport_handle)
{
    ESP_RETURN_ON_FALSE(transport_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid transport handle");

    esp_xiaozhi_mcp_item_t *mcp_item = esp_xiaozhi_mcp_get_item_by_mgr_handle(mgr_handle);
    ESP_RETURN_ON_FALSE(mcp_item, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP item or handle mismatch");

    *transport_handle = mcp_item->transport_handle;

    return ESP_OK;
}

static esp_err_t esp_xiaozhi_mcp_init(esp_mcp_mgr_handle_t handle, esp_mcp_transport_handle_t *transport_handle)
{
    esp_xiaozhi_mcp_item_t *mcp_item = calloc(1, sizeof(esp_xiaozhi_mcp_item_t));
    ESP_RETURN_ON_FALSE(mcp_item, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for MCP transport item");

    mcp_item->handle = handle;
    mcp_item->config = NULL;

    esp_err_t ret = esp_xiaozhi_transport_init(&mcp_item->transport_handle);
    if (ret != ESP_OK) {
        free(mcp_item);
        return ret;
    }

    *transport_handle = (esp_mcp_transport_handle_t)mcp_item;
    s_xiaozhi_mcp = mcp_item;

    return ESP_OK;
}

static esp_err_t esp_xiaozhi_mcp_deinit(esp_mcp_transport_handle_t handle)
{
    esp_xiaozhi_mcp_item_t *mcp_item = (esp_xiaozhi_mcp_item_t *)handle;
    ESP_RETURN_ON_FALSE(mcp_item, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_xiaozhi_transport_deinit(mcp_item->transport_handle);
    mcp_item->handle = 0;
    mcp_item->config = NULL;
    if (s_xiaozhi_mcp == mcp_item) {
        s_xiaozhi_mcp = NULL;
    }
    free(mcp_item);

    return ESP_OK;
}

static esp_err_t esp_xiaozhi_mcp_create_config(const void *config, void **config_out)
{
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Invalid configuration");

    esp_xiaozhi_mcp_config_t *old_config = (esp_xiaozhi_mcp_config_t *)config;
    esp_xiaozhi_mcp_config_t *new_config = calloc(1, sizeof(esp_xiaozhi_mcp_config_t));
    ESP_RETURN_ON_FALSE(new_config, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for new configuration");

    new_config->ctx = old_config->ctx;
    new_config->use_mqtt = old_config->use_mqtt;
    new_config->callbacks = old_config->callbacks;

    if (old_config->session_id && old_config->session_id_len > 0) {
        new_config->session_id_len = old_config->session_id_len;
        new_config->session_id = calloc(1, new_config->session_id_len + 1);
        ESP_RETURN_ON_FALSE(new_config->session_id, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for session ID");
        memcpy(new_config->session_id, old_config->session_id, new_config->session_id_len);
    }

    *config_out = new_config;

    return ESP_OK;
}

static esp_err_t esp_xiaozhi_mcp_delete_config(void *config)
{
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Invalid configuration");

    esp_xiaozhi_mcp_config_t *old_config = (esp_xiaozhi_mcp_config_t *)config;
    if (old_config->session_id) {
        free(old_config->session_id);
    }
    free(old_config);

    return ESP_OK;
}

static esp_err_t esp_xiaozhi_mcp_start(esp_mcp_transport_handle_t handle, void *config)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Invalid configuration");

    esp_xiaozhi_mcp_item_t *mcp_item = (esp_xiaozhi_mcp_item_t *)handle;
    esp_xiaozhi_mcp_config_t *new_config = (esp_xiaozhi_mcp_config_t *)config;

    mcp_item->config = new_config;

    esp_err_t ret = esp_xiaozhi_transport_start(mcp_item->transport_handle, new_config->use_mqtt, &new_config->callbacks);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to start transport: %s", esp_err_to_name(ret));

    ESP_LOGD(TAG, "MCP transport started with %s", new_config->use_mqtt ? "MQTT" : "WebSocket");

    return ESP_OK;
}

static esp_err_t esp_xiaozhi_mcp_stop(esp_mcp_transport_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_xiaozhi_mcp_item_t *mcp_item = (esp_xiaozhi_mcp_item_t *)handle;

    esp_xiaozhi_transport_stop(mcp_item->transport_handle);
    mcp_item->config = NULL;

    ESP_LOGD(TAG, "MCP transport stopped");

    return ESP_OK;
}

static esp_err_t esp_xiaozhi_mcp_register_endpoint(esp_mcp_transport_handle_t handle, const char *ep_name, void *priv_data)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(ep_name, ESP_ERR_INVALID_ARG, TAG, "Invalid endpoint name");

    ESP_LOGD(TAG, "MCP endpoint registered: %s", ep_name);

    return ESP_OK;
}

static esp_err_t esp_xiaozhi_mcp_unregister_endpoint(esp_mcp_transport_handle_t handle, const char *ep_name)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(ep_name, ESP_ERR_INVALID_ARG, TAG, "Invalid endpoint name");

    ESP_LOGD(TAG, "MCP endpoint unregistered: %s", ep_name);

    return ESP_OK;
}

const esp_mcp_transport_t esp_mcp_transport_xiaozhi = {
    .init                = esp_xiaozhi_mcp_init,
    .deinit              = esp_xiaozhi_mcp_deinit,
    .create_config       = esp_xiaozhi_mcp_create_config,
    .delete_config       = esp_xiaozhi_mcp_delete_config,
    .start               = esp_xiaozhi_mcp_start,
    .stop                = esp_xiaozhi_mcp_stop,
    .register_endpoint   = esp_xiaozhi_mcp_register_endpoint,
    .unregister_endpoint = esp_xiaozhi_mcp_unregister_endpoint,
};
