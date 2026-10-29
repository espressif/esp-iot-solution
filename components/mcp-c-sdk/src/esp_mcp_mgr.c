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
#include <esp_timer.h>
#include <esp_random.h>

#include "esp_mcp_engine.h"
#include "esp_mcp_mgr.h"
#include "esp_mcp_item.h"
#include "esp_mcp_priv.h"
#include "esp_mcp_error.h"

static const char *TAG = "esp_mcp_mgr";

#if CONFIG_MCP_TRANSPORT_HTTP_CLIENT
esp_err_t esp_mcp_http_client_set_bearer_token(esp_mcp_transport_handle_t handle, const char *token);
esp_err_t esp_mcp_http_client_set_auth_callback(esp_mcp_transport_handle_t handle,
                                                esp_mcp_mgr_auth_cb_t cb,
                                                void *user_ctx);
esp_err_t esp_mcp_http_client_set_sse_event_callback(esp_mcp_transport_handle_t handle,
                                                     esp_mcp_mgr_sse_event_cb_t cb,
                                                     void *user_ctx);
#endif

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
    char *id_key;
    char *session_id;
    esp_mcp_mgr_resp_cb_t cb;
    void *user_ctx;
    uint32_t jsonrpc_request_id;
    int64_t deadline_ms;
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
    if (node->id_key) {
        free(node->id_key);
    }
    if (node->session_id) {
        free(node->session_id);
    }
    free(node);
}

static bool esp_mcp_mgr_protocol_requires_initialized_notification(const char *protocol_version);
static bool esp_mcp_mgr_parse_initialize_protocol_version(const char *result_json,
                                                          char *out_protocol_version,
                                                          size_t out_protocol_version_len);

static bool esp_mcp_mgr_uses_builtin_http_client_transport(const esp_mcp_mgr_t *mcp_ctx)
{
#if CONFIG_MCP_TRANSPORT_HTTP_CLIENT
    return mcp_ctx &&
           mcp_ctx->config.transport.init == esp_mcp_transport_http_client.init &&
           mcp_ctx->config.transport.start == esp_mcp_transport_http_client.start &&
           mcp_ctx->config.transport.request == esp_mcp_transport_http_client.request &&
           mcp_ctx->config.transport.open_stream == esp_mcp_transport_http_client.open_stream;
#else
    (void)mcp_ctx;
    return false;
#endif
}

static esp_err_t esp_mcp_mgr_message_handler(const uint8_t *inbuf, uint16_t inlen, uint8_t **outbuf, uint16_t *outlen, void *priv_data)
{
    esp_mcp_mgr_t *mcp_ctx = (esp_mcp_mgr_t *)priv_data;
    ESP_RETURN_ON_FALSE(mcp_ctx, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP context");

    esp_mcp_t *engine = (esp_mcp_t *)mcp_ctx->config.instance;
    ESP_RETURN_ON_FALSE(engine, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP engine");

    ESP_LOGI(TAG, "Received message: %s", (char *)inbuf);
    return esp_mcp_handle_message(engine, inbuf, inlen, outbuf, outlen);
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
        BaseType_t ep_locked = xSemaphoreTake(mcp_ctx->endpoints_mutex, pdMS_TO_TICKS(1000));
        if (ep_locked != pdTRUE) {
            ESP_LOGE(TAG, "Failed to acquire endpoints mutex for cleanup, forcing cleanup");
        }
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
        if (ep_locked == pdTRUE) {
            xSemaphoreGive(mcp_ctx->endpoints_mutex);
        }
        vSemaphoreDelete(mcp_ctx->endpoints_mutex);
        mcp_ctx->endpoints_mutex = NULL;
    }

    if (mcp_ctx->pending_mutex) {
        BaseType_t pend_locked = xSemaphoreTake(mcp_ctx->pending_mutex, pdMS_TO_TICKS(1000));
        if (pend_locked != pdTRUE) {
            ESP_LOGE(TAG, "Failed to acquire pending mutex for cleanup, forcing cleanup");
        }
        esp_mcp_mgr_pending_cb_t *node, *node_next;
        SLIST_FOREACH_SAFE(node, &mcp_ctx->pending_cbs, next, node_next) {
            SLIST_REMOVE(&mcp_ctx->pending_cbs, node, esp_mcp_mgr_pending_cb, next);
            esp_mcp_mgr_pending_cb_free(node);
        }
        if (pend_locked == pdTRUE) {
            xSemaphoreGive(mcp_ctx->pending_mutex);
        }
        vSemaphoreDelete(mcp_ctx->pending_mutex);
        mcp_ctx->pending_mutex = NULL;
    }

    free(mcp_ctx);
    return ESP_OK;
}

/** Clear engine back-pointer when tearing down a context that never completed esp_mcp_mgr_init. */
static void esp_mcp_mgr_detach_engine_if_bound(esp_mcp_mgr_t *mcp_ctx)
{
    if (!mcp_ctx || !mcp_ctx->config.instance) {
        return;
    }
    esp_mcp_t *engine = (esp_mcp_t *)mcp_ctx->config.instance;
    if (engine->mgr_ctx == (void *)mcp_ctx) {
        engine->mgr_ctx = NULL;
    }
}

/** After esp_mcp_mgr_new, init failed before a valid transport handle (lists empty; mutex cleanup via esp_mcp_mgr_delete). */
static void esp_mcp_mgr_abort_init(esp_mcp_mgr_t *mcp_ctx)
{
    esp_mcp_mgr_detach_engine_if_bound(mcp_ctx);
    (void)esp_mcp_mgr_delete(mcp_ctx);
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

static void esp_mcp_mgr_pending_cb_expire_locked(esp_mcp_mgr_t *mcp_ctx)
{
    int64_t now_ms = esp_timer_get_time() / 1000;
    esp_mcp_mgr_pending_cb_t *node, *next;
    SLIST_FOREACH_SAFE(node, &mcp_ctx->pending_cbs, next, next) {
        if (node->deadline_ms > 0 && now_ms > node->deadline_ms) {
            SLIST_REMOVE(&mcp_ctx->pending_cbs, node, esp_mcp_mgr_pending_cb, next);
            if (node->cb) {
                const char *name = node->tool_name ? node->tool_name : node->ep_name;
                node->cb(ESP_ERR_TIMEOUT, name, NULL, node->user_ctx, node->jsonrpc_request_id);
            }
            esp_mcp_mgr_pending_cb_free(node);
        }
    }
}

static char *esp_mcp_mgr_build_pending_key(const char *request_id, const char *session_id)
{
    if (!request_id || !request_id[0]) {
        return NULL;
    }
    const char *sid = session_id ? session_id : "";
    size_t n = strlen(sid) + 1 + strlen(request_id) + 1;
    char *key = calloc(1, n);
    if (!key) {
        return NULL;
    }
    snprintf(key, n, "%s|%s", sid, request_id);
    return key;
}

static esp_err_t esp_mcp_mgr_pending_cb_add(esp_mcp_mgr_t *mcp_ctx,
                                            const char *ep_name,
                                            const char *tool_name,
                                            const char *request_id,
                                            const char *session_id,
                                            esp_mcp_mgr_resp_cb_t cb,
                                            void *user_ctx,
                                            uint32_t jsonrpc_request_id,
                                            uint32_t timeout_ms)
{
    if (!request_id || !request_id[0]) {
        return ESP_OK;
    }
    if (!mcp_ctx->pending_mutex || xSemaphoreTake(mcp_ctx->pending_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_mcp_mgr_pending_cb_expire_locked(mcp_ctx);
    char *pending_key = esp_mcp_mgr_build_pending_key(request_id, session_id);
    if (!pending_key) {
        xSemaphoreGive(mcp_ctx->pending_mutex);
        return ESP_ERR_NO_MEM;
    }
    esp_mcp_mgr_pending_cb_t *it = NULL;
    SLIST_FOREACH(it, &mcp_ctx->pending_cbs, next) {
        if (it->id_key && strcmp(it->id_key, pending_key) == 0) {
            free(pending_key);
            xSemaphoreGive(mcp_ctx->pending_mutex);
            return ESP_ERR_INVALID_STATE;
        }
    }
    esp_mcp_mgr_pending_cb_t *node = calloc(1, sizeof(esp_mcp_mgr_pending_cb_t));
    if (!node) {
        free(pending_key);
        xSemaphoreGive(mcp_ctx->pending_mutex);
        return ESP_ERR_NO_MEM;
    }
    node->id_key = pending_key;
    node->cb = cb;
    node->user_ctx = user_ctx;
    node->jsonrpc_request_id = jsonrpc_request_id;
    node->deadline_ms = (esp_timer_get_time() / 1000) + (timeout_ms ? timeout_ms : 30000);
    node->ep_name = ep_name ? strdup(ep_name) : NULL;
    if (ep_name && !node->ep_name) {
        free(node->id_key);
        node->id_key = NULL;
        free(node);
        xSemaphoreGive(mcp_ctx->pending_mutex);
        return ESP_ERR_NO_MEM;
    }
    node->tool_name = tool_name ? strdup(tool_name) : NULL;
    if (tool_name && !node->tool_name) {
        if (node->ep_name) {
            free(node->ep_name);
        }
        free(node->id_key);
        free(node);
        xSemaphoreGive(mcp_ctx->pending_mutex);
        return ESP_ERR_NO_MEM;
    }
    node->session_id = session_id ? strdup(session_id) : NULL;
    if (session_id && !node->session_id) {
        if (node->ep_name) {
            free(node->ep_name);
        }
        if (node->tool_name) {
            free(node->tool_name);
        }
        free(node->id_key);
        free(node);
        xSemaphoreGive(mcp_ctx->pending_mutex);
        return ESP_ERR_NO_MEM;
    }
    SLIST_INSERT_HEAD(&mcp_ctx->pending_cbs, node, next);
    xSemaphoreGive(mcp_ctx->pending_mutex);
    return ESP_OK;
}

static esp_mcp_mgr_pending_cb_t *esp_mcp_mgr_pending_cb_take(esp_mcp_mgr_t *mcp_ctx,
                                                             const char *request_id,
                                                             const char *session_id)
{
    esp_mcp_mgr_pending_cb_t *node = NULL, *tmp;
    char *pending_key = esp_mcp_mgr_build_pending_key(request_id, session_id);
    if (!pending_key) {
        return NULL;
    }
    if (!mcp_ctx->pending_mutex || xSemaphoreTake(mcp_ctx->pending_mutex, portMAX_DELAY) != pdTRUE) {
        free(pending_key);
        return NULL;
    }
    esp_mcp_mgr_pending_cb_expire_locked(mcp_ctx);
    SLIST_FOREACH(tmp, &mcp_ctx->pending_cbs, next) {
        if (tmp->id_key && strcmp(tmp->id_key, pending_key) == 0) {
            node = tmp;
            SLIST_REMOVE(&mcp_ctx->pending_cbs, tmp, esp_mcp_mgr_pending_cb, next);
            break;
        }
    }
    xSemaphoreGive(mcp_ctx->pending_mutex);
    free(pending_key);
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

    // Best-effort back pointer for engine -> manager notifications
    if (mcp_ctx->config.instance) {
        esp_mcp_t *engine = (esp_mcp_t *)mcp_ctx->config.instance;
        engine->mgr_ctx = (void *)mcp_ctx;
    }

    const esp_mcp_transport_t *transport = &mcp_ctx->config.transport;
    transport->create_config(mcp_ctx->config.config, &mcp_ctx->transport_config);
    if (!mcp_ctx->transport_config) {
        ESP_LOGE(TAG, "failed to allocate provisioning transport configuration");
        esp_mcp_mgr_abort_init(mcp_ctx);
        return ESP_ERR_NO_MEM;
    }

    transport->init((esp_mcp_mgr_handle_t)mcp_ctx, &mcp_ctx->transport_handle);
    if (!mcp_ctx->transport_handle) {
        ESP_LOGE(TAG, "Failed to initialize transport");
        transport->delete_config(mcp_ctx->transport_config);
        mcp_ctx->transport_config = NULL;
        esp_mcp_mgr_abort_init(mcp_ctx);
        return ESP_ERR_INVALID_STATE;
    }

    *handle = (esp_mcp_mgr_handle_t)mcp_ctx;

    return ESP_OK;
}

esp_err_t esp_mcp_mgr_deinit(esp_mcp_mgr_handle_t handle)
{
    esp_mcp_mgr_t *mcp_ctx = (esp_mcp_mgr_t *)handle;
    ESP_RETURN_ON_FALSE(mcp_ctx, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP handle");

    if (mcp_ctx->config.instance) {
        esp_mcp_t *engine = (esp_mcp_t *)mcp_ctx->config.instance;
        if (engine->mgr_ctx == (void *)mcp_ctx) {
            engine->mgr_ctx = NULL;
        }
    }

    const esp_mcp_transport_t *transport = &mcp_ctx->config.transport;
    if (transport->delete_config) {
        transport->delete_config(mcp_ctx->transport_config);
    }

    if (transport->deinit) {
        transport->deinit(mcp_ctx->transport_handle);
    }

    esp_err_t del_ret = esp_mcp_mgr_delete(mcp_ctx);
    return del_ret;
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
    const char *session_id = esp_mcp_get_request_session_id(mcp_instance);
    bool is_response = (ret == ESP_OK || ret == ESP_ERR_INVALID_RESPONSE) && parse_result.id_str;
    if (is_response) {
        esp_mcp_mgr_pending_cb_t *pending = esp_mcp_mgr_pending_cb_take(mcp_ctx, parse_result.id_str, session_id);
        if (!pending && session_id && session_id[0]) {
            pending = esp_mcp_mgr_pending_cb_take(mcp_ctx, parse_result.id_str, NULL);
        }
        const char *name = pending ? (pending->tool_name ? pending->tool_name : pending->ep_name) : NULL;
        bool should_auto_post_initialized = false;
        char negotiated_protocol_version[32] = {0};
        if (pending && pending->cb) {
            switch (ret) {
            case ESP_OK:
                pending->cb(parse_result.is_error, name, parse_result.output, pending->user_ctx, pending->jsonrpc_request_id);
                break;
            case ESP_ERR_INVALID_RESPONSE:
                pending->cb(parse_result.error_code, name, parse_result.error_message, pending->user_ctx, pending->jsonrpc_request_id);
                break;
            default:
                pending->cb(ret, name, NULL, pending->user_ctx, pending->jsonrpc_request_id);
                break;
            }
        }

        if (ret == ESP_OK && !parse_result.is_error && name && strcmp(name, "initialize") == 0) {
            mcp_instance->client_initialized = true;
            if (esp_mcp_mgr_parse_initialize_protocol_version(parse_result.output,
                                                              negotiated_protocol_version,
                                                              sizeof(negotiated_protocol_version))) {
                mcp_instance->client_requires_initialized_notification =
                    esp_mcp_mgr_protocol_requires_initialized_notification(negotiated_protocol_version);
            } else {
                mcp_instance->client_requires_initialized_notification = false;
            }
            /* Do not clear client_sent_initialized_notification here: POST_INIT already reset it,
             * and the initialize response callback may have sent notifications/initialized. */
            should_auto_post_initialized = mcp_instance->client_requires_initialized_notification;
        }

        if (should_auto_post_initialized && pending && pending->ep_name &&
                !mcp_instance->client_sent_initialized_notification) {
            esp_err_t init_ret = esp_mcp_mgr_post_initialized(handle, pending->ep_name);
            if (init_ret != ESP_OK) {
                ESP_LOGW(TAG, "Auto notifications/initialized failed: %s", esp_err_to_name(init_ret));
            }
        }

        esp_mcp_resp_free(parse_result.output);
        if (parse_result.error_message) {
            free((void *)parse_result.error_message);
            parse_result.error_message = NULL;
        }
        if (parse_result.id_str) {
            free(parse_result.id_str);
            parse_result.id_str = NULL;
        }
        esp_mcp_mgr_pending_cb_free(pending);
        return ESP_OK;
    }

    esp_mcp_resp_free(parse_result.output);
    if (parse_result.error_message) {
        free((void *)parse_result.error_message);
        parse_result.error_message = NULL;
    }

    if (parse_result.id_str) {
        free(parse_result.id_str);
        parse_result.id_str = NULL;
    }

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    esp_err_t handle_ret = esp_mcp_handle_message(mcp_instance, inbuf, inlen, &outbuf, &outlen);
    if (outbuf && outlen > 0) {
        const esp_mcp_transport_t *transport = &mcp_ctx->config.transport;
        esp_err_t send_ret = ESP_ERR_NOT_SUPPORTED;
        if (transport->request) {
            send_ret = transport->request(mcp_ctx->transport_handle, outbuf, outlen);
        }
        (void)esp_mcp_free_response(mcp_instance, outbuf);
        if (send_ret != ESP_OK) {
            return send_ret;
        }
    }

    return handle_ret;
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

    char req_id_str[24] = {0};
    snprintf(req_id_str, sizeof(req_id_str), "%u", (unsigned)req_id);
    esp_err_t ret = esp_mcp_mgr_pending_cb_add(mcp_ctx, ep_name, tool_name, req_id_str, NULL, cb, user_ctx,
                                               (uint32_t)req_id, 30000);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = transport->request(mcp_ctx->transport_handle, inbuf, inlen);
    if (ret != ESP_OK) {
        esp_mcp_mgr_pending_cb_t *pending = esp_mcp_mgr_pending_cb_take(mcp_ctx, req_id_str, NULL);
        esp_mcp_mgr_pending_cb_free(pending);
        return ret;
    }

    return ESP_OK;
}

static uint16_t esp_mcp_mgr_generate_request_id(void)
{
    uint16_t id = 0;
    do {
        id = (uint16_t)esp_random();
    } while (id == 0);
    return id;
}

static esp_err_t esp_mcp_mgr_parse_optional_params_json(const char *params_json, cJSON **out_params)
{
    ESP_RETURN_ON_FALSE(out_params, ESP_ERR_INVALID_ARG, TAG, "Invalid params output");
    *out_params = NULL;

    if (!(params_json && params_json[0])) {
        return ESP_OK;
    }

    cJSON *params = cJSON_Parse(params_json);
    if (!params || !cJSON_IsObject(params)) {
        cJSON_Delete(params);
        return ESP_ERR_INVALID_ARG;
    }

    *out_params = params;
    return ESP_OK;
}

static esp_err_t esp_mcp_mgr_build_request_json(const char *method,
                                                const char *params_json,
                                                uint16_t req_id,
                                                char **out_json)
{
    ESP_RETURN_ON_FALSE(method && out_json, ESP_ERR_INVALID_ARG, TAG, "Invalid args");

    cJSON *params = NULL;
    ESP_RETURN_ON_ERROR(esp_mcp_mgr_parse_optional_params_json(params_json, &params), TAG, "Invalid params JSON");

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        if (params) {
            cJSON_Delete(params);
        }
        return ESP_ERR_NO_MEM;
    }

    if (!cJSON_AddStringToObject(root, "jsonrpc", "2.0") ||
            !cJSON_AddStringToObject(root, "method", method) ||
            !cJSON_AddNumberToObject(root, "id", req_id)) {
        if (params) {
            cJSON_Delete(params);
        }
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    if (params) {
        if (!cJSON_AddItemToObject(root, "params", params)) {
            cJSON_Delete(params);
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    ESP_RETURN_ON_FALSE(json, ESP_ERR_NO_MEM, TAG, "Print failed");

    *out_json = json;
    return ESP_OK;
}

static esp_err_t esp_mcp_mgr_build_notification_json(const char *method,
                                                     const char *params_json,
                                                     char **out_json)
{
    ESP_RETURN_ON_FALSE(method && out_json, ESP_ERR_INVALID_ARG, TAG, "Invalid args");

    cJSON *params = NULL;
    ESP_RETURN_ON_ERROR(esp_mcp_mgr_parse_optional_params_json(params_json, &params), TAG, "Invalid params JSON");

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        if (params) {
            cJSON_Delete(params);
        }
        return ESP_ERR_NO_MEM;
    }

    if (!cJSON_AddStringToObject(root, "jsonrpc", "2.0") ||
            !cJSON_AddStringToObject(root, "method", method)) {
        if (params) {
            cJSON_Delete(params);
        }
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    if (params) {
        if (!cJSON_AddItemToObject(root, "params", params)) {
            cJSON_Delete(params);
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    ESP_RETURN_ON_FALSE(json, ESP_ERR_NO_MEM, TAG, "Print failed");

    *out_json = json;
    return ESP_OK;
}

static esp_err_t esp_mcp_mgr_notification_perform(esp_mcp_mgr_handle_t handle,
                                                  const uint8_t *inbuf,
                                                  uint16_t inlen)
{
    esp_mcp_mgr_t *mcp_ctx = (esp_mcp_mgr_t *)handle;
    ESP_RETURN_ON_FALSE(mcp_ctx && inbuf, ESP_ERR_INVALID_ARG, TAG, "Invalid args");

    const esp_mcp_transport_t *transport = &mcp_ctx->config.transport;
    ESP_RETURN_ON_FALSE(transport->request, ESP_ERR_NOT_SUPPORTED, TAG, "Transport does not support outbound request");

    return transport->request(mcp_ctx->transport_handle, inbuf, inlen);
}

static bool esp_mcp_mgr_client_method_skips_initialized(const char *method)
{
    if (!method) {
        return true;
    }

    return strcmp(method, "initialize") == 0 || strcmp(method, "notifications/initialized") == 0;
}

static bool esp_mcp_mgr_protocol_requires_initialized_notification(const char *protocol_version)
{
    return protocol_version && protocol_version[0] && strcmp(protocol_version, "2024-11-05") > 0;
}

static bool esp_mcp_mgr_parse_initialize_protocol_version(const char *result_json,
                                                          char *out_protocol_version,
                                                          size_t out_protocol_version_len)
{
    if (!(result_json && out_protocol_version && out_protocol_version_len > 0)) {
        return false;
    }

    cJSON *root = cJSON_Parse(result_json);
    if (!root) {
        return false;
    }

    bool parsed = false;
    cJSON *protocol_version = cJSON_GetObjectItem(root, "protocolVersion");
    if (protocol_version && cJSON_IsString(protocol_version) &&
            protocol_version->valuestring && protocol_version->valuestring[0]) {
        snprintf(out_protocol_version, out_protocol_version_len, "%s", protocol_version->valuestring);
        parsed = true;
    }
    cJSON_Delete(root);
    return parsed;
}

static esp_err_t esp_mcp_mgr_post_notification_json_internal(esp_mcp_mgr_handle_t handle,
                                                             const esp_mcp_mgr_json_notification_t *notification,
                                                             bool ensure_initialized_first)
{
    ESP_RETURN_ON_FALSE(handle && notification && notification->ep_name && notification->method,
                        ESP_ERR_INVALID_ARG, TAG, "Invalid args");

    esp_mcp_mgr_t *mcp_ctx = (esp_mcp_mgr_t *)handle;
    esp_mcp_t *mcp_instance = (esp_mcp_t *)mcp_ctx->config.instance;
    ESP_RETURN_ON_FALSE(mcp_instance, ESP_ERR_INVALID_STATE, TAG, "MCP instance not configured");

    if (ensure_initialized_first &&
            mcp_instance->client_initialized &&
            mcp_instance->client_requires_initialized_notification &&
            !mcp_instance->client_sent_initialized_notification &&
            !esp_mcp_mgr_client_method_skips_initialized(notification->method)) {
        esp_mcp_mgr_json_notification_t initialized = {
            .ep_name = notification->ep_name,
            .method = "notifications/initialized",
            .params_json = NULL,
        };
        ESP_RETURN_ON_ERROR(esp_mcp_mgr_post_notification_json_internal(handle, &initialized, false),
                            TAG,
                            "Auto notifications/initialized failed");
    }

    char *req_json = NULL;
    ESP_RETURN_ON_ERROR(esp_mcp_mgr_build_notification_json(notification->method,
                                                            notification->params_json,
                                                            &req_json),
                        TAG,
                        "Build JSON notification failed");

    size_t req_json_len = strlen(req_json);
    if (req_json_len > UINT16_MAX) {
        cJSON_free(req_json);
        return ESP_ERR_INVALID_SIZE;
    }

    esp_err_t ret = esp_mcp_mgr_notification_perform(handle, (const uint8_t *)req_json, (uint16_t)req_json_len);
    cJSON_free(req_json);
    if (ret == ESP_OK && strcmp(notification->method, "notifications/initialized") == 0) {
        mcp_instance->client_sent_initialized_notification = true;
    }
    return ret;
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
        .protocol_version = "2025-11-25",
        .name = "esp-mcp-client",
        .version = "0.1.0",
        .title = NULL,
        .description = NULL,
        .icons_json = NULL,
        .website_url = NULL,
        .capabilities_json = NULL,
        .cursor = NULL,
        .tools_list_limit = -1,
        .tool_name = NULL,
        .args_json = NULL,
    };

    switch (type) {
    case ESP_MCP_POST_INIT: {
        mcp_instance->client_initialized = false;
        mcp_instance->client_requires_initialized_notification = false;
        mcp_instance->client_sent_initialized_notification = false;
        info.protocol_version = req->u.init.protocol_version;
        info.name = req->u.init.name;
        info.version = req->u.init.version;
        info.title = req->u.init.title;
        info.description = req->u.init.description;
        info.icons_json = req->u.init.icons_json;
        info.website_url = req->u.init.website_url;
        info.capabilities_json = req->u.init.capabilities_json;
        ret = esp_mcp_info_init(mcp_instance, (const esp_mcp_info_t *)&info, &req_json, &req_id);
        ESP_RETURN_ON_ERROR(ret, TAG, "Build initialize failed");
        tool_name = "initialize";
        break;
    }
    case ESP_MCP_POST_TOOLS_LIST: {
        if (mcp_instance->client_initialized &&
                mcp_instance->client_requires_initialized_notification &&
                !mcp_instance->client_sent_initialized_notification) {
            ESP_RETURN_ON_ERROR(esp_mcp_mgr_post_initialized(handle, req->ep_name),
                                TAG,
                                "Auto notifications/initialized failed");
        }
        info.cursor = req->u.list.cursor;
        info.tools_list_limit = req->u.list.limit;
        ret = esp_mcp_tools_list(mcp_instance, (const esp_mcp_info_t *)&info, &req_json, &req_id);
        ESP_RETURN_ON_ERROR(ret, TAG, "Build tools/list failed");
        tool_name = "toolslist";
        break;
    }
    case ESP_MCP_POST_TOOLS_CALL: {
        if (mcp_instance->client_initialized &&
                mcp_instance->client_requires_initialized_notification &&
                !mcp_instance->client_sent_initialized_notification) {
            ESP_RETURN_ON_ERROR(esp_mcp_mgr_post_initialized(handle, req->ep_name),
                                TAG,
                                "Auto notifications/initialized failed");
        }
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

static esp_err_t esp_mcp_mgr_post_json_rpc(esp_mcp_mgr_handle_t handle,
                                           const char *ep_name,
                                           const char *method,
                                           const char *params_json,
                                           esp_mcp_mgr_resp_cb_t cb,
                                           void *user_ctx)
{
    ESP_RETURN_ON_FALSE(handle && ep_name && method, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    esp_mcp_mgr_json_req_t req = {
        .ep_name = ep_name,
        .method = method,
        .params_json = params_json,
        .cb = cb,
        .user_ctx = user_ctx,
    };
    return esp_mcp_mgr_post_request_json(handle, &req);
}

esp_err_t esp_mcp_mgr_post_ping(esp_mcp_mgr_handle_t handle,
                                const char *ep_name,
                                esp_mcp_mgr_resp_cb_t cb,
                                void *user_ctx)
{
    return esp_mcp_mgr_post_json_rpc(handle, ep_name, "ping", "{}", cb, user_ctx);
}

esp_err_t esp_mcp_mgr_post_resources_list(esp_mcp_mgr_handle_t handle,
                                          const char *ep_name,
                                          const char *cursor,
                                          int limit,
                                          esp_mcp_mgr_resp_cb_t cb,
                                          void *user_ctx)
{
    cJSON *params = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(params, ESP_ERR_NO_MEM, TAG, "No mem");
    if (cursor && cursor[0]) {
        cJSON_AddStringToObject(params, "cursor", cursor);
    }
    if (limit >= 0) {
        cJSON_AddNumberToObject(params, "limit", (double)limit);
    }
    char *printed = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    ESP_RETURN_ON_FALSE(printed, ESP_ERR_NO_MEM, TAG, "Print failed");
    esp_err_t ret = esp_mcp_mgr_post_json_rpc(handle, ep_name, "resources/list", printed, cb, user_ctx);
    cJSON_free(printed);
    return ret;
}

esp_err_t esp_mcp_mgr_post_resources_read(esp_mcp_mgr_handle_t handle,
                                          const char *ep_name,
                                          const char *uri,
                                          esp_mcp_mgr_resp_cb_t cb,
                                          void *user_ctx)
{
    ESP_RETURN_ON_FALSE(uri && uri[0], ESP_ERR_INVALID_ARG, TAG, "Invalid uri");
    cJSON *params = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(params, ESP_ERR_NO_MEM, TAG, "No mem");
    cJSON_AddStringToObject(params, "uri", uri);
    char *printed = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    ESP_RETURN_ON_FALSE(printed, ESP_ERR_NO_MEM, TAG, "Print failed");
    esp_err_t ret = esp_mcp_mgr_post_json_rpc(handle, ep_name, "resources/read", printed, cb, user_ctx);
    cJSON_free(printed);
    return ret;
}

esp_err_t esp_mcp_mgr_post_resources_subscribe(esp_mcp_mgr_handle_t handle,
                                               const char *ep_name,
                                               const char *uri,
                                               esp_mcp_mgr_resp_cb_t cb,
                                               void *user_ctx)
{
    ESP_RETURN_ON_FALSE(uri && uri[0], ESP_ERR_INVALID_ARG, TAG, "Invalid uri");
    cJSON *params = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(params, ESP_ERR_NO_MEM, TAG, "No mem");
    cJSON_AddStringToObject(params, "uri", uri);
    char *printed = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    ESP_RETURN_ON_FALSE(printed, ESP_ERR_NO_MEM, TAG, "Print failed");
    esp_err_t ret = esp_mcp_mgr_post_json_rpc(handle, ep_name, "resources/subscribe", printed, cb, user_ctx);
    cJSON_free(printed);
    return ret;
}

esp_err_t esp_mcp_mgr_post_resources_unsubscribe(esp_mcp_mgr_handle_t handle,
                                                 const char *ep_name,
                                                 const char *uri,
                                                 esp_mcp_mgr_resp_cb_t cb,
                                                 void *user_ctx)
{
    ESP_RETURN_ON_FALSE(uri && uri[0], ESP_ERR_INVALID_ARG, TAG, "Invalid uri");
    cJSON *params = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(params, ESP_ERR_NO_MEM, TAG, "No mem");
    cJSON_AddStringToObject(params, "uri", uri);
    char *printed = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    ESP_RETURN_ON_FALSE(printed, ESP_ERR_NO_MEM, TAG, "Print failed");
    esp_err_t ret = esp_mcp_mgr_post_json_rpc(handle, ep_name, "resources/unsubscribe", printed, cb, user_ctx);
    cJSON_free(printed);
    return ret;
}

esp_err_t esp_mcp_mgr_post_resources_templates_list(esp_mcp_mgr_handle_t handle,
                                                    const char *ep_name,
                                                    const char *cursor,
                                                    int limit,
                                                    esp_mcp_mgr_resp_cb_t cb,
                                                    void *user_ctx)
{
    cJSON *params = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(params, ESP_ERR_NO_MEM, TAG, "No mem");
    if (cursor && cursor[0]) {
        cJSON_AddStringToObject(params, "cursor", cursor);
    }
    if (limit >= 0) {
        cJSON_AddNumberToObject(params, "limit", (double)limit);
    }
    char *printed = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    ESP_RETURN_ON_FALSE(printed, ESP_ERR_NO_MEM, TAG, "Print failed");
    esp_err_t ret = esp_mcp_mgr_post_json_rpc(handle, ep_name, "resources/templates/list", printed, cb, user_ctx);
    cJSON_free(printed);
    return ret;
}

esp_err_t esp_mcp_mgr_post_prompts_list(esp_mcp_mgr_handle_t handle,
                                        const char *ep_name,
                                        const char *cursor,
                                        int limit,
                                        esp_mcp_mgr_resp_cb_t cb,
                                        void *user_ctx)
{
    if (!(cursor && cursor[0]) && limit < 0) {
        return esp_mcp_mgr_post_json_rpc(handle, ep_name, "prompts/list", NULL, cb, user_ctx);
    }
    cJSON *params = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(params, ESP_ERR_NO_MEM, TAG, "No mem");
    if (cursor && cursor[0]) {
        cJSON_AddStringToObject(params, "cursor", cursor);
    }
    if (limit >= 0) {
        cJSON_AddNumberToObject(params, "limit", (double)limit);
    }
    char *printed = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    ESP_RETURN_ON_FALSE(printed, ESP_ERR_NO_MEM, TAG, "Print failed");
    esp_err_t ret = esp_mcp_mgr_post_json_rpc(handle, ep_name, "prompts/list", printed, cb, user_ctx);
    cJSON_free(printed);
    return ret;
}

esp_err_t esp_mcp_mgr_post_prompts_get(esp_mcp_mgr_handle_t handle,
                                       const char *ep_name,
                                       const char *name,
                                       const char *arguments_json,
                                       esp_mcp_mgr_resp_cb_t cb,
                                       void *user_ctx)
{
    ESP_RETURN_ON_FALSE(name && name[0], ESP_ERR_INVALID_ARG, TAG, "Invalid name");
    cJSON *params = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(params, ESP_ERR_NO_MEM, TAG, "No mem");
    cJSON_AddStringToObject(params, "name", name);
    if (arguments_json && arguments_json[0]) {
        cJSON *args = cJSON_Parse(arguments_json);
        if (!args) {
            cJSON_Delete(params);
            return ESP_ERR_INVALID_ARG;
        }
        if (!cJSON_IsObject(args)) {
            cJSON_Delete(args);
            cJSON_Delete(params);
            return ESP_ERR_INVALID_ARG;
        }
        cJSON_AddItemToObject(params, "arguments", args);
    } else {
        cJSON_AddObjectToObject(params, "arguments");
    }
    char *printed = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    ESP_RETURN_ON_FALSE(printed, ESP_ERR_NO_MEM, TAG, "Print failed");
    esp_err_t ret = esp_mcp_mgr_post_json_rpc(handle, ep_name, "prompts/get", printed, cb, user_ctx);
    cJSON_free(printed);
    return ret;
}

esp_err_t esp_mcp_mgr_post_completion_complete(esp_mcp_mgr_handle_t handle,
                                               const char *ep_name,
                                               const char *complete_params_json,
                                               esp_mcp_mgr_resp_cb_t cb,
                                               void *user_ctx)
{
    ESP_RETURN_ON_FALSE(complete_params_json && complete_params_json[0], ESP_ERR_INVALID_ARG, TAG, "Invalid params");
    cJSON *check = cJSON_Parse(complete_params_json);
    if (!check) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!cJSON_IsObject(check)) {
        cJSON_Delete(check);
        return ESP_ERR_INVALID_ARG;
    }
    cJSON_Delete(check);
    return esp_mcp_mgr_post_json_rpc(handle, ep_name, "completion/complete", complete_params_json, cb, user_ctx);
}

esp_err_t esp_mcp_mgr_post_logging_set_level(esp_mcp_mgr_handle_t handle,
                                             const char *ep_name,
                                             const char *level,
                                             esp_mcp_mgr_resp_cb_t cb,
                                             void *user_ctx)
{
    ESP_RETURN_ON_FALSE(level && level[0], ESP_ERR_INVALID_ARG, TAG, "Invalid level");
    cJSON *params = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(params, ESP_ERR_NO_MEM, TAG, "No mem");
    cJSON_AddStringToObject(params, "level", level);
    char *printed = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    ESP_RETURN_ON_FALSE(printed, ESP_ERR_NO_MEM, TAG, "Print failed");
    esp_err_t ret = esp_mcp_mgr_post_json_rpc(handle, ep_name, "logging/setLevel", printed, cb, user_ctx);
    cJSON_free(printed);
    return ret;
}

static esp_err_t esp_mcp_mgr_post_task_id_method(esp_mcp_mgr_handle_t handle,
                                                 const char *ep_name,
                                                 const char *method,
                                                 const char *task_id,
                                                 esp_mcp_mgr_resp_cb_t cb,
                                                 void *user_ctx)
{
    ESP_RETURN_ON_FALSE(task_id && task_id[0], ESP_ERR_INVALID_ARG, TAG, "Invalid taskId");
    cJSON *params = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(params, ESP_ERR_NO_MEM, TAG, "No mem");
    cJSON_AddStringToObject(params, "taskId", task_id);
    char *printed = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    ESP_RETURN_ON_FALSE(printed, ESP_ERR_NO_MEM, TAG, "Print failed");
    esp_err_t ret = esp_mcp_mgr_post_json_rpc(handle, ep_name, method, printed, cb, user_ctx);
    cJSON_free(printed);
    return ret;
}

esp_err_t esp_mcp_mgr_post_tasks_list(esp_mcp_mgr_handle_t handle,
                                      const char *ep_name,
                                      const char *cursor,
                                      esp_mcp_mgr_resp_cb_t cb,
                                      void *user_ctx)
{
    if (!(cursor && cursor[0])) {
        return esp_mcp_mgr_post_json_rpc(handle, ep_name, "tasks/list", NULL, cb, user_ctx);
    }
    cJSON *params = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(params, ESP_ERR_NO_MEM, TAG, "No mem");
    cJSON_AddStringToObject(params, "cursor", cursor);
    char *printed = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    ESP_RETURN_ON_FALSE(printed, ESP_ERR_NO_MEM, TAG, "Print failed");
    esp_err_t ret = esp_mcp_mgr_post_json_rpc(handle, ep_name, "tasks/list", printed, cb, user_ctx);
    cJSON_free(printed);
    return ret;
}

esp_err_t esp_mcp_mgr_post_tasks_get(esp_mcp_mgr_handle_t handle,
                                     const char *ep_name,
                                     const char *task_id,
                                     esp_mcp_mgr_resp_cb_t cb,
                                     void *user_ctx)
{
    return esp_mcp_mgr_post_task_id_method(handle, ep_name, "tasks/get", task_id, cb, user_ctx);
}

esp_err_t esp_mcp_mgr_post_tasks_result(esp_mcp_mgr_handle_t handle,
                                        const char *ep_name,
                                        const char *task_id,
                                        esp_mcp_mgr_resp_cb_t cb,
                                        void *user_ctx)
{
    return esp_mcp_mgr_post_task_id_method(handle, ep_name, "tasks/result", task_id, cb, user_ctx);
}

esp_err_t esp_mcp_mgr_post_tasks_cancel(esp_mcp_mgr_handle_t handle,
                                        const char *ep_name,
                                        const char *task_id,
                                        esp_mcp_mgr_resp_cb_t cb,
                                        void *user_ctx)
{
    return esp_mcp_mgr_post_task_id_method(handle, ep_name, "tasks/cancel", task_id, cb, user_ctx);
}

esp_err_t esp_mcp_mgr_post_request_json(esp_mcp_mgr_handle_t handle, const esp_mcp_mgr_json_req_t *req)
{
    ESP_RETURN_ON_FALSE(handle && req && req->ep_name && req->method, ESP_ERR_INVALID_ARG, TAG, "Invalid args");

    esp_mcp_mgr_t *mcp_ctx = (esp_mcp_mgr_t *)handle;
    esp_mcp_t *mcp_instance = (esp_mcp_t *)mcp_ctx->config.instance;
    ESP_RETURN_ON_FALSE(mcp_instance, ESP_ERR_INVALID_STATE, TAG, "MCP instance not configured");

    if (mcp_instance->client_initialized &&
            mcp_instance->client_requires_initialized_notification &&
            !mcp_instance->client_sent_initialized_notification &&
            !esp_mcp_mgr_client_method_skips_initialized(req->method)) {
        ESP_RETURN_ON_ERROR(esp_mcp_mgr_post_initialized(handle, req->ep_name),
                            TAG,
                            "Auto notifications/initialized failed");
    }

    char *req_json = NULL;
    uint16_t req_id = esp_mcp_mgr_generate_request_id();
    ESP_RETURN_ON_ERROR(esp_mcp_mgr_build_request_json(req->method, req->params_json, req_id, &req_json), TAG, "Build JSON request failed");

    size_t req_json_len = strlen(req_json);
    if (req_json_len > UINT16_MAX) {
        cJSON_free(req_json);
        return ESP_ERR_INVALID_SIZE;
    }

    esp_err_t ret = esp_mcp_mgr_req_perform(handle,
                                            req->ep_name,
                                            req->method,
                                            req_id,
                                            (const uint8_t *)req_json,
                                            (uint16_t)req_json_len,
                                            req->cb,
                                            req->user_ctx);
    cJSON_free(req_json);
    return ret;
}

esp_err_t esp_mcp_mgr_post_notification_json(esp_mcp_mgr_handle_t handle,
                                             const esp_mcp_mgr_json_notification_t *notification)
{
    return esp_mcp_mgr_post_notification_json_internal(handle, notification, true);
}

esp_err_t esp_mcp_mgr_post_initialized(esp_mcp_mgr_handle_t handle, const char *ep_name)
{
    esp_mcp_mgr_json_notification_t notification = {
        .ep_name = ep_name,
        .method = "notifications/initialized",
        .params_json = NULL,
    };
    return esp_mcp_mgr_post_notification_json_internal(handle, &notification, false);
}

esp_err_t esp_mcp_mgr_open_stream(esp_mcp_mgr_handle_t handle)
{
    esp_mcp_mgr_t *mcp_ctx = (esp_mcp_mgr_t *)handle;
    ESP_RETURN_ON_FALSE(mcp_ctx, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    const esp_mcp_transport_t *transport = &mcp_ctx->config.transport;
    ESP_RETURN_ON_FALSE(transport->open_stream, ESP_ERR_NOT_SUPPORTED, TAG, "Transport does not support stream opening");
    return transport->open_stream(mcp_ctx->transport_handle);
}

esp_err_t esp_mcp_mgr_set_bearer_token(esp_mcp_mgr_handle_t handle, const char *bearer_token)
{
    esp_mcp_mgr_t *mcp_ctx = (esp_mcp_mgr_t *)handle;
    ESP_RETURN_ON_FALSE(mcp_ctx, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP handle");
    ESP_RETURN_ON_FALSE(esp_mcp_mgr_uses_builtin_http_client_transport(mcp_ctx),
                        ESP_ERR_NOT_SUPPORTED, TAG, "HTTP client transport not active");
#if CONFIG_MCP_TRANSPORT_HTTP_CLIENT
    return esp_mcp_http_client_set_bearer_token(mcp_ctx->transport_handle, bearer_token);
#else
    (void)bearer_token;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t esp_mcp_mgr_set_auth_callback(esp_mcp_mgr_handle_t handle,
                                        esp_mcp_mgr_auth_cb_t cb,
                                        void *user_ctx)
{
    esp_mcp_mgr_t *mcp_ctx = (esp_mcp_mgr_t *)handle;
    ESP_RETURN_ON_FALSE(mcp_ctx, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP handle");
    ESP_RETURN_ON_FALSE(esp_mcp_mgr_uses_builtin_http_client_transport(mcp_ctx),
                        ESP_ERR_NOT_SUPPORTED, TAG, "HTTP client transport not active");
#if CONFIG_MCP_TRANSPORT_HTTP_CLIENT
    return esp_mcp_http_client_set_auth_callback(mcp_ctx->transport_handle, cb, user_ctx);
#else
    (void)cb;
    (void)user_ctx;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t esp_mcp_mgr_set_sse_event_callback(esp_mcp_mgr_handle_t handle,
                                             esp_mcp_mgr_sse_event_cb_t cb,
                                             void *user_ctx)
{
    esp_mcp_mgr_t *mcp_ctx = (esp_mcp_mgr_t *)handle;
    ESP_RETURN_ON_FALSE(mcp_ctx, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP handle");
    ESP_RETURN_ON_FALSE(esp_mcp_mgr_uses_builtin_http_client_transport(mcp_ctx),
                        ESP_ERR_NOT_SUPPORTED, TAG, "HTTP client transport not active");
#if CONFIG_MCP_TRANSPORT_HTTP_CLIENT
    return esp_mcp_http_client_set_sse_event_callback(mcp_ctx->transport_handle, cb, user_ctx);
#else
    (void)cb;
    (void)user_ctx;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t esp_mcp_mgr_req_destroy_response(esp_mcp_mgr_handle_t handle, uint8_t *response_buffer)
{
    esp_mcp_mgr_t *mcp_ctx = (esp_mcp_mgr_t *)handle;
    ESP_RETURN_ON_FALSE(mcp_ctx, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP handle");
    ESP_RETURN_ON_FALSE(mcp_ctx->config.instance, ESP_ERR_INVALID_ARG, TAG, "Invalid server instance");
    ESP_RETURN_ON_FALSE(response_buffer, ESP_ERR_INVALID_ARG, TAG, "Invalid response buffer");

    esp_mcp_t *engine = (esp_mcp_t *)mcp_ctx->config.instance;

    return esp_mcp_free_response(engine, response_buffer);
}

esp_err_t esp_mcp_mgr_req_destroy_response_by_id(esp_mcp_mgr_handle_t handle, uint16_t id)
{
    (void)handle;
    (void)id;
    // Deprecated: responses are now per-request buffers.
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_mcp_mgr_emit_message(esp_mcp_mgr_handle_t handle, const char *session_id, const char *jsonrpc_message)
{
    esp_mcp_mgr_t *mcp_ctx = (esp_mcp_mgr_t *)handle;
    ESP_RETURN_ON_FALSE(mcp_ctx, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(jsonrpc_message, ESP_ERR_INVALID_ARG, TAG, "Invalid message");
    const esp_mcp_transport_t *transport = &mcp_ctx->config.transport;
    ESP_RETURN_ON_FALSE(transport->emit_message, ESP_ERR_NOT_SUPPORTED, TAG, "Transport does not support message emit");
    return transport->emit_message(mcp_ctx->transport_handle, session_id, jsonrpc_message);
}

esp_err_t esp_mcp_mgr_set_request_context(esp_mcp_mgr_handle_t handle,
                                          const char *session_id,
                                          const char *auth_subject,
                                          bool is_authenticated)
{
    esp_mcp_mgr_t *mcp_ctx = (esp_mcp_mgr_t *)handle;
    ESP_RETURN_ON_FALSE(mcp_ctx, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    esp_mcp_t *engine = (esp_mcp_t *)mcp_ctx->config.instance;
    ESP_RETURN_ON_FALSE(engine, ESP_ERR_INVALID_STATE, TAG, "MCP instance not configured");
    return esp_mcp_set_request_context(engine, session_id, auth_subject, is_authenticated);
}

esp_err_t esp_mcp_mgr_clear_session_state(esp_mcp_mgr_handle_t handle, const char *session_id)
{
    ESP_RETURN_ON_FALSE(handle && session_id && session_id[0], ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    esp_mcp_mgr_t *mcp_ctx = (esp_mcp_mgr_t *)handle;
    esp_mcp_t *engine = (esp_mcp_t *)mcp_ctx->config.instance;
    ESP_RETURN_ON_FALSE(engine, ESP_ERR_INVALID_STATE, TAG, "No MCP instance");
    return esp_mcp_clear_session_state(engine, session_id);
}

esp_err_t esp_mcp_mgr_track_pending_request(esp_mcp_mgr_handle_t handle,
                                            const char *request_id,
                                            const char *session_id,
                                            const char *request_name,
                                            esp_mcp_mgr_resp_cb_t cb,
                                            void *user_ctx,
                                            uint32_t timeout_ms)
{
    esp_mcp_mgr_t *mcp_ctx = (esp_mcp_mgr_t *)handle;
    ESP_RETURN_ON_FALSE(mcp_ctx, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    return esp_mcp_mgr_pending_cb_add(mcp_ctx, request_name, NULL, request_id, session_id, cb, user_ctx,
                                      ESP_MCP_JSONRPC_ID_UNSPEC, timeout_ms);
}

esp_err_t esp_mcp_mgr_cancel_pending_request(esp_mcp_mgr_handle_t handle,
                                             const char *request_id,
                                             const char *session_id)
{
    esp_mcp_mgr_t *mcp_ctx = (esp_mcp_mgr_t *)handle;
    ESP_RETURN_ON_FALSE(mcp_ctx && request_id && request_id[0], ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    esp_mcp_mgr_pending_cb_t *pending = esp_mcp_mgr_pending_cb_take(mcp_ctx, request_id, session_id);
    if (!pending) {
        return ESP_ERR_NOT_FOUND;
    }
    if (pending->cb) {
        const char *name = pending->tool_name ? pending->tool_name : pending->ep_name;
        pending->cb(ESP_ERR_INVALID_STATE, name, "cancelled", pending->user_ctx, pending->jsonrpc_request_id);
    }
    esp_mcp_mgr_pending_cb_free(pending);
    return ESP_OK;
}

esp_err_t esp_mcp_mgr_sweep_pending_requests(esp_mcp_mgr_handle_t handle)
{
    esp_mcp_mgr_t *mcp_ctx = (esp_mcp_mgr_t *)handle;
    ESP_RETURN_ON_FALSE(mcp_ctx, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    if (!mcp_ctx->pending_mutex || xSemaphoreTake(mcp_ctx->pending_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_mcp_mgr_pending_cb_expire_locked(mcp_ctx);
    xSemaphoreGive(mcp_ctx->pending_mutex);
    return ESP_OK;
}
