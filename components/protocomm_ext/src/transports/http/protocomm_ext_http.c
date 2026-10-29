/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_http_client.h>
#include <protocomm_ext_transports.h>
#include <protocomm_ext_http.h>

#define BASE_URL_MAX_LENGTH 128

static const char *TAG = "protocomm_ext_transport_http";

typedef struct {
    char *cookie;
    char base_url[BASE_URL_MAX_LENGTH];
    esp_http_client_handle_t http_client;
    http_event_handle_cb caller_event_handler;
    void *caller_user_data;
} protocomm_http_t;

static void http_store_session_cookie(protocomm_http_t *http, const char *set_cookie)
{
    if (!http || !set_cookie || !set_cookie[0]) {
        return;
    }
    /* Keep only "session=<id>"; drop attributes like Path=/ if present. */
    size_t n = strcspn(set_cookie, ";");
    while (n > 0 && (set_cookie[n - 1] == ' ' || set_cookie[n - 1] == '\t')) {
        n--;
    }
    char *copy = strndup(set_cookie, n);
    if (!copy) {
        return;
    }
    free(http->cookie);
    http->cookie = copy;
    ESP_LOGD(TAG, "Saved session cookie (%u bytes)", (unsigned)n);
}

/*
 * Set-Cookie arrives as a response header. esp_http_client_get_header() only
 * reads request headers, so we must capture it from HTTP_EVENT_ON_HEADER.
 * Without this cookie, each new TCP connection resets the protocomm security
 * session and Sec2 command1 fails with "Invalid state".
 *
 * Caller event_handler/user_data are preserved and invoked after cookie capture.
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    protocomm_http_t *http = (protocomm_http_t *)evt->user_data;
    if (!http) {
        return ESP_OK;
    }
    if (evt->event_id == HTTP_EVENT_ON_HEADER &&
            evt->header_key && evt->header_value &&
            strcasecmp(evt->header_key, "Set-Cookie") == 0) {
        http_store_session_cookie(http, evt->header_value);
    }
    if (http->caller_event_handler) {
        void *saved_user_data = evt->user_data;
        evt->user_data = http->caller_user_data;
        esp_err_t caller_ret = http->caller_event_handler(evt);
        evt->user_data = saved_user_data;
        return caller_ret;
    }
    return ESP_OK;
}

static esp_err_t http_init(protocomm_ext_transport_handle_t *handle, const void *config)
{
    if (!config) {
        ESP_LOGE(TAG, "Invalid config");
        return ESP_ERR_INVALID_ARG;
    }

    protocomm_http_t *http = (protocomm_http_t *)calloc(1, sizeof(protocomm_http_t));
    if (!http) {
        ESP_LOGE(TAG, "Failed to allocate http instance");
        return ESP_ERR_NO_MEM;
    }

    /* Copy caller config so we can install our event handler for Set-Cookie. */
    esp_http_client_config_t http_config = *(const esp_http_client_config_t *)config;
    http->caller_event_handler = http_config.event_handler;
    http->caller_user_data = http_config.user_data;
    http_config.event_handler = http_event_handler;
    http_config.user_data = http;

    http->http_client = esp_http_client_init(&http_config);
    if (!http->http_client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(http);
        return ESP_FAIL;
    }

    esp_err_t ret = esp_http_client_get_url(http->http_client, http->base_url, BASE_URL_MAX_LENGTH);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get URL");
        esp_http_client_cleanup(http->http_client);
        free(http);
        return ret;
    }

    /* esp_http_client may normalize to a trailing '/'; strip so "%s/%s" does not become "//ep". */
    size_t blen = strnlen(http->base_url, BASE_URL_MAX_LENGTH);
    while (blen > 0 && http->base_url[blen - 1] == '/') {
        http->base_url[--blen] = '\0';
    }

    ESP_LOGI(TAG, "HTTP transport initialized (base=%s)", http->base_url);
    *handle = (protocomm_ext_transport_handle_t)http;
    return ESP_OK;
}

static esp_err_t http_deinit(protocomm_ext_transport_handle_t handle)
{
    if (!handle) {
        ESP_LOGE(TAG, "Invalid handle");
        return ESP_ERR_INVALID_ARG;
    }

    protocomm_http_t *http = (protocomm_http_t *)handle;

    if (http->http_client) {
        esp_http_client_cleanup(http->http_client);
    }

    if (http->cookie) {
        free(http->cookie);
    }
    free(http);
    ESP_LOGI(TAG, "HTTP transport cleaned up");
    return ESP_OK;
}

static esp_err_t http_connect(protocomm_ext_transport_handle_t handle, const void *config)
{
    if (!handle) {
        ESP_LOGE(TAG, "Invalid handle");
        return ESP_ERR_INVALID_ARG;
    }
    protocomm_http_t *http = (protocomm_http_t *)handle;
    if (!http->http_client) {
        ESP_LOGE(TAG, "HTTP client not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    /* Due to the esp_http_client can not support establish socket and then send HTTP data, so we just return ESP_OK here.
     * Because we not know the HTTP /path url now, so we can not connect to the HTTP server.
     */
    ESP_LOGI(TAG, "HTTP connect successfully");
    return ESP_OK;
}

static esp_err_t http_disconnect(protocomm_ext_transport_handle_t handle)
{
    if (!handle) {
        ESP_LOGE(TAG, "Invalid handle");
        return ESP_ERR_INVALID_ARG;
    }

    protocomm_http_t *http = (protocomm_http_t *)handle;

    /* Keep the HTTP client for reuse; only drop the session cookie. */
    if (http->cookie) {
        free(http->cookie);
        http->cookie = NULL;
    }

    if (http->http_client) {
        esp_http_client_close(http->http_client);
    }

    ESP_LOGI(TAG, "HTTP session closed");
    return ESP_OK;
}

static esp_err_t http_send_data(protocomm_ext_transport_handle_t handle, const char *ep_name, const uint8_t *data, ssize_t data_len,
                                uint8_t **out_data, ssize_t *out_data_len)
{
    if (!handle || !ep_name || data_len < 0 || (data_len > 0 && !data) || !out_data || !out_data_len) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    protocomm_http_t *http = (protocomm_http_t *)handle;
    if (!http->http_client) {
        ESP_LOGE(TAG, "HTTP client not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    *out_data = NULL;
    *out_data_len = 0;

    /* Construct the full URL without producing a double slash. */
    char *url = NULL;
    const char *ep = ep_name;
    while (*ep == '/') {
        ep++;
    }
    if (asprintf(&url, "%s/%s", http->base_url, ep) < 0 || !url) {
        ESP_LOGE(TAG, "Failed to allocate URL");
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "HTTP POST %s (%d bytes)", url, (int)data_len);

    /* Match esp_prov: keep-alive + Cookie for protocomm session continuity. */
    esp_http_client_set_method(http->http_client, HTTP_METHOD_POST);
    esp_http_client_set_header(http->http_client, "Content-Type", "application/x-www-form-urlencoded");
    esp_http_client_set_header(http->http_client, "Accept", "text/plain");
    if (http->cookie) {
        esp_http_client_set_header(http->http_client, "Cookie", http->cookie);
    } else {
        esp_http_client_delete_header(http->http_client, "Cookie");
    }
    esp_http_client_set_url(http->http_client, url);
    free(url);

    esp_err_t ret = esp_http_client_open(http->http_client, data_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(ret));
        return ret;
    }

    if (data_len > 0) {
        int write_len = esp_http_client_write(http->http_client, (const char *)data, data_len);
        if (write_len < 0 || write_len != data_len) {
            ESP_LOGE(TAG, "Failed to send data (wrote %d / %d)", write_len, (int)data_len);
            esp_http_client_close(http->http_client);
            return ESP_FAIL;
        }
    }

    int64_t content_length = esp_http_client_fetch_headers(http->http_client);
    if (content_length < 0) {
        ESP_LOGE(TAG, "HTTP client fetch headers failed");
        esp_http_client_close(http->http_client);
        return ESP_FAIL;
    }

    if (content_length > INT_MAX) {
        ESP_LOGE(TAG, "HTTP Content-Length too large: %lld", (long long)content_length);
        esp_http_client_close(http->http_client);
        return ESP_FAIL;
    }

    int status_code = esp_http_client_get_status_code(http->http_client);
    if (status_code != 200) {
        ESP_LOGE(TAG, "HTTP request returned status %d", status_code);
        esp_http_client_close(http->http_client);
        return ESP_FAIL;
    }

    if (content_length == 0) {
        *out_data = NULL;
        *out_data_len = 0;
        return ESP_OK;
    }

    char *output_buffer = (char *)malloc((size_t)content_length);
    if (!output_buffer) {
        ESP_LOGE(TAG, "Failed to allocate response buffer (%lld)", (long long)content_length);
        esp_http_client_close(http->http_client);
        return ESP_ERR_NO_MEM;
    }

    int total_read = 0;
    while (total_read < content_length) {
        int data_read = esp_http_client_read(http->http_client,
                                             output_buffer + total_read,
                                             (int)content_length - total_read);
        if (data_read < 0) {
            ESP_LOGE(TAG, "Failed to read response");
            free(output_buffer);
            esp_http_client_close(http->http_client);
            return ESP_FAIL;
        }
        if (data_read == 0) {
            break;
        }
        total_read += data_read;
    }

    if (total_read != content_length) {
        ESP_LOGE(TAG, "Short HTTP body read (%d / %lld)", total_read, (long long)content_length);
        free(output_buffer);
        esp_http_client_close(http->http_client);
        return ESP_FAIL;
    }

    *out_data = (uint8_t *)output_buffer;
    *out_data_len = total_read;
    return ESP_OK;
}

const protocomm_ext_transport_t protocomm_ext_transport_http = {
    .init = http_init,
    .deinit = http_deinit,
    .connect = http_connect,
    .disconnect = http_disconnect,
    .send_data = http_send_data,
};
