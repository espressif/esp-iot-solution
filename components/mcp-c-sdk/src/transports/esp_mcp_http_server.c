/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <esp_log.h>
#include <esp_check.h>
#include <esp_http_server.h>
#include <esp_http_client.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <esp_random.h>
#include <cJSON.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>
#include <mbedtls/ecp.h>
#include <mbedtls/ecdsa.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include "esp_mcp_item.h"
#include "esp_mcp_priv.h"
#include "esp_mcp_mgr.h"

static const char *TAG = "esp_mcp_http_server";

/** Stack buffer for reflected Origin (CORS); must exceed typical Origin line length. */
#define ESP_MCP_HTTP_CORS_ORIGIN_BUF 513

#define MAX_CONTENT_LEN (1024 * 1024)  // 1MB maximum content length
#define MAX_TIMEOUT_COUNT 10            // Maximum consecutive timeouts before giving up
#if MAX_CONTENT_LEN > INT_MAX
#error "MAX_CONTENT_LEN exceeds INT_MAX"
#endif
#define SESSION_ID_LEN 32
#define AUTH_SUBJECT_LEN 64
#define SSE_EVENT_ID_LEN 80
#define PROTOCOL_VERSION_LEN 32
#define SSE_TASK_STACK_SIZE CONFIG_MCP_HTTP_SSE_TASK_STACK_SIZE
#define SSE_TASK_PRIORITY 5

#define ESP_MCP_HTTPD_201      "201 Created"                /*!< HTTP Response 201 */
#define ESP_MCP_HTTPD_202      "202 Accepted"               /*!< HTTP Response 202 */
#define ESP_MCP_HTTPD_405      "405 Method Not Allowed"     /*!< HTTP Response 405 */
#define ESP_MCP_HTTPD_406      "406 Not Acceptable"         /*!< HTTP Response 406 */
#define ESP_MCP_HTTPD_503      "503 Service Unavailable"    /*!< HTTP Response 503 */

#define ESP_MCP_HTTP_OAUTH_PROTECTED_RESOURCE_WELL_KNOWN "/.well-known/oauth-protected-resource"
#define ESP_MCP_HTTP_OAUTH_AUTHORIZATION_SERVER_WELL_KNOWN "/.well-known/oauth-authorization-server"

typedef struct esp_mcp_http_item_s esp_mcp_http_server_item_t;

typedef struct esp_mcp_sse_msg_s {
    uint32_t stream_id;
    uint32_t event_seq;
    char event_id[SSE_EVENT_ID_LEN];
    char *jsonrpc_message;
    struct esp_mcp_sse_msg_s *next;
} esp_mcp_sse_msg_t;

typedef struct esp_mcp_sse_hist_msg_s {
    char session_id[SESSION_ID_LEN];
    uint32_t stream_id;
    uint32_t event_seq;
    char event_id[SSE_EVENT_ID_LEN];
    char *jsonrpc_message;
    struct esp_mcp_sse_hist_msg_s *next;
} esp_mcp_sse_hist_msg_t;

typedef struct esp_mcp_http_session_s {
    char session_id[SESSION_ID_LEN];
    char protocol_version[PROTOCOL_VERSION_LEN];
    int64_t last_seen_ms;
    uint32_t next_stream_id;
    uint32_t resume_stream_id;
    uint32_t resume_next_event_seq;
    struct esp_mcp_http_session_s *next;
} esp_mcp_http_session_t;

typedef struct esp_mcp_sse_client_s {
    char session_id[SESSION_ID_LEN];
    char protocol_version[32];
    uint32_t stream_id;
    uint32_t next_event_seq;
    bool resuming_stream;
    httpd_req_t *req;
    int sockfd;
    TaskHandle_t task_handle;
    esp_mcp_http_server_item_t *owner;
    esp_mcp_sse_msg_t *head;
    esp_mcp_sse_msg_t *tail;
    size_t depth;
    struct esp_mcp_sse_client_s *next;
} esp_mcp_sse_client_t;
/**
 * @brief MCP HTTP item structure
 *
 * Structure containing MCP handle and HTTP server handle.
 */
struct esp_mcp_http_item_s {
    esp_mcp_mgr_handle_t handle;     /*!< MCP handle */
    httpd_handle_t   httpd;      /*!< HTTP server handle */
    volatile bool sse_teardown;
    SemaphoreHandle_t sse_mutex;
    esp_mcp_sse_client_t *sse_clients;
    esp_mcp_http_session_t *sessions;
    esp_mcp_sse_hist_msg_t *sse_history_head;
    esp_mcp_sse_hist_msg_t *sse_history_tail;
    size_t sse_history_depth;
#if CONFIG_MCP_HTTP_CORS_ENABLE
    /** Staging for Access-Control-Allow-Origin (httpd stores header value pointer; httpd task only). */
    char cors_allow_origin_staging[ESP_MCP_HTTP_CORS_ORIGIN_BUF];
#endif
};

typedef enum {
    ESP_MCP_HTTP_ERROR_406,
    ESP_MCP_HTTP_ERROR_503,
} esp_mcp_http_error_code_t;

/**
 * Prefix of esp-idf httpd_req_aux (components/esp_http_server/src/esp_httpd_priv.h) through
 * resp_hdrs_count. Used to clear stacked response headers before httpd_req_async_handler_begin so
 * MCP-Protocol-Version is not copied with pointers into the handler's stack frame.
 */
typedef struct {
    void *sd;
    char *scratch;
    size_t scratch_size_limit;
    size_t scratch_cur_size;
    size_t max_req_hdr_len;
    size_t max_uri_len;
    size_t remaining_len;
    char *status;
    char *content_type;
    bool first_chunk_sent;
    unsigned req_hdrs_count;
    unsigned resp_hdrs_count;
} esp_mcp_httpd_req_aux_hdr_prefix_t;

static const char * const s_http_supported_protocol_versions[] = {
    "2024-11-05",
    "2025-03-26",
    "2025-06-18",
    "2025-11-25",
    NULL
};

static void esp_mcp_http_sse_hist_msg_free(esp_mcp_sse_hist_msg_t *msg);

static void esp_mcp_http_clear_pending_response_hdrs(httpd_req_t *req)
{
    if (!req || !req->aux) {
        return;
    }
    esp_mcp_httpd_req_aux_hdr_prefix_t *aux = (esp_mcp_httpd_req_aux_hdr_prefix_t *)req->aux;
    aux->resp_hdrs_count = 0;
}

static esp_err_t esp_mcp_http_send_error(httpd_req_t *req, esp_mcp_http_error_code_t error, const char *msg)
{
    switch (error) {
    case ESP_MCP_HTTP_ERROR_406:
        httpd_resp_set_status(req, ESP_MCP_HTTPD_406);
        break;
    case ESP_MCP_HTTP_ERROR_503:
        httpd_resp_set_status(req, ESP_MCP_HTTPD_503);
        break;
    default:
        httpd_resp_set_status(req, HTTPD_500);
        break;
    }

    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, msg ? msg : "Service Unavailable", HTTPD_RESP_USE_STRLEN);
}

static bool esp_mcp_http_is_supported_proto(const char *proto)
{
    if (!proto) {
        return false;
    }
    if (strlen(proto) != 10) {
        return false;
    }
    for (int i = 0; i < 10; ++i) {
        if (i == 4 || i == 7) {
            if (proto[i] != '-') {
                return false;
            }
        } else if (proto[i] < '0' || proto[i] > '9') {
            return false;
        }
    }
    for (int i = 0; s_http_supported_protocol_versions[i]; ++i) {
        if (strcmp(proto, s_http_supported_protocol_versions[i]) == 0) {
            return true;
        }
    }
    // Allow project-configured default explicitly, even if outside bundled list.
    return strcmp(proto, CONFIG_MCP_HTTP_DEFAULT_PROTOCOL_VERSION) == 0;
}

static bool esp_mcp_http_is_ascii_visible(const char *s)
{
    if (!s) {
        return false;
    }
    for (const char *p = s; *p; ++p) {
        if (*p < 0x21 || *p > 0x7E) {
            return false;
        }
    }
    return true;
}

#if CONFIG_MCP_HTTP_SSE_ENABLE
static bool esp_mcp_http_is_initialize_request(const char *payload)
{
    if (!payload) {
        return false;
    }
    cJSON *root = cJSON_Parse(payload);
    if (!root) {
        return false;
    }
    bool is_init = false;
    cJSON *method = cJSON_GetObjectItem(root, "method");
    if (method && cJSON_IsString(method) && method->valuestring &&
            strcmp(method->valuestring, "initialize") == 0) {
        is_init = true;
    }
    cJSON_Delete(root);
    return is_init;
}
#endif

static esp_err_t esp_mcp_http_server_emit_message(esp_mcp_transport_handle_t handle,
                                                  const char *session_id,
                                                  const char *jsonrpc_message);
static bool esp_mcp_http_extract_session_id(httpd_req_t *req, char out_session_id[SESSION_ID_LEN]);
static esp_mcp_http_session_t *esp_mcp_http_session_find_locked(esp_mcp_http_server_item_t *mcp_http,
                                                                const char *session_id);

static bool esp_mcp_http_extract_result_protocol_version(const char *payload,
                                                         char out_proto[PROTOCOL_VERSION_LEN])
{
    if (!(payload && out_proto)) {
        return false;
    }

    out_proto[0] = '\0';
    if (!strstr(payload, "\"protocolVersion\"")) {
        return false;
    }

    cJSON *root = cJSON_Parse(payload);
    if (!root) {
        return false;
    }

    bool found = false;
    cJSON *result = cJSON_GetObjectItem(root, "result");
    cJSON *protocol_version = (result && cJSON_IsObject(result)) ? cJSON_GetObjectItem(result, "protocolVersion") : NULL;
    if (protocol_version &&
            cJSON_IsString(protocol_version) &&
            protocol_version->valuestring &&
            esp_mcp_http_is_supported_proto(protocol_version->valuestring)) {
        strlcpy(out_proto, protocol_version->valuestring, PROTOCOL_VERSION_LEN);
        found = true;
    }

    cJSON_Delete(root);
    return found;
}

static void esp_mcp_http_session_update_protocol_version(esp_mcp_http_server_item_t *mcp_http,
                                                         const char *session_id,
                                                         const char *protocol_version)
{
    if (!(mcp_http && session_id && session_id[0] && protocol_version && protocol_version[0] &&
            esp_mcp_http_is_supported_proto(protocol_version))) {
        return;
    }

    if (!mcp_http->sse_mutex || xSemaphoreTake(mcp_http->sse_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }

    esp_mcp_http_session_t *session = esp_mcp_http_session_find_locked(mcp_http, session_id);
    if (session) {
        strlcpy(session->protocol_version, protocol_version, sizeof(session->protocol_version));
    }

    xSemaphoreGive(mcp_http->sse_mutex);
}

static esp_err_t esp_mcp_http_apply_protocol_version(httpd_req_t *req,
                                                     esp_mcp_http_server_item_t *mcp_http,
                                                     char *out_proto,
                                                     size_t out_proto_len)
{
    ESP_RETURN_ON_FALSE(req, ESP_ERR_INVALID_ARG, TAG, "Invalid request");

    ESP_RETURN_ON_FALSE(out_proto && out_proto_len > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid protocol output");

    char buf[32] = {0};
    const char *proto_to_use = CONFIG_MCP_HTTP_DEFAULT_PROTOCOL_VERSION;
    size_t proto_len = httpd_req_get_hdr_value_len(req, "MCP-Protocol-Version");
    if (proto_len > 0) {
        if (proto_len >= sizeof(buf)) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "MCP-Protocol-Version too long");
            return ESP_ERR_INVALID_ARG;
        }

        if (httpd_req_get_hdr_value_str(req, "MCP-Protocol-Version", buf, sizeof(buf)) != ESP_OK) {
            ESP_LOGW(TAG, "Failed to read MCP-Protocol-Version header, using default");
        } else {
            if (!esp_mcp_http_is_ascii_visible(buf) || !esp_mcp_http_is_supported_proto(buf)) {
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Unsupported MCP-Protocol-Version");
                return ESP_ERR_INVALID_ARG;
            }
            proto_to_use = buf;
        }
    } else if (mcp_http) {
        char session_id[SESSION_ID_LEN] = {0};
        if (esp_mcp_http_extract_session_id(req, session_id) &&
                mcp_http->sse_mutex &&
                xSemaphoreTake(mcp_http->sse_mutex, portMAX_DELAY) == pdTRUE) {
            esp_mcp_http_session_t *session = esp_mcp_http_session_find_locked(mcp_http, session_id);
            if (session && session->protocol_version[0]) {
                strncpy(buf, session->protocol_version, sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';
                proto_to_use = buf;
            }
            xSemaphoreGive(mcp_http->sse_mutex);
        }
    }

    strncpy(out_proto, proto_to_use, out_proto_len - 1);
    out_proto[out_proto_len - 1] = '\0';
    return ESP_OK;
}

static bool esp_mcp_http_parse_host_port(const char *authority,
                                         char *out_host,
                                         size_t out_host_sz,
                                         char *out_port,
                                         size_t out_port_sz)
{
    if (!authority || !out_host || out_host_sz == 0 || !out_port || out_port_sz == 0) {
        return false;
    }
    out_host[0] = '\0';
    out_port[0] = '\0';

    if (authority[0] == '[') {
        const char *rb = strchr(authority, ']');
        if (!rb) {
            return false;
        }
        size_t host_len = (size_t)(rb - authority + 1);
        if (host_len >= out_host_sz) {
            return false;
        }
        memcpy(out_host, authority, host_len);
        out_host[host_len] = '\0';
        if (rb[1] == ':' && rb[2] != '\0') {
            snprintf(out_port, out_port_sz, "%s", rb + 2);
        }
        return true;
    }

    const char *colon = strrchr(authority, ':');
    if (colon && strchr(colon + 1, ':') == NULL) {
        size_t host_len = (size_t)(colon - authority);
        if (host_len >= out_host_sz) {
            return false;
        }
        memcpy(out_host, authority, host_len);
        out_host[host_len] = '\0';
        snprintf(out_port, out_port_sz, "%s", colon + 1);
    } else {
        snprintf(out_host, out_host_sz, "%s", authority);
    }
    return out_host[0] != '\0';
}

/**
 * @brief Validate Origin against Host (Streamable HTTP DNS rebinding mitigation).
 *
 * @param out_origin If non-NULL and large enough, copy the Origin header value when present and valid.
 * @param out_len    Size of out_origin buffer.
 * @return ESP_OK if Origin absent or matches Host; ESP_ERR_INVALID_ARG if Origin present but invalid;
 *         ESP_ERR_NO_MEM on allocation failure.
 */
static esp_err_t esp_mcp_http_origin_check(httpd_req_t *req, char *out_origin, size_t out_len)
{
    ESP_RETURN_ON_FALSE(req, ESP_ERR_INVALID_ARG, TAG, "Invalid request");
    if (out_origin && out_len) {
        out_origin[0] = '\0';
    }

    size_t origin_len = httpd_req_get_hdr_value_len(req, "Origin");
    if (origin_len == 0) {
        return ESP_OK;
    }

    size_t host_len = httpd_req_get_hdr_value_len(req, "Host");
    if (host_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    char *origin = calloc(1, origin_len + 1);
    char *host = calloc(1, host_len + 1);
    if (!origin || !host) {
        free(origin);
        free(host);
        return ESP_ERR_NO_MEM;
    }
    esp_err_t origin_ret = httpd_req_get_hdr_value_str(req, "Origin", origin, origin_len + 1);
    esp_err_t host_ret = httpd_req_get_hdr_value_str(req, "Host", host, host_len + 1);
    if (origin_ret != ESP_OK || host_ret != ESP_OK) {
        free(origin);
        free(host);
        return ESP_ERR_INVALID_ARG;
    }

    const char *prefix_http = "http://";
    const char *prefix_https = "https://";
    const char *origin_host = NULL;
    if (strncmp(origin, prefix_http, strlen(prefix_http)) == 0) {
        origin_host = origin + strlen(prefix_http);
    } else if (strncmp(origin, prefix_https, strlen(prefix_https)) == 0) {
        origin_host = origin + strlen(prefix_https);
    }

    bool valid = false;
    if (origin_host) {
        size_t host_part_len = strcspn(origin_host, "/?#");
        if (host_part_len > 0) {
            char authority[160] = {0};
            if (host_part_len < sizeof(authority)) {
                memcpy(authority, origin_host, host_part_len);
                authority[host_part_len] = '\0';

                char o_host[128] = {0};
                char o_port[16] = {0};
                char h_host[128] = {0};
                char h_port[16] = {0};
                bool o_ok = esp_mcp_http_parse_host_port(authority, o_host, sizeof(o_host), o_port, sizeof(o_port));
                bool h_ok = esp_mcp_http_parse_host_port(host, h_host, sizeof(h_host), h_port, sizeof(h_port));
                if (o_ok && h_ok && strcmp(o_host, h_host) == 0) {
                    valid = (o_port[0] == '\0' || h_port[0] == '\0' || strcmp(o_port, h_port) == 0);
                }
            }
        }
    }

    if (valid && out_origin && out_len > origin_len) {
        memcpy(out_origin, origin, origin_len + 1);
    }

    free(origin);
    free(host);
    return valid ? ESP_OK : ESP_ERR_INVALID_ARG;
}

#if CONFIG_MCP_HTTP_CORS_ENABLE
/** Response headers cross-origin scripts may read (fetch/XHR) when CORS is active. */
#define ESP_MCP_HTTP_CORS_EXPOSE_HEADERS "MCP-Session-Id, MCP-Protocol-Version"

static void esp_mcp_http_cors_set_expose_headers(httpd_req_t *req)
{
    (void)httpd_resp_set_hdr(req, "Access-Control-Expose-Headers", ESP_MCP_HTTP_CORS_EXPOSE_HEADERS);
}

/**
 * Copy Origin into transport staging and set Allow-Origin header.
 * Used from esp_mcp_http_validate_headers (httpd worker task only; staging is per esp_mcp_http_server_item_t).
 * esp_http_server stores the value pointer without copying or freeing it.
 */
static esp_err_t esp_mcp_http_set_cors_allow_origin_hdr_staged(httpd_req_t *req,
                                                               esp_mcp_http_server_item_t *mcp_http,
                                                               const char *origin)
{
    ESP_RETURN_ON_FALSE(req && mcp_http && origin, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    int n = snprintf(mcp_http->cors_allow_origin_staging,
                     sizeof(mcp_http->cors_allow_origin_staging),
                     "%s",
                     origin);
    if (n < 0) {
        memset(mcp_http->cors_allow_origin_staging, 0, sizeof(mcp_http->cors_allow_origin_staging));
        return ESP_FAIL;
    }
    if (n >= (int)sizeof(mcp_http->cors_allow_origin_staging)) {
        memset(mcp_http->cors_allow_origin_staging, 0, sizeof(mcp_http->cors_allow_origin_staging));
        return ESP_ERR_INVALID_SIZE;
    }
    return httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", mcp_http->cors_allow_origin_staging);
}
#endif

static esp_err_t esp_mcp_http_validate_headers(httpd_req_t *req)
{
    ESP_RETURN_ON_FALSE(req, ESP_ERR_INVALID_ARG, TAG, "Invalid request");
#if CONFIG_MCP_HTTP_CORS_ENABLE
    char origin_buf[ESP_MCP_HTTP_CORS_ORIGIN_BUF] = {0};
    esp_err_t oc = esp_mcp_http_origin_check(req, origin_buf, sizeof(origin_buf));
#else
    esp_err_t oc = esp_mcp_http_origin_check(req, NULL, 0);
#endif
    if (oc == ESP_ERR_NO_MEM) {
        return oc;
    }
    if (oc != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Invalid Origin");
        return ESP_ERR_INVALID_ARG;
    }
#if CONFIG_MCP_HTTP_CORS_ENABLE
    if (origin_buf[0] != '\0') {
        esp_mcp_http_server_item_t *mcp_http = req->user_ctx ? (esp_mcp_http_server_item_t *)req->user_ctx : NULL;
        ESP_RETURN_ON_FALSE(mcp_http, ESP_ERR_INVALID_STATE, TAG, "Missing user_ctx for CORS");
        ESP_RETURN_ON_ERROR(esp_mcp_http_set_cors_allow_origin_hdr_staged(req, mcp_http, origin_buf),
                            TAG,
                            "CORS Allow-Origin hdr");
        esp_mcp_http_cors_set_expose_headers(req);
    }
#endif
    return ESP_OK;
}

#if CONFIG_MCP_HTTP_CORS_ENABLE
static esp_err_t esp_mcp_http_options_handler(httpd_req_t *req)
{
    ESP_RETURN_ON_FALSE(req, ESP_ERR_INVALID_ARG, TAG, "Invalid request");
    char origin_buf[ESP_MCP_HTTP_CORS_ORIGIN_BUF] = {0};
    esp_err_t oc = esp_mcp_http_origin_check(req, origin_buf, sizeof(origin_buf));
    if (oc == ESP_ERR_NO_MEM) {
        return oc;
    }
    if (oc != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Invalid Origin");
        return ESP_ERR_INVALID_ARG;
    }

    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "POST, GET, DELETE, HEAD, OPTIONS");
    httpd_resp_set_hdr(req,
                       "Access-Control-Allow-Headers",
                       "Accept, Authorization, Content-Type, Last-Event-ID, MCP-Protocol-Version, MCP-Session-Id");
    httpd_resp_set_hdr(req, "Access-Control-Max-Age", "86400");
    esp_mcp_http_cors_set_expose_headers(req);
    if (origin_buf[0] != '\0') {
        ESP_RETURN_ON_ERROR(httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", origin_buf),
                            TAG,
                            "CORS Allow-Origin hdr");
    }
    return httpd_resp_send(req, NULL, 0);
}
#endif

#if CONFIG_MCP_HTTP_AUTH_ENABLE
static esp_err_t esp_mcp_http_send_jsonrpc_error(httpd_req_t *req,
                                                 const char *http_status,
                                                 int code,
                                                 const char *message,
                                                 const char *data_k,
                                                 const char *data_v,
                                                 const char *www_authenticate)
{
    ESP_RETURN_ON_FALSE(req && http_status && message, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    httpd_resp_set_status(req, http_status);
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return ESP_ERR_NO_MEM;
    }
    if (!cJSON_AddStringToObject(root, "jsonrpc", "2.0")) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    cJSON *id_null = cJSON_CreateNull();
    if (!id_null || !cJSON_AddItemToObject(root, "id", id_null)) {
        if (id_null) {
            cJSON_Delete(id_null);
        }
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    cJSON *error = cJSON_CreateObject();
    if (!error || !cJSON_AddItemToObject(root, "error", error)) {
        if (error) {
            cJSON_Delete(error);
        }
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    if (!cJSON_AddNumberToObject(error, "code", code) ||
            !cJSON_AddStringToObject(error, "message", message)) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    if (data_k && data_v) {
        cJSON *data = cJSON_CreateObject();
        if (!data) {
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
        if (!cJSON_AddItemToObject(error, "data", data)) {
            cJSON_Delete(data);
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
        if (!cJSON_AddStringToObject(data, data_k, data_v)) {
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
    }
    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    ESP_RETURN_ON_FALSE(payload, ESP_ERR_NO_MEM, TAG, "No mem");
    if (www_authenticate && www_authenticate[0]) {
        esp_err_t hdr_ret = httpd_resp_set_hdr(req, "WWW-Authenticate", www_authenticate);
        if (hdr_ret != ESP_OK) {
            free(payload);
            return hdr_ret;
        }
    }
    esp_err_t ret = httpd_resp_send(req, payload, HTTPD_RESP_USE_STRLEN);
    free(payload);
    return ret;
}

static void esp_mcp_http_build_www_authenticate(httpd_req_t *req,
                                                const char *error_name,
                                                char *out_header,
                                                size_t out_header_size)
{
    if (!req || !out_header || out_header_size == 0) {
        return;
    }
    out_header[0] = '\0';
    size_t host_len = httpd_req_get_hdr_value_len(req, "Host");
    char host[128] = {0};
    const char *host_val = NULL;
    if (host_len > 0 && host_len < sizeof(host) &&
            httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) == ESP_OK) {
        host_val = host;
    }
    size_t used = 0;
    int w = snprintf(out_header, out_header_size, "Bearer realm=\"mcp\"");
    if (w < 0) {
        out_header[0] = '\0';
        return;
    }
    used = (size_t)w;

    if (error_name && error_name[0] && used < out_header_size) {
        w = snprintf(out_header + used, out_header_size - used, ", error=\"%s\"", error_name);
        if (w < 0) {
            out_header[0] = '\0';
            return;
        }
        if ((size_t)w >= (out_header_size - used)) {
            out_header[used] = '\0';
            return;
        }
        used += (size_t)w;
    }

#ifdef CONFIG_MCP_HTTP_AUTH_REQUIRED_SCOPE
    if (CONFIG_MCP_HTTP_AUTH_REQUIRED_SCOPE[0] != '\0' && used < out_header_size) {
        w = snprintf(out_header + used, out_header_size - used, ", scope=\"%s\"", CONFIG_MCP_HTTP_AUTH_REQUIRED_SCOPE);
        if (w < 0) {
            out_header[0] = '\0';
            return;
        }
        if ((size_t)w >= (out_header_size - used)) {
            out_header[used] = '\0';
            return;
        }
        used += (size_t)w;
    }
#endif

#ifdef CONFIG_MCP_HTTP_OAUTH_RESOURCE
    if (used < out_header_size) {
        w = snprintf(out_header + used, out_header_size - used, ", resource=\"%s\"", CONFIG_MCP_HTTP_OAUTH_RESOURCE);
        if (w < 0) {
            out_header[0] = '\0';
            return;
        }
        if ((size_t)w >= (out_header_size - used)) {
            out_header[used] = '\0';
            return;
        }
        used += (size_t)w;
    }
#endif

    if (host_val && used < out_header_size) {
        const char *uri = req->uri;
        bool endpoint_path_present = uri &&
                                     uri[0] == '/' &&
                                     uri[1] != '\0' &&
                                     strncmp(uri, "/.well-known/", strlen("/.well-known/")) != 0;
        if (endpoint_path_present) {
            w = snprintf(out_header + used,
                         out_header_size - used,
                         ", resource_metadata=\"http://%s%s%s\"",
                         host_val,
                         ESP_MCP_HTTP_OAUTH_PROTECTED_RESOURCE_WELL_KNOWN,
                         uri);
        } else {
            w = snprintf(out_header + used,
                         out_header_size - used,
                         ", resource_metadata=\"http://%s%s\"",
                         host_val,
                         ESP_MCP_HTTP_OAUTH_PROTECTED_RESOURCE_WELL_KNOWN);
        }
        if (w < 0 || (size_t)w >= (out_header_size - used)) {
            out_header[used] = '\0';
            return;
        }
    }
}
#endif

#if CONFIG_MCP_HTTP_AUTH_JWT_VERIFY_ENABLE
typedef enum {
    ESP_MCP_HTTP_JWT_VERIFY_OK = 0,
    ESP_MCP_HTTP_JWT_VERIFY_INVALID = 1,
    ESP_MCP_HTTP_JWT_VERIFY_SCOPE = 2,
} esp_mcp_http_jwt_verify_result_t;

typedef struct {
    char *json;
    int64_t fetched_at_us;
    SemaphoreHandle_t mutex;
} esp_mcp_http_jwks_cache_t;

static int esp_mcp_http_b64url_val(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 26;
    }
    if (c >= '0' && c <= '9') {
        return c - '0' + 52;
    }
    if (c == '-') {
        return 62;
    }
    if (c == '_') {
        return 63;
    }
    return -1;
}

static char *esp_mcp_http_strndup_simple(const char *src, size_t n)
{
    if (!src) {
        return NULL;
    }
    char *out = calloc(1, n + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, src, n);
    out[n] = '\0';
    return out;
}

#if CONFIG_MCP_HTTP_AUTH_JWT_VERIFY_SIGNATURE
static esp_mcp_http_jwks_cache_t s_jwks_cache = {0};

static bool esp_mcp_http_jwks_cache_lock(void)
{
    if (!s_jwks_cache.mutex) {
        static portMUX_TYPE s_init_lock = portMUX_INITIALIZER_UNLOCKED;
        portENTER_CRITICAL(&s_init_lock);
        if (!s_jwks_cache.mutex) {
            s_jwks_cache.mutex = xSemaphoreCreateMutex();
        }
        portEXIT_CRITICAL(&s_init_lock);
    }
    if (!s_jwks_cache.mutex) {
        return false;
    }
    return xSemaphoreTake(s_jwks_cache.mutex, pdMS_TO_TICKS(1000)) == pdTRUE;
}

static void esp_mcp_http_jwks_cache_unlock(void)
{
    if (s_jwks_cache.mutex) {
        xSemaphoreGive(s_jwks_cache.mutex);
    }
}

static esp_err_t esp_mcp_http_fetch_jwks_json(char **out_json)
{
    ESP_RETURN_ON_FALSE(out_json, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    *out_json = NULL;
    esp_http_client_config_t cfg = {
        .url = CONFIG_MCP_HTTP_OAUTH_JWKS_URI,
        .timeout_ms = CONFIG_MCP_HTTP_AUTH_JWKS_FETCH_TIMEOUT_MS,
        .method = HTTP_METHOD_GET,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        return ESP_FAIL;
    }
    esp_err_t ret = esp_http_client_open(client, 0);
    if (ret != ESP_OK) {
        esp_http_client_cleanup(client);
        return ret;
    }
    int status = esp_http_client_fetch_headers(client);
    if (status < 0 || esp_http_client_get_status_code(client) != 200) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    if (status > 0 && status > CONFIG_MCP_HTTP_AUTH_JWKS_MAX_SIZE) {
        ESP_LOGW(TAG, "JWKS response too large by header: %d", status);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_INVALID_SIZE;
    }
    size_t cap = 1024;
    size_t len = 0;
    char *buf = calloc(1, cap);
    if (!buf) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }
    char chunk[512];
    int n = 0;
    while ((n = esp_http_client_read(client, chunk, sizeof(chunk))) > 0) {
        if (len + (size_t)n > (size_t)CONFIG_MCP_HTTP_AUTH_JWKS_MAX_SIZE) {
            free(buf);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            ESP_LOGW(TAG, "JWKS response exceeds max size: %u", (unsigned)(len + (size_t)n));
            return ESP_ERR_INVALID_SIZE;
        }
        if (len + (size_t)n + 1 > cap) {
            size_t new_cap = cap * 2;
            while (new_cap < len + (size_t)n + 1) {
                new_cap *= 2;
            }
            if (new_cap > (size_t)CONFIG_MCP_HTTP_AUTH_JWKS_MAX_SIZE + 1) {
                new_cap = (size_t)CONFIG_MCP_HTTP_AUTH_JWKS_MAX_SIZE + 1;
            }
            char *new_buf = realloc(buf, new_cap);
            if (!new_buf) {
                free(buf);
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                return ESP_ERR_NO_MEM;
            }
            buf = new_buf;
            cap = new_cap;
        }
        memcpy(buf + len, chunk, (size_t)n);
        len += (size_t)n;
        buf[len] = '\0';
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if (n < 0 || len == 0) {
        free(buf);
        return ESP_FAIL;
    }
    *out_json = buf;
    return ESP_OK;
}

static esp_err_t esp_mcp_http_get_jwks_json(char **out_json)
{
    ESP_RETURN_ON_FALSE(out_json, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    *out_json = NULL;
    if (CONFIG_MCP_HTTP_AUTH_JWKS_JSON_OVERRIDE[0] != '\0') {
        *out_json = strdup(CONFIG_MCP_HTTP_AUTH_JWKS_JSON_OVERRIDE);
        return *out_json ? ESP_OK : ESP_ERR_NO_MEM;
    }
    if (!esp_mcp_http_jwks_cache_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    int64_t now_us = esp_timer_get_time();
    int64_t ttl_us = (int64_t)CONFIG_MCP_HTTP_AUTH_JWKS_CACHE_TTL_SEC * 1000000LL;
    if (s_jwks_cache.json && (now_us - s_jwks_cache.fetched_at_us) <= ttl_us) {
        *out_json = strdup(s_jwks_cache.json);
        esp_mcp_http_jwks_cache_unlock();
        return *out_json ? ESP_OK : ESP_ERR_NO_MEM;
    }
    char *fresh = NULL;
    esp_err_t ret = esp_mcp_http_fetch_jwks_json(&fresh);
    if (ret == ESP_OK && !fresh) {
        esp_mcp_http_jwks_cache_unlock();
        return ESP_ERR_INVALID_STATE;
    }
    if (ret == ESP_OK && fresh) {
        free(s_jwks_cache.json);
        s_jwks_cache.json = fresh;
        s_jwks_cache.fetched_at_us = now_us;
        *out_json = strdup(s_jwks_cache.json);
        ret = *out_json ? ESP_OK : ESP_ERR_NO_MEM;
    }
    esp_mcp_http_jwks_cache_unlock();
    return ret;
}
#endif

static uint8_t *esp_mcp_http_b64url_decode(const char *in, size_t *out_len)
{
    if (!in || !out_len) {
        return NULL;
    }
    size_t in_len = strlen(in);
    if (in_len > (SIZE_MAX / 3)) {
        return NULL;
    }
    size_t padding = 0;
    if (in_len > 0 && in[in_len - 1] == '=') {
        padding++;
    }
    if (in_len > 1 && in[in_len - 2] == '=') {
        padding++;
    }
    size_t out_cap = ((in_len + 3) / 4) * 3;
    if (padding <= out_cap) {
        out_cap -= padding;
    }
    out_cap += 4; // small safety headroom for malformed edge cases
    if (out_cap > SIZE_MAX - 1) {
        return NULL;
    }
    uint8_t *out = calloc(1, out_cap + 1);
    if (!out) {
        return NULL;
    }
    uint32_t acc = 0;
    int bits = 0;
    size_t o = 0;
    for (size_t i = 0; i < in_len; ++i) {
        char ch = in[i];
        if (ch == '=') {
            break;
        }
        int v = esp_mcp_http_b64url_val(ch);
        if (v < 0) {
            free(out);
            return NULL;
        }
        acc = (acc << 6) | (uint32_t)v;
        bits += 6;
        while (bits >= 8) {
            bits -= 8;
            if (o >= out_cap) {
                free(out);
                return NULL;
            }
            out[o++] = (uint8_t)((acc >> bits) & 0xFF);
        }
    }
    out[o] = '\0';
    *out_len = o;
    return out;
}

static bool esp_mcp_http_scope_contains(const char *scope_str, const char *required_scope)
{
    if (!scope_str || !required_scope || !required_scope[0]) {
        return false;
    }
    const char *p = scope_str;
    size_t req_len = strlen(required_scope);
    while (*p) {
        while (*p == ' ') {
            p++;
        }
        if (!*p) {
            break;
        }
        const char *end = strchr(p, ' ');
        size_t tok_len = end ? (size_t)(end - p) : strlen(p);
        if (tok_len == req_len && strncmp(p, required_scope, req_len) == 0) {
            return true;
        }
        if (!end) {
            break;
        }
        p = end + 1;
    }
    return false;
}

static int64_t esp_mcp_http_unix_time_now_sec(void)
{
    time_t now = time(NULL);
    if (now > 0) {
        return (int64_t)now;
    }

    struct timeval tv = {0};
    if (gettimeofday(&tv, NULL) == 0 && tv.tv_sec > 0) {
        return (int64_t)tv.tv_sec;
    }
    return 0;
}

static bool esp_mcp_http_aud_matches(const cJSON *aud, const char *expected_aud)
{
    if (!aud || !expected_aud || !expected_aud[0]) {
        return false;
    }
    if (cJSON_IsString(aud) && aud->valuestring) {
        return strcmp(aud->valuestring, expected_aud) == 0;
    }
    if (cJSON_IsArray(aud)) {
        const cJSON *it = NULL;
        cJSON_ArrayForEach(it, (cJSON *)aud) {
            if (cJSON_IsString(it) && it->valuestring && strcmp(it->valuestring, expected_aud) == 0) {
                return true;
            }
        }
    }
    return false;
}

#if CONFIG_MCP_HTTP_AUTH_JWT_VERIFY_SIGNATURE
static int esp_mcp_http_sha256(const uint8_t *in, size_t in_len, uint8_t out_hash[32])
{
    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!md) {
        return -1;
    }
    return mbedtls_md(md, in, in_len, out_hash);
}

static bool esp_mcp_http_jwk_alg_compatible(const cJSON *jwk, const char *alg)
{
    cJSON *jwk_alg = cJSON_GetObjectItem((cJSON *)jwk, "alg");
    if (!jwk_alg || !cJSON_IsString(jwk_alg) || !jwk_alg->valuestring) {
        return true;
    }
    return strcmp(jwk_alg->valuestring, alg) == 0;
}

static cJSON *esp_mcp_http_jwks_select_key(cJSON *jwks, const char *kid, const char *alg)
{
    if (!jwks || !cJSON_IsObject(jwks)) {
        return NULL;
    }
    cJSON *keys = cJSON_GetObjectItem(jwks, "keys");
    if (!keys || !cJSON_IsArray(keys)) {
        return NULL;
    }
    cJSON *it = NULL;
    cJSON_ArrayForEach(it, keys) {
        if (!cJSON_IsObject(it) || !esp_mcp_http_jwk_alg_compatible(it, alg)) {
            continue;
        }
        cJSON *it_kid = cJSON_GetObjectItem(it, "kid");
        if (kid && kid[0]) {
            if (it_kid && cJSON_IsString(it_kid) && it_kid->valuestring && strcmp(it_kid->valuestring, kid) == 0) {
                return it;
            }
            continue;
        }
        return it;
    }
    return NULL;
}

static int esp_mcp_http_verify_rs256_jwk(const cJSON *jwk,
                                         const uint8_t *hash,
                                         const uint8_t *sig,
                                         size_t sig_len)
{
    cJSON *n_item = cJSON_GetObjectItem((cJSON *)jwk, "n");
    cJSON *e_item = cJSON_GetObjectItem((cJSON *)jwk, "e");
    if (!(n_item && cJSON_IsString(n_item) && n_item->valuestring &&
            e_item && cJSON_IsString(e_item) && e_item->valuestring)) {
        return -1;
    }
    size_t n_len = 0;
    size_t e_len = 0;
    uint8_t *n_bin = esp_mcp_http_b64url_decode(n_item->valuestring, &n_len);
    uint8_t *e_bin = esp_mcp_http_b64url_decode(e_item->valuestring, &e_len);
    if (!n_bin || !e_bin) {
        free(n_bin);
        free(e_bin);
        return -1;
    }
    mbedtls_rsa_context rsa;
    mbedtls_rsa_init(&rsa);
    int rc = mbedtls_rsa_import_raw(&rsa, n_bin, n_len, NULL, 0, NULL, 0, NULL, 0, e_bin, e_len);
    if (rc == 0) {
        rc = mbedtls_rsa_complete(&rsa);
    }
    if (rc == 0) {
        mbedtls_rsa_set_padding(&rsa, MBEDTLS_RSA_PKCS_V15, MBEDTLS_MD_SHA256);
        size_t rsa_len = mbedtls_rsa_get_len(&rsa);
        if (sig_len != rsa_len) {
            rc = -1;
        } else {
            rc = mbedtls_rsa_pkcs1_verify(&rsa, MBEDTLS_MD_SHA256, 32, hash, sig);
        }
    }
    mbedtls_rsa_free(&rsa);
    free(n_bin);
    free(e_bin);
    return rc;
}

static int esp_mcp_http_verify_es256_jwk(const cJSON *jwk,
                                         const uint8_t *hash,
                                         const uint8_t *sig,
                                         size_t sig_len)
{
    if (sig_len != 64) {
        return -1;
    }
    cJSON *x_item = cJSON_GetObjectItem((cJSON *)jwk, "x");
    cJSON *y_item = cJSON_GetObjectItem((cJSON *)jwk, "y");
    if (!(x_item && cJSON_IsString(x_item) && x_item->valuestring &&
            y_item && cJSON_IsString(y_item) && y_item->valuestring)) {
        return -1;
    }
    size_t x_len = 0;
    size_t y_len = 0;
    uint8_t *x_bin = esp_mcp_http_b64url_decode(x_item->valuestring, &x_len);
    uint8_t *y_bin = esp_mcp_http_b64url_decode(y_item->valuestring, &y_len);
    if (!x_bin || !y_bin || x_len != 32 || y_len != 32) {
        free(x_bin);
        free(y_bin);
        return -1;
    }
    mbedtls_ecp_group grp;
    mbedtls_ecp_point q;
    mbedtls_mpi r;
    mbedtls_mpi s;
    uint8_t pub_uncompressed[65];
    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&q);
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);
    int rc = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
    if (rc == 0) {
        pub_uncompressed[0] = 0x04;
        memcpy(&pub_uncompressed[1], x_bin, 32);
        memcpy(&pub_uncompressed[33], y_bin, 32);
        rc = mbedtls_ecp_point_read_binary(&grp, &q, pub_uncompressed, sizeof(pub_uncompressed));
    }
    if (rc == 0) {
        rc = mbedtls_mpi_read_binary(&r, sig, 32);
    }
    if (rc == 0) {
        rc = mbedtls_mpi_read_binary(&s, sig + 32, 32);
    }
    if (rc == 0) {
        rc = mbedtls_ecdsa_verify(&grp, hash, 32, &q, &r, &s);
    }
    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&q);
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);
    free(x_bin);
    free(y_bin);
    return rc;
}

static int esp_mcp_http_verify_jwt_signature(const char *alg,
                                             const char *kid,
                                             const char *signing_input,
                                             size_t signing_len,
                                             const char *sig_b64url)
{
    if (!alg || !sig_b64url || !signing_input) {
        return -1;
    }
    if (strcasecmp(alg, "none") == 0) {
        ESP_LOGW(TAG, "JWT 'none' algorithm rejected");
        return -1;
    }
    if (strcmp(alg, "RS256") != 0 && strcmp(alg, "ES256") != 0) {
        ESP_LOGW(TAG, "Unsupported JWT algorithm rejected: %s", alg);
        return -1;
    }
    char *jwks_json = NULL;
    if (esp_mcp_http_get_jwks_json(&jwks_json) != ESP_OK || !jwks_json) {
        return -1;
    }
    cJSON *jwks = cJSON_Parse(jwks_json);
    free(jwks_json);
    if (!jwks) {
        return -1;
    }
    cJSON *jwk = esp_mcp_http_jwks_select_key(jwks, kid, alg);
    if (!jwk) {
        cJSON_Delete(jwks);
        return -1;
    }
    size_t sig_len = 0;
    uint8_t *sig = esp_mcp_http_b64url_decode(sig_b64url, &sig_len);
    if (!sig) {
        cJSON_Delete(jwks);
        return -1;
    }
    uint8_t hash[32] = {0};
    int rc = esp_mcp_http_sha256((const uint8_t *)signing_input, signing_len, hash);
    if (rc == 0) {
        if (strcmp(alg, "RS256") == 0) {
            rc = esp_mcp_http_verify_rs256_jwk(jwk, hash, sig, sig_len);
        } else {
            rc = esp_mcp_http_verify_es256_jwk(jwk, hash, sig, sig_len);
        }
    }
    free(sig);
    cJSON_Delete(jwks);
    return rc;
}
#endif

static esp_mcp_http_jwt_verify_result_t esp_mcp_http_verify_jwt_skeleton(const char *token,
                                                                         char out_subject[AUTH_SUBJECT_LEN],
                                                                         char out_reason[64])
{
    if (!token || !token[0]) {
        snprintf(out_reason, 64, "%s", "jwt_empty");
        return ESP_MCP_HTTP_JWT_VERIFY_INVALID;
    }
    const char *dot1 = strchr(token, '.');
    const char *dot2 = dot1 ? strchr(dot1 + 1, '.') : NULL;
    if (!dot1 || !dot2 || strchr(dot2 + 1, '.')) {
        snprintf(out_reason, 64, "%s", "jwt_format_invalid");
        return ESP_MCP_HTTP_JWT_VERIFY_INVALID;
    }
    char *head_b64 = esp_mcp_http_strndup_simple(token, (size_t)(dot1 - token));
    char *payload_b64 = esp_mcp_http_strndup_simple(dot1 + 1, (size_t)(dot2 - dot1 - 1));
    if (!head_b64 || !payload_b64) {
        free(head_b64);
        free(payload_b64);
        snprintf(out_reason, 64, "%s", "jwt_no_mem");
        return ESP_MCP_HTTP_JWT_VERIFY_INVALID;
    }
    size_t head_len = 0;
    size_t payload_len = 0;
    uint8_t *head_json = esp_mcp_http_b64url_decode(head_b64, &head_len);
    uint8_t *payload_json = esp_mcp_http_b64url_decode(payload_b64, &payload_len);
    free(head_b64);
    free(payload_b64);
    if (!head_json || !payload_json) {
        free(head_json);
        free(payload_json);
        snprintf(out_reason, 64, "%s", "jwt_base64_invalid");
        return ESP_MCP_HTTP_JWT_VERIFY_INVALID;
    }

    cJSON *head_obj = cJSON_Parse((const char *)head_json);
    cJSON *payload_obj = cJSON_Parse((const char *)payload_json);
    free(head_json);
    free(payload_json);
    if (!head_obj || !payload_obj || !cJSON_IsObject(head_obj) || !cJSON_IsObject(payload_obj)) {
        if (head_obj) {
            cJSON_Delete(head_obj);
        }
        if (payload_obj) {
            cJSON_Delete(payload_obj);
        }
        snprintf(out_reason, 64, "%s", "jwt_json_invalid");
        return ESP_MCP_HTTP_JWT_VERIFY_INVALID;
    }

    cJSON *alg = cJSON_GetObjectItem(head_obj, "alg");
    if (!(alg && cJSON_IsString(alg) && alg->valuestring && alg->valuestring[0])) {
        cJSON_Delete(head_obj);
        cJSON_Delete(payload_obj);
        snprintf(out_reason, 64, "%s", "jwt_alg_missing");
        return ESP_MCP_HTTP_JWT_VERIFY_INVALID;
    }
    if (strcmp(alg->valuestring, "RS256") != 0 && strcmp(alg->valuestring, "ES256") != 0) {
        cJSON_Delete(head_obj);
        cJSON_Delete(payload_obj);
        snprintf(out_reason, 64, "%s", "jwt_alg_not_allowed");
        return ESP_MCP_HTTP_JWT_VERIFY_INVALID;
    }
    cJSON *kid = cJSON_GetObjectItem(head_obj, "kid");
#if defined(CONFIG_MCP_HTTP_AUTH_JWT_REQUIRE_KID) && CONFIG_MCP_HTTP_AUTH_JWT_REQUIRE_KID
    if (!(kid && cJSON_IsString(kid) && kid->valuestring && kid->valuestring[0])) {
        cJSON_Delete(head_obj);
        cJSON_Delete(payload_obj);
        snprintf(out_reason, 64, "%s", "jwt_kid_missing");
        return ESP_MCP_HTTP_JWT_VERIFY_INVALID;
    }
#endif

    cJSON *iss = cJSON_GetObjectItem(payload_obj, "iss");
    if (CONFIG_MCP_HTTP_AUTH_JWT_EXPECT_ISS[0] != '\0' &&
            !(iss && cJSON_IsString(iss) && iss->valuestring &&
              strcmp(iss->valuestring, CONFIG_MCP_HTTP_AUTH_JWT_EXPECT_ISS) == 0)) {
        cJSON_Delete(head_obj);
        cJSON_Delete(payload_obj);
        snprintf(out_reason, 64, "%s", "jwt_iss_mismatch");
        return ESP_MCP_HTTP_JWT_VERIFY_INVALID;
    }
    cJSON *aud = cJSON_GetObjectItem(payload_obj, "aud");
    if (CONFIG_MCP_HTTP_AUTH_JWT_EXPECT_AUD[0] != '\0' &&
            !esp_mcp_http_aud_matches(aud, CONFIG_MCP_HTTP_AUTH_JWT_EXPECT_AUD)) {
        cJSON_Delete(head_obj);
        cJSON_Delete(payload_obj);
        snprintf(out_reason, 64, "%s", "jwt_aud_mismatch");
        return ESP_MCP_HTTP_JWT_VERIFY_INVALID;
    }

    int64_t now = esp_mcp_http_unix_time_now_sec();
    cJSON *exp = cJSON_GetObjectItem(payload_obj, "exp");
    if (!(exp && cJSON_IsNumber(exp))) {
        cJSON_Delete(head_obj);
        cJSON_Delete(payload_obj);
        snprintf(out_reason, 64, "%s", "jwt_exp_missing");
        return ESP_MCP_HTTP_JWT_VERIFY_INVALID;
    }
    if ((int64_t)exp->valuedouble + CONFIG_MCP_HTTP_AUTH_JWT_CLOCK_SKEW_SEC < now) {
        cJSON_Delete(head_obj);
        cJSON_Delete(payload_obj);
        snprintf(out_reason, 64, "%s", "jwt_expired");
        return ESP_MCP_HTTP_JWT_VERIFY_INVALID;
    }
    cJSON *nbf = cJSON_GetObjectItem(payload_obj, "nbf");
    if (nbf && cJSON_IsNumber(nbf) &&
            (int64_t)nbf->valuedouble - CONFIG_MCP_HTTP_AUTH_JWT_CLOCK_SKEW_SEC > now) {
        cJSON_Delete(head_obj);
        cJSON_Delete(payload_obj);
        snprintf(out_reason, 64, "%s", "jwt_nbf_in_future");
        return ESP_MCP_HTTP_JWT_VERIFY_INVALID;
    }
    cJSON *iat = cJSON_GetObjectItem(payload_obj, "iat");
    if (iat && cJSON_IsNumber(iat) &&
            (int64_t)iat->valuedouble - CONFIG_MCP_HTTP_AUTH_JWT_CLOCK_SKEW_SEC > now) {
        cJSON_Delete(head_obj);
        cJSON_Delete(payload_obj);
        snprintf(out_reason, 64, "%s", "jwt_iat_in_future");
        return ESP_MCP_HTTP_JWT_VERIFY_INVALID;
    }

    cJSON *scope = cJSON_GetObjectItem(payload_obj, "scope");
    cJSON *scp = cJSON_GetObjectItem(payload_obj, "scp");
    bool scope_ok = true;
#ifdef CONFIG_MCP_HTTP_AUTH_REQUIRED_SCOPE
    if (CONFIG_MCP_HTTP_AUTH_REQUIRED_SCOPE[0] != '\0') {
        scope_ok = false;
        if (scope && cJSON_IsString(scope) && scope->valuestring) {
            scope_ok = esp_mcp_http_scope_contains(scope->valuestring, CONFIG_MCP_HTTP_AUTH_REQUIRED_SCOPE);
        } else if (scp && cJSON_IsString(scp) && scp->valuestring) {
            scope_ok = esp_mcp_http_scope_contains(scp->valuestring, CONFIG_MCP_HTTP_AUTH_REQUIRED_SCOPE);
        } else if (scp && cJSON_IsArray(scp)) {
            cJSON *it = NULL;
            cJSON_ArrayForEach(it, scp) {
                if (cJSON_IsString(it) && it->valuestring &&
                        strcmp(it->valuestring, CONFIG_MCP_HTTP_AUTH_REQUIRED_SCOPE) == 0) {
                    scope_ok = true;
                    break;
                }
            }
        }
    }
#endif
    if (!scope_ok) {
        cJSON_Delete(head_obj);
        cJSON_Delete(payload_obj);
        snprintf(out_reason, 64, "%s", "jwt_scope_insufficient");
        return ESP_MCP_HTTP_JWT_VERIFY_SCOPE;
    }

#if CONFIG_MCP_HTTP_AUTH_JWT_VERIFY_SIGNATURE
    const char *kid_s = (kid && cJSON_IsString(kid) && kid->valuestring) ? kid->valuestring : NULL;
    size_t signing_len = (size_t)(dot2 - token);
    const char *sig_b64 = dot2 + 1;
    if (esp_mcp_http_verify_jwt_signature(alg->valuestring, kid_s, token, signing_len, sig_b64) != 0) {
        cJSON_Delete(head_obj);
        cJSON_Delete(payload_obj);
        snprintf(out_reason, 64, "%s", "jwt_signature_invalid");
        return ESP_MCP_HTTP_JWT_VERIFY_INVALID;
    }
#endif

    cJSON *sub = cJSON_GetObjectItem(payload_obj, "sub");
    if (sub && cJSON_IsString(sub) && sub->valuestring && sub->valuestring[0]) {
        snprintf(out_subject, AUTH_SUBJECT_LEN, "%s", sub->valuestring);
    } else {
        snprintf(out_subject, AUTH_SUBJECT_LEN, "%s", "jwt");
    }
    cJSON_Delete(head_obj);
    cJSON_Delete(payload_obj);
    out_reason[0] = '\0';
    return ESP_MCP_HTTP_JWT_VERIFY_OK;
}
#endif

#if CONFIG_MCP_HTTP_AUTH_ENABLE && (CONFIG_MCP_HTTP_AUTH_SCOPE_ENABLE || !CONFIG_MCP_HTTP_AUTH_JWT_VERIFY_ENABLE)
/** Compare user input (a) to expected secret (b) in time independent of len(a); avoids leaking len(b). */
static bool esp_mcp_ct_streq(const char *a, const char *b)
{
    if (!a || !b) {
        return false;
    }

    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    volatile unsigned char diff = (unsigned char)(len_a ^ len_b);

    for (size_t i = 0; i < len_b; ++i) {
        unsigned char ca = (i < len_a) ? ((const unsigned char *)a)[i] : 0;
        unsigned char cb = ((const unsigned char *)b)[i];
        diff |= (unsigned char)(ca ^ cb);
    }
    return diff == 0;
}
#endif

static esp_err_t esp_mcp_http_check_authorization(httpd_req_t *req,
                                                  bool *out_authenticated,
                                                  char out_subject[AUTH_SUBJECT_LEN])
{
    ESP_RETURN_ON_FALSE(req && out_authenticated && out_subject, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    *out_authenticated = false;
    out_subject[0] = '\0';
#if !CONFIG_MCP_HTTP_AUTH_ENABLE
    *out_authenticated = true;
    snprintf(out_subject, AUTH_SUBJECT_LEN, "anonymous");
    return ESP_OK;
#else
    size_t vlen = httpd_req_get_hdr_value_len(req, "Authorization");
    if (vlen == 0) {
        char www_auth[384] = {0};
        esp_mcp_http_build_www_authenticate(req, NULL, www_auth, sizeof(www_auth));
        return esp_mcp_http_send_jsonrpc_error(req,
                                               "401 Unauthorized",
                                               -32001,
                                               "Unauthorized",
                                               "authScheme",
                                               "Bearer",
                                               www_auth);
    }
    char *value = calloc(1, vlen + 1);
    if (!value) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t get_ret = httpd_req_get_hdr_value_str(req, "Authorization", value, vlen + 1);
    if (get_ret != ESP_OK) {
        free(value);
        return esp_mcp_http_send_jsonrpc_error(req,
                                               "401 Unauthorized",
                                               -32001,
                                               "Unauthorized",
                                               "reason",
                                               "missing_header",
                                               NULL);
    }
    const char *prefix = "Bearer ";
    bool ok = false;
    char fail_reason[64] = {0};
#if !CONFIG_MCP_HTTP_AUTH_JWT_VERIFY_ENABLE
    (void)fail_reason;
#endif
    if (strncmp(value, prefix, strlen(prefix)) == 0) {
        const char *token = value + strlen(prefix);
#if CONFIG_MCP_HTTP_AUTH_SCOPE_ENABLE
        if (token[0] != '\0' && esp_mcp_ct_streq(token, CONFIG_MCP_HTTP_AUTH_LOW_SCOPE_TOKEN)) {
            free(value);
            char www_auth[384] = {0};
            esp_mcp_http_build_www_authenticate(req, "insufficient_scope", www_auth, sizeof(www_auth));
            return esp_mcp_http_send_jsonrpc_error(req,
                                                   "403 Forbidden",
                                                   -32003,
                                                   "Forbidden",
                                                   "reason",
                                                   "scope_insufficient",
                                                   www_auth);
        }
#endif
        if (token[0] != '\0'
#if !CONFIG_MCP_HTTP_AUTH_JWT_VERIFY_ENABLE
                && esp_mcp_ct_streq(token, CONFIG_MCP_HTTP_AUTH_BEARER_TOKEN)
#endif
           ) {
#if CONFIG_MCP_HTTP_AUTH_JWT_VERIFY_ENABLE
            esp_mcp_http_jwt_verify_result_t vr = esp_mcp_http_verify_jwt_skeleton(token, out_subject, fail_reason);
            if (vr == ESP_MCP_HTTP_JWT_VERIFY_OK) {
                ok = true;
            } else if (vr == ESP_MCP_HTTP_JWT_VERIFY_SCOPE) {
                free(value);
                char www_auth[384] = {0};
                esp_mcp_http_build_www_authenticate(req, "insufficient_scope", www_auth, sizeof(www_auth));
                return esp_mcp_http_send_jsonrpc_error(req,
                                                       "403 Forbidden",
                                                       -32003,
                                                       "Forbidden",
                                                       "reason",
                                                       "scope_insufficient",
                                                       www_auth);
            }
#else
            ok = true;
            snprintf(out_subject, AUTH_SUBJECT_LEN, "%s", "bearer");
#endif
        }
    }
    free(value);
    if (!ok) {
        char www_auth[384] = {0};
        esp_mcp_http_build_www_authenticate(req, "invalid_token", www_auth, sizeof(www_auth));
        const char *public_reason = "invalid_token";
        return esp_mcp_http_send_jsonrpc_error(req,
                                               "403 Forbidden",
                                               -32003,
                                               "Forbidden",
                                               "reason",
                                               public_reason,
                                               www_auth);
    }
    *out_authenticated = true;
    return ESP_OK;
#endif
}

static bool esp_mcp_http_header_contains(httpd_req_t *req, const char *header_name, const char *needle)
{
    size_t vlen = httpd_req_get_hdr_value_len(req, header_name);
    if (vlen == 0) {
        return false;
    }
    char *value = calloc(1, vlen + 1);
    if (!value) {
        return false;
    }
    bool matched = false;
    if (httpd_req_get_hdr_value_str(req, header_name, value, vlen + 1) == ESP_OK) {
        // Accept */* as wildcard that matches any content type
        if (strstr(value, "*/*")) {
            matched = true;
        } else if (strstr(value, needle)) {
            matched = true;
        }
    }
    free(value);
    return matched;
}

static bool esp_mcp_http_extract_session_id(httpd_req_t *req, char out_session_id[SESSION_ID_LEN])
{
    if (!req || !out_session_id) {
        return false;
    }
    size_t sid_len = httpd_req_get_hdr_value_len(req, "MCP-Session-Id");
    if (sid_len > 0 && sid_len < SESSION_ID_LEN) {
        if (httpd_req_get_hdr_value_str(req, "MCP-Session-Id", out_session_id, SESSION_ID_LEN) == ESP_OK &&
                esp_mcp_http_is_ascii_visible(out_session_id)) {
            return true;
        }
    }
    size_t query_len = httpd_req_get_url_query_len(req);
    if (query_len > 0 && query_len < 256) {
        char *query = calloc(1, query_len + 1);
        if (!query) {
            return false;
        }
        bool matched = false;
        if (httpd_req_get_url_query_str(req, query, query_len + 1) == ESP_OK) {
            char session_buf[SESSION_ID_LEN] = {0};
            if (httpd_query_key_value(query, "sessionId", session_buf, sizeof(session_buf)) == ESP_OK &&
                    esp_mcp_http_is_ascii_visible(session_buf)) {
                strlcpy(out_session_id, session_buf, SESSION_ID_LEN);
                matched = true;
            } else if (httpd_query_key_value(query, "session_id", session_buf, sizeof(session_buf)) == ESP_OK &&
                       esp_mcp_http_is_ascii_visible(session_buf)) {
                strlcpy(out_session_id, session_buf, SESSION_ID_LEN);
                matched = true;
            }
        }
        free(query);
        if (matched) {
            return true;
        }
    }
    return false;
}

static void esp_mcp_http_generate_session_id(char out_session_id[SESSION_ID_LEN])
{
    if (!out_session_id) {
        return;
    }
    // SESSION_ID_LEN is 32, so generate 30 hex chars + NUL (120 bits entropy).
    uint8_t rnd[15] = {0};
    static const char hex[] = "0123456789abcdef";
    esp_fill_random(rnd, sizeof(rnd));
    for (size_t i = 0; i < sizeof(rnd); i++) {
        out_session_id[i * 2] = hex[(rnd[i] >> 4) & 0xF];
        out_session_id[i * 2 + 1] = hex[rnd[i] & 0xF];
    }
    out_session_id[sizeof(rnd) * 2] = '\0';
}

static esp_mcp_http_session_t *esp_mcp_http_session_find_locked(esp_mcp_http_server_item_t *mcp_http, const char *session_id)
{
    if (!mcp_http || !session_id || !session_id[0]) {
        return NULL;
    }
    for (esp_mcp_http_session_t *it = mcp_http->sessions; it; it = it->next) {
        if (strcmp(it->session_id, session_id) == 0) {
            return it;
        }
    }
    return NULL;
}

static void esp_mcp_http_session_sweep_expired_locked(esp_mcp_http_server_item_t *mcp_http)
{
    if (!mcp_http) {
        return;
    }
    int64_t now_ms = esp_timer_get_time() / 1000;
    esp_mcp_http_session_t **pp = &mcp_http->sessions;
    while (*pp) {
        if ((now_ms - (*pp)->last_seen_ms) > CONFIG_MCP_HTTP_SESSION_TTL_MS) {
            esp_mcp_http_session_t *drop = *pp;
            for (esp_mcp_sse_client_t *client = mcp_http->sse_clients; client; client = client->next) {
                if (strcmp(client->session_id, drop->session_id) == 0 && client->sockfd >= 0) {
                    (void)httpd_sess_trigger_close(mcp_http->httpd, client->sockfd);
                }
            }
            (void)esp_mcp_mgr_clear_session_state(mcp_http->handle, drop->session_id);
            esp_mcp_sse_hist_msg_t **pp_hist = &mcp_http->sse_history_head;
            while (*pp_hist) {
                if (strcmp((*pp_hist)->session_id, drop->session_id) == 0) {
                    esp_mcp_sse_hist_msg_t *hist_drop = *pp_hist;
                    *pp_hist = hist_drop->next;
                    if (mcp_http->sse_history_depth > 0) {
                        mcp_http->sse_history_depth--;
                    }
                    esp_mcp_http_sse_hist_msg_free(hist_drop);
                    continue;
                }
                pp_hist = &(*pp_hist)->next;
            }
            *pp = drop->next;
            free(drop);
            continue;
        }
        pp = &(*pp)->next;
    }
    mcp_http->sse_history_tail = NULL;
    for (esp_mcp_sse_hist_msg_t *it = mcp_http->sse_history_head; it; it = it->next) {
        mcp_http->sse_history_tail = it;
    }
}

static esp_err_t esp_mcp_http_session_add_locked(esp_mcp_http_server_item_t *mcp_http, const char *session_id)
{
    ESP_RETURN_ON_FALSE(mcp_http && session_id && session_id[0], ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    if (esp_mcp_http_session_find_locked(mcp_http, session_id)) {
        ESP_LOGW(TAG, "Session id collision detected, regenerate and retry");
        return ESP_ERR_INVALID_STATE;
    }
    esp_mcp_http_session_t *node = calloc(1, sizeof(esp_mcp_http_session_t));
    ESP_RETURN_ON_FALSE(node, ESP_ERR_NO_MEM, TAG, "No mem");
    snprintf(node->session_id, sizeof(node->session_id), "%s", session_id);
    snprintf(node->protocol_version, sizeof(node->protocol_version), "%s", CONFIG_MCP_HTTP_DEFAULT_PROTOCOL_VERSION);
    node->last_seen_ms = esp_timer_get_time() / 1000;
    node->next_stream_id = 1;
    node->resume_stream_id = 0;
    node->resume_next_event_seq = 1;
    node->next = mcp_http->sessions;
    mcp_http->sessions = node;
    return ESP_OK;
}

static bool esp_mcp_http_session_remove_locked(esp_mcp_http_server_item_t *mcp_http, const char *session_id)
{
    if (!mcp_http || !session_id || !session_id[0]) {
        return false;
    }
    esp_mcp_http_session_t **pp = &mcp_http->sessions;
    while (*pp) {
        if (strcmp((*pp)->session_id, session_id) == 0) {
            esp_mcp_http_session_t *drop = *pp;
            *pp = drop->next;
            free(drop);
            return true;
        }
        pp = &(*pp)->next;
    }
    return false;
}

static esp_err_t esp_mcp_http_resolve_session_id(httpd_req_t *req,
                                                 esp_mcp_http_server_item_t *mcp_http,
                                                 char out_session_id[SESSION_ID_LEN])
{
    ESP_RETURN_ON_FALSE(req && mcp_http && out_session_id, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    bool provided = esp_mcp_http_extract_session_id(req, out_session_id);
    if (!mcp_http->sse_mutex || xSemaphoreTake(mcp_http->sse_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_mcp_http_session_sweep_expired_locked(mcp_http);
    esp_err_t ret = ESP_OK;
    if (provided) {
        esp_mcp_http_session_t *sess = esp_mcp_http_session_find_locked(mcp_http, out_session_id);
        if (!sess) {
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Unknown MCP session");
            ret = ESP_ERR_NOT_FOUND;
        } else {
            sess->last_seen_ms = esp_timer_get_time() / 1000;
        }
    } else {
        ret = ESP_ERR_INVALID_STATE;
        for (int attempt = 0; attempt < 4 && ret == ESP_ERR_INVALID_STATE; attempt++) {
            esp_mcp_http_generate_session_id(out_session_id);
            ret = esp_mcp_http_session_add_locked(mcp_http, out_session_id);
        }
    }
    xSemaphoreGive(mcp_http->sse_mutex);
    return ret;
}

#if CONFIG_MCP_HTTP_SSE_ENABLE
static bool esp_mcp_http_sse_parse_event_id(const char *event_id,
                                            uint32_t *out_stream_id,
                                            uint32_t *out_event_seq)
{
    if (!(event_id && out_stream_id && out_event_seq)) {
        return false;
    }

    const char *sep = strchr(event_id, '-');
    if (!sep || sep == event_id || !sep[1]) {
        return false;
    }

    char stream_buf[16] = {0};
    char seq_buf[16] = {0};
    size_t stream_len = (size_t)(sep - event_id);
    size_t seq_len = strlen(sep + 1);
    if (stream_len >= sizeof(stream_buf) || seq_len >= sizeof(seq_buf)) {
        return false;
    }

    memcpy(stream_buf, event_id, stream_len);
    memcpy(seq_buf, sep + 1, seq_len);

    char *stream_end = NULL;
    char *seq_end = NULL;
    unsigned long stream_id = strtoul(stream_buf, &stream_end, 16);
    unsigned long event_seq = strtoul(seq_buf, &seq_end, 16);
    if (!(stream_end && *stream_end == '\0' && seq_end && *seq_end == '\0')) {
        return false;
    }
    if (stream_id == 0 || stream_id > UINT32_MAX || event_seq == 0 || event_seq > UINT32_MAX) {
        return false;
    }

    *out_stream_id = (uint32_t)stream_id;
    *out_event_seq = (uint32_t)event_seq;
    return true;
}

static void esp_mcp_http_sse_format_event_id(uint32_t stream_id,
                                             uint32_t event_seq,
                                             char out_event_id[SSE_EVENT_ID_LEN])
{
    if (!out_event_id) {
        return;
    }
    snprintf(out_event_id, SSE_EVENT_ID_LEN, "%08" PRIx32 "-%08" PRIx32, stream_id, event_seq);
}

static void esp_mcp_http_sse_history_reset_locked(esp_mcp_http_server_item_t *mcp_http)
{
    if (!mcp_http) {
        return;
    }
    esp_mcp_sse_hist_msg_t *hist = mcp_http->sse_history_head;
    while (hist) {
        esp_mcp_sse_hist_msg_t *next = hist->next;
        esp_mcp_http_sse_hist_msg_free(hist);
        hist = next;
    }
    mcp_http->sse_history_head = NULL;
    mcp_http->sse_history_tail = NULL;
    mcp_http->sse_history_depth = 0;
}

static uint32_t esp_mcp_http_session_next_stream_id_locked(esp_mcp_http_server_item_t *mcp_http,
                                                           esp_mcp_http_session_t *session)
{
    if (!session) {
        return 0;
    }
    if (session->next_stream_id == 0) {
        session->next_stream_id = 1;
    }
    uint32_t id = session->next_stream_id++;
    if (session->next_stream_id == 0) {
        ESP_LOGW(TAG, "SSE stream id overflow, reset replay history");
        session->next_stream_id = 1;
        if (mcp_http) {
            esp_mcp_http_sse_history_reset_locked(mcp_http);
        }
    }
    return id;
}

static void esp_mcp_http_session_select_stream_locked(esp_mcp_http_session_t *session,
                                                      uint32_t stream_id,
                                                      uint32_t next_event_seq)
{
    if (!session || stream_id == 0) {
        return;
    }
    session->resume_stream_id = stream_id;
    session->resume_next_event_seq = next_event_seq > 0 ? next_event_seq : 1;
}

static void esp_mcp_http_sse_assign_event_identity(char event_id[SSE_EVENT_ID_LEN],
                                                   uint32_t *out_stream_id,
                                                   uint32_t *out_event_seq,
                                                   uint32_t stream_id,
                                                   uint32_t event_seq)
{
    if (event_id) {
        esp_mcp_http_sse_format_event_id(stream_id, event_seq, event_id);
    }
    if (out_stream_id) {
        *out_stream_id = stream_id;
    }
    if (out_event_seq) {
        *out_event_seq = event_seq;
    }
}
#endif

static void esp_mcp_http_sse_msg_free(esp_mcp_sse_msg_t *msg)
{
    if (!msg) {
        return;
    }
    free(msg->jsonrpc_message);
    free(msg);
}

static void esp_mcp_http_sse_hist_msg_free(esp_mcp_sse_hist_msg_t *msg)
{
    if (!msg) {
        return;
    }
    free(msg->jsonrpc_message);
    free(msg);
}

static void esp_mcp_http_sse_client_drain(esp_mcp_sse_client_t *client)
{
    while (client && client->head) {
        esp_mcp_sse_msg_t *msg = client->head;
        client->head = msg->next;
        if (!client->head) {
            client->tail = NULL;
        }
        esp_mcp_http_sse_msg_free(msg);
    }
    if (client) {
        client->depth = 0;
    }
}

#if CONFIG_MCP_HTTP_SSE_ENABLE
static esp_mcp_sse_client_t *esp_mcp_http_sse_find_preferred_client_locked(esp_mcp_http_server_item_t *mcp_http,
                                                                           const char *session_id)
{
    if (!(mcp_http && session_id && session_id[0])) {
        return NULL;
    }

    for (esp_mcp_sse_client_t *it = mcp_http->sse_clients; it; it = it->next) {
        if (strcmp(it->session_id, session_id) == 0) {
            return it;
        }
    }
    return NULL;
}

static esp_err_t esp_mcp_http_sse_reserve_client_event_locked(esp_mcp_http_server_item_t *mcp_http,
                                                              esp_mcp_http_session_t *session,
                                                              esp_mcp_sse_client_t *client,
                                                              char event_id[SSE_EVENT_ID_LEN],
                                                              uint32_t *out_stream_id,
                                                              uint32_t *out_event_seq)
{
    ESP_RETURN_ON_FALSE(session && client, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    if (client->stream_id == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    if (client->next_event_seq == 0) {
        client->next_event_seq = 1;
    }

    uint32_t event_seq = client->next_event_seq++;
    if (client->next_event_seq == 0) {
        ESP_LOGW(TAG, "SSE event seq overflow, reset replay history");
        client->next_event_seq = 1;
        if (mcp_http) {
            esp_mcp_http_sse_history_reset_locked(mcp_http);
        }
    }
    esp_mcp_http_sse_assign_event_identity(event_id, out_stream_id, out_event_seq, client->stream_id, event_seq);
    esp_mcp_http_session_select_stream_locked(session, client->stream_id, client->next_event_seq);
    return ESP_OK;
}

static esp_err_t esp_mcp_http_sse_reserve_session_event_locked(esp_mcp_http_server_item_t *mcp_http,
                                                               esp_mcp_http_session_t *session,
                                                               char event_id[SSE_EVENT_ID_LEN],
                                                               uint32_t *out_stream_id,
                                                               uint32_t *out_event_seq)
{
    ESP_RETURN_ON_FALSE(session, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    if (session->resume_stream_id == 0) {
        return ESP_ERR_NOT_FOUND;
    }
    if (session->resume_next_event_seq == 0) {
        session->resume_next_event_seq = 1;
    }

    uint32_t event_seq = session->resume_next_event_seq++;
    if (session->resume_next_event_seq == 0) {
        ESP_LOGW(TAG, "SSE session event seq overflow, reset replay history");
        session->resume_next_event_seq = 1;
        if (mcp_http) {
            esp_mcp_http_sse_history_reset_locked(mcp_http);
        }
    }
    esp_mcp_http_sse_assign_event_identity(event_id,
                                           out_stream_id,
                                           out_event_seq,
                                           session->resume_stream_id,
                                           event_seq);
    return ESP_OK;
}

static esp_err_t esp_mcp_http_sse_add_client(esp_mcp_http_server_item_t *mcp_http, esp_mcp_sse_client_t *client)
{
    ESP_RETURN_ON_FALSE(mcp_http && client, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    if (!mcp_http->sse_mutex || xSemaphoreTake(mcp_http->sse_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }
    size_t count = 0;
    for (esp_mcp_sse_client_t *it = mcp_http->sse_clients; it; it = it->next) {
        count++;
    }
    if (count >= CONFIG_MCP_HTTP_SSE_MAX_CLIENTS) {
        xSemaphoreGive(mcp_http->sse_mutex);
        return ESP_ERR_NO_MEM;
    }

    esp_mcp_http_session_t *session = esp_mcp_http_session_find_locked(mcp_http, client->session_id);
    if (!session) {
        xSemaphoreGive(mcp_http->sse_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    if (client->stream_id == 0) {
        client->stream_id = esp_mcp_http_session_next_stream_id_locked(mcp_http, session);
    }
    if (client->next_event_seq == 0) {
        client->next_event_seq = 1;
    }
    esp_mcp_http_session_select_stream_locked(session, client->stream_id, client->next_event_seq);

    client->next = mcp_http->sse_clients;
    mcp_http->sse_clients = client;
    xSemaphoreGive(mcp_http->sse_mutex);
    return ESP_OK;
}

static esp_err_t esp_mcp_http_sse_remove_client(esp_mcp_http_server_item_t *mcp_http, esp_mcp_sse_client_t *client)
{
    if (!mcp_http || !client) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!mcp_http->sse_mutex || xSemaphoreTake(mcp_http->sse_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire SSE mutex while removing client");
        return ESP_ERR_INVALID_STATE;
    }
    esp_mcp_sse_client_t **pp = &mcp_http->sse_clients;
    while (*pp) {
        if (*pp == client) {
            *pp = client->next;
            break;
        }
        pp = &(*pp)->next;
    }
    esp_mcp_http_session_t *session = esp_mcp_http_session_find_locked(mcp_http, client->session_id);
    if (session && session->resume_stream_id == client->stream_id) {
        esp_mcp_sse_client_t *replacement = esp_mcp_http_sse_find_preferred_client_locked(mcp_http, client->session_id);
        if (replacement) {
            esp_mcp_http_session_select_stream_locked(session, replacement->stream_id, replacement->next_event_seq);
        } else {
            esp_mcp_http_session_select_stream_locked(session,
                                                      client->stream_id,
                                                      client->next_event_seq > 0 ? client->next_event_seq : 1);
        }
    }
    esp_mcp_http_sse_client_drain(client);
    xSemaphoreGive(mcp_http->sse_mutex);
    return ESP_OK;
}

static esp_mcp_sse_msg_t *esp_mcp_http_sse_pop_one(esp_mcp_http_server_item_t *mcp_http, esp_mcp_sse_client_t *client)
{
    if (!mcp_http || !client || !mcp_http->sse_mutex) {
        return NULL;
    }
    if (xSemaphoreTake(mcp_http->sse_mutex, portMAX_DELAY) != pdTRUE) {
        return NULL;
    }
    esp_mcp_sse_msg_t *msg = client->head;
    if (msg) {
        client->head = msg->next;
        if (!client->head) {
            client->tail = NULL;
        }
        msg->next = NULL;
        if (client->depth > 0) {
            client->depth--;
        }
    }
    xSemaphoreGive(mcp_http->sse_mutex);
    return msg;
}

static esp_err_t esp_mcp_http_sse_enqueue_locked(esp_mcp_sse_client_t *client,
                                                 const char *event_id,
                                                 uint32_t stream_id,
                                                 uint32_t event_seq,
                                                 const char *jsonrpc_message)
{
    ESP_RETURN_ON_FALSE(client && jsonrpc_message, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    esp_mcp_sse_msg_t *msg = calloc(1, sizeof(esp_mcp_sse_msg_t));
    ESP_RETURN_ON_FALSE(msg, ESP_ERR_NO_MEM, TAG, "No mem");
    msg->jsonrpc_message = strdup(jsonrpc_message);
    if (!msg->jsonrpc_message) {
        free(msg);
        return ESP_ERR_NO_MEM;
    }
    msg->stream_id = stream_id;
    msg->event_seq = event_seq;
    if (event_id && event_id[0]) {
        snprintf(msg->event_id, sizeof(msg->event_id), "%s", event_id);
    }
    if (client->tail) {
        client->tail->next = msg;
    } else {
        client->head = msg;
    }
    client->tail = msg;
    client->depth++;
    if (client->depth > CONFIG_MCP_HTTP_SSE_QUEUE_LEN) {
        esp_mcp_sse_msg_t *drop = client->head;
        client->head = drop->next;
        if (!client->head) {
            client->tail = NULL;
        }
        client->depth--;
        ESP_LOGW(TAG,
                 "SSE queue overflow: drop oldest event=%s session=%s depth=%u limit=%d",
                 drop->event_id,
                 client->session_id,
                 (unsigned)client->depth,
                 CONFIG_MCP_HTTP_SSE_QUEUE_LEN);
        esp_mcp_http_sse_msg_free(drop);
    }
    return ESP_OK;
}

static void esp_mcp_http_sse_history_append_locked(esp_mcp_http_server_item_t *mcp_http,
                                                   const char *event_id,
                                                   const char *session_id,
                                                   uint32_t stream_id,
                                                   uint32_t event_seq,
                                                   const char *jsonrpc_message)
{
    if (!mcp_http || !session_id || !session_id[0] || !event_id || !event_id[0] || !jsonrpc_message) {
        return;
    }
    esp_mcp_sse_hist_msg_t *msg = calloc(1, sizeof(esp_mcp_sse_hist_msg_t));
    if (!msg) {
        return;
    }
    msg->jsonrpc_message = strdup(jsonrpc_message);
    if (!msg->jsonrpc_message) {
        free(msg);
        return;
    }
    snprintf(msg->session_id, sizeof(msg->session_id), "%s", session_id);
    msg->stream_id = stream_id;
    msg->event_seq = event_seq;
    snprintf(msg->event_id, sizeof(msg->event_id), "%s", event_id);
    if (mcp_http->sse_history_tail) {
        mcp_http->sse_history_tail->next = msg;
    } else {
        mcp_http->sse_history_head = msg;
    }
    mcp_http->sse_history_tail = msg;
    mcp_http->sse_history_depth++;
    while (mcp_http->sse_history_depth > CONFIG_MCP_HTTP_SSE_REPLAY_DEPTH) {
        esp_mcp_sse_hist_msg_t *drop = mcp_http->sse_history_head;
        if (!drop) {
            break;
        }
        mcp_http->sse_history_head = drop->next;
        if (!mcp_http->sse_history_head) {
            mcp_http->sse_history_tail = NULL;
        }
        if (mcp_http->sse_history_depth > 0) {
            mcp_http->sse_history_depth--;
        }
        esp_mcp_http_sse_hist_msg_free(drop);
    }
}

static void esp_mcp_http_sse_replay_to_client_locked(esp_mcp_http_server_item_t *mcp_http,
                                                     esp_mcp_sse_client_t *client,
                                                     uint32_t stream_id,
                                                     uint32_t last_event_seq)
{
    if (!mcp_http || !client) {
        return;
    }
    size_t replayed = 0;
    size_t failed = 0;
    for (esp_mcp_sse_hist_msg_t *it = mcp_http->sse_history_head; it; it = it->next) {
        if (it->stream_id != stream_id) {
            continue;
        }
        if (it->event_seq <= last_event_seq) {
            continue;
        }
        if (strcmp(it->session_id, client->session_id) != 0) {
            continue;
        }
        if (replayed >= CONFIG_MCP_HTTP_SSE_QUEUE_LEN) {
            ESP_LOGW(TAG, "SSE replay truncated at queue limit=%d for session=%s", CONFIG_MCP_HTTP_SSE_QUEUE_LEN, client->session_id);
            break;
        }
        esp_err_t ret = esp_mcp_http_sse_enqueue_locked(client,
                                                        it->event_id,
                                                        it->stream_id,
                                                        it->event_seq,
                                                        it->jsonrpc_message);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to enqueue replay event: %s", esp_err_to_name(ret));
            failed++;
            break;
        }
        if (it->event_seq >= client->next_event_seq) {
            client->next_event_seq = it->event_seq + 1;
        }
        replayed++;
    }
    if (failed > 0) {
        ESP_LOGW(TAG, "SSE replay dropped %u event(s) for session=%s", (unsigned)failed, client->session_id);
    }
}

static esp_err_t esp_mcp_http_sse_send_event(httpd_req_t *req, const char *event_id, const char *jsonrpc_message)
{
    ESP_RETURN_ON_FALSE(req && event_id && event_id[0] && jsonrpc_message, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    char header[160] = {0};
    int header_len = snprintf(header, sizeof(header), "id: %s\nevent: message\n", event_id);
    ESP_RETURN_ON_FALSE(header_len > 0 && header_len < (int)sizeof(header), ESP_FAIL, TAG, "SSE header build failed");
    esp_err_t ret = httpd_resp_send_chunk(req, header, (size_t)header_len);
    if (ret != ESP_OK) {
        return ret;
    }

    const char *p = jsonrpc_message;
    while (true) {
        const char *line_end = strchr(p, '\n');
        size_t line_len = line_end ? (size_t)(line_end - p) : strlen(p);
        while (line_len > 0 && p[line_len - 1] == '\r') {
            line_len--;
        }
        size_t payload_len = 6 + line_len + 1;
        char *payload = calloc(1, payload_len + 1);
        ESP_RETURN_ON_FALSE(payload, ESP_ERR_NO_MEM, TAG, "No mem");
        snprintf(payload, payload_len + 1, "data: %.*s\n", (int)line_len, p);
        ret = httpd_resp_send_chunk(req, payload, payload_len);
        free(payload);
        if (ret != ESP_OK) {
            return ret;
        }
        if (!line_end) {
            break;
        }
        p = line_end + 1;
    }

    return httpd_resp_send_chunk(req, "\n", 1);
}

static esp_err_t esp_mcp_http_sse_send_priming_event(httpd_req_t *req, const char *event_id)
{
    ESP_RETURN_ON_FALSE(req && event_id && event_id[0], ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    size_t payload_len = snprintf(NULL, 0, "id: %s\ndata:\n\n", event_id);
    char *payload = calloc(1, payload_len + 1);
    ESP_RETURN_ON_FALSE(payload, ESP_ERR_NO_MEM, TAG, "No mem");
    snprintf(payload, payload_len + 1, "id: %s\ndata:\n\n", event_id);
    esp_err_t ret = httpd_resp_send_chunk(req, payload, payload_len);
    free(payload);
    return ret;
}

static esp_err_t esp_mcp_http_sse_send_endpoint(httpd_req_t *req, const char *endpoint)
{
    ESP_RETURN_ON_FALSE(req && endpoint, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    size_t payload_len = snprintf(NULL, 0, "event: endpoint\ndata: %s\n\n", endpoint);
    char *payload = calloc(1, payload_len + 1);
    ESP_RETURN_ON_FALSE(payload, ESP_ERR_NO_MEM, TAG, "No mem");
    snprintf(payload, payload_len + 1, "event: endpoint\ndata: %s\n\n", endpoint);
    esp_err_t ret = httpd_resp_send_chunk(req, payload, payload_len);
    free(payload);
    return ret;
}

static esp_err_t esp_mcp_http_sse_send_comment(httpd_req_t *req, const char *comment)
{
    ESP_RETURN_ON_FALSE(req && comment, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    size_t payload_len = snprintf(NULL, 0, ": %s\n\n", comment);
    char *payload = calloc(1, payload_len + 1);
    ESP_RETURN_ON_FALSE(payload, ESP_ERR_NO_MEM, TAG, "No mem");
    snprintf(payload, payload_len + 1, ": %s\n\n", comment);
    esp_err_t ret = httpd_resp_send_chunk(req, payload, payload_len);
    free(payload);
    return ret;
}

static uint32_t esp_mcp_http_sse_effective_heartbeat_ms(void)
{
    uint32_t configured = (uint32_t)CONFIG_MCP_HTTP_SSE_HEARTBEAT_MS;
    if (configured < 1000U) {
        ESP_LOGW(TAG, "SSE heartbeat too small (%" PRIu32 "ms), clamp to 1000ms", configured);
        return 1000U;
    }
    if (configured > 300000U) {
        ESP_LOGW(TAG, "SSE heartbeat too large (%" PRIu32 "ms), clamp to 300000ms", configured);
        return 300000U;
    }
    return configured;
}

static void esp_mcp_http_build_sse_endpoint(httpd_req_t *req,
                                            const char *session_id,
                                            char *out_endpoint,
                                            size_t out_len)
{
    if (!req || !session_id || !out_endpoint || out_len == 0) {
        return;
    }
    out_endpoint[0] = '\0';
    const char *uri = req->uri;
    const char *sep = strchr(uri, '?') ? "&" : "?";
    const char *param = "sessionId=";
    size_t uri_len = strlen(uri);
    size_t sep_len = strlen(sep);
    size_t param_len = strlen(param);
    size_t sid_len = strlen(session_id);
    if (uri_len + sep_len + param_len + sid_len + 1 > out_len) {
        ESP_LOGW(TAG, "Legacy SSE endpoint truncated; buffer too small");
        return;
    }

    memcpy(out_endpoint, uri, uri_len);
    memcpy(out_endpoint + uri_len, sep, sep_len);
    memcpy(out_endpoint + uri_len + sep_len, param, param_len);
    memcpy(out_endpoint + uri_len + sep_len + param_len, session_id, sid_len);
    out_endpoint[uri_len + sep_len + param_len + sid_len] = '\0';
}

static bool esp_mcp_http_is_legacy_sse_endpoint(const httpd_req_t *req)
{
    if (!req) {
        return false;
    }

    size_t uri_len = strlen(req->uri);
    const char *qmark = strchr(req->uri, '?');
    if (qmark) {
        uri_len = (size_t)(qmark - req->uri);
    }

    return uri_len >= 4 && strncmp(req->uri + uri_len - 4, "_sse", 4) == 0;
}

static void esp_mcp_http_sse_client_task(void *arg)
{
    esp_mcp_sse_client_t *client = (esp_mcp_sse_client_t *)arg;
#if CONFIG_MCP_HTTP_CORS_ENABLE
    char cors_origin[ESP_MCP_HTTP_CORS_ORIGIN_BUF] = {0};
#endif
    if (!client || !client->owner || !client->req) {
        vTaskDelete(NULL);
        return;
    }

    esp_err_t ret = httpd_resp_set_type(client->req, "text/event-stream");
    if (ret == ESP_OK) {
        ret = httpd_resp_set_hdr(client->req, "Cache-Control", "no-cache");
    }
    if (ret == ESP_OK) {
        ret = httpd_resp_set_hdr(client->req, "Connection", "keep-alive");
    }
    if (ret == ESP_OK) {
        ret = httpd_resp_set_hdr(client->req, "X-Accel-Buffering", "no");
    }
    if (ret == ESP_OK) {
        ret = httpd_resp_set_hdr(client->req, "MCP-Session-Id", client->session_id);
    }
    if (ret == ESP_OK) {
        ret = httpd_resp_set_hdr(client->req, "MCP-Protocol-Version", client->protocol_version[0] ? client->protocol_version : CONFIG_MCP_HTTP_DEFAULT_PROTOCOL_VERSION);
    }
#if CONFIG_MCP_HTTP_CORS_ENABLE
    if (ret == ESP_OK) {
        if (esp_mcp_http_origin_check(client->req, cors_origin, sizeof(cors_origin)) == ESP_OK && cors_origin[0] != '\0') {
            ret = httpd_resp_set_hdr(client->req, "Access-Control-Allow-Origin", cors_origin);
            if (ret == ESP_OK) {
                esp_mcp_http_cors_set_expose_headers(client->req);
            }
        }
    }
#endif
    if (ret == ESP_OK) {
        if (esp_mcp_http_is_legacy_sse_endpoint(client->req)) {
            char endpoint[128] = {0};
            esp_mcp_http_build_sse_endpoint(client->req, client->session_id, endpoint, sizeof(endpoint));
            if (endpoint[0]) {
                ESP_LOGI(TAG, "Legacy SSE endpoint for session %s: %s", client->session_id, endpoint);
                ret = esp_mcp_http_sse_send_endpoint(client->req, endpoint);
            }
        } else if (!client->resuming_stream) {
            char priming_event_id[SSE_EVENT_ID_LEN] = {0};
            if (client->owner->sse_mutex &&
                    xSemaphoreTake(client->owner->sse_mutex, portMAX_DELAY) == pdTRUE) {
                esp_mcp_http_session_t *session =
                    esp_mcp_http_session_find_locked(client->owner, client->session_id);
                if (session) {
                    ret = esp_mcp_http_sse_reserve_client_event_locked(client->owner,
                                                                       session,
                                                                       client,
                                                                       priming_event_id,
                                                                       NULL,
                                                                       NULL);
                } else {
                    ret = ESP_ERR_NOT_FOUND;
                }
                xSemaphoreGive(client->owner->sse_mutex);
            } else {
                ret = ESP_ERR_INVALID_STATE;
            }
            if (ret == ESP_OK) {
                ret = esp_mcp_http_sse_send_priming_event(client->req, priming_event_id);
            }
        }
    }
    if (ret == ESP_OK) {
        ret = esp_mcp_http_sse_send_comment(client->req, "mcp-sse-open");
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SSE client setup failed for session=%s: %s", client->session_id, esp_err_to_name(ret));
    }

    uint32_t heartbeat_ms = esp_mcp_http_sse_effective_heartbeat_ms();
    int64_t last_heartbeat_ms = esp_timer_get_time() / 1000;
    while (ret == ESP_OK && client->owner && !client->owner->sse_teardown) {
        (void)esp_mcp_mgr_sweep_pending_requests(client->owner->handle);
        esp_mcp_sse_msg_t *msg = esp_mcp_http_sse_pop_one(client->owner, client);
        if (msg) {
            if (!msg->event_id[0]) {
                if (client->owner->sse_mutex &&
                        xSemaphoreTake(client->owner->sse_mutex, portMAX_DELAY) == pdTRUE) {
                    esp_mcp_http_session_t *session =
                        esp_mcp_http_session_find_locked(client->owner, client->session_id);
                    if (session &&
                            esp_mcp_http_sse_reserve_client_event_locked(client->owner,
                                                                         session,
                                                                         client,
                                                                         msg->event_id,
                                                                         &msg->stream_id,
                                                                         &msg->event_seq) == ESP_OK) {
                        esp_mcp_http_sse_history_append_locked(client->owner,
                                                               msg->event_id,
                                                               client->session_id,
                                                               msg->stream_id,
                                                               msg->event_seq,
                                                               msg->jsonrpc_message);
                    } else {
                        ret = ESP_ERR_INVALID_STATE;
                    }
                    xSemaphoreGive(client->owner->sse_mutex);
                } else {
                    ret = ESP_ERR_INVALID_STATE;
                }
            }
            if (ret != ESP_OK) {
                esp_mcp_http_sse_msg_free(msg);
                continue;
            }
            ret = esp_mcp_http_sse_send_event(client->req, msg->event_id, msg->jsonrpc_message);
            esp_mcp_http_sse_msg_free(msg);
            last_heartbeat_ms = esp_timer_get_time() / 1000;
            continue;
        }

        int64_t now_ms = esp_timer_get_time() / 1000;
        if ((now_ms - last_heartbeat_ms) >= (int64_t)heartbeat_ms) {
            ret = esp_mcp_http_sse_send_comment(client->req, "ping");
            last_heartbeat_ms = now_ms;
        } else {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }

    // Provide client reconnection hint before stream close.
    char retry_payload[32] = {0};
    int retry_len = snprintf(retry_payload, sizeof(retry_payload), "retry: %" PRIu32 "\n\n", heartbeat_ms);
    if (retry_len > 0) {
        (void)httpd_resp_send_chunk(client->req, retry_payload, retry_len);
    }

    esp_err_t rm_ret = esp_mcp_http_sse_remove_client(client->owner, client);
    if (rm_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to remove SSE client: %s", esp_err_to_name(rm_ret));
    }
    (void)httpd_resp_send_chunk(client->req, NULL, 0);
    (void)httpd_req_async_handler_complete(client->req);
    free(client);
    vTaskDelete(NULL);
}
#endif

static esp_err_t esp_mcp_http_head_handler(httpd_req_t *req)
{
    ESP_RETURN_ON_FALSE(req, ESP_ERR_INVALID_ARG, TAG, "Invalid request");
    ESP_RETURN_ON_FALSE(req->user_ctx, ESP_ERR_INVALID_ARG, TAG, "Invalid user context");
    esp_mcp_http_server_item_t *mcp_http = (esp_mcp_http_server_item_t *)req->user_ctx;
    char proto_hdr[32] = {0};
    ESP_RETURN_ON_ERROR(esp_mcp_http_apply_protocol_version(req, mcp_http, proto_hdr, sizeof(proto_hdr)),
                        TAG,
                        "Protocol header validation failed");
    ESP_RETURN_ON_ERROR(httpd_resp_set_hdr(req, "MCP-Protocol-Version", proto_hdr), TAG, "Protocol header set failed");
    ESP_RETURN_ON_ERROR(esp_mcp_http_validate_headers(req), TAG, "Header validation failed");
    bool is_authenticated = false;
    char auth_subject[AUTH_SUBJECT_LEN] = {0};
    ESP_RETURN_ON_ERROR(esp_mcp_http_check_authorization(req, &is_authenticated, auth_subject), TAG, "Auth failed");
    (void)is_authenticated;
    (void)auth_subject;

    httpd_resp_set_status(req, "204 No Content");
#if CONFIG_MCP_HTTP_CORS_ENABLE
    (void)httpd_resp_set_hdr(req, "Allow", "POST, GET, DELETE, HEAD, OPTIONS");
#else
    (void)httpd_resp_set_hdr(req, "Allow", "POST, GET, DELETE, HEAD");
#endif
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t esp_mcp_http_get_handler(httpd_req_t *req)
{
    ESP_RETURN_ON_FALSE(req, ESP_ERR_INVALID_ARG, TAG, "Invalid request");
    ESP_RETURN_ON_FALSE(req->user_ctx, ESP_ERR_INVALID_ARG, TAG, "Invalid user context");
    esp_mcp_http_server_item_t *mcp_http = (esp_mcp_http_server_item_t *)req->user_ctx;
    char proto_hdr[32] = {0};
    ESP_RETURN_ON_ERROR(esp_mcp_http_apply_protocol_version(req, mcp_http, proto_hdr, sizeof(proto_hdr)),
                        TAG,
                        "Protocol header validation failed");
    ESP_RETURN_ON_ERROR(esp_mcp_http_validate_headers(req), TAG, "Header validation failed");
    bool is_authenticated = false;
    char auth_subject[AUTH_SUBJECT_LEN] = {0};
    ESP_RETURN_ON_ERROR(esp_mcp_http_check_authorization(req, &is_authenticated, auth_subject), TAG, "Auth failed");
    (void)is_authenticated;
    (void)auth_subject;

#if !CONFIG_MCP_HTTP_SSE_ENABLE
    httpd_resp_set_status(req, ESP_MCP_HTTPD_405);
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "SSE stream is not enabled", HTTPD_RESP_USE_STRLEN);
#else
    if (!esp_mcp_http_header_contains(req, "Accept", "text/event-stream")) {
        (void)esp_mcp_http_send_error(req, ESP_MCP_HTTP_ERROR_406, "Accept must include text/event-stream");
        return ESP_ERR_INVALID_ARG;
    }

    char session_id[SESSION_ID_LEN] = {0};
    ESP_RETURN_ON_ERROR(esp_mcp_http_resolve_session_id(req, mcp_http, session_id), TAG, "Session id resolve failed");
    esp_mcp_http_session_update_protocol_version(mcp_http, session_id, proto_hdr);
    esp_mcp_http_clear_pending_response_hdrs(req);
    httpd_req_t *async_req = NULL;
    esp_err_t async_ret = httpd_req_async_handler_begin(req, &async_req);
    if (async_ret != ESP_OK || !async_req) {
        (void)esp_mcp_http_send_error(req, ESP_MCP_HTTP_ERROR_503, "Failed to start async SSE");
        return ESP_FAIL;
    }
    char last_event_id[SSE_EVENT_ID_LEN] = {0};
    size_t leid_len = httpd_req_get_hdr_value_len(async_req, "Last-Event-ID");
    if (leid_len > 0 && leid_len < sizeof(last_event_id)) {
        char leid_buf[SSE_EVENT_ID_LEN] = {0};
        if (httpd_req_get_hdr_value_str(async_req, "Last-Event-ID", leid_buf, sizeof(leid_buf)) == ESP_OK) {
            snprintf(last_event_id, sizeof(last_event_id), "%s", leid_buf);
        }
    }

    esp_mcp_sse_client_t *client = calloc(1, sizeof(esp_mcp_sse_client_t));
    if (!client) {
        (void)httpd_req_async_handler_complete(async_req);
        return ESP_ERR_NO_MEM;
    }
    client->sockfd = -1;
    snprintf(client->session_id, sizeof(client->session_id), "%s", session_id);
    strncpy(client->protocol_version, proto_hdr, sizeof(client->protocol_version) - 1);
    client->req = async_req;
    client->owner = mcp_http;
    client->sockfd = httpd_req_to_sockfd(async_req);

    if (last_event_id[0]) {
        uint32_t stream_id = 0;
        uint32_t event_seq = 0;
        if (esp_mcp_http_sse_parse_event_id(last_event_id, &stream_id, &event_seq)) {
            client->stream_id = stream_id;
            client->next_event_seq = event_seq + 1;
            client->resuming_stream = true;
            if (mcp_http->sse_mutex &&
                    xSemaphoreTake(mcp_http->sse_mutex, portMAX_DELAY) == pdTRUE) {
                esp_mcp_http_session_t *session = esp_mcp_http_session_find_locked(mcp_http, session_id);
                if (session &&
                        session->resume_stream_id == stream_id &&
                        session->resume_next_event_seq > client->next_event_seq) {
                    client->next_event_seq = session->resume_next_event_seq;
                }
                xSemaphoreGive(mcp_http->sse_mutex);
            }
        }
    }

    esp_err_t add_ret = esp_mcp_http_sse_add_client(mcp_http, client);
    if (add_ret != ESP_OK) {
        free(client);
        (void)httpd_req_async_handler_complete(async_req);
        return add_ret;
    }

    if (last_event_id[0] && mcp_http->sse_mutex && xSemaphoreTake(mcp_http->sse_mutex, portMAX_DELAY) == pdTRUE) {
        uint32_t stream_id = 0;
        uint32_t event_seq = 0;
        if (esp_mcp_http_sse_parse_event_id(last_event_id, &stream_id, &event_seq)) {
            esp_mcp_http_sse_replay_to_client_locked(mcp_http, client, stream_id, event_seq);
            esp_mcp_http_session_t *session = esp_mcp_http_session_find_locked(mcp_http, session_id);
            if (session) {
                esp_mcp_http_session_select_stream_locked(session, client->stream_id, client->next_event_seq);
            }
        }
        xSemaphoreGive(mcp_http->sse_mutex);
    }

    if (xTaskCreate(esp_mcp_http_sse_client_task, "mcp_sse", SSE_TASK_STACK_SIZE, client, SSE_TASK_PRIORITY, &client->task_handle) != pdPASS) {
        esp_err_t rm_ret = esp_mcp_http_sse_remove_client(mcp_http, client);
        if (rm_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to remove SSE client on task-create failure: %s", esp_err_to_name(rm_ret));
        }
        (void)httpd_req_async_handler_complete(async_req);
        free(client);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
#endif
}

#if CONFIG_MCP_HTTP_OAUTH_METADATA_ENABLE
static bool esp_mcp_http_add_string_array(cJSON *root,
                                          const char *key,
                                          const char *const *values,
                                          size_t value_count)
{
    if (!(root && key && values)) {
        return false;
    }

    cJSON *arr = cJSON_CreateArray();
    if (!arr) {
        return false;
    }

    for (size_t i = 0; i < value_count; ++i) {
        cJSON *item = cJSON_CreateString(values[i]);
        if (!item) {
            cJSON_Delete(arr);
            return false;
        }
        cJSON_AddItemToArray(arr, item);
    }

    cJSON_AddItemToObject(root, key, arr);
    return true;
}

static esp_err_t esp_mcp_http_send_json_cjson(httpd_req_t *req, cJSON *root)
{
    ESP_RETURN_ON_FALSE(req && root, ESP_ERR_INVALID_ARG, TAG, "Invalid args");

    httpd_resp_set_type(req, "application/json");
    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    ESP_RETURN_ON_FALSE(payload, ESP_ERR_NO_MEM, TAG, "No mem");
    esp_err_t ret = httpd_resp_send(req, payload, HTTPD_RESP_USE_STRLEN);
    free(payload);
    return ret;
}

static esp_err_t esp_mcp_http_oauth_protected_resource_handler(httpd_req_t *req)
{
    ESP_RETURN_ON_FALSE(req, ESP_ERR_INVALID_ARG, TAG, "Invalid request");
    char proto_hdr[32] = {0};
    ESP_RETURN_ON_ERROR(esp_mcp_http_apply_protocol_version(req, NULL, proto_hdr, sizeof(proto_hdr)),
                        TAG,
                        "Protocol header validation failed");
    ESP_RETURN_ON_ERROR(esp_mcp_http_validate_headers(req), TAG, "Header validation failed");

    cJSON *root = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(root, ESP_ERR_NO_MEM, TAG, "No mem");

    const char *auth_servers[] = {
        CONFIG_MCP_HTTP_OAUTH_AUTH_SERVER,
    };
    const char *bearer_methods[] = {
        "header",
    };

    if (!cJSON_AddStringToObject(root, "resource", CONFIG_MCP_HTTP_OAUTH_RESOURCE) ||
            !esp_mcp_http_add_string_array(root,
                                           "authorization_servers",
                                           auth_servers,
                                           sizeof(auth_servers) / sizeof(auth_servers[0])) ||
            !esp_mcp_http_add_string_array(root,
                                           "bearer_methods_supported",
                                           bearer_methods,
                                           sizeof(bearer_methods) / sizeof(bearer_methods[0]))) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }

#ifdef CONFIG_MCP_HTTP_AUTH_REQUIRED_SCOPE
    if (CONFIG_MCP_HTTP_AUTH_REQUIRED_SCOPE[0] != '\0') {
        const char *scopes[] = {
            CONFIG_MCP_HTTP_AUTH_REQUIRED_SCOPE,
        };
        if (!esp_mcp_http_add_string_array(root,
                                           "scopes_supported",
                                           scopes,
                                           sizeof(scopes) / sizeof(scopes[0]))) {
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
    }
#endif

    return esp_mcp_http_send_json_cjson(req, root);
}

static esp_err_t esp_mcp_http_oauth_authorization_server_handler(httpd_req_t *req)
{
    ESP_RETURN_ON_FALSE(req, ESP_ERR_INVALID_ARG, TAG, "Invalid request");
    char proto_hdr[32] = {0};
    ESP_RETURN_ON_ERROR(esp_mcp_http_apply_protocol_version(req, NULL, proto_hdr, sizeof(proto_hdr)),
                        TAG,
                        "Protocol header validation failed");
    ESP_RETURN_ON_ERROR(esp_mcp_http_validate_headers(req), TAG, "Header validation failed");

    cJSON *root = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(root, ESP_ERR_NO_MEM, TAG, "No mem");

    const char *response_types[] = {
        "code",
    };
    const char *grant_types[] = {
        "authorization_code",
        "refresh_token",
    };
    const char *token_auth_methods[] = {
        "none",
    };
    const char *pkce_methods[] = {
        "S256",
    };

    if (!cJSON_AddStringToObject(root, "issuer", CONFIG_MCP_HTTP_OAUTH_ISSUER) ||
            !cJSON_AddStringToObject(root, "authorization_endpoint", CONFIG_MCP_HTTP_OAUTH_AUTHORIZATION_ENDPOINT) ||
            !cJSON_AddStringToObject(root, "token_endpoint", CONFIG_MCP_HTTP_OAUTH_TOKEN_ENDPOINT) ||
            !cJSON_AddStringToObject(root, "registration_endpoint", CONFIG_MCP_HTTP_OAUTH_REGISTRATION_ENDPOINT) ||
            !cJSON_AddStringToObject(root, "jwks_uri", CONFIG_MCP_HTTP_OAUTH_JWKS_URI) ||
            !esp_mcp_http_add_string_array(root,
                                           "response_types_supported",
                                           response_types,
                                           sizeof(response_types) / sizeof(response_types[0])) ||
            !esp_mcp_http_add_string_array(root,
                                           "grant_types_supported",
                                           grant_types,
                                           sizeof(grant_types) / sizeof(grant_types[0])) ||
            !esp_mcp_http_add_string_array(root,
                                           "token_endpoint_auth_methods_supported",
                                           token_auth_methods,
                                           sizeof(token_auth_methods) / sizeof(token_auth_methods[0])) ||
            !esp_mcp_http_add_string_array(root,
                                           "code_challenge_methods_supported",
                                           pkce_methods,
                                           sizeof(pkce_methods) / sizeof(pkce_methods[0]))) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }

    return esp_mcp_http_send_json_cjson(req, root);
}

static char *esp_mcp_http_build_oauth_metadata_inserted_uri(const char *well_known_path, const char *ep_name)
{
    if (!(well_known_path && ep_name && ep_name[0])) {
        return NULL;
    }

    size_t uri_len = strlen(well_known_path) + strlen(ep_name) + 2;
    char *uri = calloc(1, uri_len);
    if (!uri) {
        return NULL;
    }

    snprintf(uri, uri_len, "%s/%s", well_known_path, ep_name);
    return uri;
}

static char *esp_mcp_http_build_oauth_metadata_appended_uri(const char *well_known_path, const char *ep_name)
{
    if (!(well_known_path && ep_name && ep_name[0])) {
        return NULL;
    }

    size_t uri_len = strlen(well_known_path) + strlen(ep_name) + 2;
    char *uri = calloc(1, uri_len);
    if (!uri) {
        return NULL;
    }

    snprintf(uri, uri_len, "/%s%s", ep_name, well_known_path);
    return uri;
}

static esp_err_t esp_mcp_http_register_oauth_metadata_endpoint_variants(esp_mcp_http_server_item_t *mcp_http,
                                                                        const char *ep_name)
{
    ESP_RETURN_ON_FALSE(mcp_http && mcp_http->httpd && ep_name && ep_name[0],
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "Invalid args");

    char *resource_inserted =
        esp_mcp_http_build_oauth_metadata_inserted_uri(ESP_MCP_HTTP_OAUTH_PROTECTED_RESOURCE_WELL_KNOWN, ep_name);
    char *resource_appended =
        esp_mcp_http_build_oauth_metadata_appended_uri(ESP_MCP_HTTP_OAUTH_PROTECTED_RESOURCE_WELL_KNOWN, ep_name);
    char *auth_inserted =
        esp_mcp_http_build_oauth_metadata_inserted_uri(ESP_MCP_HTTP_OAUTH_AUTHORIZATION_SERVER_WELL_KNOWN, ep_name);
    char *auth_appended =
        esp_mcp_http_build_oauth_metadata_appended_uri(ESP_MCP_HTTP_OAUTH_AUTHORIZATION_SERVER_WELL_KNOWN, ep_name);

    if (!(resource_inserted && resource_appended && auth_inserted && auth_appended)) {
        free(resource_inserted);
        free(resource_appended);
        free(auth_inserted);
        free(auth_appended);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;
    httpd_uri_t handlers[] = {
        {
            .uri = resource_inserted,
            .method = HTTP_GET,
            .handler = esp_mcp_http_oauth_protected_resource_handler,
            .user_ctx = mcp_http,
        },
        {
            .uri = resource_appended,
            .method = HTTP_GET,
            .handler = esp_mcp_http_oauth_protected_resource_handler,
            .user_ctx = mcp_http,
        },
        {
            .uri = auth_inserted,
            .method = HTTP_GET,
            .handler = esp_mcp_http_oauth_authorization_server_handler,
            .user_ctx = mcp_http,
        },
        {
            .uri = auth_appended,
            .method = HTTP_GET,
            .handler = esp_mcp_http_oauth_authorization_server_handler,
            .user_ctx = mcp_http,
        },
    };

    for (size_t i = 0; i < sizeof(handlers) / sizeof(handlers[0]); ++i) {
        ret = httpd_register_uri_handler(mcp_http->httpd, &handlers[i]);
        if (ret != ESP_OK) {
            for (size_t j = 0; j < i; ++j) {
                (void)httpd_unregister_uri_handler(mcp_http->httpd, handlers[j].uri, handlers[j].method);
            }
            break;
        }
    }

    free(resource_inserted);
    free(resource_appended);
    free(auth_inserted);
    free(auth_appended);
    return ret;
}

static esp_err_t esp_mcp_http_unregister_oauth_metadata_endpoint_variants(esp_mcp_http_server_item_t *mcp_http,
                                                                          const char *ep_name)
{
    ESP_RETURN_ON_FALSE(mcp_http && mcp_http->httpd && ep_name && ep_name[0],
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "Invalid args");

    char *resource_inserted =
        esp_mcp_http_build_oauth_metadata_inserted_uri(ESP_MCP_HTTP_OAUTH_PROTECTED_RESOURCE_WELL_KNOWN, ep_name);
    char *resource_appended =
        esp_mcp_http_build_oauth_metadata_appended_uri(ESP_MCP_HTTP_OAUTH_PROTECTED_RESOURCE_WELL_KNOWN, ep_name);
    char *auth_inserted =
        esp_mcp_http_build_oauth_metadata_inserted_uri(ESP_MCP_HTTP_OAUTH_AUTHORIZATION_SERVER_WELL_KNOWN, ep_name);
    char *auth_appended =
        esp_mcp_http_build_oauth_metadata_appended_uri(ESP_MCP_HTTP_OAUTH_AUTHORIZATION_SERVER_WELL_KNOWN, ep_name);

    if (!(resource_inserted && resource_appended && auth_inserted && auth_appended)) {
        free(resource_inserted);
        free(resource_appended);
        free(auth_inserted);
        free(auth_appended);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;
    const char *uris[] = {
        resource_inserted,
        resource_appended,
        auth_inserted,
        auth_appended,
    };

    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); ++i) {
        esp_err_t one_ret = httpd_unregister_uri(mcp_http->httpd, uris[i]);
        if (one_ret != ESP_OK && one_ret != ESP_ERR_NOT_FOUND && ret == ESP_OK) {
            ret = one_ret;
        }
    }

    free(resource_inserted);
    free(resource_appended);
    free(auth_inserted);
    free(auth_appended);
    return ret;
}
#endif

static esp_err_t esp_mcp_http_post_handler_internal(httpd_req_t *req, bool sse_only)
{
    ESP_RETURN_ON_FALSE(req, ESP_ERR_INVALID_ARG, TAG, "Invalid request");
    ESP_RETURN_ON_FALSE(req->user_ctx, ESP_ERR_INVALID_ARG, TAG, "Invalid user context");
    esp_mcp_http_server_item_t *mcp_http = (esp_mcp_http_server_item_t *)req->user_ctx;
    char proto_hdr[32] = {0};
    ESP_RETURN_ON_ERROR(esp_mcp_http_apply_protocol_version(req, mcp_http, proto_hdr, sizeof(proto_hdr)),
                        TAG,
                        "Protocol header validation failed");
    ESP_RETURN_ON_ERROR(httpd_resp_set_hdr(req, "MCP-Protocol-Version", proto_hdr), TAG, "Protocol header set failed");
    ESP_RETURN_ON_ERROR(esp_mcp_http_validate_headers(req), TAG, "Header validation failed");
    if (!esp_mcp_http_header_contains(req, "Content-Type", "application/json")) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content-Type must include application/json");
        return ESP_ERR_INVALID_ARG;
    }
    bool accept_json = esp_mcp_http_header_contains(req, "Accept", "application/json");
    bool accept_sse = esp_mcp_http_header_contains(req, "Accept", "text/event-stream");
    bool has_accept = httpd_req_get_hdr_value_len(req, "Accept") > 0;
    if (sse_only) {
        if (!accept_json && !accept_sse) {
            if (!has_accept) {
                accept_json = true;
            } else {
                (void)esp_mcp_http_send_error(req,
                                              ESP_MCP_HTTP_ERROR_406,
                                              "Accept must include application/json or text/event-stream");
                return ESP_ERR_INVALID_ARG;
            }
        }
    } else {
#if !CONFIG_MCP_HTTP_SSE_ENABLE
        /* JSON-only transport build: clients may send Accept: application/json (typical curl/SDK). */
        if (has_accept && !accept_json) {
            (void)esp_mcp_http_send_error(req,
                                          ESP_MCP_HTTP_ERROR_406,
                                          "Accept must include application/json");
            return ESP_ERR_INVALID_ARG;
        }
#else
        /* Streamable HTTP: allow JSON-only, SSE-only (initialize upgrade), or both; 406 if neither. */
        if (has_accept && !accept_json && !accept_sse) {
            (void)esp_mcp_http_send_error(req,
                                          ESP_MCP_HTTP_ERROR_406,
                                          "Accept must include application/json and/or text/event-stream");
            return ESP_ERR_INVALID_ARG;
        }
#endif
    }

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    int content_len = req->content_len;
    size_t total_len = 0;
    size_t cur_len = 0;
    int recv_len = 0;
    esp_err_t ret = ESP_OK;
    char session_id[SESSION_ID_LEN] = {0};
    bool session_provided = esp_mcp_http_extract_session_id(req, session_id);
    if (sse_only && !session_provided) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "MCP-Session-Id or sessionId query required");
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(esp_mcp_http_resolve_session_id(req, mcp_http, session_id), TAG, "Session id resolve failed");
    esp_mcp_http_session_update_protocol_version(mcp_http, session_id, proto_hdr);
    bool is_authenticated = false;
    char auth_subject[AUTH_SUBJECT_LEN] = {0};
    ESP_RETURN_ON_ERROR(esp_mcp_http_check_authorization(req, &is_authenticated, auth_subject), TAG, "Auth failed");

    if (content_len < 0) {
        ESP_LOGE(TAG, "Invalid content length: %d", content_len);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid content length");
        return ESP_ERR_INVALID_ARG;
    }
    total_len = (size_t)content_len;

    if (total_len > (size_t)MAX_CONTENT_LEN) {
        ESP_LOGE(TAG, "Content length too large: %u (max: %d)", (unsigned)total_len, MAX_CONTENT_LEN);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content length too large");
        return ESP_ERR_INVALID_SIZE;
    }
    if (total_len > UINT16_MAX) {
        ESP_LOGE(TAG, "Content length exceeds MCP payload limit: %u", (unsigned)total_len);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content length exceeds MCP payload limit");
        return ESP_ERR_INVALID_SIZE;
    }

    if (total_len > (SIZE_MAX - 1)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid content length");
        return ESP_ERR_INVALID_SIZE;
    }

    char *mbuf = calloc(1, total_len + 1);
    ESP_RETURN_ON_FALSE(mbuf, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for message buffer");

    int timeout_count = 0;
    while (cur_len < total_len) {
        recv_len = httpd_req_recv(req, mbuf + cur_len, (size_t)(total_len - cur_len));
        if (recv_len <= 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                timeout_count++;
                if (timeout_count >= MAX_TIMEOUT_COUNT) {
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive timeout exceeded");
                    free(mbuf);
                    ESP_LOGE(TAG, "Receive timeout exceeded after %d consecutive timeouts", timeout_count);
                    return ESP_FAIL;
                }
                continue;
            }
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
            free(mbuf);
            ESP_LOGE(TAG, "Failed to receive data, error: %d", recv_len);
            return ESP_FAIL;
        }
        timeout_count = 0;
        cur_len += recv_len;
    }
    mbuf[total_len] = '\0';

#if CONFIG_MCP_HTTP_SSE_ENABLE
    bool is_initialize = esp_mcp_http_is_initialize_request(mbuf);
    /* POST with Accept: text/event-stream only is reserved for initialize → SSE upgrade. */
    if (!sse_only && has_accept && !accept_json && accept_sse && !is_initialize) {
        (void)esp_mcp_http_send_error(req,
                                      ESP_MCP_HTTP_ERROR_406,
                                      "Accept must include application/json for non-initialize requests");
        free(mbuf);
        return ESP_ERR_INVALID_ARG;
    }
    /* Prefer JSON when Accept lists application/json or a full wildcard. */
    bool sse_requested = accept_sse && !accept_json;
#endif

    char endpoint_buf[CONFIG_HTTPD_MAX_URI_LEN + 1] = {0};
    size_t uri_len = strnlen(req->uri, sizeof(endpoint_buf) - 1);
    const char *qmark = memchr(req->uri, '?', uri_len);
    size_t path_len = qmark ? (size_t)(qmark - req->uri) : uri_len;
    if (path_len >= sizeof(endpoint_buf)) {
        path_len = sizeof(endpoint_buf) - 1;
    }
    memcpy(endpoint_buf, req->uri, path_len);
    endpoint_buf[path_len] = '\0';
    if (sse_only && path_len > 4 && strcmp(endpoint_buf + path_len - 4, "_sse") == 0) {
        endpoint_buf[path_len - 4] = '\0';
    }
    const char *endpoint_name = endpoint_buf;
    if (endpoint_buf[0] == '/') {
        endpoint_name = endpoint_buf + 1;
    }
    ESP_LOGI(TAG, "POST endpoint resolved: uri=%s endpoint=%s", req->uri, endpoint_name);

    (void)esp_mcp_mgr_set_request_context(mcp_http->handle, session_id, auth_subject, is_authenticated);
    esp_mcp_mgr_req_handle(mcp_http->handle, endpoint_name, (const uint8_t *)mbuf, (uint16_t)total_len, &outbuf, &outlen);
    (void)esp_mcp_mgr_set_request_context(mcp_http->handle, NULL, NULL, false);
    free(mbuf);

    if (outbuf && outlen > 0) {
        char negotiated_proto[PROTOCOL_VERSION_LEN] = {0};
        if (esp_mcp_http_extract_result_protocol_version((const char *)outbuf, negotiated_proto)) {
            strlcpy(proto_hdr, negotiated_proto, sizeof(proto_hdr));
            esp_mcp_http_session_update_protocol_version(mcp_http, session_id, negotiated_proto);
            esp_err_t hdr_ret = httpd_resp_set_hdr(req, "MCP-Protocol-Version", proto_hdr);
            if (hdr_ret != ESP_OK) {
                esp_mcp_mgr_req_destroy_response(mcp_http->handle, outbuf);
                ESP_RETURN_ON_ERROR(hdr_ret, TAG, "Protocol header update failed");
            }
        }
    }

#if CONFIG_MCP_HTTP_SSE_ENABLE
    // If SSE is requested and supported, switch to SSE streaming mode
    if (!sse_only && sse_requested && is_initialize && outbuf && outlen > 0) {
        esp_mcp_sse_client_t *client = calloc(1, sizeof(esp_mcp_sse_client_t));
        if (!client) {
            esp_mcp_mgr_req_destroy_response(mcp_http->handle, outbuf);
            ESP_LOGE(TAG, "Failed to allocate SSE client");
            return ESP_ERR_NO_MEM;
        }

        strncpy(client->session_id, session_id, sizeof(client->session_id) - 1);
        client->session_id[sizeof(client->session_id) - 1] = '\0';
        strncpy(client->protocol_version, proto_hdr, sizeof(client->protocol_version) - 1);
        client->protocol_version[sizeof(client->protocol_version) - 1] = '\0';

        esp_mcp_http_clear_pending_response_hdrs(req);
        ret = esp_mcp_http_validate_headers(req);
        if (ret != ESP_OK) {
            free(client);
            esp_mcp_mgr_req_destroy_response(mcp_http->handle, outbuf);
            return ret;
        }
        ret = httpd_resp_set_hdr(req, "MCP-Protocol-Version", client->protocol_version);
        if (ret != ESP_OK) {
            free(client);
            esp_mcp_mgr_req_destroy_response(mcp_http->handle, outbuf);
            ESP_RETURN_ON_ERROR(ret, TAG, "MCP-Protocol-Version rebuild failed");
        }

        httpd_req_t *async_req = NULL;
        ret = httpd_req_async_handler_begin(req, &async_req);
        if (ret != ESP_OK || !async_req) {
            free(client);
            esp_mcp_mgr_req_destroy_response(mcp_http->handle, outbuf);
            (void)esp_mcp_http_send_error(req, ESP_MCP_HTTP_ERROR_503, "Failed to start async SSE");
            return ESP_FAIL;
        }

        client->req = async_req;
        client->owner = mcp_http;
        client->head = NULL;
        client->tail = NULL;
        client->depth = 0;
        client->sockfd = httpd_req_to_sockfd(async_req);

        if (outbuf && outlen > 0) {
            esp_mcp_sse_msg_t *msg = calloc(1, sizeof(esp_mcp_sse_msg_t));
            if (!msg) {
                esp_mcp_mgr_req_destroy_response(mcp_http->handle, outbuf);
                free(client);
                (void)httpd_req_async_handler_complete(async_req);
                return ESP_ERR_NO_MEM;
            }
            msg->jsonrpc_message = strdup((char *)outbuf);
            if (!msg->jsonrpc_message) {
                esp_mcp_http_sse_msg_free(msg);
                esp_mcp_mgr_req_destroy_response(mcp_http->handle, outbuf);
                free(client);
                (void)httpd_req_async_handler_complete(async_req);
                return ESP_ERR_NO_MEM;
            }
            client->head = msg;
            client->tail = msg;
            client->depth = 1;
        }

        ret = esp_mcp_http_sse_add_client(mcp_http, client);
        if (ret != ESP_OK) {
            esp_mcp_http_sse_client_drain(client);
            esp_mcp_mgr_req_destroy_response(mcp_http->handle, outbuf);
            free(client);
            (void)httpd_req_async_handler_complete(async_req);
            return ret;
        }
        esp_mcp_mgr_req_destroy_response(mcp_http->handle, outbuf);

        char task_name[32];
        snprintf(task_name, sizeof(task_name), "mcp_sse_post_%d", client->sockfd);

        BaseType_t task_ret = xTaskCreate(
                                  esp_mcp_http_sse_client_task,
                                  task_name,
                                  SSE_TASK_STACK_SIZE,
                                  client,
                                  SSE_TASK_PRIORITY,
                                  &client->task_handle
                              );

        if (task_ret != pdPASS) {
            ESP_LOGE(TAG, "Failed to create SSE task");
            esp_mcp_http_sse_remove_client(mcp_http, client);
            (void)httpd_req_async_handler_complete(async_req);
            free(client);
            return ESP_FAIL;
        }

        return ESP_OK;
    }
#endif

    if (sse_only) {
        if (outbuf && outlen > 0) {
            esp_err_t emit_ret = esp_mcp_http_server_emit_message((esp_mcp_transport_handle_t)mcp_http,
                                                                  session_id,
                                                                  (char *)outbuf);
            esp_mcp_mgr_req_destroy_response(mcp_http->handle, outbuf);
            if (emit_ret != ESP_OK) {
                httpd_resp_set_status(req, ESP_MCP_HTTPD_503);
                httpd_resp_set_type(req, "text/plain");
                return httpd_resp_send(req, "No active SSE client", HTTPD_RESP_USE_STRLEN);
            }
        } else if (outbuf) {
            esp_mcp_mgr_req_destroy_response(mcp_http->handle, outbuf);
        }
        httpd_resp_set_status(req, ESP_MCP_HTTPD_202);
        httpd_resp_set_hdr(req, "MCP-Session-Id", session_id);
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, NULL, 0);
    }

    // Standard JSON response mode
    if (outbuf && outlen > 0) {
        httpd_resp_set_hdr(req, "MCP-Session-Id", session_id);
        httpd_resp_set_hdr(req, "Connection", "keep-alive");
        httpd_resp_set_type(req, "application/json");
        ret = httpd_resp_send(req, (char *)outbuf, outlen);
        esp_mcp_mgr_req_destroy_response(mcp_http->handle, outbuf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send response: %s", esp_err_to_name(ret));
            return ESP_FAIL;
        }
    } else {
        if (outbuf) {
            esp_mcp_mgr_req_destroy_response(mcp_http->handle, outbuf);
        }
        httpd_resp_set_status(req, ESP_MCP_HTTPD_202);
        httpd_resp_set_hdr(req, "MCP-Session-Id", session_id);
        httpd_resp_set_type(req, "application/json");
        ret = httpd_resp_send(req, NULL, 0);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_FAIL, TAG, "Failed to send empty response %s", esp_err_to_name(ret));
    }

    return ESP_OK;
}

static esp_err_t esp_mcp_http_post_handler(httpd_req_t *req)
{
    return esp_mcp_http_post_handler_internal(req, false);
}

#if CONFIG_MCP_HTTP_SSE_ENABLE
static esp_err_t esp_mcp_http_post_sse_only_handler(httpd_req_t *req)
{
    return esp_mcp_http_post_handler_internal(req, true);
}
#endif

static esp_err_t esp_mcp_http_delete_handler(httpd_req_t *req)
{
    ESP_RETURN_ON_FALSE(req, ESP_ERR_INVALID_ARG, TAG, "Invalid request");
    ESP_RETURN_ON_FALSE(req->user_ctx, ESP_ERR_INVALID_ARG, TAG, "Invalid user context");
    esp_mcp_http_server_item_t *mcp_http = (esp_mcp_http_server_item_t *)req->user_ctx;
    char proto_hdr[32] = {0};
    ESP_RETURN_ON_ERROR(esp_mcp_http_apply_protocol_version(req, mcp_http, proto_hdr, sizeof(proto_hdr)),
                        TAG,
                        "Protocol header validation failed");
    ESP_RETURN_ON_ERROR(httpd_resp_set_hdr(req, "MCP-Protocol-Version", proto_hdr), TAG, "Protocol header set failed");
    ESP_RETURN_ON_ERROR(esp_mcp_http_validate_headers(req), TAG, "Header validation failed");
    bool is_authenticated = false;
    char auth_subject[AUTH_SUBJECT_LEN] = {0};
    ESP_RETURN_ON_ERROR(esp_mcp_http_check_authorization(req, &is_authenticated, auth_subject), TAG, "Auth failed");
    (void)is_authenticated;
    (void)auth_subject;

    char session_id[SESSION_ID_LEN] = {0};
    if (!esp_mcp_http_extract_session_id(req, session_id)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "MCP-Session-Id required");
        return ESP_ERR_INVALID_ARG;
    }

    if (!mcp_http->sse_mutex || xSemaphoreTake(mcp_http->sse_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }
    bool removed = esp_mcp_http_session_remove_locked(mcp_http, session_id);
    if (removed) {
        (void)esp_mcp_mgr_clear_session_state(mcp_http->handle, session_id);
        esp_mcp_sse_hist_msg_t **pp_hist = &mcp_http->sse_history_head;
        mcp_http->sse_history_tail = NULL;
        while (*pp_hist) {
            if (strcmp((*pp_hist)->session_id, session_id) == 0) {
                esp_mcp_sse_hist_msg_t *drop = *pp_hist;
                *pp_hist = drop->next;
                if (mcp_http->sse_history_depth > 0) {
                    mcp_http->sse_history_depth--;
                }
                esp_mcp_http_sse_hist_msg_free(drop);
                continue;
            }
            mcp_http->sse_history_tail = *pp_hist;
            pp_hist = &(*pp_hist)->next;
        }
        for (esp_mcp_sse_client_t *client = mcp_http->sse_clients; client; client = client->next) {
            if (strcmp(client->session_id, session_id) == 0 && client->sockfd >= 0) {
                (void)httpd_sess_trigger_close(mcp_http->httpd, client->sockfd);
            }
        }
    }
    xSemaphoreGive(mcp_http->sse_mutex);

    if (!removed) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Unknown MCP session");
        return ESP_ERR_NOT_FOUND;
    }

    httpd_resp_set_status(req, HTTPD_200);
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t esp_mcp_http_server_init(esp_mcp_mgr_handle_t handle, esp_mcp_transport_handle_t *transport_handle)
{
    ESP_RETURN_ON_FALSE(transport_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid transport handle pointer");

    esp_mcp_http_server_item_t *mcp_http = calloc(1, sizeof(esp_mcp_http_server_item_t));
    ESP_RETURN_ON_FALSE(mcp_http, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for HTTP item");

    mcp_http->handle = handle;
    mcp_http->sse_mutex = xSemaphoreCreateMutex();
    if (!mcp_http->sse_mutex) {
        free(mcp_http);
        return ESP_ERR_NO_MEM;
    }
    *transport_handle = (esp_mcp_transport_handle_t)mcp_http;

    return ESP_OK;
}

static esp_err_t esp_mcp_http_server_deinit(esp_mcp_transport_handle_t handle)
{
    esp_mcp_http_server_item_t *mcp_http = (esp_mcp_http_server_item_t *)handle;
    ESP_RETURN_ON_FALSE(mcp_http, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

#if CONFIG_MCP_HTTP_SSE_ENABLE
    mcp_http->sse_teardown = true;
    if (mcp_http->sse_mutex && xSemaphoreTake(mcp_http->sse_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        for (esp_mcp_sse_client_t *c = mcp_http->sse_clients; c; c = c->next) {
            if (c->sockfd >= 0 && mcp_http->httpd) {
                (void)httpd_sess_trigger_close(mcp_http->httpd, c->sockfd);
            }
        }
        xSemaphoreGive(mcp_http->sse_mutex);
        for (int i = 0; i < 100; i++) {
            bool drained = false;
            if (xSemaphoreTake(mcp_http->sse_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                drained = (mcp_http->sse_clients == NULL);
                xSemaphoreGive(mcp_http->sse_mutex);
            }
            if (drained) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
#endif

    if (mcp_http->sse_mutex && xSemaphoreTake(mcp_http->sse_mutex, portMAX_DELAY) == pdTRUE) {
        esp_mcp_sse_client_t *client = mcp_http->sse_clients;
        while (client) {
            esp_mcp_sse_client_t *next = client->next;
            esp_mcp_http_sse_client_drain(client);
            free(client);
            client = next;
        }
        mcp_http->sse_clients = NULL;
        esp_mcp_sse_hist_msg_t *hist = mcp_http->sse_history_head;
        while (hist) {
            esp_mcp_sse_hist_msg_t *next = hist->next;
            esp_mcp_http_sse_hist_msg_free(hist);
            hist = next;
        }
        mcp_http->sse_history_head = NULL;
        mcp_http->sse_history_tail = NULL;
        mcp_http->sse_history_depth = 0;
        esp_mcp_http_session_t *sess = mcp_http->sessions;
        while (sess) {
            esp_mcp_http_session_t *next = sess->next;
            free(sess);
            sess = next;
        }
        mcp_http->sessions = NULL;
        xSemaphoreGive(mcp_http->sse_mutex);
    } else {
        /* Sessions are used outside SSE paths; free them if SSE mutex was not taken. */
        esp_mcp_http_session_t *sess = mcp_http->sessions;
        while (sess) {
            esp_mcp_http_session_t *next_sess = sess->next;
            free(sess);
            sess = next_sess;
        }
        mcp_http->sessions = NULL;
    }
    if (mcp_http->sse_mutex) {
        vSemaphoreDelete(mcp_http->sse_mutex);
        mcp_http->sse_mutex = NULL;
    }
    mcp_http->handle = 0;
    free(mcp_http);

    return ESP_OK;
}

static esp_err_t esp_mcp_http_server_create_config(const void *config, void **config_out)
{
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Invalid configuration");
    ESP_RETURN_ON_FALSE(config_out, ESP_ERR_INVALID_ARG, TAG, "Invalid config output pointer");

    httpd_config_t *http_config = (httpd_config_t *)config;
    httpd_config_t *http_new_config = calloc(1, sizeof(httpd_config_t));
    ESP_RETURN_ON_FALSE(http_new_config, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for HTTP configuration");

    *http_new_config = *http_config;
#if CONFIG_MCP_HTTP_OAUTH_METADATA_ENABLE
    // OAuth metadata compatibility routes consume additional URI handler slots.
    // Bump the floor to avoid handler registration failure for common endpoint setups.
    if (http_new_config->max_uri_handlers < 16) {
        http_new_config->max_uri_handlers = 16;
    }
#endif
    *config_out = http_new_config;

    return ESP_OK;
}

static esp_err_t esp_mcp_http_server_delete_config(void *config)
{
    httpd_config_t *http_config = (httpd_config_t *)config;
    ESP_RETURN_ON_FALSE(http_config, ESP_ERR_INVALID_ARG, TAG, "Invalid configuration");

    free(http_config);

    return ESP_OK;
}

static esp_err_t esp_mcp_http_server_start(esp_mcp_transport_handle_t handle, void *config)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Invalid configuration");

    esp_mcp_http_server_item_t *mcp_http = (esp_mcp_http_server_item_t *)handle;
    httpd_config_t *http_config = (httpd_config_t *)config;

    esp_err_t ret = httpd_start(&mcp_http->httpd, http_config);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_FAIL, TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));

#if CONFIG_MCP_HTTP_OAUTH_METADATA_ENABLE
    httpd_uri_t metadata_handler = {
        .uri = ESP_MCP_HTTP_OAUTH_PROTECTED_RESOURCE_WELL_KNOWN,
        .method = HTTP_GET,
        .handler = esp_mcp_http_oauth_protected_resource_handler,
        .user_ctx = mcp_http,
    };
    ret = httpd_register_uri_handler(mcp_http->httpd, &metadata_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "OAuth metadata handler register failed: %s", esp_err_to_name(ret));
        httpd_stop(mcp_http->httpd);
        return ret;
    }

    httpd_uri_t oauth_auth_server_handler = {
        .uri = ESP_MCP_HTTP_OAUTH_AUTHORIZATION_SERVER_WELL_KNOWN,
        .method = HTTP_GET,
        .handler = esp_mcp_http_oauth_authorization_server_handler,
        .user_ctx = mcp_http,
    };
    ret = httpd_register_uri_handler(mcp_http->httpd, &oauth_auth_server_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "OAuth auth server metadata handler register failed: %s", esp_err_to_name(ret));
        httpd_stop(mcp_http->httpd);
        return ret;
    }
#endif

    return ESP_OK;
}

static esp_err_t esp_mcp_http_server_stop(esp_mcp_transport_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_mcp_http_server_item_t *mcp_http = (esp_mcp_http_server_item_t *)handle;

#if CONFIG_MCP_HTTP_SSE_ENABLE
    mcp_http->sse_teardown = true;
    if (mcp_http->sse_mutex && xSemaphoreTake(mcp_http->sse_mutex, portMAX_DELAY) == pdTRUE) {
        for (esp_mcp_sse_client_t *client = mcp_http->sse_clients; client; client = client->next) {
            if (client->sockfd >= 0) {
                (void)httpd_sess_trigger_close(mcp_http->httpd, client->sockfd);
            }
        }
        xSemaphoreGive(mcp_http->sse_mutex);
    }

    // Give async SSE tasks a chance to observe socket close and self-cleanup.
    for (int i = 0; i < 100; i++) {
        bool empty = true;
        if (mcp_http->sse_mutex && xSemaphoreTake(mcp_http->sse_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            empty = (mcp_http->sse_clients == NULL);
            xSemaphoreGive(mcp_http->sse_mutex);
        }
        if (empty) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
#endif

    esp_err_t ret = httpd_stop(mcp_http->httpd);
    ESP_RETURN_ON_ERROR(ret, TAG, "HTTP server stop failed: %s", esp_err_to_name(ret));

    return ESP_OK;
}

static esp_err_t esp_mcp_http_server_register_endpoint(esp_mcp_transport_handle_t handle, const char *ep_name, void *priv_data)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(ep_name, ESP_ERR_INVALID_ARG, TAG, "Invalid endpoint name");

    esp_mcp_http_server_item_t *mcp_http = (esp_mcp_http_server_item_t *)handle;

    size_t ep_uri_len = strlen(ep_name) + 2;
    char *ep_uri = calloc(1, ep_uri_len);
    ESP_RETURN_ON_FALSE(ep_uri, ESP_ERR_NO_MEM, TAG, "Malloc failed for ep uri");

    snprintf(ep_uri, ep_uri_len, "/%s", ep_name);

    httpd_uri_t config_handler = {
        .uri      = ep_uri,
        .method   = HTTP_POST,
        .handler  = esp_mcp_http_post_handler,
        .user_ctx = mcp_http
    };

    httpd_uri_t get_handler = {
        .uri      = ep_uri,
        .method   = HTTP_GET,
        .handler  = esp_mcp_http_get_handler,
        .user_ctx = mcp_http
    };
    httpd_uri_t delete_handler = {
        .uri      = ep_uri,
        .method   = HTTP_DELETE,
        .handler  = esp_mcp_http_delete_handler,
        .user_ctx = mcp_http
    };
    httpd_uri_t head_handler = {
        .uri      = ep_uri,
        .method   = HTTP_HEAD,
        .handler  = esp_mcp_http_head_handler,
        .user_ctx = mcp_http
    };
#if CONFIG_MCP_HTTP_CORS_ENABLE
    httpd_uri_t options_handler = {
        .uri      = ep_uri,
        .method   = HTTP_OPTIONS,
        .handler  = esp_mcp_http_options_handler,
        .user_ctx = mcp_http
    };
#endif

#if CONFIG_MCP_HTTP_SSE_ENABLE
    size_t sse_ep_uri_len = strlen(ep_name) + 6;
    char *sse_ep_uri = calloc(1, sse_ep_uri_len);
    if (!sse_ep_uri) {
        free(ep_uri);
        ESP_LOGE(TAG, "Malloc failed for sse ep uri");
        return ESP_ERR_NO_MEM;
    }

    snprintf(sse_ep_uri, sse_ep_uri_len, "/%s_sse", ep_name);

    httpd_uri_t sse_post_handler = {
        .uri      = sse_ep_uri,
        .method   = HTTP_POST,
        .handler  = esp_mcp_http_post_sse_only_handler,
        .user_ctx = mcp_http
    };
    httpd_uri_t sse_get_handler = {
        .uri      = sse_ep_uri,
        .method   = HTTP_GET,
        .handler  = esp_mcp_http_get_handler,
        .user_ctx = mcp_http
    };
    httpd_uri_t sse_delete_handler = {
        .uri      = sse_ep_uri,
        .method   = HTTP_DELETE,
        .handler  = esp_mcp_http_delete_handler,
        .user_ctx = mcp_http
    };
    httpd_uri_t sse_head_handler = {
        .uri      = sse_ep_uri,
        .method   = HTTP_HEAD,
        .handler  = esp_mcp_http_head_handler,
        .user_ctx = mcp_http
    };
#if CONFIG_MCP_HTTP_CORS_ENABLE
    httpd_uri_t sse_options_handler = {
        .uri      = sse_ep_uri,
        .method   = HTTP_OPTIONS,
        .handler  = esp_mcp_http_options_handler,
        .user_ctx = mcp_http
    };
#endif
#endif

    httpd_uri_t *handlers[10] = {0};
    size_t handlers_count = 0;
    handlers[handlers_count++] = &config_handler;
    handlers[handlers_count++] = &get_handler;
    handlers[handlers_count++] = &delete_handler;
    handlers[handlers_count++] = &head_handler;
#if CONFIG_MCP_HTTP_CORS_ENABLE
    handlers[handlers_count++] = &options_handler;
#endif
#if CONFIG_MCP_HTTP_SSE_ENABLE
    handlers[handlers_count++] = &sse_post_handler;
    handlers[handlers_count++] = &sse_get_handler;
    handlers[handlers_count++] = &sse_delete_handler;
    handlers[handlers_count++] = &sse_head_handler;
#if CONFIG_MCP_HTTP_CORS_ENABLE
    handlers[handlers_count++] = &sse_options_handler;
#endif
#endif

    esp_err_t ret = ESP_OK;
    size_t registered_count = 0;
    for (size_t i = 0; i < handlers_count; ++i) {
        ret = httpd_register_uri_handler(mcp_http->httpd, handlers[i]);
        if (ret != ESP_OK) {
            break;
        }
        registered_count++;
    }

#if CONFIG_MCP_HTTP_OAUTH_METADATA_ENABLE
    if (ret == ESP_OK) {
        ret = esp_mcp_http_register_oauth_metadata_endpoint_variants(mcp_http, ep_name);
    }
#endif

    if (ret != ESP_OK) {
        for (size_t i = 0; i < registered_count; ++i) {
            (void)httpd_unregister_uri_handler(mcp_http->httpd, handlers[i]->uri, handlers[i]->method);
        }
#if CONFIG_MCP_HTTP_OAUTH_METADATA_ENABLE
        (void)esp_mcp_http_unregister_oauth_metadata_endpoint_variants(mcp_http, ep_name);
#endif
    }

#if CONFIG_MCP_HTTP_SSE_ENABLE
    free(sse_ep_uri);
#endif
    free(ep_uri);
    ESP_RETURN_ON_ERROR(ret, TAG, "Uri handler register failed: %s", esp_err_to_name(ret));

    return ESP_OK;
}

static esp_err_t esp_mcp_http_server_unregister_endpoint(esp_mcp_transport_handle_t handle, const char *ep_name)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(ep_name, ESP_ERR_INVALID_ARG, TAG, "Invalid endpoint name");

    esp_mcp_http_server_item_t *mcp_http = (esp_mcp_http_server_item_t *)handle;

    size_t ep_uri_len = strlen(ep_name) + 2;
    char *ep_uri = calloc(1, ep_uri_len);
    ESP_RETURN_ON_FALSE(ep_uri, ESP_ERR_NO_MEM, TAG, "Malloc failed for ep uri");

    snprintf(ep_uri, ep_uri_len, "/%s", ep_name);

    esp_err_t ret = httpd_unregister_uri(mcp_http->httpd, ep_uri);
    free(ep_uri);
#if CONFIG_MCP_HTTP_SSE_ENABLE
    size_t sse_ep_uri_len = strlen(ep_name) + 6;
    char *sse_ep_uri = calloc(1, sse_ep_uri_len);
    ESP_RETURN_ON_FALSE(sse_ep_uri, ESP_ERR_NO_MEM, TAG, "Malloc failed for sse ep uri");
    snprintf(sse_ep_uri, sse_ep_uri_len, "/%s_sse", ep_name);
    esp_err_t ret_sse = httpd_unregister_uri(mcp_http->httpd, sse_ep_uri);
    free(sse_ep_uri);
    if (ret == ESP_OK && ret_sse != ESP_OK && ret_sse != ESP_ERR_NOT_FOUND) {
        ret = ret_sse;
    }
#endif
#if CONFIG_MCP_HTTP_OAUTH_METADATA_ENABLE
    esp_err_t ret_oauth = esp_mcp_http_unregister_oauth_metadata_endpoint_variants(mcp_http, ep_name);
    if (ret == ESP_OK && ret_oauth != ESP_OK && ret_oauth != ESP_ERR_NOT_FOUND) {
        ret = ret_oauth;
    }
#endif
    ESP_RETURN_ON_ERROR(ret, TAG, "Uri handler unregister failed: %s", esp_err_to_name(ret));

    return ESP_OK;
}

static esp_err_t esp_mcp_http_server_emit_message(esp_mcp_transport_handle_t handle, const char *session_id, const char *jsonrpc_message)
{
    esp_mcp_http_server_item_t *mcp_http = (esp_mcp_http_server_item_t *)handle;
    ESP_RETURN_ON_FALSE(mcp_http && jsonrpc_message, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
#if !CONFIG_MCP_HTTP_SSE_ENABLE
    (void)session_id;
    ESP_LOGD(TAG, "SSE disabled, skip emit_message delivery");
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (!mcp_http->sse_mutex || xSemaphoreTake(mcp_http->sse_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }
    bool enqueued = false;
    esp_err_t emit_ret = ESP_ERR_NOT_FOUND;
    if (session_id && session_id[0]) {
        esp_mcp_http_session_t *session = esp_mcp_http_session_find_locked(mcp_http, session_id);
        if (session) {
            esp_mcp_sse_client_t *client = esp_mcp_http_sse_find_preferred_client_locked(mcp_http, session_id);
            char event_id[SSE_EVENT_ID_LEN] = {0};
            uint32_t stream_id = 0;
            uint32_t event_seq = 0;
            if (client) {
                emit_ret = esp_mcp_http_sse_reserve_client_event_locked(mcp_http,
                                                                        session,
                                                                        client,
                                                                        event_id,
                                                                        &stream_id,
                                                                        &event_seq);
                if (emit_ret == ESP_OK) {
                    emit_ret = esp_mcp_http_sse_enqueue_locked(client,
                                                               event_id,
                                                               stream_id,
                                                               event_seq,
                                                               jsonrpc_message);
                }
            } else {
                emit_ret = esp_mcp_http_sse_reserve_session_event_locked(mcp_http,
                                                                         session,
                                                                         event_id,
                                                                         &stream_id,
                                                                         &event_seq);
            }
            if (emit_ret == ESP_OK) {
                esp_mcp_http_sse_history_append_locked(mcp_http,
                                                       event_id,
                                                       session->session_id,
                                                       stream_id,
                                                       event_seq,
                                                       jsonrpc_message);
                enqueued = true;
            }
        }
    } else {
        for (esp_mcp_http_session_t *session = mcp_http->sessions; session; session = session->next) {
            esp_mcp_sse_client_t *client =
                esp_mcp_http_sse_find_preferred_client_locked(mcp_http, session->session_id);
            char event_id[SSE_EVENT_ID_LEN] = {0};
            uint32_t stream_id = 0;
            uint32_t event_seq = 0;
            esp_err_t one_ret = ESP_ERR_NOT_FOUND;
            if (client) {
                one_ret = esp_mcp_http_sse_reserve_client_event_locked(mcp_http,
                                                                       session,
                                                                       client,
                                                                       event_id,
                                                                       &stream_id,
                                                                       &event_seq);
                if (one_ret == ESP_OK) {
                    one_ret = esp_mcp_http_sse_enqueue_locked(client,
                                                              event_id,
                                                              stream_id,
                                                              event_seq,
                                                              jsonrpc_message);
                }
            } else {
                one_ret = esp_mcp_http_sse_reserve_session_event_locked(mcp_http,
                                                                        session,
                                                                        event_id,
                                                                        &stream_id,
                                                                        &event_seq);
            }
            if (one_ret == ESP_OK) {
                esp_mcp_http_sse_history_append_locked(mcp_http,
                                                       event_id,
                                                       session->session_id,
                                                       stream_id,
                                                       event_seq,
                                                       jsonrpc_message);
                enqueued = true;
            }
        }
    }
    xSemaphoreGive(mcp_http->sse_mutex);
    return enqueued ? ESP_OK : emit_ret;
#endif
}

const esp_mcp_transport_t esp_mcp_transport_http_server = {
    .init                = esp_mcp_http_server_init,
    .deinit              = esp_mcp_http_server_deinit,
    .create_config       = esp_mcp_http_server_create_config,
    .delete_config       = esp_mcp_http_server_delete_config,
    .start               = esp_mcp_http_server_start,
    .stop                = esp_mcp_http_server_stop,
    .register_endpoint   = esp_mcp_http_server_register_endpoint,
    .unregister_endpoint = esp_mcp_http_server_unregister_endpoint,
    .emit_message        = esp_mcp_http_server_emit_message,
};

// Deprecated alias for backward compatibility
__attribute__((deprecated("Use esp_mcp_transport_http_server or esp_mcp_transport_http_client instead")))
const esp_mcp_transport_t esp_mcp_transport_http = esp_mcp_transport_http_server;
