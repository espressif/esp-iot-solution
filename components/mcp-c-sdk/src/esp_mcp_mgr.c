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
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <sys/queue.h>
#include <cJSON.h>

#include "esp_mcp_engine.h"
#include "esp_mcp_mgr.h"
#include "esp_mcp_priv.h"
#include "esp_mcp_error.h"
#include "esp_mcp_item.h"

static const char *TAG = "esp_mcp_mgr";

/**
 * @brief MCP endpoint structure
 */
typedef struct esp_mcp_mgr_ep {
    const char *ep_name;                            /*!< Endpoint name (e.g., "tools/list") */
    esp_mcp_ep_handler_t handler;                   /*!< Endpoint handler function pointer */
    void *priv_data;                                /*!< Private data passed to handler */
    SLIST_ENTRY(esp_mcp_mgr_ep) next;               /*!< Next endpoint in the list */
} esp_mcp_mgr_ep_t;

/**
 * @brief MCP manager structure (transport manager / router)
 */
typedef struct esp_mcp_mgr_s {
    esp_mcp_mgr_config_t config;                    /*!< MCP configuration */
    void *transport_config;                         /*!< MCP transport configuration pointer */
    esp_mcp_transport_handle_t transport_handle;    /*!< MCP transport handle */

    SLIST_HEAD(ep_table_t, esp_mcp_mgr_ep) endpoints;   /*!< Endpoint list head */
    SemaphoreHandle_t endpoints_mutex;              /*!< Mutex to protect endpoints list */
} esp_mcp_mgr_t;

static esp_err_t esp_mcp_mgr_message_handler(const uint8_t *inbuf, uint16_t inlen, uint8_t **outbuf, uint16_t *outlen, void *priv_data)
{
    uint8_t *tmp_outbuf = NULL;
    uint16_t tmp_outlen = 0;
    esp_mcp_mgr_t *mcp_ctx = (esp_mcp_mgr_t *)priv_data;
    ESP_RETURN_ON_FALSE(mcp_ctx, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP context");

    esp_mcp_t *engine = (esp_mcp_t *)mcp_ctx->config.instance;
    ESP_RETURN_ON_FALSE(engine, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP engine");

    ESP_LOGI(TAG, "Received message: %s", (char *)inbuf);
    esp_mcp_parse_message(engine, (char *)inbuf);
    esp_mcp_get_mbuf(engine, &tmp_outbuf, &tmp_outlen);

    if (tmp_outbuf && tmp_outlen > 0) {
        ESP_LOGI(TAG, "Sending response: %s", (char *)tmp_outbuf);
        *outbuf = tmp_outbuf;
        *outlen = tmp_outlen;
    }

    return ESP_OK;
}

static esp_err_t esp_mcp_mgr_new(esp_mcp_mgr_t **mcp_ctx)
{
    ESP_RETURN_ON_FALSE(mcp_ctx, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    *mcp_ctx = (esp_mcp_mgr_t *)calloc(1, sizeof(esp_mcp_mgr_t));
    ESP_RETURN_ON_FALSE(*mcp_ctx, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for MCP item");

    SLIST_INIT(&(*mcp_ctx)->endpoints);

    (*mcp_ctx)->endpoints_mutex = xSemaphoreCreateMutex();
    if (!(*mcp_ctx)->endpoints_mutex) {
        ESP_LOGE(TAG, "Failed to create endpoints mutex");
        free(*mcp_ctx);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static esp_err_t esp_mcp_mgr_delete(esp_mcp_mgr_t *mcp_ctx)
{
    ESP_RETURN_ON_FALSE(mcp_ctx, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    if (mcp_ctx->endpoints_mutex) {
        if (xSemaphoreTake(mcp_ctx->endpoints_mutex, portMAX_DELAY) == pdTRUE) {
            esp_mcp_mgr_ep_t *ep, *ep_next;
            SLIST_FOREACH_SAFE(ep, &mcp_ctx->endpoints, next, ep_next) {
                if (ep) {
                    if (ep->ep_name) {
                        free((void *)ep->ep_name);
                    }
                    free(ep);
                }
            }
            xSemaphoreGive(mcp_ctx->endpoints_mutex);
            vSemaphoreDelete(mcp_ctx->endpoints_mutex);
        }
    }

    free(mcp_ctx);
    return ESP_OK;
}

static esp_mcp_mgr_ep_t *esp_mcp_mgr_search_endpoint(esp_mcp_mgr_t *mcp_ctx, const char *ep_name)
{
    if (!mcp_ctx || !ep_name) {
        return NULL;
    }

    if (xSemaphoreTake(mcp_ctx->endpoints_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take endpoints mutex");
        return NULL;
    }

    esp_mcp_mgr_ep_t *ep;
    SLIST_FOREACH(ep, &mcp_ctx->endpoints, next) {
        if (ep && strcmp(ep->ep_name, ep_name) == 0) {
            xSemaphoreGive(mcp_ctx->endpoints_mutex);
            return ep;
        }
    }

    xSemaphoreGive(mcp_ctx->endpoints_mutex);
    return NULL;
}

static esp_err_t esp_mcp_mgr_add_endpoint_internal(esp_mcp_mgr_t *mcp_ctx,
                                                   const char *ep_name,
                                                   esp_mcp_ep_handler_t handler,
                                                   void *priv_data)
{
    ESP_RETURN_ON_FALSE(mcp_ctx && ep_name && handler, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    if (xSemaphoreTake(mcp_ctx->endpoints_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take endpoints mutex");
        return ESP_ERR_INVALID_STATE;
    }

    esp_mcp_mgr_ep_t *ep;
    SLIST_FOREACH(ep, &mcp_ctx->endpoints, next) {
        if (ep && strcmp(ep->ep_name, ep_name) == 0) {
            xSemaphoreGive(mcp_ctx->endpoints_mutex);
            ESP_LOGE(TAG, "Endpoint %s already exists", ep_name);
            return ESP_ERR_INVALID_STATE;
        }
    }

    ep = (esp_mcp_mgr_ep_t *)calloc(1, sizeof(esp_mcp_mgr_ep_t));
    if (!ep) {
        xSemaphoreGive(mcp_ctx->endpoints_mutex);
        ESP_LOGE(TAG, "Failed to allocate endpoint");
        return ESP_ERR_NO_MEM;
    }

    ep->ep_name = strdup(ep_name);
    if (!ep->ep_name) {
        ESP_LOGE(TAG, "Failed to allocate memory for endpoint name");
        free(ep);
        xSemaphoreGive(mcp_ctx->endpoints_mutex);
        return ESP_ERR_NO_MEM;
    }

    ep->handler = handler;
    ep->priv_data = priv_data;

    SLIST_INSERT_HEAD(&mcp_ctx->endpoints, ep, next);
    xSemaphoreGive(mcp_ctx->endpoints_mutex);

    return ESP_OK;
}

static esp_err_t esp_mcp_mgr_remove_endpoint_internal(esp_mcp_mgr_t *mcp_ctx, const char *ep_name)
{
    ESP_RETURN_ON_FALSE(mcp_ctx && ep_name, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    if (xSemaphoreTake(mcp_ctx->endpoints_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take endpoints mutex");
        return ESP_ERR_INVALID_STATE;
    }

    esp_mcp_mgr_ep_t *ep;
    SLIST_FOREACH(ep, &mcp_ctx->endpoints, next) {
        if (ep && strcmp(ep->ep_name, ep_name) == 0) {
            SLIST_REMOVE(&mcp_ctx->endpoints, ep, esp_mcp_mgr_ep, next);
            xSemaphoreGive(mcp_ctx->endpoints_mutex);

            if (ep->ep_name) {
                free((void *)ep->ep_name);
            }
            free(ep);
            return ESP_OK;
        }
    }

    xSemaphoreGive(mcp_ctx->endpoints_mutex);
    ESP_LOGE(TAG, "Endpoint %s not found", ep_name);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t esp_mcp_mgr_init(esp_mcp_mgr_config_t config, esp_mcp_mgr_handle_t *handle)
{
    void *fn_ptrs[] = {
        config.transport.init,
        config.transport.deinit,
        config.transport.start,
        config.transport.stop,
        config.transport.create_config,
        config.transport.delete_config,
        config.transport.register_endpoint,
        config.transport.unregister_endpoint,
    };

    for (size_t i = 0; i < sizeof(fn_ptrs) / sizeof(fn_ptrs[0]); i++) {
        if (!fn_ptrs[i]) {
            return ESP_ERR_INVALID_ARG;
        }
    }

    esp_mcp_mgr_t *mcp_ctx = NULL;
    esp_err_t ret = esp_mcp_mgr_new(&mcp_ctx);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to create MCP context: %s", esp_err_to_name(ret));
    ESP_RETURN_ON_FALSE(mcp_ctx, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for MCP manager context");

    mcp_ctx->config = config;

    const esp_mcp_transport_t *transport = &mcp_ctx->config.transport;
    transport->create_config(mcp_ctx->config.config, &mcp_ctx->transport_config);
    if (!mcp_ctx->transport_config) {
        ESP_LOGE(TAG, "failed to allocate provisioning transport configuration");
        free(mcp_ctx);
        return ESP_ERR_NO_MEM;
    }

    transport->init((esp_mcp_mgr_handle_t)mcp_ctx, &mcp_ctx->transport_handle);
    if (!mcp_ctx->transport_handle) {
        ESP_LOGE(TAG, "Failed to initialize transport");
        transport->delete_config(mcp_ctx->transport_config);
        mcp_ctx->transport_config = NULL;
        free(mcp_ctx);
        return ESP_ERR_INVALID_STATE;
    }

    *handle = (esp_mcp_mgr_handle_t)mcp_ctx;

    return ESP_OK;
}

esp_err_t esp_mcp_mgr_deinit(esp_mcp_mgr_handle_t handle)
{
    esp_mcp_mgr_t *mcp_ctx = (esp_mcp_mgr_t *)handle;
    ESP_RETURN_ON_FALSE(mcp_ctx, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP handle");

    const esp_mcp_transport_t *transport = &mcp_ctx->config.transport;
    if (transport->delete_config) {
        transport->delete_config(mcp_ctx->transport_config);
    }

    if (transport->deinit) {
        transport->deinit(mcp_ctx->transport_handle);
    }

    esp_mcp_mgr_delete(mcp_ctx);

    return ESP_OK;
}

esp_err_t esp_mcp_mgr_start(esp_mcp_mgr_handle_t handle)
{
    esp_mcp_mgr_t *mcp_ctx = (esp_mcp_mgr_t *)handle;
    ESP_RETURN_ON_FALSE(mcp_ctx, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP handle");

    const esp_mcp_transport_t *transport = &mcp_ctx->config.transport;

    return transport->start(mcp_ctx->transport_handle, mcp_ctx->transport_config);
}

esp_err_t esp_mcp_mgr_stop(esp_mcp_mgr_handle_t handle)
{
    esp_mcp_mgr_t *mcp_ctx = (esp_mcp_mgr_t *)handle;
    ESP_RETURN_ON_FALSE(mcp_ctx, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP handle");

    const esp_mcp_transport_t *transport = &mcp_ctx->config.transport;

    return transport->stop(mcp_ctx->transport_handle);
}

esp_err_t esp_mcp_mgr_register_endpoint(esp_mcp_mgr_handle_t handle, const char *ep_name, void *priv_data)
{
    esp_mcp_mgr_t *mcp_ctx = (esp_mcp_mgr_t *)handle;
    ESP_RETURN_ON_FALSE(mcp_ctx, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP handle");

    esp_err_t ret = esp_mcp_mgr_add_endpoint_internal(mcp_ctx, ep_name, esp_mcp_mgr_message_handler, mcp_ctx);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to add endpoint: %s", esp_err_to_name(ret));

    const esp_mcp_transport_t *transport = &mcp_ctx->config.transport;

    return transport->register_endpoint(mcp_ctx->transport_handle, ep_name, priv_data);
}

esp_err_t esp_mcp_mgr_unregister_endpoint(esp_mcp_mgr_handle_t handle, const char *ep_name)
{
    esp_mcp_mgr_t *mcp_ctx = (esp_mcp_mgr_t *)handle;
    ESP_RETURN_ON_FALSE(mcp_ctx, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP handle");

    esp_err_t ret = esp_mcp_mgr_remove_endpoint_internal(mcp_ctx, ep_name);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to remove endpoint: %s", esp_err_to_name(ret));

    const esp_mcp_transport_t *transport = &mcp_ctx->config.transport;

    return transport->unregister_endpoint(mcp_ctx->transport_handle, ep_name);
}

esp_err_t esp_mcp_mgr_req_handle(esp_mcp_mgr_handle_t handle,
                                 const char *ep_name,
                                 const uint8_t *inbuf,
                                 uint16_t inlen,
                                 uint8_t **outbuf,
                                 uint16_t *outlen)
{
    esp_mcp_mgr_t *mcp_ctx = (esp_mcp_mgr_t *)handle;
    ESP_RETURN_ON_FALSE(mcp_ctx && ep_name, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    esp_mcp_mgr_ep_t *ep = esp_mcp_mgr_search_endpoint(mcp_ctx, ep_name);
    if (!ep) {
        ESP_LOGE(TAG, "Endpoint %s not found", ep_name);
        return ESP_ERR_NOT_FOUND;
    }

    // Note: We release the mutex before calling the handler to avoid deadlock
    // The handler callback should not modify the endpoints list
    return ep->handler((const uint8_t *)inbuf, inlen, outbuf, outlen, ep->priv_data);
}

esp_err_t esp_mcp_mgr_req_destroy_response(esp_mcp_mgr_handle_t handle, uint8_t *response_buffer)
{
    esp_mcp_mgr_t *mcp_ctx = (esp_mcp_mgr_t *)handle;
    ESP_RETURN_ON_FALSE(mcp_ctx, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP handle");
    ESP_RETURN_ON_FALSE(mcp_ctx->config.instance, ESP_ERR_INVALID_ARG, TAG, "Invalid server instance");
    ESP_RETURN_ON_FALSE(response_buffer, ESP_ERR_INVALID_ARG, TAG, "Invalid response buffer");

    esp_mcp_t *engine = (esp_mcp_t *)mcp_ctx->config.instance;

    return esp_mcp_remove_mbuf(engine, response_buffer);
}

esp_err_t esp_mcp_mgr_req_destroy_response_by_id(esp_mcp_mgr_handle_t handle, uint16_t id)
{
    esp_mcp_mgr_t *mcp_ctx = (esp_mcp_mgr_t *)handle;
    ESP_RETURN_ON_FALSE(mcp_ctx, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP handle");
    ESP_RETURN_ON_FALSE(mcp_ctx->config.instance, ESP_ERR_INVALID_ARG, TAG, "Invalid server instance");
    ESP_RETURN_ON_FALSE(id != 0, ESP_ERR_INVALID_ARG, TAG, "Invalid id");

    esp_mcp_t *engine = (esp_mcp_t *)mcp_ctx->config.instance;
    ESP_RETURN_ON_FALSE(engine->mbuf.outbuf, ESP_ERR_NOT_FOUND, TAG, "Response not found");
    ESP_RETURN_ON_FALSE(engine->mbuf.id == id, ESP_ERR_NOT_FOUND, TAG, "Response id not found");

    return esp_mcp_remove_mbuf(engine, engine->mbuf.outbuf);
}
