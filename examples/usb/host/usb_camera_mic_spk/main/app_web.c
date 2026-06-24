/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "app_web.h"
#include "app_uac_manager.h"
#include "app_uvc_manager.h"

#define APP_WEB_JSON_BUF_SIZE        8192
#define APP_WEB_WS_RX_BUF_SIZE       4096
#define APP_WEB_AUDIO_HEADER_SIZE    4
#define APP_WEB_AUDIO_MIC_TYPE       0x01
#define APP_WEB_AUDIO_SPK_TYPE       0x02
#define APP_WEB_CAPTIVE_PORTAL_URL   "http://192.168.4.1/"
#define APP_WEB_CAMERA_HEADER_SIZE   16
#define APP_WEB_CAMERA_FORMAT_MJPEG  1
#define APP_WEB_CAMERA_FRAME_WAIT_MS 500
#define APP_WEB_CAMERA_WS_RX_BUF_SIZE 256

extern const uint8_t web_index_html_start[] asm("_binary_index_html_start");
extern const uint8_t web_index_html_end[] asm("_binary_index_html_end");
extern const uint8_t web_style_css_start[] asm("_binary_style_css_start");
extern const uint8_t web_style_css_end[] asm("_binary_style_css_end");
extern const uint8_t web_app_js_start[] asm("_binary_app_js_start");
extern const uint8_t web_app_js_end[] asm("_binary_app_js_end");
extern const uint8_t web_mic_player_worklet_js_start[] asm("_binary_mic_player_worklet_js_start");
extern const uint8_t web_mic_player_worklet_js_end[] asm("_binary_mic_player_worklet_js_end");

typedef enum {
    APP_AUDIO_WS_WORK_STATE,
    APP_AUDIO_WS_WORK_TEXT,
    APP_AUDIO_WS_WORK_BINARY,
} app_audio_ws_work_type_t;

typedef struct {
    app_audio_ws_work_type_t type;
    bool reset_spk_on_fail;
    size_t len;
    uint8_t data[];
} app_audio_ws_work_msg_t;

static const char *TAG = "app_web";
static httpd_handle_t s_server;
static portMUX_TYPE s_ws_mux = portMUX_INITIALIZER_UNLOCKED;
static int s_audio_ws_fd = -1;
static int s_camera_ws_fd = -1;

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

static const char *cjson_get_string(const cJSON *root, const char *key)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

static esp_err_t app_web_get_state_json(char *buf, size_t len)
{
    if (!buf || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    char *audio_json = calloc(1, len);
    if (!audio_json) {
        ESP_LOGE(TAG, "Allocate audio state JSON failed");
        return ESP_ERR_NO_MEM;
    }
    esp_err_t ret = app_uac_get_state_json(audio_json, len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Build audio state JSON failed: %s", esp_err_to_name(ret));
        free(audio_json);
        return ret;
    }

    cJSON *root = cJSON_Parse(audio_json);
    free(audio_json);
    if (!root) {
        ESP_LOGE(TAG, "Parse audio state JSON failed");
        return ESP_FAIL;
    }

    ret = app_uvc_get_state_json(root);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Build camera state JSON failed: %s", esp_err_to_name(ret));
        cJSON_Delete(root);
        return ret;
    }
    if (!cJSON_PrintPreallocated(root, buf, len, false)) {
        ESP_LOGE(TAG, "Print combined state JSON failed");
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    cJSON_Delete(root);
    return ESP_OK;
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

static int ws_get_fd(int *slot)
{
    portENTER_CRITICAL(&s_ws_mux);
    int fd = *slot;
    portEXIT_CRITICAL(&s_ws_mux);
    return fd;
}

static bool ws_is_current(int *slot, int fd)
{
    bool current = false;
    portENTER_CRITICAL(&s_ws_mux);
    current = fd >= 0 && *slot == fd;
    portEXIT_CRITICAL(&s_ws_mux);
    return current;
}

static bool ws_clear_if_current(int *slot, int fd)
{
    bool cleared = false;
    portENTER_CRITICAL(&s_ws_mux);
    if (fd >= 0 && *slot == fd) {
        *slot = -1;
        cleared = true;
    }
    portEXIT_CRITICAL(&s_ws_mux);
    return cleared;
}

static int ws_set_current(int *slot, int fd)
{
    if (fd < 0) {
        return -1;
    }

    int old_fd = -1;
    portENTER_CRITICAL(&s_ws_mux);
    old_fd = *slot;
    *slot = fd;
    portEXIT_CRITICAL(&s_ws_mux);
    return old_fd;
}

static int audio_ws_get_fd(void)
{
    return ws_get_fd(&s_audio_ws_fd);
}

static bool audio_ws_is_current(int fd)
{
    return ws_is_current(&s_audio_ws_fd, fd);
}

static void audio_ws_open(int fd)
{
    int old_fd = ws_set_current(&s_audio_ws_fd, fd);
    if (old_fd >= 0 && old_fd != fd) {
        ESP_LOGI(TAG, "Replacing audio WebSocket fd=%d with fd=%d", old_fd, fd);
        app_uac_reset_spk_refill_budget();
        if (s_server) {
            httpd_sess_trigger_close(s_server, old_fd);
        }
    }
}

static bool audio_ws_clear_if_current(int fd)
{
    bool cleared = ws_clear_if_current(&s_audio_ws_fd, fd);
    if (cleared) {
        ESP_LOGI(TAG, "Audio WebSocket session cleared fd=%d", fd);
        app_uac_reset_spk_refill_budget();
    }
    return cleared;
}

static bool camera_ws_is_current(int fd)
{
    return ws_is_current(&s_camera_ws_fd, fd);
}

static void camera_ws_open(int fd)
{
    int old_fd = ws_set_current(&s_camera_ws_fd, fd);
    if (old_fd >= 0 && old_fd != fd) {
        ESP_LOGI(TAG, "Replacing camera WebSocket fd=%d with fd=%d", old_fd, fd);
        if (s_server) {
            httpd_sess_trigger_close(s_server, old_fd);
        }
    }
}

static bool camera_ws_clear_if_current(int fd)
{
    bool cleared = ws_clear_if_current(&s_camera_ws_fd, fd);
    if (cleared) {
        ESP_LOGI(TAG, "Camera WebSocket session cleared fd=%d", fd);
    }
    return cleared;
}

static esp_err_t audio_ws_send_frame_to_current(httpd_ws_type_t type, const uint8_t *data, size_t len)
{
    int fd = audio_ws_get_fd();
    if (!s_server) {
        ESP_LOGW(TAG, "Send audio WS frame skipped: server is not started type=%d len=%u", type, (unsigned)len);
        return ESP_ERR_INVALID_STATE;
    }
    if (fd < 0) {
        ESP_LOGD(TAG, "Send audio WS frame skipped: no active session type=%d len=%u", type, (unsigned)len);
        return ESP_ERR_NOT_FOUND;
    }
    httpd_ws_client_info_t info = httpd_ws_get_fd_info(s_server, fd);
    if (info != HTTPD_WS_CLIENT_WEBSOCKET) {
        ESP_LOGW(TAG, "Send audio WS frame skipped: fd=%d info=%s type=%d len=%u", fd, ws_info_name(info), type, (unsigned)len);
        audio_ws_clear_if_current(fd);
        return ESP_ERR_INVALID_STATE;
    }

    httpd_ws_frame_t frame = {
        .type = type,
        .payload = (uint8_t *)data,
        .len = len,
    };
    esp_err_t ret = httpd_ws_send_frame_async(s_server, fd, &frame);
    if (ret == ESP_OK) {
        if (type == HTTPD_WS_TYPE_TEXT && len > 0 && data[0] == '{') {
            ESP_LOGD(TAG, "Sent audio WS text fd=%d len=%u", fd, (unsigned)len);
        }
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Send audio WS frame failed fd=%d: %s", fd, esp_err_to_name(ret));
    audio_ws_clear_if_current(fd);
    if (s_server) {
        httpd_sess_trigger_close(s_server, fd);
    }
    return ret;
}

static void audio_ws_work_handler(void *arg)
{
    app_audio_ws_work_msg_t *msg = (app_audio_ws_work_msg_t *)arg;
    if (!msg) {
        return;
    }

    esp_err_t ret = ESP_OK;
    if (msg->type == APP_AUDIO_WS_WORK_STATE) {
        char *json = calloc(1, APP_WEB_JSON_BUF_SIZE);
        if (!json) {
            ESP_LOGE(TAG, "Allocate state JSON failed");
            free(msg);
            return;
        }
        ret = app_web_get_state_json(json, APP_WEB_JSON_BUF_SIZE);
        if (ret == ESP_OK) {
            ret = audio_ws_send_frame_to_current(HTTPD_WS_TYPE_TEXT, (const uint8_t *)json, strlen(json));
        } else {
            ESP_LOGW(TAG, "Build state JSON for WS failed: %s", esp_err_to_name(ret));
        }
        free(json);
    } else if (msg->type == APP_AUDIO_WS_WORK_TEXT) {
        ret = audio_ws_send_frame_to_current(HTTPD_WS_TYPE_TEXT, msg->data, msg->len);
    } else if (msg->type == APP_AUDIO_WS_WORK_BINARY) {
        ret = audio_ws_send_frame_to_current(HTTPD_WS_TYPE_BINARY, msg->data, msg->len);
    }

    if (ret != ESP_OK) {
        if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGD(TAG, "Audio WS work skipped type=%d because no client is connected", msg->type);
        } else {
            ESP_LOGW(TAG, "Audio WS work send failed type=%d current_fd=%d: %s", msg->type, audio_ws_get_fd(), esp_err_to_name(ret));
        }
        if (msg->reset_spk_on_fail) {
            app_uac_reset_spk_refill_budget();
        }
    }
    free(msg);
}

static esp_err_t queue_audio_ws_work(app_audio_ws_work_type_t type, const uint8_t *data, size_t len, bool reset_spk_on_fail)
{
    if (!s_server) {
        return ESP_ERR_INVALID_STATE;
    }
    if (type != APP_AUDIO_WS_WORK_STATE && (!data || len == 0)) {
        return ESP_ERR_INVALID_ARG;
    }

    app_audio_ws_work_msg_t *msg = calloc(1, sizeof(app_audio_ws_work_msg_t) + len);
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

    esp_err_t ret = httpd_queue_work(s_server, audio_ws_work_handler, msg);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Queue WS work failed type=%d: %s", type, esp_err_to_name(ret));
        free(msg);
    }
    return ret;
}

static esp_err_t queue_audio_ws_mic_pcm(const uint8_t *data, size_t len)
{
    if (!s_server) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!data || len == 0 || len > UINT16_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    app_audio_ws_work_msg_t *msg = calloc(1, sizeof(app_audio_ws_work_msg_t) + APP_WEB_AUDIO_HEADER_SIZE + len);
    if (!msg) {
        ESP_LOGE(TAG, "Allocate MIC WS work failed len=%u", (unsigned)len);
        return ESP_ERR_NO_MEM;
    }
    msg->type = APP_AUDIO_WS_WORK_BINARY;
    msg->len = APP_WEB_AUDIO_HEADER_SIZE + len;
    msg->data[0] = APP_WEB_AUDIO_MIC_TYPE;
    msg->data[1] = 0;
    msg->data[2] = len & 0xff;
    msg->data[3] = (len >> 8) & 0xff;
    memcpy(msg->data + APP_WEB_AUDIO_HEADER_SIZE, data, len);

    esp_err_t ret = httpd_queue_work(s_server, audio_ws_work_handler, msg);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Queue MIC WS work failed len=%u: %s", (unsigned)len, esp_err_to_name(ret));
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

static esp_err_t mic_player_worklet_js_handler(httpd_req_t *req)
{
    return send_embedded_file(req, "application/javascript", web_mic_player_worklet_js_start, web_mic_player_worklet_js_end);
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
    esp_err_t ret = app_web_get_state_json(json, APP_WEB_JSON_BUF_SIZE);
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
    esp_err_t ret = app_web_get_state_json(json, APP_WEB_JSON_BUF_SIZE);
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
        if (ret == ESP_OK) {
            ret = send_text_to_req_ws(req, "{\"type\":\"spk_reset_done\"}");
        }
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
    if (!audio_ws_is_current(ws_fd)) {
        ESP_LOGW(TAG, "Drop SPK PCM from stale fd=%d current=%d", ws_fd, audio_ws_get_fd());
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

static esp_err_t audio_ws_handler(httpd_req_t *req)
{
    int ws_fd = httpd_req_to_sockfd(req);
    httpd_ws_client_info_t info = httpd_ws_get_fd_info(s_server, ws_fd);
    if (info == HTTPD_WS_CLIENT_WEBSOCKET && !audio_ws_is_current(ws_fd)) {
        audio_ws_open(ws_fd);
        ESP_LOGI(TAG, "Audio WebSocket session active fd=%d", ws_fd);
    }
    if (req->method == HTTP_GET) {
        audio_ws_open(ws_fd);
        ESP_LOGI(TAG, "Audio WebSocket connected fd=%d", ws_fd);
        app_web_broadcast_state();
        return ESP_OK;
    }

    httpd_ws_frame_t frame = {0};
    esp_err_t ret = httpd_ws_recv_frame(req, &frame, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Get audio WS frame length failed: %s", esp_err_to_name(ret));
        return ret;
    }
    if (frame.len > APP_WEB_WS_RX_BUF_SIZE) {
        ESP_LOGW(TAG, "Audio WS frame too large: %u", frame.len);
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
            ESP_LOGI(TAG, "Audio WebSocket closed fd=%d", ws_fd);
            audio_ws_clear_if_current(ws_fd);
        }
    } else {
        ESP_LOGE(TAG, "Receive audio WS frame failed: %s", esp_err_to_name(ret));
    }
    free(buf);
    return ret;
}

static void app_web_close_fn(httpd_handle_t hd, int sockfd)
{
    (void)hd;
    int current_fd = audio_ws_get_fd();
    if (sockfd == current_fd) {
        ESP_LOGI(TAG, "HTTPD closing current audio WebSocket fd=%d", sockfd);
    } else {
        ESP_LOGD(TAG, "HTTPD closing HTTP fd=%d current_audio_ws_fd=%d", sockfd, current_fd);
    }
    audio_ws_clear_if_current(sockfd);
    camera_ws_clear_if_current(sockfd);
    close(sockfd);
}

static void put_le16(uint8_t *buf, uint16_t value)
{
    buf[0] = value & 0xff;
    buf[1] = (value >> 8) & 0xff;
}

static void put_le32(uint8_t *buf, uint32_t value)
{
    buf[0] = value & 0xff;
    buf[1] = (value >> 8) & 0xff;
    buf[2] = (value >> 16) & 0xff;
    buf[3] = (value >> 24) & 0xff;
}

static esp_err_t camera_ws_send_packet(int fd, const uint8_t *data, size_t len)
{
    if (!s_server || fd < 0 || !data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!camera_ws_is_current(fd)) {
        return ESP_ERR_INVALID_STATE;
    }
    httpd_ws_client_info_t info = httpd_ws_get_fd_info(s_server, fd);
    if (info != HTTPD_WS_CLIENT_WEBSOCKET) {
        ESP_LOGW(TAG, "Camera WS send skipped: fd=%d info=%s len=%u", fd, ws_info_name(info), (unsigned)len);
        camera_ws_clear_if_current(fd);
        return ESP_ERR_INVALID_STATE;
    }

    httpd_ws_frame_t frame = {
        .type = HTTPD_WS_TYPE_BINARY,
        .payload = (uint8_t *)data,
        .len = len,
    };
    esp_err_t ret = httpd_ws_send_frame_async(s_server, fd, &frame);
    if (ret == ESP_OK) {
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Camera WS send failed fd=%d: %s", fd, esp_err_to_name(ret));
    camera_ws_clear_if_current(fd);
    if (s_server) {
        httpd_sess_trigger_close(s_server, fd);
    }
    return ret;
}

static void camera_ws_stream_task(void *arg)
{
    int fd = (int)(intptr_t)arg;
    int64_t last_frame_us = 0;
    int64_t window_start_us = 0;
    uint32_t window_frame_count = 0;
    bool stream_ready_logged = false;
    uint32_t frame_count = 0;
    ESP_LOGI(TAG, "Camera WS stream task started fd=%d", fd);
    while (camera_ws_is_current(fd)) {
        app_uvc_format_t format = app_uvc_get_format();
        bool streaming = app_uvc_is_streaming();
        if (format != APP_UVC_FORMAT_MJPEG || !streaming) {
            stream_ready_logged = false;
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        if (!stream_ready_logged) {
            ESP_LOGI(TAG, "Camera WS stream ready fd=%d", fd);
            stream_ready_logged = true;
        }

        uvc_host_frame_t *frame = app_uvc_get_frame(pdMS_TO_TICKS(APP_WEB_CAMERA_FRAME_WAIT_MS));
        if (!frame) {
            ESP_LOGW(TAG, "Get camera frame timeout fd=%d streaming=%d current=%d", fd, app_uvc_is_streaming(), camera_ws_is_current(fd));
            continue;
        }

        size_t packet_len = APP_WEB_CAMERA_HEADER_SIZE + frame->data_len;
        uint8_t *packet = malloc(packet_len);
        if (!packet) {
            ESP_LOGE(TAG, "Allocate camera WS packet failed len=%u", (unsigned)packet_len);
            app_uvc_return_frame(frame);
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        packet[0] = 'C';
        packet[1] = 1;
        packet[2] = APP_WEB_CAMERA_FORMAT_MJPEG;
        packet[3] = 0;
        put_le32(packet + 4, frame->data_len);
        put_le32(packet + 8, (uint32_t)(esp_timer_get_time() / 1000));
        put_le16(packet + 12, frame->vs_format.h_res);
        put_le16(packet + 14, frame->vs_format.v_res);
        memcpy(packet + APP_WEB_CAMERA_HEADER_SIZE, frame->data, frame->data_len);
        size_t frame_len = frame->data_len;
        esp_err_t frame_ret = app_uvc_return_frame(frame);
        if (frame_ret != ESP_OK) {
            ESP_LOGW(TAG, "Return camera WS frame failed: %s", esp_err_to_name(frame_ret));
        }

        esp_err_t ret = camera_ws_send_packet(fd, packet, packet_len);
        free(packet);
        if (ret != ESP_OK) {
            ESP_LOGI(TAG, "Camera WS stream stopped fd=%d after %"PRIu32" frames: %s", fd, frame_count, esp_err_to_name(ret));
            break;
        }

        frame_count++;
        int64_t now_us = esp_timer_get_time();
        float inst_fps = last_frame_us > 0 ? 1000000.0f / (float)(now_us - last_frame_us) : 0.0f;
        last_frame_us = now_us;
        if (window_start_us == 0) {
            window_start_us = now_us;
            window_frame_count = 0;
        }
        window_frame_count++;
        if (frame_count == 1 || frame_count % 30 == 0) {
            int64_t window_us = now_us - window_start_us;
            float avg_fps = window_us > 0 ? (float)window_frame_count * 1000000.0f / (float)window_us : 0.0f;
            ESP_LOGI(TAG, "Camera WS frame sent fd=%d count=%"PRIu32" len=%u avg_fps=%.2f inst_fps=%.2f window_frames=%"PRIu32,
                     fd, frame_count, (unsigned)frame_len, avg_fps, inst_fps, window_frame_count);
            window_start_us = now_us;
            window_frame_count = 0;
        }
    }
    ESP_LOGI(TAG, "Camera WS stream task exit fd=%d frames=%"PRIu32, fd, frame_count);
    camera_ws_clear_if_current(fd);
    vTaskDelete(NULL);
}

static esp_err_t camera_ws_start_stream(int fd)
{
    if (fd < 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (camera_ws_is_current(fd)) {
        return ESP_OK;
    }

    camera_ws_open(fd);
    if (xTaskCreate(camera_ws_stream_task, "camera_ws", 4096, (void *)(intptr_t)fd, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Create camera WebSocket stream task failed");
        camera_ws_clear_if_current(fd);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t handle_camera_ws_text(const char *msg)
{
    bool b = false;
    esp_err_t ret = ESP_OK;
    cJSON *root = cJSON_Parse(msg);
    if (!root) {
        ESP_LOGW(TAG, "Parse camera WS JSON failed");
        app_web_send_error("invalid_json", "Invalid camera JSON command");
        return ESP_ERR_INVALID_ARG;
    }

    const char *type = cjson_get_type(root);
    if (!type) {
        ESP_LOGW(TAG, "Camera WS command missing type");
        ret = ESP_ERR_INVALID_ARG;
    } else if (strcmp(type, "set_camera_enabled") == 0 && cjson_get_bool(root, "enabled", &b)) {
        ESP_LOGI(TAG, "Camera WS command set_enabled=%d", b);
        ret = app_uvc_set_enabled(b);
    } else if (strcmp(type, "set_camera_format") == 0) {
        const char *format = cjson_get_string(root, "format");
        uint32_t resolution = 0;
        if (!format || !cjson_get_u32(root, "resolution", &resolution)) {
            ret = ESP_ERR_INVALID_ARG;
        } else if (strcmp(format, "mjpeg") == 0) {
            ret = app_uvc_set_format(APP_UVC_FORMAT_MJPEG, (uint8_t)resolution);
        } else if (strcmp(format, "h264") == 0) {
            ret = app_uvc_set_format(APP_UVC_FORMAT_H264, (uint8_t)resolution);
        } else {
            ESP_LOGW(TAG, "Unsupported camera WS format command: %s", format);
            ret = ESP_ERR_NOT_SUPPORTED;
        }
    } else {
        ESP_LOGW(TAG, "Unsupported camera WS command: %s", type ? type : "unknown");
        ret = ESP_ERR_NOT_SUPPORTED;
    }

    if (ret != ESP_OK) {
        app_web_send_error("camera_command_failed", esp_err_to_name(ret));
    }
    cJSON_Delete(root);
    return ret;
}

static esp_err_t camera_ws_handler(httpd_req_t *req)
{
    int ws_fd = httpd_req_to_sockfd(req);
    if (req->method == HTTP_GET) {
        return ESP_OK;
    }

    httpd_ws_frame_t frame = {0};
    esp_err_t ret = httpd_ws_recv_frame(req, &frame, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Get camera WS frame length failed: %s", esp_err_to_name(ret));
        return ret;
    }
    if (frame.len > APP_WEB_CAMERA_WS_RX_BUF_SIZE) {
        ESP_LOGW(TAG, "Camera WS control frame too large: %u", (unsigned)frame.len);
        return ESP_ERR_INVALID_SIZE;
    }
    uint8_t payload[APP_WEB_CAMERA_WS_RX_BUF_SIZE + 1] = {0};
    if (frame.len > 0) {
        frame.payload = payload;
        ret = httpd_ws_recv_frame(req, &frame, frame.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Receive camera WS frame payload failed: %s", esp_err_to_name(ret));
            return ret;
        }
        payload[frame.len] = '\0';
    }
    if (frame.type == HTTPD_WS_TYPE_CLOSE) {
        ESP_LOGI(TAG, "Camera WebSocket closed fd=%d", ws_fd);
        camera_ws_clear_if_current(ws_fd);
    } else {
        // Consume the browser control frame before starting the producer to keep the TCP stream aligned.
        if (frame.type == HTTPD_WS_TYPE_TEXT && frame.len > 0) {
            ret = handle_camera_ws_text((const char *)payload);
            if (ret != ESP_OK) {
                return ret;
            }
        }
        ret = camera_ws_start_stream(ws_fd);
    }
    return ret;
}

esp_err_t app_web_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 14;
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
        {.uri = "/assets/mic-player-worklet.js", .method = HTTP_GET, .handler = mic_player_worklet_js_handler},
        {.uri = "/api/devices", .method = HTTP_GET, .handler = devices_handler},
        {.uri = "/camera_ws", .method = HTTP_GET, .handler = camera_ws_handler, .is_websocket = true},
        {.uri = "/ws", .method = HTTP_GET, .handler = audio_ws_handler, .is_websocket = true},
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
        if (strcmp(uris[i].uri, "/camera_ws") == 0) {
            ESP_LOGW(TAG, "Registered camera WebSocket URI");
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
    return audio_ws_get_fd() >= 0;
}

esp_err_t app_web_broadcast_state(void)
{
    if (!s_server) {
        ESP_LOGW(TAG, "Skip state broadcast: server is not started");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = queue_audio_ws_work(APP_AUDIO_WS_WORK_STATE, NULL, 0, false);
    return ret;
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
    esp_err_t ret = queue_audio_ws_work(APP_AUDIO_WS_WORK_TEXT, (const uint8_t *)json, strlen(json), false);
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
    return queue_audio_ws_work(APP_AUDIO_WS_WORK_TEXT, (const uint8_t *)json, strlen(json), true);
}

esp_err_t app_web_send_mic_pcm(const uint8_t *data, size_t len)
{
    // Prefix raw PCM with a compact frame header so the browser can route it.
    if (!data || len == 0 || len > UINT16_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    return queue_audio_ws_mic_pcm(data, len);
}
