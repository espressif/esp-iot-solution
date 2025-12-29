/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

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
#include "esp_mcp_item.h"
#include "esp_mcp_priv.h"
#include "esp_mcp_error.h"

static const char *TAG = "esp_mcp_mgr";

typedef enum {
    ESP_MCP_POST_INIT = 0,
    ESP_MCP_POST_TOOLS_LIST,
    ESP_MCP_POST_TOOLS_CALL,
} esp_mcp_mgr_post_t;

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

    SLIST_HEAD(pending_cb_table_t, esp_mcp_mgr_pending_cb) pending_cbs; /*!< Pending callbacks keyed by id */
    SemaphoreHandle_t pending_mutex;
} esp_mcp_mgr_t;

typedef struct esp_mcp_mgr_pending_cb {
    char *ep_name;
    char *tool_name;
    uint16_t id;
    esp_mcp_mgr_resp_cb_t cb;
    void *user_ctx;
    SLIST_ENTRY(esp_mcp_mgr_pending_cb) next;
} esp_mcp_mgr_pending_cb_t;

static void esp_mcp_mgr_pending_cb_free(esp_mcp_mgr_pending_cb_t *node)
{
    if (!node) {
        return;
    }
    if (node->ep_name) {
        free(node->ep_name);
    }
    if (node->tool_name) {
        free(node->tool_name);
    }
    free(node);
}

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
    SLIST_INIT(&(*mcp_ctx)->pending_cbs);

    (*mcp_ctx)->endpoints_mutex = xSemaphoreCreateMutex();
    if (!(*mcp_ctx)->endpoints_mutex) {
        ESP_LOGE(TAG, "Failed to create endpoints mutex");
        free(*mcp_ctx);
        return ESP_ERR_NO_MEM;
    }

    (*mcp_ctx)->pending_mutex = xSemaphoreCreateMutex();
    if (!(*mcp_ctx)->pending_mutex) {
        ESP_LOGE(TAG, "Failed to create pending mutex");
        vSemaphoreDelete((*mcp_ctx)->endpoints_mutex);
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
                SLIST_REMOVE(&mcp_ctx->endpoints, ep, esp_mcp_mgr_ep, next);
                if (ep) {
                    if (ep->ep_name) {
                        free((void *)ep->ep_name);
                    }
                    free(ep);
                }
            }
            xSemaphoreGive(mcp_ctx->endpoints_mutex);
        }
        vSemaphoreDelete(mcp_ctx->endpoints_mutex);
    }

    if (mcp_ctx->pending_mutex) {
        if (xSemaphoreTake(mcp_ctx->pending_mutex, portMAX_DELAY) == pdTRUE) {
            esp_mcp_mgr_pending_cb_t *node, *node_next;
            SLIST_FOREACH_SAFE(node, &mcp_ctx->pending_cbs, next, node_next) {
                SLIST_REMOVE(&mcp_ctx->pending_cbs, node, esp_mcp_mgr_pending_cb, next);
                esp_mcp_mgr_pending_cb_free(node);
            }
            xSemaphoreGive(mcp_ctx->pending_mutex);
        }
        vSemaphoreDelete(mcp_ctx->pending_mutex);
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

static esp_err_t esp_mcp_mgr_pending_cb_add(esp_mcp_mgr_t *mcp_ctx, const char *ep_name, const char *tool_name, uint16_t id, esp_mcp_mgr_resp_cb_t cb, void *user_ctx)
{
    if (!cb || id == 0) {
        return ESP_OK;
    }
    if (!mcp_ctx->pending_mutex || xSemaphoreTake(mcp_ctx->pending_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_mcp_mgr_pending_cb_t *node = calloc(1, sizeof(esp_mcp_mgr_pending_cb_t));
    if (!node) {
        xSemaphoreGive(mcp_ctx->pending_mutex);
        return ESP_ERR_NO_MEM;
    }
    node->id = id;
    node->cb = cb;
    node->user_ctx = user_ctx;
    node->ep_name = ep_name ? strdup(ep_name) : NULL;
    if (ep_name && !node->ep_name) {
        free(node);
        xSemaphoreGive(mcp_ctx->pending_mutex);
        return ESP_ERR_NO_MEM;
    }
    node->tool_name = tool_name ? strdup(tool_name) : NULL;
    if (tool_name && !node->tool_name) {
        if (node->ep_name) {
            free(node->ep_name);
        }
        free(node);
        xSemaphoreGive(mcp_ctx->pending_mutex);
        return ESP_ERR_NO_MEM;
    }
    SLIST_INSERT_HEAD(&mcp_ctx->pending_cbs, node, next);
    xSemaphoreGive(mcp_ctx->pending_mutex);
    return ESP_OK;
}

static esp_mcp_mgr_pending_cb_t *esp_mcp_mgr_pending_cb_take(esp_mcp_mgr_t *mcp_ctx, uint16_t id)
{
    esp_mcp_mgr_pending_cb_t *node = NULL, *tmp;
    if (!mcp_ctx->pending_mutex || xSemaphoreTake(mcp_ctx->pending_mutex, portMAX_DELAY) != pdTRUE) {
        return NULL;
    }
    SLIST_FOREACH(tmp, &mcp_ctx->pending_cbs, next) {
        if (tmp->id == id) {
            node = tmp;
            SLIST_REMOVE(&mcp_ctx->pending_cbs, tmp, esp_mcp_mgr_pending_cb, next);
            break;
        }
    }
    xSemaphoreGive(mcp_ctx->pending_mutex);
    return node;
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
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

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

esp_err_t esp_mcp_mgr_perform_handle(esp_mcp_mgr_handle_t handle, const uint8_t *inbuf, uint16_t inlen)
{
    esp_mcp_mgr_t *mcp_ctx = (esp_mcp_mgr_t *)handle;
    ESP_RETURN_ON_FALSE(mcp_ctx && inbuf, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    esp_mcp_resp_t parse_result = {0};
    esp_mcp_t *mcp_instance = (esp_mcp_t *)mcp_ctx->config.instance;
    ESP_RETURN_ON_FALSE(mcp_instance, ESP_ERR_INVALID_STATE, TAG, "MCP instance not configured");

    esp_err_t ret = esp_mcp_resp_parse(mcp_instance, (const char *)inbuf, &parse_result);
    esp_mcp_mgr_pending_cb_t *pending = esp_mcp_mgr_pending_cb_take(mcp_ctx, parse_result.id);
    if (pending && pending->cb) {
        const char *name = pending->tool_name ? pending->tool_name : pending->ep_name;
        switch (ret) {
        case ESP_OK:
            pending->cb(parse_result.is_error, name, parse_result.output, pending->user_ctx);
            break;
        case ESP_ERR_INVALID_RESPONSE:
            pending->cb(parse_result.error_code, name, parse_result.error_message, pending->user_ctx);
            break;
        default:
            pending->cb(ret, name, NULL, pending->user_ctx);
            break;
        }
    }

    esp_mcp_resp_free(parse_result.output);
    if (parse_result.error_message) {
        free((void *)parse_result.error_message);
        parse_result.error_message = NULL;
    }
    esp_mcp_mgr_pending_cb_free(pending);
    return ESP_OK;
}

static esp_err_t esp_mcp_mgr_req_perform(esp_mcp_mgr_handle_t handle,
                                         const char *ep_name, const char *tool_name, uint16_t req_id,
                                         const uint8_t *inbuf, uint16_t inlen,
                                         esp_mcp_mgr_resp_cb_t cb, void *user_ctx)
{
    esp_mcp_mgr_t *mcp_ctx = (esp_mcp_mgr_t *)handle;
    ESP_RETURN_ON_FALSE(mcp_ctx && ep_name && inbuf, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    const esp_mcp_transport_t *transport = &mcp_ctx->config.transport;
    ESP_RETURN_ON_FALSE(transport->request, ESP_ERR_NOT_SUPPORTED, TAG, "Transport does not support outbound request");

    esp_err_t ret = esp_mcp_mgr_pending_cb_add(mcp_ctx, ep_name, tool_name, req_id, cb, user_ctx);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = transport->request(mcp_ctx->transport_handle, inbuf, inlen);
    if (ret != ESP_OK) {
        esp_mcp_mgr_pending_cb_t *pending = esp_mcp_mgr_pending_cb_take(mcp_ctx, req_id);
        esp_mcp_mgr_pending_cb_free(pending);
        return ret;
    }

    return ESP_OK;
}

static esp_err_t esp_mcp_mgr_post_req(esp_mcp_mgr_handle_t handle, const esp_mcp_mgr_req_t *req, esp_mcp_mgr_post_t type)
{
    ESP_RETURN_ON_FALSE(handle && req && req->ep_name, ESP_ERR_INVALID_ARG, TAG, "Invalid args");

    esp_mcp_mgr_t *mcp_ctx = (esp_mcp_mgr_t *)handle;
    esp_mcp_t *mcp_instance = (esp_mcp_t *)mcp_ctx->config.instance;
    ESP_RETURN_ON_FALSE(mcp_instance, ESP_ERR_INVALID_STATE, TAG, "MCP instance not configured");

    char *req_json = NULL;
    uint16_t req_id = 0;
    esp_err_t ret = ESP_OK;
    const char *ep_name = req->ep_name;
    const char *tool_name = NULL;
    esp_mcp_info_t info = {
        .protocol_version = "2024-11-05",
        .name = "esp-mcp-client",
        .version = "0.1.0",
        .cursor = NULL,
        .tool_name = NULL,
        .args_json = NULL,
    };

    switch (type) {
    case ESP_MCP_POST_INIT: {
        info.protocol_version = req->u.init.protocol_version;
        info.name = req->u.init.name;
        info.version = req->u.init.version;
        ret = esp_mcp_info_init(mcp_instance, (const esp_mcp_info_t *)&info, &req_json, &req_id);
        ESP_RETURN_ON_ERROR(ret, TAG, "Build initialize failed");
        tool_name = "initialize";
        break;
    }
    case ESP_MCP_POST_TOOLS_LIST: {
        info.cursor = req->u.list.cursor;
        ret = esp_mcp_tools_list(mcp_instance, (const esp_mcp_info_t *)&info, &req_json, &req_id);
        ESP_RETURN_ON_ERROR(ret, TAG, "Build tools/list failed");
        tool_name = "toolslist";
        break;
    }
    case ESP_MCP_POST_TOOLS_CALL: {
        ESP_RETURN_ON_FALSE(req->u.call.tool_name, ESP_ERR_INVALID_ARG, TAG, "Invalid tool name");
        info.tool_name = req->u.call.tool_name;
        info.args_json = req->u.call.args_json;
        ret = esp_mcp_tools_call(mcp_instance, (const esp_mcp_info_t *)&info, &req_json, &req_id);
        ESP_RETURN_ON_ERROR(ret, TAG, "Build tools/call failed");
        tool_name = req->u.call.tool_name;
        break;
    }
    default:
        ret = ESP_ERR_INVALID_ARG;
        break;
    }

    if (req_json) {
        size_t req_json_len = strlen(req_json);
        if (req_json_len > UINT16_MAX) {
            ESP_LOGE(TAG, "Request JSON too long: %zu bytes (max: %u)", req_json_len, UINT16_MAX);
            esp_mcp_resp_free(req_json);
            return ESP_ERR_INVALID_SIZE;
        }
        ret = esp_mcp_mgr_req_perform(handle, ep_name, tool_name, req_id, (const uint8_t *)req_json, (uint16_t)req_json_len, req->cb, req->user_ctx);
        esp_mcp_resp_free(req_json);
    }
    return ret;
}

esp_err_t esp_mcp_mgr_post_info_init(esp_mcp_mgr_handle_t handle, const esp_mcp_mgr_req_t *req)
{
    ESP_RETURN_ON_FALSE(req, ESP_ERR_INVALID_ARG, TAG, "Invalid request");
    return esp_mcp_mgr_post_req(handle, req, ESP_MCP_POST_INIT);
}

esp_err_t esp_mcp_mgr_post_tools_list(esp_mcp_mgr_handle_t handle, const esp_mcp_mgr_req_t *req)
{
    ESP_RETURN_ON_FALSE(req, ESP_ERR_INVALID_ARG, TAG, "Invalid request");
    return esp_mcp_mgr_post_req(handle, req, ESP_MCP_POST_TOOLS_LIST);
}

esp_err_t esp_mcp_mgr_post_tools_call(esp_mcp_mgr_handle_t handle, const esp_mcp_mgr_req_t *req)
{
    ESP_RETURN_ON_FALSE(req, ESP_ERR_INVALID_ARG, TAG, "Invalid request");
    return esp_mcp_mgr_post_req(handle, req, ESP_MCP_POST_TOOLS_CALL);
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
