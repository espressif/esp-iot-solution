/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_web.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "app_uac_manager.h"

#define APP_WEB_JSON_BUF_SIZE        4096
#define APP_WEB_WS_RX_BUF_SIZE       4096
#define APP_WEB_AUDIO_HEADER_SIZE    4
#define APP_WEB_AUDIO_MIC_TYPE       0x01
#define APP_WEB_AUDIO_SPK_TYPE       0x02
#define APP_WEB_CAPTIVE_PORTAL_URL   "http://192.168.4.1/"
#define APP_WEB_WS_FAIL_LIMIT        3

extern const uint8_t web_index_html_start[] asm("_binary_index_html_start");
extern const uint8_t web_index_html_end[] asm("_binary_index_html_end");
extern const uint8_t web_style_css_start[] asm("_binary_style_css_start");
extern const uint8_t web_style_css_end[] asm("_binary_style_css_end");
extern const uint8_t web_app_js_start[] asm("_binary_app_js_start");
extern const uint8_t web_app_js_end[] asm("_binary_app_js_end");

typedef enum {
    APP_WS_WORK_STATE,
    APP_WS_WORK_TEXT,
    APP_WS_WORK_BINARY,
} app_ws_work_type_t;

typedef struct {
    app_ws_work_type_t type;
    bool reset_spk_on_fail;
    size_t len;
    uint8_t data[];
} app_ws_work_msg_t;

static const char *TAG = "app_web";
static httpd_handle_t s_server;
static portMUX_TYPE s_ws_mux = portMUX_INITIALIZER_UNLOCKED;
static int s_ws_fd = -1;
static uint32_t s_ws_send_fail_count;

static bool cjson_get_bool(const cJSON *root, const char *key, bool *value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsBool(item)) {
        return false;
    }
    *value = cJSON_IsTrue(item);
    return true;
}

static bool cjson_get_u32(const cJSON *root, const char *key, uint32_t *value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsNumber(item) || item->valuedouble < 0 || item->valuedouble > UINT32_MAX) {
        return false;
    }
    *value = (uint32_t)item->valuedouble;
    return true;
}

static const char *cjson_get_type(const cJSON *root)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, "type");
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

static const char *ws_info_name(httpd_ws_client_info_t info)
{
    switch (info) {
    case HTTPD_WS_CLIENT_INVALID:
        return "invalid";
    case HTTPD_WS_CLIENT_HTTP:
        return "http";
    case HTTPD_WS_CLIENT_WEBSOCKET:
        return "websocket";
    default:
        return "unknown";
    }
}

static int ws_session_get_fd(void)
{
    portENTER_CRITICAL(&s_ws_mux);
    int fd = s_ws_fd;
    portEXIT_CRITICAL(&s_ws_mux);
    return fd;
}

static bool ws_session_is_current(int fd)
{
    bool current = false;
    portENTER_CRITICAL(&s_ws_mux);
    current = fd >= 0 && s_ws_fd == fd;
    portEXIT_CRITICAL(&s_ws_mux);
    return current;
}

static void ws_session_clear_if_current(int fd)
{
    bool cleared = false;
    portENTER_CRITICAL(&s_ws_mux);
    if (fd >= 0 && s_ws_fd == fd) {
        s_ws_fd = -1;
        s_ws_send_fail_count = 0;
        cleared = true;
    }
    portEXIT_CRITICAL(&s_ws_mux);
    if (cleared) {
        ESP_LOGI(TAG, "WebSocket session cleared fd=%d", fd);
        app_uac_reset_spk_refill_budget();
    }
}

static void ws_session_open(int fd)
{
    if (fd < 0) {
        return;
    }

    int old_fd = -1;
    portENTER_CRITICAL(&s_ws_mux);
    old_fd = s_ws_fd;
    s_ws_fd = fd;
    s_ws_send_fail_count = 0;
    portEXIT_CRITICAL(&s_ws_mux);

    if (old_fd >= 0 && old_fd != fd) {
        ESP_LOGI(TAG, "Replacing WebSocket fd=%d with fd=%d", old_fd, fd);
        app_uac_reset_spk_refill_budget();
        if (s_server) {
            httpd_sess_trigger_close(s_server, old_fd);
        }
    }
}

static esp_err_t ws_send_frame_to_current(httpd_ws_type_t type, const uint8_t *data, size_t len)
{
    int fd = ws_session_get_fd();
    if (!s_server) {
        ESP_LOGW(TAG, "Send WS frame skipped: server is not started type=%d len=%u", type, (unsigned)len);
        return ESP_ERR_INVALID_STATE;
    }
    if (fd < 0) {
        ESP_LOGD(TAG, "Send WS frame skipped: no active session type=%d len=%u", type, (unsigned)len);
        return ESP_ERR_NOT_FOUND;
    }
    httpd_ws_client_info_t info = httpd_ws_get_fd_info(s_server, fd);
    if (info != HTTPD_WS_CLIENT_WEBSOCKET) {
        ESP_LOGW(TAG, "Send WS frame skipped: fd=%d info=%s type=%d len=%u", fd, ws_info_name(info), type, (unsigned)len);
        ws_session_clear_if_current(fd);
        return ESP_ERR_INVALID_STATE;
    }

    httpd_ws_frame_t frame = {
        .type = type,
        .payload = (uint8_t *)data,
        .len = len,
    };
    esp_err_t ret = httpd_ws_send_frame_async(s_server, fd, &frame);
    if (ret == ESP_OK) {
        portENTER_CRITICAL(&s_ws_mux);
        s_ws_send_fail_count = 0;
        portEXIT_CRITICAL(&s_ws_mux);
        if (type == HTTPD_WS_TYPE_TEXT && len > 0 && data[0] == '{') {
            ESP_LOGD(TAG, "Sent WS text fd=%d len=%u", fd, (unsigned)len);
        }
        return ESP_OK;
    }

    uint32_t fail_count = 0;
    portENTER_CRITICAL(&s_ws_mux);
    if (s_ws_fd == fd) {
        s_ws_send_fail_count++;
        fail_count = s_ws_send_fail_count;
    }
    portEXIT_CRITICAL(&s_ws_mux);
    ESP_LOGW(TAG, "Send WS frame failed fd=%d count=%"PRIu32": %s", fd, fail_count, esp_err_to_name(ret));
    if (fail_count >= APP_WEB_WS_FAIL_LIMIT) {
        ws_session_clear_if_current(fd);
        if (s_server) {
            httpd_sess_trigger_close(s_server, fd);
        }
    }
    return ret;
}

static void ws_work_handler(void *arg)
{
    app_ws_work_msg_t *msg = (app_ws_work_msg_t *)arg;
    if (!msg) {
        return;
    }

    esp_err_t ret = ESP_OK;
    if (msg->type == APP_WS_WORK_STATE) {
        char *json = calloc(1, APP_WEB_JSON_BUF_SIZE);
        if (!json) {
            ESP_LOGE(TAG, "Allocate state JSON failed");
            free(msg);
            return;
        }
        ret = app_uac_get_state_json(json, APP_WEB_JSON_BUF_SIZE);
        if (ret == ESP_OK) {
            ret = ws_send_frame_to_current(HTTPD_WS_TYPE_TEXT, (const uint8_t *)json, strlen(json));
        } else {
            ESP_LOGW(TAG, "Build state JSON for WS failed: %s", esp_err_to_name(ret));
        }
        free(json);
    } else if (msg->type == APP_WS_WORK_TEXT) {
        ret = ws_send_frame_to_current(HTTPD_WS_TYPE_TEXT, msg->data, msg->len);
    } else if (msg->type == APP_WS_WORK_BINARY) {
        ret = ws_send_frame_to_current(HTTPD_WS_TYPE_BINARY, msg->data, msg->len);
    }

    if (ret != ESP_OK) {
        if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGD(TAG, "WS work skipped type=%d because no client is connected", msg->type);
        } else {
            ESP_LOGW(TAG, "WS work send failed type=%d current_fd=%d: %s", msg->type, ws_session_get_fd(), esp_err_to_name(ret));
        }
        if (msg->reset_spk_on_fail) {
            app_uac_reset_spk_refill_budget();
        }
    }
    free(msg);
}

static esp_err_t queue_ws_work(app_ws_work_type_t type, const uint8_t *data, size_t len, bool reset_spk_on_fail)
{
    if (!s_server) {
        return ESP_ERR_INVALID_STATE;
    }
    if (type != APP_WS_WORK_STATE && (!data || len == 0)) {
        return ESP_ERR_INVALID_ARG;
    }

    app_ws_work_msg_t *msg = calloc(1, sizeof(app_ws_work_msg_t) + len);
    if (!msg) {
        ESP_LOGE(TAG, "Allocate WS work failed");
        return ESP_ERR_NO_MEM;
    }
    msg->type = type;
    msg->reset_spk_on_fail = reset_spk_on_fail;
    msg->len = len;
    if (len > 0) {
        memcpy(msg->data, data, len);
    }

    esp_err_t ret = httpd_queue_work(s_server, ws_work_handler, msg);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Queue WS work failed type=%d: %s", type, esp_err_to_name(ret));
        free(msg);
    }
    return ret;
}

static esp_err_t send_text_to_req_ws(httpd_req_t *req, const char *text)
{
    if (!req || !text) {
        return ESP_ERR_INVALID_ARG;
    }
    httpd_ws_frame_t frame = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)text,
        .len = strlen(text),
    };
    esp_err_t ret = httpd_ws_send_frame(req, &frame);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Send WS text response failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t send_embedded_file(httpd_req_t *req, const char *type, const uint8_t *start, const uint8_t *end)
{
    httpd_resp_set_type(req, type);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, (const char *)start, end - start);
}

static esp_err_t index_handler(httpd_req_t *req)
{
    return send_embedded_file(req, "text/html", web_index_html_start, web_index_html_end);
}

static esp_err_t style_handler(httpd_req_t *req)
{
    return send_embedded_file(req, "text/css", web_style_css_start, web_style_css_end);
}

static esp_err_t app_js_handler(httpd_req_t *req)
{
    return send_embedded_file(req, "application/javascript", web_app_js_start, web_app_js_end);
}

static esp_err_t captive_probe_handler(httpd_req_t *req)
{
    // Redirect captive portal probes to the local web console.
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", APP_WEB_CAPTIVE_PORTAL_URL);
    httpd_resp_set_hdr(req, "Connection", "close");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t captive_404_handler(httpd_req_t *req, httpd_err_code_t error)
{
    (void)error;
    return captive_probe_handler(req);
}

static esp_err_t devices_handler(httpd_req_t *req)
{
    char *json = calloc(1, APP_WEB_JSON_BUF_SIZE);
    if (!json) {
        ESP_LOGE(TAG, "Allocate devices JSON failed");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no memory");
        return ESP_ERR_NO_MEM;
    }
    esp_err_t ret = app_uac_get_state_json(json, APP_WEB_JSON_BUF_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Build devices JSON failed: %s", esp_err_to_name(ret));
        free(json);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "state json failed");
        return ret;
    }
    httpd_resp_set_type(req, "application/json");
    ret = httpd_resp_sendstr(req, json);
    free(json);
    return ret;
}

static esp_err_t send_state_to_req_ws(httpd_req_t *req)
{
    char *json = calloc(1, APP_WEB_JSON_BUF_SIZE);
    if (!json) {
        ESP_LOGE(TAG, "Allocate state JSON failed");
        return ESP_ERR_NO_MEM;
    }
    esp_err_t ret = app_uac_get_state_json(json, APP_WEB_JSON_BUF_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Build state JSON failed: %s", esp_err_to_name(ret));
        free(json);
        return ret;
    }
    ret = send_text_to_req_ws(req, json);
    free(json);
    return ret;
}

static esp_err_t send_spk_refill_budget_to_req_ws(httpd_req_t *req, uint32_t bytes)
{
    if (bytes == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    char json[64];
    snprintf(json, sizeof(json), "{\"type\":\"spk_refill_budget\",\"bytes\":%"PRIu32"}", bytes);
    return send_text_to_req_ws(req, json);
}

static esp_err_t handle_ws_text(httpd_req_t *req, const char *msg)
{
    bool b = false;
    uint32_t v = 0;
    esp_err_t ret = ESP_OK;
    cJSON *root = cJSON_Parse(msg);
    if (!root) {
        ESP_LOGW(TAG, "Parse WS JSON failed");
        app_web_send_error("invalid_json", "Invalid JSON command");
        return ESP_ERR_INVALID_ARG;
    }
    const char *type = cjson_get_type(root);
    if (!type) {
        ESP_LOGW(TAG, "WS command missing type");
        ret = ESP_ERR_INVALID_ARG;
    } else if (strcmp(type, "get_state") == 0) {
        ret = send_state_to_req_ws(req);
    } else if (strcmp(type, "set_mic_enabled") == 0 && cjson_get_bool(root, "enabled", &b)) {
        ret = app_uac_set_enabled(APP_UAC_STREAM_MIC, b);
    } else if (strcmp(type, "set_spk_enabled") == 0 && cjson_get_bool(root, "enabled", &b)) {
        ret = app_uac_set_enabled(APP_UAC_STREAM_SPK, b);
    } else if (strcmp(type, "set_mic_mute") == 0 && cjson_get_bool(root, "mute", &b)) {
        ret = app_uac_set_mute(APP_UAC_STREAM_MIC, b);
    } else if (strcmp(type, "set_spk_mute") == 0 && cjson_get_bool(root, "mute", &b)) {
        ret = app_uac_set_mute(APP_UAC_STREAM_SPK, b);
    } else if (strcmp(type, "set_mic_volume") == 0 && cjson_get_u32(root, "volume", &v)) {
        ret = app_uac_set_volume(APP_UAC_STREAM_MIC, (uint8_t)v);
    } else if (strcmp(type, "set_spk_volume") == 0 && cjson_get_u32(root, "volume", &v)) {
        ret = app_uac_set_volume(APP_UAC_STREAM_SPK, (uint8_t)v);
    } else if (strcmp(type, "set_mic_format") == 0) {
        uint32_t alt = 0;
        uint32_t freq = 0;
        bool valid = cjson_get_u32(root, "alt", &alt) && cjson_get_u32(root, "sample_freq", &freq);
        ret = valid ? app_uac_set_format(APP_UAC_STREAM_MIC, (uint8_t)alt, freq) : ESP_ERR_INVALID_ARG;
    } else if (strcmp(type, "set_spk_format") == 0) {
        uint32_t alt = 0;
        uint32_t freq = 0;
        bool valid = cjson_get_u32(root, "alt", &alt) && cjson_get_u32(root, "sample_freq", &freq);
        ret = valid ? app_uac_set_format(APP_UAC_STREAM_SPK, (uint8_t)alt, freq) : ESP_ERR_INVALID_ARG;
    } else if (strcmp(type, "request_spk_refill_budget") == 0) {
        uint32_t bytes = 0;
        ret = app_uac_request_spk_refill_budget(&bytes);
        if (ret == ESP_OK && bytes > 0) {
            ret = send_spk_refill_budget_to_req_ws(req, bytes);
            if (ret != ESP_OK) {
                app_uac_reset_spk_refill_budget();
            }
        }
    } else if (strcmp(type, "stop_spk_playback") == 0 || strcmp(type, "clear_spk_queue") == 0) {
        ret = app_uac_clear_spk_queue();
    } else {
        ESP_LOGW(TAG, "Unsupported WS command: %s", type ? type : "unknown");
        ret = ESP_ERR_NOT_SUPPORTED;
    }

    if (ret != ESP_OK) {
        app_web_send_error("command_failed", esp_err_to_name(ret));
    }
    cJSON_Delete(root);
    return ret;
}

static esp_err_t handle_ws_binary(httpd_req_t *req, const uint8_t *payload, size_t len)
{
    // Binary frames are reserved for browser-to-speaker PCM chunks.
    if (len <= APP_WEB_AUDIO_HEADER_SIZE || payload[0] != APP_WEB_AUDIO_SPK_TYPE) {
        ESP_LOGW(TAG, "Invalid WS binary frame");
        return ESP_ERR_INVALID_ARG;
    }
    int ws_fd = httpd_req_to_sockfd(req);
    if (!ws_session_is_current(ws_fd)) {
        ESP_LOGW(TAG, "Drop SPK PCM from stale fd=%d current=%d", ws_fd, ws_session_get_fd());
        return ESP_ERR_INVALID_STATE;
    }
    uint16_t payload_len = payload[2] | (payload[3] << 8);
    if (payload_len != len - APP_WEB_AUDIO_HEADER_SIZE) {
        ESP_LOGW(TAG, "Invalid PCM payload length");
        return ESP_ERR_INVALID_SIZE;
    }
    esp_err_t ret = app_uac_queue_spk_pcm(payload + APP_WEB_AUDIO_HEADER_SIZE, payload_len);
    if (ret != ESP_OK) {
        app_web_send_error("spk_pcm_failed", esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    int ws_fd = httpd_req_to_sockfd(req);
    httpd_ws_client_info_t info = httpd_ws_get_fd_info(s_server, ws_fd);
    if (info == HTTPD_WS_CLIENT_WEBSOCKET && !ws_session_is_current(ws_fd)) {
        ws_session_open(ws_fd);
        ESP_LOGI(TAG, "WebSocket session active fd=%d", ws_fd);
    }
    if (req->method == HTTP_GET) {
        ws_session_open(ws_fd);
        ESP_LOGI(TAG, "WebSocket connected fd=%d", ws_fd);
        app_web_broadcast_state();
        return ESP_OK;
    }

    httpd_ws_frame_t frame = {0};
    esp_err_t ret = httpd_ws_recv_frame(req, &frame, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Get WS frame length failed: %s", esp_err_to_name(ret));
        return ret;
    }
    if (frame.len > APP_WEB_WS_RX_BUF_SIZE) {
        ESP_LOGW(TAG, "WS frame too large: %u", frame.len);
        return ESP_ERR_INVALID_SIZE;
    }
    uint8_t *buf = calloc(1, frame.len + 1);
    if (!buf) {
        ESP_LOGE(TAG, "Allocate WS rx buffer failed");
        return ESP_ERR_NO_MEM;
    }
    frame.payload = buf;
    ret = httpd_ws_recv_frame(req, &frame, frame.len);
    if (ret == ESP_OK) {
        if (frame.type == HTTPD_WS_TYPE_TEXT) {
            buf[frame.len] = '\0';
            ret = handle_ws_text(req, (const char *)buf);
        } else if (frame.type == HTTPD_WS_TYPE_BINARY) {
            ret = handle_ws_binary(req, buf, frame.len);
        } else if (frame.type == HTTPD_WS_TYPE_CLOSE) {
            int ws_fd = httpd_req_to_sockfd(req);
            ESP_LOGI(TAG, "WebSocket closed fd=%d", ws_fd);
            ws_session_clear_if_current(ws_fd);
        }
    } else {
        ESP_LOGE(TAG, "Receive WS frame failed: %s", esp_err_to_name(ret));
    }
    free(buf);
    return ret;
}

static void app_web_close_fn(httpd_handle_t hd, int sockfd)
{
    (void)hd;
    int current_fd = ws_session_get_fd();
    if (sockfd == current_fd) {
        ESP_LOGI(TAG, "HTTPD closing current WebSocket fd=%d", sockfd);
    } else {
        ESP_LOGD(TAG, "HTTPD closing HTTP fd=%d current_ws_fd=%d", sockfd, current_fd);
    }
    ws_session_clear_if_current(sockfd);
    close(sockfd);
}

esp_err_t app_web_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 12;
    config.close_fn = app_web_close_fn;
    config.stack_size = 6144;
    config.task_priority = 4;

    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP server start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    const httpd_uri_t uris[] = {
        {.uri = "/", .method = HTTP_GET, .handler = index_handler},
        {.uri = "/assets/style.css", .method = HTTP_GET, .handler = style_handler},
        {.uri = "/assets/app.js", .method = HTTP_GET, .handler = app_js_handler},
        {.uri = "/api/devices", .method = HTTP_GET, .handler = devices_handler},
        {.uri = "/ws", .method = HTTP_GET, .handler = ws_handler, .is_websocket = true},
        {.uri = "/connecttest.txt", .method = HTTP_GET, .handler = captive_probe_handler},
        {.uri = "/redirect", .method = HTTP_GET, .handler = captive_probe_handler},
        {.uri = "/generate_204", .method = HTTP_GET, .handler = captive_probe_handler},
        {.uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = captive_probe_handler},
        {.uri = "/ncsi.txt", .method = HTTP_GET, .handler = captive_probe_handler},
    };
    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        ret = httpd_register_uri_handler(s_server, &uris[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Register URI %s failed: %s", uris[i].uri, esp_err_to_name(ret));
            return ret;
        }
    }
    ret = httpd_register_err_handler(s_server, HTTPD_404_NOT_FOUND, captive_404_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Register captive 404 handler failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "HTTP server started");
    return ESP_OK;
}

bool app_web_has_client(void)
{
    return ws_session_get_fd() >= 0;
}

esp_err_t app_web_broadcast_state(void)
{
    if (!s_server) {
        ESP_LOGW(TAG, "Skip state broadcast: server is not started");
        return ESP_ERR_INVALID_STATE;
    }

    return queue_ws_work(APP_WS_WORK_STATE, NULL, 0, false);
}

esp_err_t app_web_send_error(const char *code, const char *message)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        ESP_LOGE(TAG, "Create error JSON failed");
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(root, "type", "error");
    cJSON_AddStringToObject(root, "code", code ? code : "unknown");
    cJSON_AddStringToObject(root, "message", message ? message : "unknown error");
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) {
        ESP_LOGE(TAG, "Print error JSON failed");
        return ESP_ERR_NO_MEM;
    }
    esp_err_t ret = queue_ws_work(APP_WS_WORK_TEXT, (const uint8_t *)json, strlen(json), false);
    cJSON_free(json);
    return ret;
}

esp_err_t app_web_send_spk_refill_budget_bytes(uint32_t bytes)
{
    if (bytes == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    char json[64];
    snprintf(json, sizeof(json), "{\"type\":\"spk_refill_budget\",\"bytes\":%"PRIu32"}", bytes);
    return queue_ws_work(APP_WS_WORK_TEXT, (const uint8_t *)json, strlen(json), true);
}

esp_err_t app_web_send_mic_pcm(const uint8_t *data, size_t len)
{
    // Prefix raw PCM with a compact frame header so the browser can route it.
    if (!data || len == 0 || len > UINT16_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t *packet = malloc(len + APP_WEB_AUDIO_HEADER_SIZE);
    if (!packet) {
        ESP_LOGE(TAG, "Allocate MIC packet failed");
        return ESP_ERR_NO_MEM;
    }
    packet[0] = APP_WEB_AUDIO_MIC_TYPE;
    packet[1] = 0;
    packet[2] = len & 0xff;
    packet[3] = (len >> 8) & 0xff;
    memcpy(packet + APP_WEB_AUDIO_HEADER_SIZE, data, len);
    esp_err_t ret = queue_ws_work(APP_WS_WORK_BINARY, packet, len + APP_WEB_AUDIO_HEADER_SIZE, false);
    free(packet);
    return ret;
}
