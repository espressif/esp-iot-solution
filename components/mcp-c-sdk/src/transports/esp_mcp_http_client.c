/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <esp_log.h>
#include <esp_check.h>
#include <esp_http_client.h>
#include "http_parser.h"

#include "esp_mcp_mgr.h"

static const char *TAG = "esp_mcp_http_client";

typedef struct {
    esp_mcp_mgr_handle_t mgr_handle;
    esp_http_client_handle_t client;
    uint8_t *outbuf;
    uint32_t outlen;
    uint32_t content_len;
    char *base_url;
} esp_mcp_http_client_item_t;

static void esp_mcp_http_client_cleanup_buf(esp_mcp_http_client_item_t *item)
{
    if (item->outbuf) {
        free(item->outbuf);
        item->outbuf = NULL;
        item->outlen = 0;
        item->content_len = 0;
    }
}

static esp_err_t esp_mcp_http_client_event_handler(esp_http_client_event_t *event)
{
    esp_mcp_http_client_item_t *item = (esp_mcp_http_client_item_t *)event->user_data;
    ESP_RETURN_ON_FALSE(item, ESP_ERR_INVALID_ARG, TAG, "Invalid user data");

    switch (event->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
        esp_mcp_http_client_cleanup_buf(item);
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADERS_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADERS_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER: %s: %s", event->header_key, event->header_value);
        break;
    case HTTP_EVENT_ON_DATA: {
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA: %d", event->data_len);
        bool chunked = esp_http_client_is_chunked_response(event->client);

        if (!item->outbuf) {
            int cl = esp_http_client_get_content_length(event->client);
            item->content_len = (!chunked && cl > 0) ? (uint32_t)cl : 0;
            uint32_t alloc = item->content_len ? (item->content_len + 1) : (uint32_t)event->data_len + 1;
            item->outbuf = calloc(1, alloc);
            if (!item->outbuf) {
                ESP_LOGE(TAG, "Failed to allocate output buffer");
                return ESP_ERR_NO_MEM;
            }
        }

        size_t to_copy = event->data_len;
        if (chunked || item->content_len == 0) {
            uint32_t need = item->outlen + to_copy + 1;
            uint8_t *newbuf = realloc(item->outbuf, need);
            if (!newbuf) {
                ESP_LOGE(TAG, "Failed to grow output buffer");
                esp_mcp_http_client_cleanup_buf(item);
                return ESP_ERR_NO_MEM;
            }
            item->outbuf = newbuf;
        } else if (item->outlen + to_copy > item->content_len) {
            ESP_LOGW(TAG, "Response body exceeds Content-Length: content_len=%lu, outlen=%lu, data_len=%d (data may be truncated; parse errors in esp_mcp_mgr_perform_handle may be caused by this)",
                     (unsigned long)item->content_len, (unsigned long)item->outlen, event->data_len);
            int remain = (int)item->content_len - (int)item->outlen;
            if (remain <= 0) {
                ESP_LOGW(TAG, "Discarding chunk: already received full content_length");
                break;
            }
            if (to_copy > (uint32_t)remain) {
                ESP_LOGW(TAG, "Truncating chunk: copying %d bytes, discarding %u bytes", (int)remain, (unsigned)(to_copy - (uint32_t)remain));
                to_copy = (uint32_t)remain;
            }
        }

        if (to_copy > 0) {
            memcpy(item->outbuf + item->outlen, event->data, to_copy);
            item->outlen += to_copy;
        }
        break;
    }
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        if (item->outbuf) {
            item->outbuf[item->outlen] = '\0';
            esp_mcp_mgr_perform_handle(item->mgr_handle, item->outbuf, item->outlen);
            esp_mcp_http_client_cleanup_buf(item);
        }
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        esp_mcp_http_client_cleanup_buf(item);
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
        break;
    }
    return ESP_OK;
}

static char *esp_mcp_http_client_extract_base_url(const char *full_url)
{
    if (!full_url) {
        return NULL;
    }

    size_t url_len = strlen(full_url);
    struct http_parser_url u;
    http_parser_url_init(&u);

    if (http_parser_parse_url(full_url, url_len, 0, &u) != 0) {
        return strdup(full_url);
    }

    if (!(u.field_set & (1 << UF_SCHEMA)) || !(u.field_set & (1 << UF_HOST))) {
        return strdup(full_url);
    }

    const char *scheme = full_url + u.field_data[UF_SCHEMA].off;
    size_t scheme_len = u.field_data[UF_SCHEMA].len;
    const char *host = full_url + u.field_data[UF_HOST].off;
    size_t host_len = u.field_data[UF_HOST].len;

    /* scheme + "://" + host + optional ":port" + '\0'; port at most 5 digits */
    size_t base_len = scheme_len + 3 + host_len;
    if (u.field_set & (1 << UF_PORT)) {
        base_len += 1 + 5;  /* ":" + "65535" */
    }
    char *base_url = calloc(1, base_len + 1);
    if (!base_url) {
        return NULL;
    }

    char *p = base_url;
    memcpy(p, scheme, scheme_len);
    p += scheme_len;
    memcpy(p, "://", 3);
    p += 3;
    memcpy(p, host, host_len);
    p += host_len;
    if (u.field_set & (1 << UF_PORT)) {
        p += snprintf(p, 7, ":%u", (unsigned)u.port);
    }
    *p = '\0';

    return base_url;
}

static char *esp_mcp_http_client_build_url(const char *base_url, const char *ep_name)
{
    if (!base_url || !ep_name) {
        return NULL;
    }

    size_t base_len = strlen(base_url);
    size_t ep_len = strlen(ep_name);
    bool base_has_slash = (base_len > 0 && base_url[base_len - 1] == '/');
    bool ep_has_slash = (ep_len > 0 && ep_name[0] == '/');

    // Ensure exactly one slash between base_url and ep_name.
    size_t extra = (base_has_slash || ep_has_slash) ? 0 : 1;
    size_t trim = (base_has_slash && ep_has_slash) ? 1 : 0;

    size_t url_len = base_len + extra + ep_len - trim;
    char *url = calloc(1, url_len + 1);
    if (!url) {
        return NULL;
    }

    if (base_has_slash && ep_has_slash) {
        snprintf(url, url_len + 1, "%s%.*s", base_url, (int)(ep_len - 1), ep_name + 1);
    } else if (!base_has_slash && !ep_has_slash) {
        snprintf(url, url_len + 1, "%s/%s", base_url, ep_name);
    } else {
        snprintf(url, url_len + 1, "%s%s", base_url, ep_has_slash ? ep_name + 1 : ep_name);
    }
    return url;
}

static esp_err_t esp_mcp_http_client_init(esp_mcp_mgr_handle_t handle, esp_mcp_transport_handle_t *transport_handle)
{
    ESP_RETURN_ON_FALSE(transport_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid transport handle");

    esp_mcp_http_client_item_t *item = calloc(1, sizeof(esp_mcp_http_client_item_t));
    ESP_RETURN_ON_FALSE(item, ESP_ERR_NO_MEM, TAG, "Failed to allocate HTTP client item");

    item->mgr_handle = handle;
    *transport_handle = (esp_mcp_transport_handle_t)item;
    return ESP_OK;
}

static esp_err_t esp_mcp_http_client_deinit(esp_mcp_transport_handle_t handle)
{
    esp_mcp_http_client_item_t *item = (esp_mcp_http_client_item_t *)handle;
    ESP_RETURN_ON_FALSE(item, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    // Note: esp_mcp_http_client_stop should be called first, but we clean up here as a safety measure
    if (item->client) {
        esp_http_client_cleanup(item->client);
        item->client = NULL;
    }

    esp_mcp_http_client_cleanup_buf(item);
    if (item->base_url) {
        free(item->base_url);
        item->base_url = NULL;
    }
    item->mgr_handle = 0;
    free(item);
    return ESP_OK;
}

static void esp_mcp_http_client_config_cleanup(esp_http_client_config_t *cfg)
{
    if (!cfg) {
        return;
    }

    if (cfg->url) {
        free((void *)cfg->url);
    }
    if (cfg->cert_pem) {
        free((void *)cfg->cert_pem);
    }
    if (cfg->client_cert_pem) {
        free((void *)cfg->client_cert_pem);
    }
    if (cfg->client_key_pem) {
        free((void *)cfg->client_key_pem);
    }
    if (cfg->username) {
        free((void *)cfg->username);
    }
    if (cfg->password) {
        free((void *)cfg->password);
    }
    if (cfg->host) {
        free((void *)cfg->host);
    }
    if (cfg->path) {
        free((void *)cfg->path);
    }
    free(cfg);
}

static esp_err_t esp_mcp_http_client_config_dup_string(const char *src, const char **dst)
{
    if (!src) {
        *dst = NULL;
        return ESP_OK;
    }
    *dst = strdup(src);
    return (*dst) ? ESP_OK : ESP_ERR_NO_MEM;
}

static esp_err_t esp_mcp_http_client_create_config(const void *config, void **config_out)
{
    ESP_RETURN_ON_FALSE(config && config_out, ESP_ERR_INVALID_ARG, TAG, "Invalid config");

    const esp_http_client_config_t *in = (const esp_http_client_config_t *)config;
    ESP_RETURN_ON_FALSE(in->url, ESP_ERR_INVALID_ARG, TAG, "Invalid url");

    esp_http_client_config_t *out = calloc(1, sizeof(esp_http_client_config_t));
    ESP_RETURN_ON_FALSE(out, ESP_ERR_NO_MEM, TAG, "Failed to allocate config");

    out->timeout_ms = in->timeout_ms;
    out->skip_cert_common_name_check = in->skip_cert_common_name_check;
    out->auth_type = in->auth_type;
    out->transport_type = in->transport_type;
    out->port = in->port;

    esp_err_t ret = ESP_OK;
    if ((ret = esp_mcp_http_client_config_dup_string(in->url, &out->url)) != ESP_OK ||
            (ret = esp_mcp_http_client_config_dup_string(in->cert_pem, &out->cert_pem)) != ESP_OK ||
            (ret = esp_mcp_http_client_config_dup_string(in->client_cert_pem, &out->client_cert_pem)) != ESP_OK ||
            (ret = esp_mcp_http_client_config_dup_string(in->client_key_pem, &out->client_key_pem)) != ESP_OK ||
            (ret = esp_mcp_http_client_config_dup_string(in->username, &out->username)) != ESP_OK ||
            (ret = esp_mcp_http_client_config_dup_string(in->password, &out->password)) != ESP_OK ||
            (ret = esp_mcp_http_client_config_dup_string(in->host, &out->host)) != ESP_OK ||
            (ret = esp_mcp_http_client_config_dup_string(in->path, &out->path)) != ESP_OK) {
        esp_mcp_http_client_config_cleanup(out);
        return ret;
    }

    *config_out = out;
    return ESP_OK;
}

static esp_err_t esp_mcp_http_client_delete_config(void *config)
{
    esp_http_client_config_t *cfg = (esp_http_client_config_t *)config;
    ESP_RETURN_ON_FALSE(cfg, ESP_ERR_INVALID_ARG, TAG, "Invalid config");

    esp_mcp_http_client_config_cleanup(cfg);
    return ESP_OK;
}

/**
 * @brief Start HTTP client transport
 *
 * @note This function modifies the config object passed by the caller:
 *       - Sets cfg->method to HTTP_METHOD_POST
 *       - Sets cfg->event_handler to esp_mcp_http_client_event_handler
 *       - Sets cfg->user_data to the transport item
 *       The caller should not reuse or inspect the config after calling this function.
 */
static esp_err_t esp_mcp_http_client_start(esp_mcp_transport_handle_t handle, void *config)
{
    esp_mcp_http_client_item_t *item = (esp_mcp_http_client_item_t *)handle;
    ESP_RETURN_ON_FALSE(item && config, ESP_ERR_INVALID_ARG, TAG, "Invalid args");

    esp_http_client_config_t *cfg = (esp_http_client_config_t *)config;
    ESP_RETURN_ON_FALSE(cfg->url, ESP_ERR_INVALID_ARG, TAG, "URL not configured");

    item->base_url = esp_mcp_http_client_extract_base_url(cfg->url);
    ESP_RETURN_ON_FALSE(item->base_url, ESP_ERR_NO_MEM, TAG, "Failed to extract base URL");

    cfg->method = HTTP_METHOD_POST;
    cfg->event_handler = esp_mcp_http_client_event_handler;
    cfg->user_data = item;
    cfg->keep_alive_enable = true;

    item->client = esp_http_client_init(cfg);
    if (!item->client) {
        free(item->base_url);
        item->base_url = NULL;
        ESP_RETURN_ON_FALSE(false, ESP_ERR_NO_MEM, TAG, "Failed to initialize HTTP client");
    }

    esp_err_t ret = esp_http_client_set_header(item->client, "Content-Type", "application/json");
    if (ret != ESP_OK) {
        free(item->base_url);
        item->base_url = NULL;
        esp_http_client_cleanup(item->client);
        item->client = NULL;
        ESP_RETURN_ON_ERROR(ret, TAG, "Failed to set Content-Type header");
    }

    return ESP_OK;
}

static esp_err_t esp_mcp_http_client_stop(esp_mcp_transport_handle_t handle)
{
    esp_mcp_http_client_item_t *item = (esp_mcp_http_client_item_t *)handle;
    ESP_RETURN_ON_FALSE(item, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    if (item->client) {
        esp_http_client_cleanup(item->client);
        item->client = NULL;
    }

    esp_mcp_http_client_cleanup_buf(item);

    if (item->base_url) {
        free(item->base_url);
        item->base_url = NULL;
    }

    return ESP_OK;
}

static esp_err_t esp_mcp_http_client_register_endpoint(esp_mcp_transport_handle_t handle, const char *ep_name, void *priv_data)
{
    esp_mcp_http_client_item_t *item = (esp_mcp_http_client_item_t *)handle;
    ESP_RETURN_ON_FALSE(item && item->client, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(ep_name, ESP_ERR_INVALID_ARG, TAG, "Invalid endpoint name");
    ESP_RETURN_ON_FALSE(item->base_url, ESP_ERR_INVALID_STATE, TAG, "Base URL not initialized");

    char *new_url = esp_mcp_http_client_build_url(item->base_url, ep_name);
    ESP_RETURN_ON_FALSE(new_url, ESP_ERR_NO_MEM, TAG, "Failed to build URL");

    esp_err_t err = esp_http_client_set_url(item->client, new_url);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set URL: %s", esp_err_to_name(err));
    }
    free(new_url);

    return err;
}

static esp_err_t esp_mcp_http_client_unregister_endpoint(esp_mcp_transport_handle_t handle, const char *ep_name)
{
    (void)handle;
    (void)ep_name;
    return ESP_OK;
}

static esp_err_t esp_mcp_http_client_request(esp_mcp_transport_handle_t handle, const uint8_t *inbuf, uint16_t inlen)
{
    esp_mcp_http_client_item_t *item = (esp_mcp_http_client_item_t *)handle;
    ESP_RETURN_ON_FALSE(item && inbuf, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    ESP_RETURN_ON_FALSE(item->client, ESP_ERR_INVALID_STATE, TAG, "HTTP client not initialized");

    esp_http_client_set_post_field(item->client, (const char *)inbuf, inlen);
    esp_err_t err = esp_http_client_perform(item->client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "http perform failed: %s", esp_err_to_name(err));
        esp_http_client_close(item->client);
        return ESP_FAIL;
    }

    return ESP_OK;
}

const esp_mcp_transport_t esp_mcp_transport_http_client = {
    .init = esp_mcp_http_client_init,
    .deinit = esp_mcp_http_client_deinit,
    .start = esp_mcp_http_client_start,
    .stop = esp_mcp_http_client_stop,
    .create_config = esp_mcp_http_client_create_config,
    .delete_config = esp_mcp_http_client_delete_config,
    .register_endpoint = esp_mcp_http_client_register_endpoint,
    .unregister_endpoint = esp_mcp_http_client_unregister_endpoint,
    .request = esp_mcp_http_client_request,
};
