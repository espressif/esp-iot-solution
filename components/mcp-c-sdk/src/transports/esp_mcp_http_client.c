/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#include <esp_log.h>
#include <esp_check.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <cJSON.h>
#include "http_parser.h"

#include "esp_mcp_mgr.h"

static const char *TAG = "esp_mcp_http_client";

#define DEFAULT_PROTOCOL_VERSION "2025-11-25"

typedef struct {
    uint8_t *outbuf;
    uint32_t outlen;
    uint32_t content_len;
    bool is_sse;
    bool preserve_on_disconnect;
    char *sse_buf;
    size_t sse_buf_len;
    size_t sse_buf_capacity;
    char *sse_event_data;
    size_t sse_event_data_len;
    size_t sse_event_data_capacity;
    char sse_event_name[32];
    char sse_event_id[80];
} esp_mcp_http_client_io_state_t;

typedef struct esp_mcp_http_client_item_s esp_mcp_http_client_item_t;

typedef struct {
    esp_mcp_http_client_item_t *item;
    esp_mcp_http_client_io_state_t *io;
    const char *channel_name;
    bool suppress_dispatch;
} esp_mcp_http_client_event_ctx_t;

typedef struct {
    bool present;
    char error[64];
    char scope[128];
    char resource[256];
    char resource_metadata[320];
} esp_mcp_http_bearer_challenge_t;

typedef struct {
    bool present;
    char resource[256];
    char authorization_server[256];
    char scopes_supported[128];
    char metadata_url[320];
} esp_mcp_http_protected_resource_metadata_t;

typedef struct {
    bool present;
    bool pkce_s256_supported;
    char issuer[256];
    char authorization_endpoint[320];
    char token_endpoint[320];
    char registration_endpoint[320];
    char metadata_url[320];
} esp_mcp_http_auth_server_metadata_t;

struct esp_mcp_http_client_item_s {
    esp_mcp_mgr_handle_t mgr_handle;
    esp_http_client_handle_t request_client;
    esp_http_client_handle_t stream_client;
    esp_mcp_http_client_io_state_t request_io;
    esp_mcp_http_client_io_state_t stream_io;
    esp_mcp_http_client_event_ctx_t request_evt;
    esp_mcp_http_client_event_ctx_t stream_evt;
    esp_http_client_config_t *base_config;
    char *base_url;
    char *endpoint_url;
    char *session_id;
    char *bearer_token;
    esp_mcp_mgr_auth_cb_t auth_cb;
    void *auth_cb_user_ctx;
    esp_mcp_mgr_sse_event_cb_t sse_event_cb;
    void *sse_event_user_ctx;
    char protocol_version[32];
    char last_event_id[80];
};

static esp_err_t esp_mcp_http_client_event_handler(esp_http_client_event_t *event);
static void esp_mcp_http_client_dispatch_response_buffer(esp_mcp_http_client_item_t *item,
                                                         esp_mcp_http_client_io_state_t *io);
static void esp_mcp_http_client_config_cleanup(esp_http_client_config_t *cfg);

static void esp_mcp_http_client_update_protocol_version(esp_mcp_http_client_item_t *item, const char *payload)
{
    if (!item || !payload) {
        return;
    }

    // Fast path: most responses do not carry protocolVersion.
    if (!strstr(payload, "\"protocolVersion\"")) {
        return;
    }

    cJSON *root = cJSON_Parse(payload);
    if (!root) {
        return;
    }

    const cJSON *result = cJSON_GetObjectItem(root, "result");
    const cJSON *protocol_version = (result && cJSON_IsObject(result)) ? cJSON_GetObjectItem(result, "protocolVersion") : NULL;
    if (protocol_version && cJSON_IsString(protocol_version) && protocol_version->valuestring && protocol_version->valuestring[0] != '\0') {
        if (strncmp(item->protocol_version, protocol_version->valuestring, sizeof(item->protocol_version) - 1) != 0) {
            snprintf(item->protocol_version, sizeof(item->protocol_version), "%s", protocol_version->valuestring);
            if (item->request_client) {
                esp_err_t ret = esp_http_client_set_header(item->request_client, "MCP-Protocol-Version", item->protocol_version);
                if (ret != ESP_OK) {
                    ESP_LOGW(TAG, "Failed to update request MCP-Protocol-Version header: %s", esp_err_to_name(ret));
                }
            }
            if (item->stream_client) {
                esp_err_t ret = esp_http_client_set_header(item->stream_client, "MCP-Protocol-Version", item->protocol_version);
                if (ret != ESP_OK) {
                    ESP_LOGW(TAG, "Failed to update stream MCP-Protocol-Version header: %s", esp_err_to_name(ret));
                }
            }
            ESP_LOGI(TAG, "Negotiated protocol version updated to %s", item->protocol_version);
        }
    }

    cJSON_Delete(root);
}

static void esp_mcp_http_client_cleanup_io_state(esp_mcp_http_client_io_state_t *io)
{
    if (!io) {
        return;
    }
    if (io->outbuf) {
        free(io->outbuf);
        io->outbuf = NULL;
        io->outlen = 0;
        io->content_len = 0;
    }
    if (io->sse_buf) {
        free(io->sse_buf);
        io->sse_buf = NULL;
        io->sse_buf_len = 0;
        io->sse_buf_capacity = 0;
    }
    if (io->sse_event_data) {
        free(io->sse_event_data);
        io->sse_event_data = NULL;
        io->sse_event_data_len = 0;
        io->sse_event_data_capacity = 0;
    }
    io->sse_event_name[0] = '\0';
    io->sse_event_id[0] = '\0';
    io->is_sse = false;
    io->preserve_on_disconnect = false;
}

static void esp_mcp_http_client_store_last_event_id(esp_mcp_http_client_item_t *item,
                                                    const char *event_id)
{
    if (!(item && event_id && event_id[0])) {
        return;
    }

    snprintf(item->last_event_id, sizeof(item->last_event_id), "%s", event_id);
}

static char *esp_mcp_strcasestr(const char *haystack, const char *needle)
{
    if (!haystack || !needle) {
        return NULL;
    }
    size_t needle_len = strlen(needle);
    if (needle_len == 0) {
        return (char *)haystack;
    }
    for (; *haystack; haystack++) {
        if (strncasecmp(haystack, needle, needle_len) == 0) {
            return (char *)haystack;
        }
    }
    return NULL;
}

static bool esp_mcp_http_client_sse_append(char **dst_buf,
                                           size_t *dst_len,
                                           size_t *dst_capacity,
                                           const char *data,
                                           size_t data_len)
{
    if (!dst_buf || !dst_len || !dst_capacity || !data || data_len == 0) {
        return false;
    }

    size_t needed = *dst_len + data_len + 1;
    if (needed > *dst_capacity) {
        size_t new_capacity = (*dst_capacity == 0) ? 1024 : (*dst_capacity * 2);
        if (new_capacity < needed) {
            new_capacity = needed;
        }
        char *new_buf = realloc(*dst_buf, new_capacity);
        if (!new_buf) {
            ESP_LOGE(TAG, "Failed to allocate SSE buffer");
            return false;
        }
        *dst_buf = new_buf;
        *dst_capacity = new_capacity;
    }

    memcpy(*dst_buf + *dst_len, data, data_len);
    *dst_len += data_len;
    (*dst_buf)[*dst_len] = '\0';
    return true;
}

static esp_mcp_sse_event_category_t esp_mcp_http_client_classify_sse_event(const char *event_name,
                                                                           const char *payload)
{
    bool is_default_event = (!event_name || event_name[0] == '\0' || strcmp(event_name, "message") == 0);
    if (!is_default_event) {
        return ESP_MCP_SSE_EVENT_CATEGORY_NON_JSONRPC;
    }
    if (!payload || payload[0] == '\0') {
        return ESP_MCP_SSE_EVENT_CATEGORY_UNKNOWN;
    }

    cJSON *root = cJSON_Parse(payload);
    if (!root) {
        return ESP_MCP_SSE_EVENT_CATEGORY_NON_JSONRPC;
    }
    cJSON *method = cJSON_GetObjectItem(root, "method");
    cJSON *id = cJSON_GetObjectItem(root, "id");
    esp_mcp_sse_event_category_t category = ESP_MCP_SSE_EVENT_CATEGORY_UNKNOWN;
    if (method && cJSON_IsString(method) && method->valuestring) {
        const char *m = method->valuestring;
        if (strncmp(m, "notifications/message", strlen("notifications/message")) == 0) {
            category = ESP_MCP_SSE_EVENT_CATEGORY_NOTIFICATION_MESSAGE;
        } else if (strncmp(m, "notifications/progress", strlen("notifications/progress")) == 0) {
            category = ESP_MCP_SSE_EVENT_CATEGORY_NOTIFICATION_PROGRESS;
        } else if (strncmp(m, "notifications/resources/", strlen("notifications/resources/")) == 0) {
            category = ESP_MCP_SSE_EVENT_CATEGORY_NOTIFICATION_RESOURCES;
        } else if (strncmp(m, "notifications/tasks/", strlen("notifications/tasks/")) == 0) {
            category = ESP_MCP_SSE_EVENT_CATEGORY_NOTIFICATION_TASKS;
        } else if (strncmp(m, "notifications/", strlen("notifications/")) == 0) {
            category = ESP_MCP_SSE_EVENT_CATEGORY_NOTIFICATION_OTHER;
        }
    } else if (id && (cJSON_IsString(id) || cJSON_IsNumber(id))) {
        category = ESP_MCP_SSE_EVENT_CATEGORY_JSONRPC_RESPONSE;
    }
    cJSON_Delete(root);
    return category;
}

static void esp_mcp_http_client_dispatch_sse_event(esp_mcp_http_client_item_t *item,
                                                   esp_mcp_http_client_io_state_t *io,
                                                   const char *channel_name)
{
    if (!item || !io) {
        return;
    }

    if (!io->sse_event_data || io->sse_event_data_len == 0) {
        esp_mcp_http_client_store_last_event_id(item, io->sse_event_id);
        io->sse_event_name[0] = '\0';
        io->sse_event_id[0] = '\0';
        return;
    }

    size_t payload_len = io->sse_event_data_len;
    if (payload_len > 0 && io->sse_event_data[payload_len - 1] == '\n') {
        payload_len--;
        io->sse_event_data[payload_len] = '\0';
    }
    if (payload_len == 0) {
        esp_mcp_http_client_store_last_event_id(item, io->sse_event_id);
        io->sse_event_data_len = 0;
        io->sse_event_data[0] = '\0';
        io->sse_event_name[0] = '\0';
        io->sse_event_id[0] = '\0';
        return;
    }

    ESP_LOGD(TAG, "[%s] SSE event: name='%s' id='%s' payload='%.*s'",
             channel_name ? channel_name : "unknown",
             io->sse_event_name,
             io->sse_event_id,
             (int)payload_len,
             io->sse_event_data);
    esp_mcp_http_client_store_last_event_id(item, io->sse_event_id);

    if (item->sse_event_cb) {
        esp_mcp_sse_event_category_t category = esp_mcp_http_client_classify_sse_event(io->sse_event_name,
                                                                                       io->sse_event_data);
        (void)item->sse_event_cb(channel_name,
                                 io->sse_event_name,
                                 io->sse_event_id,
                                 category,
                                 io->sse_event_data,
                                 item->sse_event_user_ctx);
    }

    bool is_default_event = (io->sse_event_name[0] == '\0' || strcmp(io->sse_event_name, "message") == 0);
    if (is_default_event) {
        if (payload_len > UINT16_MAX) {
            ESP_LOGE(TAG, "SSE JSON-RPC payload exceeds %" PRIu16 " bytes", (uint16_t)UINT16_MAX);
        } else {
            esp_mcp_http_client_update_protocol_version(item, io->sse_event_data);
            (void)esp_mcp_mgr_set_request_context(item->mgr_handle, item->session_id, NULL, true);
            esp_mcp_mgr_perform_handle(item->mgr_handle, (const uint8_t *)io->sse_event_data, (uint16_t)payload_len);
            (void)esp_mcp_mgr_set_request_context(item->mgr_handle, NULL, NULL, false);
        }
    } else {
        ESP_LOGD(TAG, "[%s] Ignore non-JSON-RPC SSE event '%s'", channel_name ? channel_name : "unknown", io->sse_event_name);
    }

    io->sse_event_data_len = 0;
    io->sse_event_data[0] = '\0';
    io->sse_event_name[0] = '\0';
    io->sse_event_id[0] = '\0';
}

static void esp_mcp_http_client_process_sse_line(esp_mcp_http_client_item_t *item,
                                                 esp_mcp_http_client_io_state_t *io,
                                                 const char *channel_name,
                                                 const char *line,
                                                 size_t line_len)
{
    if (!item || !io || !line) {
        return;
    }

    if (line_len == 0) {
        esp_mcp_http_client_dispatch_sse_event(item, io, channel_name);
        return;
    }

    if (line[0] == ':') {
        return;
    }

    const char *value = "";
    size_t value_len = 0;
    size_t field_len = line_len;
    const char *colon = memchr(line, ':', line_len);
    if (colon) {
        field_len = (size_t)(colon - line);
        value = colon + 1;
        value_len = line_len - field_len - 1;
        if (value_len > 0 && value[0] == ' ') {
            value++;
            value_len--;
        }
    }

    if (field_len == 4 && strncmp(line, "data", 4) == 0) {
        if (!esp_mcp_http_client_sse_append(&io->sse_event_data,
                                            &io->sse_event_data_len,
                                            &io->sse_event_data_capacity,
                                            value,
                                            value_len)) {
            return;
        }
        (void)esp_mcp_http_client_sse_append(&io->sse_event_data,
                                             &io->sse_event_data_len,
                                             &io->sse_event_data_capacity,
                                             "\n",
                                             1);
        return;
    }

    if (field_len == 5 && strncmp(line, "event", 5) == 0) {
        size_t n = value_len;
        if (n >= sizeof(io->sse_event_name)) {
            n = sizeof(io->sse_event_name) - 1;
        }
        memcpy(io->sse_event_name, value, n);
        io->sse_event_name[n] = '\0';
        return;
    }

    if (field_len == 2 && strncmp(line, "id", 2) == 0) {
        size_t n = value_len;
        if (n >= sizeof(io->sse_event_id)) {
            n = sizeof(io->sse_event_id) - 1;
        }
        memcpy(io->sse_event_id, value, n);
        io->sse_event_id[n] = '\0';
        return;
    }
}

static void esp_mcp_http_client_process_sse_data(esp_mcp_http_client_item_t *item,
                                                 esp_mcp_http_client_io_state_t *io,
                                                 const char *channel_name,
                                                 const char *data,
                                                 size_t data_len)
{
    if (!item || !io || !data || data_len == 0) {
        return;
    }

    if (!esp_mcp_http_client_sse_append(&io->sse_buf,
                                        &io->sse_buf_len,
                                        &io->sse_buf_capacity,
                                        data,
                                        data_len)) {
        return;
    }

    size_t offset = 0;
    while (offset < io->sse_buf_len) {
        size_t i = offset;
        while (i < io->sse_buf_len && io->sse_buf[i] != '\n' && io->sse_buf[i] != '\r') {
            i++;
        }

        if (i >= io->sse_buf_len) {
            break;
        }

        size_t line_len = i - offset;
        size_t newline_len = 1;
        if (io->sse_buf[i] == '\r' && (i + 1) < io->sse_buf_len && io->sse_buf[i + 1] == '\n') {
            newline_len = 2;
        }

        esp_mcp_http_client_process_sse_line(item, io, channel_name, io->sse_buf + offset, line_len);
        offset = i + newline_len;
    }

    if (offset > 0) {
        size_t remaining = io->sse_buf_len - offset;
        if (remaining > 0) {
            memmove(io->sse_buf, io->sse_buf + offset, remaining);
        }
        io->sse_buf_len = remaining;
        io->sse_buf[io->sse_buf_len] = '\0';
    }
}

static esp_err_t esp_mcp_http_client_event_handler(esp_http_client_event_t *event)
{
    esp_mcp_http_client_event_ctx_t *ctx = (esp_mcp_http_client_event_ctx_t *)event->user_data;
    ESP_RETURN_ON_FALSE(ctx && ctx->item && ctx->io, ESP_ERR_INVALID_ARG, TAG, "Invalid user data");
    esp_mcp_http_client_item_t *item = ctx->item;
    esp_mcp_http_client_io_state_t *io = ctx->io;
    const char *channel_name = ctx->channel_name ? ctx->channel_name : "unknown";

    switch (event->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGE(TAG, "[%s] HTTP_EVENT_ERROR", channel_name);
        esp_mcp_http_client_cleanup_io_state(io);
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "[%s] HTTP_EVENT_ON_CONNECTED", channel_name);
        break;
    case HTTP_EVENT_HEADERS_SENT:
        ESP_LOGD(TAG, "[%s] HTTP_EVENT_HEADERS_SENT", channel_name);
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "[%s] HTTP_EVENT_ON_HEADER: %s: %s", channel_name, event->header_key, event->header_value);
        if (!ctx->suppress_dispatch && event->header_key && event->header_value &&
                strcasecmp(event->header_key, "MCP-Session-Id") == 0) {
            char *new_session_id = strdup(event->header_value);
            if (new_session_id) {
                free(item->session_id);
                item->session_id = new_session_id;
            } else {
                ESP_LOGE(TAG, "[%s] Failed to duplicate MCP-Session-Id", channel_name);
            }
        }
        if (event->header_key && event->header_value &&
                strcasecmp(event->header_key, "Content-Type") == 0) {
            if (esp_mcp_strcasestr(event->header_value, "text/event-stream") != NULL) {
                io->is_sse = true;
                ESP_LOGD(TAG, "[%s] SSE stream detected", channel_name);
            }
        }
        break;
    case HTTP_EVENT_ON_DATA: {
        ESP_LOGD(TAG, "[%s] HTTP_EVENT_ON_DATA: %d", channel_name, event->data_len);

        // Handle SSE stream
        if (io->is_sse) {
            esp_mcp_http_client_process_sse_data(item, io, channel_name, event->data, event->data_len);
            break;
        }

        bool chunked = esp_http_client_is_chunked_response(event->client);

        if (!io->outbuf) {
            int cl = esp_http_client_get_content_length(event->client);
            io->content_len = (!chunked && cl > 0) ? (uint32_t)cl : 0;
            uint32_t alloc = io->content_len ? (io->content_len + 1) : (uint32_t)event->data_len + 1;
            io->outbuf = calloc(1, alloc);
            if (!io->outbuf) {
                ESP_LOGE(TAG, "Failed to allocate output buffer");
                return ESP_ERR_NO_MEM;
            }
        }

        size_t to_copy = event->data_len;
        if (chunked || io->content_len == 0) {
            uint32_t need = io->outlen + to_copy + 1;
            uint8_t *newbuf = realloc(io->outbuf, need);
            if (!newbuf) {
                ESP_LOGE(TAG, "Failed to grow output buffer");
                esp_mcp_http_client_cleanup_io_state(io);
                return ESP_ERR_NO_MEM;
            }
            io->outbuf = newbuf;
        } else if (io->outlen + to_copy > io->content_len) {
            ESP_LOGW(TAG, "Response body exceeds Content-Length: content_len=%lu, outlen=%lu, data_len=%d (data may be truncated; parse errors in esp_mcp_mgr_perform_handle may be caused by this)",
                     (unsigned long)io->content_len, (unsigned long)io->outlen, event->data_len);
            int remain = (int)io->content_len - (int)io->outlen;
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
            memcpy(io->outbuf + io->outlen, event->data, to_copy);
            io->outlen += to_copy;
        }
        break;
    }
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "[%s] HTTP_EVENT_ON_FINISH", channel_name);
        if (io->outbuf) {
            io->outbuf[io->outlen] = '\0';
            if (ctx->suppress_dispatch) {
                io->preserve_on_disconnect = true;
            } else {
                /* Dispatch 401/403 bodies too so esp_mcp_mgr pending callbacks are resolved. */
                esp_mcp_http_client_dispatch_response_buffer(item, io);
            }
        }
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "[%s] HTTP_EVENT_DISCONNECTED", channel_name);
        if (!io->preserve_on_disconnect) {
            esp_mcp_http_client_cleanup_io_state(io);
        } else {
            io->preserve_on_disconnect = false;
        }
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGI(TAG, "[%s] HTTP_EVENT_REDIRECT", channel_name);
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
    } else {
        const char *sep = (!base_has_slash && !ep_has_slash) ? "/" : "";
        snprintf(url, url_len + 1, "%s%s%s", base_url, sep, ep_name);
    }
    return url;
}

static void esp_mcp_http_client_dispatch_response_buffer(esp_mcp_http_client_item_t *item,
                                                         esp_mcp_http_client_io_state_t *io)
{
    if (!(item && io && io->outbuf)) {
        return;
    }

    io->outbuf[io->outlen] = '\0';
    esp_mcp_http_client_update_protocol_version(item, (const char *)io->outbuf);
    (void)esp_mcp_mgr_set_request_context(item->mgr_handle, item->session_id, NULL, true);
    esp_mcp_mgr_perform_handle(item->mgr_handle, io->outbuf, io->outlen);
    (void)esp_mcp_mgr_set_request_context(item->mgr_handle, NULL, NULL, false);
    esp_mcp_http_client_cleanup_io_state(io);
}

static esp_err_t esp_mcp_http_client_update_bearer_token(esp_mcp_http_client_item_t *item, const char *token)
{
    ESP_RETURN_ON_FALSE(item, ESP_ERR_INVALID_ARG, TAG, "Invalid item");

    if (item->bearer_token) {
        free(item->bearer_token);
        item->bearer_token = NULL;
    }
    if (token) {
        item->bearer_token = strdup(token);
        ESP_RETURN_ON_FALSE(item->bearer_token, ESP_ERR_NO_MEM, TAG, "No mem");
    }
    return ESP_OK;
}

static esp_err_t esp_mcp_http_client_apply_bearer_header(esp_http_client_handle_t client, const char *token)
{
    ESP_RETURN_ON_FALSE(client, ESP_ERR_INVALID_ARG, TAG, "Invalid client");

    if (!(token && token[0])) {
        esp_err_t ret = esp_http_client_delete_header(client, "Authorization");
        // Header may not exist on first request; treat that as success.
        return (ret == ESP_ERR_NOT_FOUND) ? ESP_OK : ret;
    }

    size_t header_len = strlen(token) + sizeof("Bearer ");
    char *header_value = calloc(1, header_len);
    ESP_RETURN_ON_FALSE(header_value, ESP_ERR_NO_MEM, TAG, "No mem");
    snprintf(header_value, header_len, "Bearer %s", token);
    esp_err_t ret = esp_http_client_set_header(client, "Authorization", header_value);
    free(header_value);
    return ret;
}

static char *esp_mcp_http_client_dup_response_header(esp_http_client_handle_t client, const char *key)
{
    if (!(client && key)) {
        return NULL;
    }
    char *value = NULL;
    if (esp_http_client_get_header(client, key, &value) != ESP_OK || !value) {
        return NULL;
    }
    return strdup(value);
}

static char *esp_mcp_http_client_extract_url_path(const char *full_url)
{
    if (!full_url) {
        return NULL;
    }

    size_t url_len = strlen(full_url);
    struct http_parser_url u;
    http_parser_url_init(&u);
    if (http_parser_parse_url(full_url, url_len, 0, &u) != 0) {
        return strdup("/");
    }

    if (!(u.field_set & (1 << UF_PATH)) || u.field_data[UF_PATH].len == 0) {
        return strdup("/");
    }

    const char *path = full_url + u.field_data[UF_PATH].off;
    size_t path_len = u.field_data[UF_PATH].len;
    char *out = calloc(1, path_len + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, path, path_len);
    out[path_len] = '\0';
    return out;
}

static char *esp_mcp_http_client_build_well_known_url(const char *origin,
                                                      const char *well_known_path,
                                                      const char *resource_path)
{
    if (!(origin && well_known_path)) {
        return NULL;
    }

    if (!(resource_path && resource_path[0] && strcmp(resource_path, "/") != 0)) {
        return esp_mcp_http_client_build_url(origin, well_known_path);
    }

    char *base = esp_mcp_http_client_build_url(origin, well_known_path);
    if (!base) {
        return NULL;
    }
    char *url = esp_mcp_http_client_build_url(base, resource_path);
    free(base);
    return url;
}

static bool esp_mcp_http_client_extract_auth_param(const char *header,
                                                   const char *param_name,
                                                   char *out,
                                                   size_t out_len)
{
    if (!(header && param_name && out && out_len > 0)) {
        return false;
    }

    out[0] = '\0';
    char quoted_pattern[64] = {0};
    snprintf(quoted_pattern, sizeof(quoted_pattern), "%s=\"", param_name);
    char *start = esp_mcp_strcasestr(header, quoted_pattern);
    if (start) {
        start += strlen(quoted_pattern);
        char *end = strchr(start, '"');
        if (!end) {
            return false;
        }
        size_t n = (size_t)(end - start);
        if (n >= out_len) {
            n = out_len - 1;
        }
        memcpy(out, start, n);
        out[n] = '\0';
        return true;
    }

    char bare_pattern[64] = {0};
    snprintf(bare_pattern, sizeof(bare_pattern), "%s=", param_name);
    start = esp_mcp_strcasestr(header, bare_pattern);
    if (!start) {
        return false;
    }
    start += strlen(bare_pattern);
    while (*start == ' ' || *start == '\t') {
        start++;
    }
    size_t n = 0;
    while (start[n] && start[n] != ',' && start[n] != ' ' && start[n] != '\t') {
        n++;
    }
    if (n == 0) {
        return false;
    }
    if (n >= out_len) {
        n = out_len - 1;
    }
    memcpy(out, start, n);
    out[n] = '\0';
    return true;
}

static bool esp_mcp_http_client_parse_bearer_challenge(const char *header,
                                                       esp_mcp_http_bearer_challenge_t *challenge)
{
    if (!(header && challenge)) {
        return false;
    }

    memset(challenge, 0, sizeof(*challenge));
    while (*header == ' ' || *header == '\t') {
        header++;
    }
    if (strncasecmp(header, "Bearer", 6) != 0) {
        return false;
    }

    challenge->present = true;
    (void)esp_mcp_http_client_extract_auth_param(header, "error", challenge->error, sizeof(challenge->error));
    (void)esp_mcp_http_client_extract_auth_param(header, "scope", challenge->scope, sizeof(challenge->scope));
    (void)esp_mcp_http_client_extract_auth_param(header, "resource", challenge->resource, sizeof(challenge->resource));
    (void)esp_mcp_http_client_extract_auth_param(header,
                                                 "resource_metadata",
                                                 challenge->resource_metadata,
                                                 sizeof(challenge->resource_metadata));
    return true;
}

static void esp_mcp_http_client_join_string_array(const cJSON *array, char *out, size_t out_len)
{
    if (!(out && out_len > 0)) {
        return;
    }
    out[0] = '\0';
    if (!cJSON_IsArray(array)) {
        return;
    }

    size_t used = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, array) {
        if (!(cJSON_IsString(item) && item->valuestring && item->valuestring[0])) {
            continue;
        }
        int written = snprintf(out + used, out_len - used, "%s%s", used ? " " : "", item->valuestring);
        if (written < 0) {
            out[0] = '\0';
            return;
        }
        if ((size_t)written >= out_len - used) {
            out[out_len - 1] = '\0';
            return;
        }
        used += (size_t)written;
        if (used >= out_len) {
            out[out_len - 1] = '\0';
            return;
        }
    }
}

static bool esp_mcp_http_client_parse_protected_resource_metadata(const char *json,
                                                                  const char *metadata_url,
                                                                  esp_mcp_http_protected_resource_metadata_t *meta)
{
    if (!(json && meta)) {
        return false;
    }

    memset(meta, 0, sizeof(*meta));
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return false;
    }

    cJSON *resource = cJSON_GetObjectItem(root, "resource");
    cJSON *auth_servers = cJSON_GetObjectItem(root, "authorization_servers");
    cJSON *scopes = cJSON_GetObjectItem(root, "scopes_supported");
    if (cJSON_IsString(resource) && resource->valuestring) {
        snprintf(meta->resource, sizeof(meta->resource), "%s", resource->valuestring);
    }
    if (cJSON_IsArray(auth_servers)) {
        cJSON *first = cJSON_GetArrayItem(auth_servers, 0);
        if (cJSON_IsString(first) && first->valuestring) {
            snprintf(meta->authorization_server, sizeof(meta->authorization_server), "%s", first->valuestring);
        }
    }
    esp_mcp_http_client_join_string_array(scopes, meta->scopes_supported, sizeof(meta->scopes_supported));
    if (metadata_url) {
        snprintf(meta->metadata_url, sizeof(meta->metadata_url), "%s", metadata_url);
    }
    meta->present = (meta->resource[0] != '\0' || meta->authorization_server[0] != '\0');

    cJSON_Delete(root);
    return meta->present;
}

static bool esp_mcp_http_client_parse_auth_server_metadata(const char *json,
                                                           const char *metadata_url,
                                                           esp_mcp_http_auth_server_metadata_t *meta)
{
    if (!(json && meta)) {
        return false;
    }

    memset(meta, 0, sizeof(*meta));
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return false;
    }

    cJSON *issuer = cJSON_GetObjectItem(root, "issuer");
    cJSON *authorization_endpoint = cJSON_GetObjectItem(root, "authorization_endpoint");
    cJSON *token_endpoint = cJSON_GetObjectItem(root, "token_endpoint");
    cJSON *registration_endpoint = cJSON_GetObjectItem(root, "registration_endpoint");
    cJSON *pkce_methods = cJSON_GetObjectItem(root, "code_challenge_methods_supported");

    if (cJSON_IsString(issuer) && issuer->valuestring) {
        snprintf(meta->issuer, sizeof(meta->issuer), "%s", issuer->valuestring);
    }
    if (cJSON_IsString(authorization_endpoint) && authorization_endpoint->valuestring) {
        snprintf(meta->authorization_endpoint, sizeof(meta->authorization_endpoint), "%s", authorization_endpoint->valuestring);
    }
    if (cJSON_IsString(token_endpoint) && token_endpoint->valuestring) {
        snprintf(meta->token_endpoint, sizeof(meta->token_endpoint), "%s", token_endpoint->valuestring);
    }
    if (cJSON_IsString(registration_endpoint) && registration_endpoint->valuestring) {
        snprintf(meta->registration_endpoint, sizeof(meta->registration_endpoint), "%s", registration_endpoint->valuestring);
    }
    if (cJSON_IsArray(pkce_methods)) {
        cJSON *method = NULL;
        cJSON_ArrayForEach(method, pkce_methods) {
            if (cJSON_IsString(method) && method->valuestring &&
                    strcmp(method->valuestring, "S256") == 0) {
                meta->pkce_s256_supported = true;
                break;
            }
        }
    }
    if (metadata_url) {
        snprintf(meta->metadata_url, sizeof(meta->metadata_url), "%s", metadata_url);
    }
    meta->present = (meta->issuer[0] != '\0' || meta->authorization_endpoint[0] != '\0' || meta->token_endpoint[0] != '\0');

    cJSON_Delete(root);
    return meta->present;
}

static esp_err_t esp_mcp_http_client_fetch_json(esp_mcp_http_client_item_t *item,
                                                const char *url,
                                                char **out_payload,
                                                int *out_status)
{
    ESP_RETURN_ON_FALSE(item && item->base_config && url && out_payload, ESP_ERR_INVALID_ARG, TAG, "Invalid args");

    *out_payload = NULL;
    if (out_status) {
        *out_status = 0;
    }

    esp_mcp_http_client_io_state_t io = {0};
    esp_mcp_http_client_event_ctx_t evt = {
        .item = item,
        .io = &io,
        .channel_name = "auth-meta",
        .suppress_dispatch = true,
    };
    esp_http_client_config_t cfg = *item->base_config;
    cfg.url = url;
    cfg.method = HTTP_METHOD_GET;
    cfg.user_data = &evt;
    cfg.event_handler = esp_mcp_http_client_event_handler;
    cfg.keep_alive_enable = true;
    cfg.max_authorization_retries = -1;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    ESP_RETURN_ON_FALSE(client, ESP_ERR_NO_MEM, TAG, "Failed to init auth metadata client");

    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "MCP-Protocol-Version", item->protocol_version);
    esp_err_t ret = esp_http_client_perform(client);
    if (out_status) {
        *out_status = esp_http_client_get_status_code(client);
    }
    if (ret == ESP_OK && io.outbuf) {
        *out_payload = strdup((const char *)io.outbuf);
        if (!*out_payload) {
            ret = ESP_ERR_NO_MEM;
        }
    }
    esp_http_client_cleanup(client);
    esp_mcp_http_client_cleanup_io_state(&io);
    return ret;
}

static esp_err_t esp_mcp_http_client_discover_protected_resource_metadata(esp_mcp_http_client_item_t *item,
                                                                          const esp_mcp_http_bearer_challenge_t *challenge,
                                                                          esp_mcp_http_protected_resource_metadata_t *meta)
{
    ESP_RETURN_ON_FALSE(item && meta, ESP_ERR_INVALID_ARG, TAG, "Invalid args");

    memset(meta, 0, sizeof(*meta));
    char *explicit_url = NULL;
    char *path_url = NULL;
    char *root_url = NULL;
    char *endpoint_path = NULL;
    char *payload = NULL;
    int status = 0;
    esp_err_t ret = ESP_ERR_NOT_FOUND;

    if (challenge && challenge->resource_metadata[0]) {
        explicit_url = strdup(challenge->resource_metadata);
        ESP_GOTO_ON_FALSE(explicit_url, ESP_ERR_NO_MEM, cleanup, TAG, "No mem");
    } else if (item->endpoint_url && item->base_url) {
        endpoint_path = esp_mcp_http_client_extract_url_path(item->endpoint_url);
        ESP_GOTO_ON_FALSE(endpoint_path, ESP_ERR_NO_MEM, cleanup, TAG, "No mem");
        path_url = esp_mcp_http_client_build_well_known_url(item->base_url,
                                                            "/.well-known/oauth-protected-resource",
                                                            endpoint_path);
        root_url = esp_mcp_http_client_build_well_known_url(item->base_url,
                                                            "/.well-known/oauth-protected-resource",
                                                            "/");
        ESP_GOTO_ON_FALSE(path_url && root_url, ESP_ERR_NO_MEM, cleanup, TAG, "No mem");
    }

    const char *candidates[] = {
        explicit_url,
        path_url,
        root_url,
    };
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        if (!(candidates[i] && candidates[i][0])) {
            continue;
        }
        free(payload);
        payload = NULL;
        ret = esp_mcp_http_client_fetch_json(item, candidates[i], &payload, &status);
        if (ret != ESP_OK || status != HttpStatus_Ok || !payload) {
            continue;
        }
        if (esp_mcp_http_client_parse_protected_resource_metadata(payload, candidates[i], meta)) {
            ret = ESP_OK;
            goto cleanup;
        }
    }

    ret = ESP_ERR_NOT_FOUND;

cleanup:
    free(payload);
    free(explicit_url);
    free(path_url);
    free(root_url);
    free(endpoint_path);
    return ret;
}

static esp_err_t esp_mcp_http_client_discover_auth_server_metadata(esp_mcp_http_client_item_t *item,
                                                                   const char *issuer_url,
                                                                   esp_mcp_http_auth_server_metadata_t *meta)
{
    ESP_RETURN_ON_FALSE(item && issuer_url && meta, ESP_ERR_INVALID_ARG, TAG, "Invalid args");

    memset(meta, 0, sizeof(*meta));
    char *issuer_origin = esp_mcp_http_client_extract_base_url(issuer_url);
    char *issuer_path = esp_mcp_http_client_extract_url_path(issuer_url);
    char *oauth_inserted = NULL;
    char *oidc_inserted = NULL;
    char *oidc_appended = NULL;
    char *payload = NULL;
    int status = 0;
    esp_err_t ret = ESP_ERR_NOT_FOUND;

    ESP_GOTO_ON_FALSE(issuer_origin && issuer_path, ESP_ERR_NO_MEM, cleanup, TAG, "No mem");
    oauth_inserted = esp_mcp_http_client_build_well_known_url(issuer_origin,
                                                              "/.well-known/oauth-authorization-server",
                                                              issuer_path);
    oidc_inserted = esp_mcp_http_client_build_well_known_url(issuer_origin,
                                                             "/.well-known/openid-configuration",
                                                             issuer_path);
    oidc_appended = esp_mcp_http_client_build_url(issuer_url, ".well-known/openid-configuration");
    ESP_GOTO_ON_FALSE(oauth_inserted && oidc_inserted && oidc_appended, ESP_ERR_NO_MEM, cleanup, TAG, "No mem");

    const char *candidates[] = {
        oauth_inserted,
        oidc_inserted,
        oidc_appended,
    };
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        free(payload);
        payload = NULL;
        ret = esp_mcp_http_client_fetch_json(item, candidates[i], &payload, &status);
        if (ret != ESP_OK || status != HttpStatus_Ok || !payload) {
            continue;
        }
        if (esp_mcp_http_client_parse_auth_server_metadata(payload, candidates[i], meta)) {
            ret = ESP_OK;
            goto cleanup;
        }
    }

    ret = ESP_ERR_NOT_FOUND;

cleanup:
    free(payload);
    free(issuer_origin);
    free(issuer_path);
    free(oauth_inserted);
    free(oidc_inserted);
    free(oidc_appended);
    return ret;
}

static esp_err_t esp_mcp_http_client_try_authenticate(esp_mcp_http_client_item_t *item,
                                                      esp_http_client_handle_t client,
                                                      int http_status)
{
    ESP_RETURN_ON_FALSE(item && client, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    ESP_RETURN_ON_FALSE(item->auth_cb, ESP_ERR_NOT_SUPPORTED, TAG, "No auth callback");

    char *www_authenticate = esp_mcp_http_client_dup_response_header(client, "WWW-Authenticate");
    esp_mcp_http_bearer_challenge_t challenge = {0};
    if (www_authenticate) {
        (void)esp_mcp_http_client_parse_bearer_challenge(www_authenticate, &challenge);
    }

    esp_mcp_http_protected_resource_metadata_t resource_meta = {0};
    esp_mcp_http_auth_server_metadata_t auth_meta = {0};
    if (esp_mcp_http_client_discover_protected_resource_metadata(item, &challenge, &resource_meta) == ESP_OK &&
            resource_meta.authorization_server[0]) {
        (void)esp_mcp_http_client_discover_auth_server_metadata(item,
                                                                resource_meta.authorization_server,
                                                                &auth_meta);
    }

    esp_mcp_mgr_auth_context_t ctx = {
        .status_code = http_status,
        .request_url = item->endpoint_url,
        .resource = challenge.resource[0] ? challenge.resource :
        (resource_meta.resource[0] ? resource_meta.resource : NULL),
        .required_scope = challenge.scope[0] ? challenge.scope :
        (resource_meta.scopes_supported[0] ? resource_meta.scopes_supported : NULL),
        .resource_metadata_url = challenge.resource_metadata[0] ? challenge.resource_metadata :
        (resource_meta.metadata_url[0] ? resource_meta.metadata_url : NULL),
        .authorization_server = resource_meta.authorization_server[0] ? resource_meta.authorization_server : NULL,
        .authorization_metadata_url = auth_meta.metadata_url[0] ? auth_meta.metadata_url : NULL,
        .issuer = auth_meta.issuer[0] ? auth_meta.issuer : NULL,
        .authorization_endpoint = auth_meta.authorization_endpoint[0] ? auth_meta.authorization_endpoint : NULL,
        .token_endpoint = auth_meta.token_endpoint[0] ? auth_meta.token_endpoint : NULL,
        .registration_endpoint = auth_meta.registration_endpoint[0] ? auth_meta.registration_endpoint : NULL,
        .pkce_s256_supported = auth_meta.pkce_s256_supported,
    };

    char new_token[512] = {0};
    esp_err_t ret = item->auth_cb(&ctx, new_token, sizeof(new_token), item->auth_cb_user_ctx);
    if (ret == ESP_OK && new_token[0]) {
        ret = esp_mcp_http_client_update_bearer_token(item, new_token);
    } else if (ret == ESP_OK && !new_token[0]) {
        ret = ESP_ERR_NOT_FOUND;
    }

    free(www_authenticate);
    return ret;
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
    if (item->request_client) {
        esp_http_client_cleanup(item->request_client);
        item->request_client = NULL;
    }
    if (item->stream_client) {
        esp_http_client_cleanup(item->stream_client);
        item->stream_client = NULL;
    }

    esp_mcp_http_client_cleanup_io_state(&item->request_io);
    esp_mcp_http_client_cleanup_io_state(&item->stream_io);
    if (item->base_url) {
        free(item->base_url);
        item->base_url = NULL;
    }
    if (item->endpoint_url) {
        free(item->endpoint_url);
        item->endpoint_url = NULL;
    }
    if (item->session_id) {
        free(item->session_id);
        item->session_id = NULL;
    }
    if (item->bearer_token) {
        free(item->bearer_token);
        item->bearer_token = NULL;
    }
    if (item->base_config) {
        esp_mcp_http_client_config_cleanup(item->base_config);
        item->base_config = NULL;
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
    out->crt_bundle_attach = esp_crt_bundle_attach;

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
 * Duplicates the caller's `esp_http_client_config_t` so the original may be stack-allocated
 * or freed after this function returns.
 */
static esp_err_t esp_mcp_http_client_start(esp_mcp_transport_handle_t handle, void *config)
{
    esp_mcp_http_client_item_t *item = (esp_mcp_http_client_item_t *)handle;
    ESP_RETURN_ON_FALSE(item && config, ESP_ERR_INVALID_ARG, TAG, "Invalid args");

    void *owned_raw = NULL;
    ESP_RETURN_ON_ERROR(esp_mcp_http_client_create_config(config, &owned_raw), TAG, "Failed to duplicate HTTP client config");
    esp_http_client_config_t *cfg = (esp_http_client_config_t *)owned_raw;

    item->base_url = esp_mcp_http_client_extract_base_url(cfg->url);
    if (!item->base_url) {
        esp_mcp_http_client_config_cleanup(cfg);
        return ESP_ERR_NO_MEM;
    }

    item->request_evt.item = item;
    item->request_evt.io = &item->request_io;
    item->request_evt.channel_name = "request";
    item->request_evt.suppress_dispatch = false;
    item->stream_evt.item = item;
    item->stream_evt.io = &item->stream_io;
    item->stream_evt.channel_name = "stream";
    item->stream_evt.suppress_dispatch = false;

    cfg->method = HTTP_METHOD_POST;
    cfg->event_handler = esp_mcp_http_client_event_handler;
    cfg->user_data = &item->request_evt;
    cfg->keep_alive_enable = true;

    item->request_client = esp_http_client_init(cfg);
    if (!item->request_client) {
        free(item->base_url);
        item->base_url = NULL;
        esp_mcp_http_client_config_cleanup(cfg);
        ESP_RETURN_ON_FALSE(false, ESP_ERR_NO_MEM, TAG, "Failed to initialize request HTTP client");
    }

    esp_http_client_config_t stream_cfg = *cfg;
    stream_cfg.method = HTTP_METHOD_GET;
    stream_cfg.user_data = &item->stream_evt;
    item->stream_client = esp_http_client_init(&stream_cfg);
    if (!item->stream_client) {
        free(item->base_url);
        item->base_url = NULL;
        esp_http_client_cleanup(item->request_client);
        item->request_client = NULL;
        esp_mcp_http_client_config_cleanup(cfg);
        ESP_RETURN_ON_FALSE(false, ESP_ERR_NO_MEM, TAG, "Failed to initialize stream HTTP client");
    }

    esp_err_t ret = esp_http_client_set_header(item->request_client, "Content-Type", "application/json");
    if (ret != ESP_OK) {
        free(item->base_url);
        item->base_url = NULL;
        esp_http_client_cleanup(item->request_client);
        item->request_client = NULL;
        esp_http_client_cleanup(item->stream_client);
        item->stream_client = NULL;
        esp_mcp_http_client_config_cleanup(cfg);
        ESP_RETURN_ON_ERROR(ret, TAG, "Failed to set Content-Type header");
    }

    // Set MCP headers per MCP 2025-11-25 spec
    esp_err_t mcp_ret = esp_http_client_set_header(item->request_client, "Accept", "application/json, text/event-stream");
    if (mcp_ret == ESP_OK) {
        mcp_ret = esp_http_client_set_header(item->stream_client, "Accept", "text/event-stream");
    }
    if (mcp_ret == ESP_OK) {
        // Keep persistent connections whenever server/network allows it.
        mcp_ret = esp_http_client_set_header(item->request_client, "Connection", "keep-alive");
    }
    if (mcp_ret == ESP_OK) {
        mcp_ret = esp_http_client_set_header(item->stream_client, "Connection", "keep-alive");
    }
    strncpy(item->protocol_version, DEFAULT_PROTOCOL_VERSION, sizeof(item->protocol_version) - 1);
    item->protocol_version[sizeof(item->protocol_version) - 1] = '\0';
    if (mcp_ret == ESP_OK) {
        mcp_ret = esp_http_client_set_header(item->request_client, "MCP-Protocol-Version", item->protocol_version);
    }
    if (mcp_ret == ESP_OK) {
        mcp_ret = esp_http_client_set_header(item->stream_client, "MCP-Protocol-Version", item->protocol_version);
    }
    if (mcp_ret != ESP_OK) {
        free(item->base_url);
        item->base_url = NULL;
        esp_http_client_cleanup(item->request_client);
        item->request_client = NULL;
        esp_http_client_cleanup(item->stream_client);
        item->stream_client = NULL;
        esp_mcp_http_client_config_cleanup(cfg);
        ESP_RETURN_ON_ERROR(mcp_ret, TAG, "Failed to set MCP protocol headers");
    }

    item->base_config = cfg;
    return ESP_OK;
}

static esp_err_t esp_mcp_http_client_stop(esp_mcp_transport_handle_t handle)
{
    esp_mcp_http_client_item_t *item = (esp_mcp_http_client_item_t *)handle;
    ESP_RETURN_ON_FALSE(item, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    if (item->request_client) {
        esp_http_client_cleanup(item->request_client);
        item->request_client = NULL;
    }
    if (item->stream_client) {
        esp_http_client_cleanup(item->stream_client);
        item->stream_client = NULL;
    }

    esp_mcp_http_client_cleanup_io_state(&item->request_io);
    esp_mcp_http_client_cleanup_io_state(&item->stream_io);

    if (item->base_url) {
        free(item->base_url);
        item->base_url = NULL;
    }
    if (item->base_config) {
        esp_mcp_http_client_config_cleanup(item->base_config);
        item->base_config = NULL;
    }
    if (item->endpoint_url) {
        free(item->endpoint_url);
        item->endpoint_url = NULL;
    }
    if (item->session_id) {
        free(item->session_id);
        item->session_id = NULL;
    }
    if (item->bearer_token) {
        free(item->bearer_token);
        item->bearer_token = NULL;
    }
    item->last_event_id[0] = '\0';

    return ESP_OK;
}

static esp_err_t esp_mcp_http_client_register_endpoint(esp_mcp_transport_handle_t handle, const char *ep_name, void *priv_data)
{
    esp_mcp_http_client_item_t *item = (esp_mcp_http_client_item_t *)handle;
    ESP_RETURN_ON_FALSE(item && item->request_client && item->stream_client, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(ep_name, ESP_ERR_INVALID_ARG, TAG, "Invalid endpoint name");
    ESP_RETURN_ON_FALSE(item->base_url, ESP_ERR_INVALID_STATE, TAG, "Base URL not initialized");

    char *new_url = esp_mcp_http_client_build_url(item->base_url, ep_name);
    ESP_RETURN_ON_FALSE(new_url, ESP_ERR_NO_MEM, TAG, "Failed to build URL");

    esp_err_t err = esp_http_client_set_url(item->request_client, new_url);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set request URL: %s", esp_err_to_name(err));
    }
    if (err == ESP_OK) {
        err = esp_http_client_set_url(item->stream_client, new_url);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set stream URL: %s", esp_err_to_name(err));
        }
    }
    if (err == ESP_OK) {
        char *dup_url = strdup(new_url);
        if (!dup_url) {
            err = ESP_ERR_NO_MEM;
        } else {
            free(item->endpoint_url);
            item->endpoint_url = dup_url;
        }
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
    ESP_RETURN_ON_FALSE(item->request_client, ESP_ERR_INVALID_STATE, TAG, "HTTP request client not initialized");

    esp_mcp_http_client_cleanup_io_state(&item->request_io);

    if (item->session_id) {
        esp_http_client_set_header(item->request_client, "MCP-Session-Id", item->session_id);
    } else {
        esp_http_client_delete_header(item->request_client, "MCP-Session-Id");
    }
    ESP_RETURN_ON_ERROR(esp_mcp_http_client_apply_bearer_header(item->request_client, item->bearer_token), TAG, "Failed to set Authorization header");

    esp_http_client_set_method(item->request_client, HTTP_METHOD_POST);
    esp_http_client_set_header(item->request_client, "Accept", "application/json, text/event-stream");
    esp_http_client_set_post_field(item->request_client, (const char *)inbuf, inlen);
    esp_err_t err = esp_http_client_perform(item->request_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "http perform failed: %s", esp_err_to_name(err));
        if (err != ESP_ERR_NOT_SUPPORTED) {
            esp_http_client_close(item->request_client);
            return ESP_FAIL;
        }
    }

    int status = esp_http_client_get_status_code(item->request_client);
    if (status == HttpStatus_Unauthorized || status == HttpStatus_Forbidden) {
        esp_err_t auth_ret = esp_mcp_http_client_try_authenticate(item, item->request_client, status);
        if (auth_ret == ESP_OK) {
            esp_mcp_http_client_cleanup_io_state(&item->request_io);
            esp_err_t bearer_ret = esp_mcp_http_client_apply_bearer_header(item->request_client, item->bearer_token);
            if (bearer_ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to refresh Authorization header");
                esp_http_client_close(item->request_client);
                return bearer_ret;
            }
            // Ensure a clean retry path after auth challenge.
            esp_http_client_close(item->request_client);
            esp_http_client_set_method(item->request_client, HTTP_METHOD_POST);
            esp_http_client_set_header(item->request_client, "Accept", "application/json, text/event-stream");
            esp_http_client_set_post_field(item->request_client, (const char *)inbuf, inlen);
            err = esp_http_client_perform(item->request_client);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "http retry failed: %s", esp_err_to_name(err));
                esp_http_client_close(item->request_client);
                return ESP_FAIL;
            }
            status = esp_http_client_get_status_code(item->request_client);
        } else if (auth_ret == ESP_ERR_NOT_SUPPORTED) {
            // esp-http-client could not handle the auth scheme internally; we can only proceed if our auth callback succeeds.
            esp_http_client_close(item->request_client);
            return ESP_FAIL;
        } else {
            esp_http_client_close(item->request_client);
            return ESP_FAIL;
        }
    } else if (err == ESP_ERR_NOT_SUPPORTED) {
        // Without a 401/403 challenge to drive our auth flow, treat ESP_ERR_NOT_SUPPORTED as a hard failure.
        esp_http_client_close(item->request_client);
        return ESP_FAIL;
    }

    if (item->request_io.outbuf) {
        esp_mcp_http_client_dispatch_response_buffer(item, &item->request_io);
        return ESP_OK;
    }

    return (status >= 200 && status < 300) ? ESP_OK : ESP_FAIL;
}

static esp_err_t esp_mcp_http_client_open_stream(esp_mcp_transport_handle_t handle)
{
    esp_mcp_http_client_item_t *item = (esp_mcp_http_client_item_t *)handle;
    ESP_RETURN_ON_FALSE(item, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(item->stream_client, ESP_ERR_INVALID_STATE, TAG, "HTTP stream client not initialized");

    esp_mcp_http_client_cleanup_io_state(&item->stream_io);

    // This call blocks until the stream closes; callers should run it in a dedicated task.
    if (item->session_id) {
        esp_http_client_set_header(item->stream_client, "MCP-Session-Id", item->session_id);
    } else {
        esp_http_client_delete_header(item->stream_client, "MCP-Session-Id");
    }
    if (item->last_event_id[0]) {
        esp_http_client_set_header(item->stream_client, "Last-Event-ID", item->last_event_id);
    } else {
        esp_http_client_delete_header(item->stream_client, "Last-Event-ID");
    }
    ESP_RETURN_ON_ERROR(esp_mcp_http_client_apply_bearer_header(item->stream_client, item->bearer_token), TAG, "Failed to set Authorization header");
    esp_http_client_set_header(item->stream_client, "Accept", "text/event-stream");
    esp_http_client_set_method(item->stream_client, HTTP_METHOD_GET);
    esp_http_client_set_post_field(item->stream_client, NULL, 0);

    esp_err_t err = esp_http_client_perform(item->stream_client);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "GET stream perform failed: %s", esp_err_to_name(err));
        if (err != ESP_ERR_NOT_SUPPORTED) {
            esp_http_client_close(item->stream_client);
            return ESP_FAIL;
        }
    }

    int status = esp_http_client_get_status_code(item->stream_client);
    if (status == HttpStatus_Unauthorized || status == HttpStatus_Forbidden) {
        esp_err_t auth_ret = esp_mcp_http_client_try_authenticate(item, item->stream_client, status);
        if (auth_ret == ESP_OK) {
            esp_mcp_http_client_cleanup_io_state(&item->stream_io);
            esp_err_t bearer_ret = esp_mcp_http_client_apply_bearer_header(item->stream_client, item->bearer_token);
            if (bearer_ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to refresh Authorization header");
                esp_http_client_close(item->stream_client);
                return bearer_ret;
            }
            // Re-open stream after auth challenge instead of reusing stale state.
            esp_http_client_close(item->stream_client);
            esp_http_client_set_method(item->stream_client, HTTP_METHOD_GET);
            esp_http_client_set_header(item->stream_client, "Accept", "text/event-stream");
            esp_http_client_set_post_field(item->stream_client, NULL, 0);
            err = esp_http_client_perform(item->stream_client);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "GET stream retry failed: %s", esp_err_to_name(err));
                esp_http_client_close(item->stream_client);
                return ESP_FAIL;
            }
            status = esp_http_client_get_status_code(item->stream_client);
        } else if (auth_ret == ESP_ERR_NOT_SUPPORTED) {
            esp_http_client_close(item->stream_client);
            return ESP_FAIL;
        } else {
            esp_http_client_close(item->stream_client);
            return ESP_FAIL;
        }
    } else if (err == ESP_ERR_NOT_SUPPORTED) {
        esp_http_client_close(item->stream_client);
        return ESP_FAIL;
    }
    if (status < 200 || status >= 300) {
        ESP_LOGW(TAG, "GET stream returned HTTP %d", status);
        esp_mcp_http_client_cleanup_io_state(&item->stream_io);
        esp_http_client_close(item->stream_client);
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t esp_mcp_http_client_terminate(esp_mcp_transport_handle_t handle)
{
    esp_mcp_http_client_item_t *item = (esp_mcp_http_client_item_t *)handle;
    ESP_RETURN_ON_FALSE(item, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(item->request_client, ESP_ERR_INVALID_STATE, TAG, "HTTP request client not initialized");

    if (!item->session_id) {
        ESP_LOGD(TAG, "No session to terminate");
        return ESP_OK;
    }

    esp_mcp_http_client_cleanup_io_state(&item->request_io);

    // Set session header for DELETE request
    esp_http_client_set_header(item->request_client, "MCP-Session-Id", item->session_id);
    ESP_RETURN_ON_ERROR(esp_mcp_http_client_apply_bearer_header(item->request_client, item->bearer_token), TAG, "Failed to set Authorization header");

    // Change method to DELETE
    esp_http_client_set_method(item->request_client, HTTP_METHOD_DELETE);

    // Perform DELETE request
    esp_err_t err = esp_http_client_perform(item->request_client);
    // Reset method back to POST for future requests
    esp_http_client_set_method(item->request_client, HTTP_METHOD_POST);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "DELETE request failed: %s", esp_err_to_name(err));
        if (err != ESP_ERR_NOT_SUPPORTED) {
            esp_http_client_close(item->request_client);
            return ESP_FAIL;
        }
    }

    int status = esp_http_client_get_status_code(item->request_client);
    if (status == HttpStatus_Unauthorized || status == HttpStatus_Forbidden) {
        esp_err_t auth_ret = esp_mcp_http_client_try_authenticate(item, item->request_client, status);
        if (auth_ret == ESP_OK) {
            esp_mcp_http_client_cleanup_io_state(&item->request_io);
            esp_err_t bearer_ret = esp_mcp_http_client_apply_bearer_header(item->request_client, item->bearer_token);
            if (bearer_ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to refresh Authorization header");
                esp_http_client_close(item->request_client);
                esp_http_client_set_method(item->request_client, HTTP_METHOD_POST);
                return bearer_ret;
            }
            esp_http_client_close(item->request_client);
            esp_http_client_set_method(item->request_client, HTTP_METHOD_DELETE);
            esp_http_client_set_post_field(item->request_client, NULL, 0);
            err = esp_http_client_perform(item->request_client);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "DELETE retry failed: %s", esp_err_to_name(err));
                esp_http_client_close(item->request_client);
                esp_http_client_set_method(item->request_client, HTTP_METHOD_POST);
                return ESP_FAIL;
            }
            status = esp_http_client_get_status_code(item->request_client);
        } else if (auth_ret == ESP_ERR_NOT_SUPPORTED) {
            esp_http_client_close(item->request_client);
            esp_http_client_set_method(item->request_client, HTTP_METHOD_POST);
            return ESP_FAIL;
        } else {
            esp_http_client_close(item->request_client);
            esp_http_client_set_method(item->request_client, HTTP_METHOD_POST);
            return ESP_FAIL;
        }
    } else if (err == ESP_ERR_NOT_SUPPORTED) {
        esp_http_client_close(item->request_client);
        esp_http_client_set_method(item->request_client, HTTP_METHOD_POST);
        return ESP_FAIL;
    }
    if (status >= 200 && status < 300) {
        ESP_LOGI(TAG, "Session terminated successfully");
    } else {
        ESP_LOGW(TAG, "Session termination returned status %d", status);
        esp_mcp_http_client_cleanup_io_state(&item->request_io);
        esp_http_client_set_method(item->request_client, HTTP_METHOD_POST);
        return ESP_FAIL;
    }

    // Clear session ID
    free(item->session_id);
    item->session_id = NULL;
    item->last_event_id[0] = '\0';
    esp_mcp_http_client_cleanup_io_state(&item->request_io);
    esp_http_client_set_method(item->request_client, HTTP_METHOD_POST);

    return ESP_OK;
}

esp_err_t esp_mcp_http_client_set_bearer_token(esp_mcp_transport_handle_t handle, const char *token)
{
    esp_mcp_http_client_item_t *item = (esp_mcp_http_client_item_t *)handle;
    ESP_RETURN_ON_FALSE(item, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    return esp_mcp_http_client_update_bearer_token(item, token);
}

esp_err_t esp_mcp_http_client_set_auth_callback(esp_mcp_transport_handle_t handle,
                                                esp_mcp_mgr_auth_cb_t cb,
                                                void *user_ctx)
{
    esp_mcp_http_client_item_t *item = (esp_mcp_http_client_item_t *)handle;
    ESP_RETURN_ON_FALSE(item, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    item->auth_cb = cb;
    item->auth_cb_user_ctx = user_ctx;
    return ESP_OK;
}

esp_err_t esp_mcp_http_client_set_sse_event_callback(esp_mcp_transport_handle_t handle,
                                                     esp_mcp_mgr_sse_event_cb_t cb,
                                                     void *user_ctx)
{
    esp_mcp_http_client_item_t *item = (esp_mcp_http_client_item_t *)handle;
    ESP_RETURN_ON_FALSE(item, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    item->sse_event_cb = cb;
    item->sse_event_user_ctx = user_ctx;
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
    .open_stream = esp_mcp_http_client_open_stream,
    .terminate = esp_mcp_http_client_terminate,
};
