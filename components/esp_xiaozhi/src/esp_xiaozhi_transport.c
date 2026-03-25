/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include <esp_log.h>
#include <esp_check.h>
#include "esp_xiaozhi_transport.h"
#include "esp_xiaozhi_mqtt.h"
#include "esp_xiaozhi_websocket.h"

static const char *TAG = "ESP_XIAOZHI_TRANSPORT";

typedef struct {
    bool use_mqtt;
    bool started;
    esp_xiaozhi_mqtt_handle_t mqtt_handle;
    esp_xiaozhi_websocket_handle_t websocket_handle;
} esp_xiaozhi_transport_ctx_t;

esp_err_t esp_xiaozhi_transport_init(esp_xiaozhi_transport_handle_t *handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_xiaozhi_transport_ctx_t *ctx = calloc(1, sizeof(esp_xiaozhi_transport_ctx_t));
    ESP_RETURN_ON_FALSE(ctx, ESP_ERR_NO_MEM, TAG, "Failed to allocate transport context");

    *handle = (esp_xiaozhi_transport_handle_t)ctx;
    return ESP_OK;
}

esp_err_t esp_xiaozhi_transport_deinit(esp_xiaozhi_transport_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_xiaozhi_transport_ctx_t *ctx = (esp_xiaozhi_transport_ctx_t *)handle;
    if (ctx->started) {
        esp_xiaozhi_transport_stop(handle);
    }
    free(ctx);
    return ESP_OK;
}

esp_err_t esp_xiaozhi_transport_start(esp_xiaozhi_transport_handle_t handle, bool use_mqtt,
                                      const esp_xiaozhi_transport_callbacks_t *callbacks)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(callbacks, ESP_ERR_INVALID_ARG, TAG, "Invalid callbacks");

    esp_xiaozhi_transport_ctx_t *ctx = (esp_xiaozhi_transport_ctx_t *)handle;
    ctx->use_mqtt = use_mqtt;
    ctx->started = true;

    esp_err_t ret;
    if (use_mqtt) {
        ret = esp_xiaozhi_mqtt_init(&ctx->mqtt_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to init MQTT: %s", esp_err_to_name(ret));
            ctx->started = false;
            return ret;
        }
        ret = esp_xiaozhi_mqtt_set_callbacks(ctx->mqtt_handle, (esp_xiaozhi_transport_handle_t)ctx, callbacks);
        if (ret != ESP_OK) {
            esp_xiaozhi_mqtt_deinit(ctx->mqtt_handle);
            ctx->started = false;
            return ret;
        }
        ret = esp_xiaozhi_mqtt_start(ctx->mqtt_handle);
        if (ret != ESP_OK) {
            esp_xiaozhi_mqtt_deinit(ctx->mqtt_handle);
            ctx->started = false;
            return ret;
        }
        ESP_LOGD(TAG, "Transport started with MQTT");
    } else {
        ret = esp_xiaozhi_websocket_init(&ctx->websocket_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to init WebSocket: %s", esp_err_to_name(ret));
            ctx->started = false;
            return ret;
        }
        ret = esp_xiaozhi_websocket_set_callbacks(ctx->websocket_handle, (esp_xiaozhi_transport_handle_t)ctx, callbacks);
        if (ret != ESP_OK) {
            esp_xiaozhi_websocket_deinit(ctx->websocket_handle);
            ctx->started = false;
            return ret;
        }
        ret = esp_xiaozhi_websocket_start(ctx->websocket_handle);
        if (ret != ESP_OK) {
            esp_xiaozhi_websocket_deinit(ctx->websocket_handle);
            ctx->started = false;
            return ret;
        }
        ESP_LOGD(TAG, "Transport started with WebSocket");
    }

    return ESP_OK;
}

esp_err_t esp_xiaozhi_transport_stop(esp_xiaozhi_transport_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_xiaozhi_transport_ctx_t *ctx = (esp_xiaozhi_transport_ctx_t *)handle;
    if (!ctx->started) {
        return ESP_OK;
    }

    if (ctx->use_mqtt) {
        esp_xiaozhi_mqtt_stop(ctx->mqtt_handle);
        esp_xiaozhi_mqtt_deinit(ctx->mqtt_handle);
    } else {
        esp_xiaozhi_websocket_stop(ctx->websocket_handle);
        esp_xiaozhi_websocket_deinit(ctx->websocket_handle);
    }
    ctx->started = false;

    return ESP_OK;
}

esp_err_t esp_xiaozhi_transport_send_text(esp_xiaozhi_transport_handle_t handle, const char *text)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(text, ESP_ERR_INVALID_ARG, TAG, "Invalid text");

    esp_xiaozhi_transport_ctx_t *ctx = (esp_xiaozhi_transport_ctx_t *)handle;
    if (ctx->use_mqtt) {
        return esp_xiaozhi_mqtt_send_text(ctx->mqtt_handle, text);
    } else {
        return esp_xiaozhi_websocket_send_text(ctx->websocket_handle, text);
    }
}

esp_err_t esp_xiaozhi_transport_send_binary(esp_xiaozhi_transport_handle_t handle, const uint8_t *data, size_t data_len)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(data, ESP_ERR_INVALID_ARG, TAG, "Invalid data");

    esp_xiaozhi_transport_ctx_t *ctx = (esp_xiaozhi_transport_ctx_t *)handle;
    if (ctx->use_mqtt) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return esp_xiaozhi_websocket_send_binary(ctx->websocket_handle, data, data_len);
}
