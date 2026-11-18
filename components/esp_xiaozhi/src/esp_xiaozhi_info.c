/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/param.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>

#include <esp_check.h>
#include <esp_log.h>
#include <esp_app_desc.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <esp_tls.h>
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "esp_xiaozhi_keystore.h"
#include "esp_xiaozhi_board.h"
#include "esp_xiaozhi_info.h"
#include "esp_xiaozhi_info_priv.h"
#include "esp_xiaozhi_protocol_priv.h"

static const char *TAG = "ESP_XIAOZHI_INFO";

typedef struct {
    char *output_buffer;
    size_t output_len;
    size_t output_capacity;
    esp_err_t result;
} esp_xiaozhi_chat_http_ctx_t;

static char *str_dup(const char *str)
{
    if (!str) {
        return NULL;
    }
    size_t len = strlen(str) + 1;
    char *dup = malloc(len);
    if (dup) {
        memcpy(dup, str, len);
    }
    return dup;
}

static void str_free(char **str)
{
    if (str && *str) {
        free(*str);
        *str = NULL;
    }
}

static esp_err_t str_replace(char **str, const char *value)
{
    ESP_RETURN_ON_FALSE(str != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid string destination");
    ESP_RETURN_ON_FALSE(value != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid string value");

    char *dup = str_dup(value);
    ESP_RETURN_ON_FALSE(dup != NULL, ESP_ERR_NO_MEM, TAG, "Failed to duplicate string");

    str_free(str);
    *str = dup;
    return ESP_OK;
}

static void esp_xiaozhi_chat_http_ctx_cleanup(esp_xiaozhi_chat_http_ctx_t *ctx)
{
    if (ctx != NULL) {
        free(ctx->output_buffer);
        ctx->output_buffer = NULL;
        ctx->output_len = 0;
        ctx->output_capacity = 0;
    }
}

static esp_err_t esp_xiaozhi_chat_http_ctx_append(esp_xiaozhi_chat_http_ctx_t *ctx, const char *data, size_t data_len)
{
    ESP_RETURN_ON_FALSE(ctx != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid HTTP context");
    ESP_RETURN_ON_FALSE(data != NULL || data_len == 0, ESP_ERR_INVALID_ARG, TAG, "Invalid HTTP data");

    if (data_len == 0) {
        return ESP_OK;
    }

    if (data_len >= CONFIG_XIAOZHI_INFO_MAX_RESPONSE_SIZE ||
            ctx->output_len >= CONFIG_XIAOZHI_INFO_MAX_RESPONSE_SIZE - data_len) {
        return ESP_ERR_INVALID_SIZE;
    }
    size_t required_capacity = ctx->output_len + data_len + 1;
    if (required_capacity > CONFIG_XIAOZHI_INFO_MAX_RESPONSE_SIZE) {
        ESP_LOGE(TAG, "HTTP response too large: %zu bytes (max %d)",
                 required_capacity, CONFIG_XIAOZHI_INFO_MAX_RESPONSE_SIZE);
        return ESP_ERR_NO_MEM;
    }
    if (required_capacity > ctx->output_capacity) {
        size_t new_capacity = ctx->output_capacity > 0 ? ctx->output_capacity : 512;
        while (new_capacity < required_capacity) {
            new_capacity *= 2;
        }
        if (new_capacity > CONFIG_XIAOZHI_INFO_MAX_RESPONSE_SIZE) {
            new_capacity = CONFIG_XIAOZHI_INFO_MAX_RESPONSE_SIZE;
        }

        char *new_buffer = realloc(ctx->output_buffer, new_capacity);
        ESP_RETURN_ON_FALSE(new_buffer != NULL, ESP_ERR_NO_MEM, TAG, "Failed to grow HTTP response buffer");

        ctx->output_buffer = new_buffer;
        ctx->output_capacity = new_capacity;
    }

    memcpy(ctx->output_buffer + ctx->output_len, data, data_len);
    ctx->output_len += data_len;
    ctx->output_buffer[ctx->output_len] = '\0';
    return ESP_OK;
}

static esp_err_t esp_xiaozhi_chat_store_json_config(const char *name_space, const cJSON *config)
{
    ESP_RETURN_ON_FALSE(name_space != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid namespace");
    ESP_RETURN_ON_FALSE(cJSON_IsObject(config), ESP_ERR_INVALID_ARG, TAG, "Invalid config object");

    esp_err_t ret = ESP_OK;
    esp_xiaozhi_chat_keystore_t xiaozhi_chat_keystore = {0};

    do {
        ret = esp_xiaozhi_chat_keystore_init(&xiaozhi_chat_keystore, name_space, true);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize keystore '%s': %s", name_space, esp_err_to_name(ret));
            break;
        }

        cJSON *item = NULL;
        cJSON_ArrayForEach(item, config) {
            if (item->string == NULL) {
                ret = ESP_ERR_INVALID_RESPONSE;
                ESP_LOGE(TAG, "Invalid config key in namespace '%s'", name_space);
                break;
            }

            if (cJSON_IsString(item)) {
                ret = esp_xiaozhi_chat_keystore_set_string(&xiaozhi_chat_keystore, item->string, item->valuestring);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to store '%s' in namespace '%s': %s",
                             item->string, name_space, esp_err_to_name(ret));
                    break;
                }
            } else if (cJSON_IsNumber(item) && isfinite(item->valuedouble) &&
                       item->valuedouble == item->valueint) {
                ret = esp_xiaozhi_chat_keystore_set_int(&xiaozhi_chat_keystore, item->string, item->valueint);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to store '%s' in namespace '%s': %s",
                             item->string, name_space, esp_err_to_name(ret));
                    break;
                }
            } else {
                ESP_LOGW(TAG, "Skip unsupported config item '%s' in namespace '%s'", item->string, name_space);
            }
        }
    } while (0);

    {
        esp_err_t deinit_ret = esp_xiaozhi_chat_keystore_deinit(&xiaozhi_chat_keystore);
        if (ret == ESP_OK) {
            ret = deinit_ret;
        } else if (deinit_ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to close keystore '%s': %s", name_space, esp_err_to_name(deinit_ret));
        }
    }

    return ret;
}

static int *esp_xiaozhi_chat_ota_parse_version(const char *version, size_t *count)
{
    if (!version || !version[0] || !count) {
        return NULL;
    }

    *count = 0;

    size_t components = 1;
    for (const char *p = version; *p; p++) {
        if (*p == '.') {
            components++;
        }
    }

    int *version_numbers = malloc(components * sizeof(int));
    if (!version_numbers) {
        return NULL;
    }

    const char *start = version;
    for (size_t i = 0; i < components; i++) {
        const char *dot = strchr(start, '.');
        size_t len = dot ? (size_t)(dot - start) : strlen(start);
        if (len == 0) {
            free(version_numbers);
            return NULL;
        }
        long val = 0;
        for (size_t j = 0; j < len; j++) {
            if (!isdigit((unsigned char)start[j])) {
                free(version_numbers);
                return NULL;
            }
            val = val * 10 + (start[j] - '0');
            if (val > INT_MAX) {
                free(version_numbers);
                return NULL;
            }
        }
        version_numbers[i] = (int)val;
        start = dot ? dot + 1 : start + len;
    }

    *count = components;
    return version_numbers;
}

static void esp_xiaozhi_chat_ota_free(int *version_array)
{
    if (version_array) {
        free(version_array);
    }
}

static bool esp_xiaozhi_chat_ota_available(const char *current_version, const char *new_version)
{
    if (!current_version || !new_version) {
        return false;
    }

    size_t current_count, new_count;
    int *current_nums = esp_xiaozhi_chat_ota_parse_version(current_version, &current_count);
    int *new_nums = esp_xiaozhi_chat_ota_parse_version(new_version, &new_count);

    if (!current_nums || !new_nums) {
        esp_xiaozhi_chat_ota_free(current_nums);
        esp_xiaozhi_chat_ota_free(new_nums);
        return false;
    }

    size_t min_count = (current_count < new_count) ? current_count : new_count;

    for (size_t i = 0; i < min_count; i++) {
        if (new_nums[i] > current_nums[i]) {
            esp_xiaozhi_chat_ota_free(current_nums);
            esp_xiaozhi_chat_ota_free(new_nums);
            return true;
        } else if (new_nums[i] < current_nums[i]) {
            esp_xiaozhi_chat_ota_free(current_nums);
            esp_xiaozhi_chat_ota_free(new_nums);
            return false;
        }
    }

    bool result = new_count > current_count;
    esp_xiaozhi_chat_ota_free(current_nums);
    esp_xiaozhi_chat_ota_free(new_nums);
    return result;
}

esp_err_t esp_xiaozhi_info_parse_response(const char *data, size_t data_len, esp_xiaozhi_chat_info_t *info)
{
    ESP_RETURN_ON_FALSE(data != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid HTTP data");
    ESP_RETURN_ON_FALSE(info != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid info");

    esp_err_t ret = ESP_OK;
    const char *end = NULL;
    cJSON *root = cJSON_ParseWithLengthOpts(data, data_len, &end, false);
    while (end && end < data + data_len && isspace((unsigned char) * end)) {
        end++;
    }
    if (!cJSON_IsObject(root) || end != data + data_len) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }

    do {
        str_free(&info->activation_message);
        str_free(&info->activation_code);
        str_free(&info->activation_challenge);
        str_free(&info->firmware_version);
        str_free(&info->firmware_url);
        info->activation_timeout_ms = 0;
        info->has_activation_code = false;
        info->has_activation_challenge = false;
        info->has_mqtt_config = false;
        info->has_websocket_config = false;
        info->has_server_time = false;
        info->has_new_version = false;

        cJSON *activation = cJSON_GetObjectItem(root, "activation");
        if (cJSON_IsObject(activation)) {
            cJSON *message = cJSON_GetObjectItem(activation, "message");
            if (cJSON_IsString(message)) {
                ret = str_replace(&info->activation_message, message->valuestring);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to store activation message: %s", esp_err_to_name(ret));
                    break;
                }
            }
            cJSON *code = cJSON_GetObjectItem(activation, "code");
            if (cJSON_IsString(code)) {
                ret = str_replace(&info->activation_code, code->valuestring);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to store activation code: %s", esp_err_to_name(ret));
                    break;
                }
                info->has_activation_code = true;
            }
            cJSON *challenge = cJSON_GetObjectItem(activation, "challenge");
            if (cJSON_IsString(challenge)) {
                ret = str_replace(&info->activation_challenge, challenge->valuestring);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to store activation challenge: %s", esp_err_to_name(ret));
                    break;
                }
                info->has_activation_challenge = true;
            }
            cJSON *timeout_ms = cJSON_GetObjectItem(activation, "timeout_ms");
            if (cJSON_IsNumber(timeout_ms)) {
                info->activation_timeout_ms = timeout_ms->valueint;
            }
        }

        cJSON *mqtt = cJSON_GetObjectItem(root, "mqtt");
        if (cJSON_IsObject(mqtt)) {
            ret = esp_xiaozhi_chat_store_json_config("mqtt", mqtt);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to persist MQTT config: %s", esp_err_to_name(ret));
                break;
            }
            info->has_mqtt_config = true;
        } else {
            ESP_LOGW(TAG, "No mqtt section found");
        }

        cJSON *websocket = cJSON_GetObjectItem(root, "websocket");
        if (cJSON_IsObject(websocket)) {
            ret = esp_xiaozhi_chat_store_json_config("websocket", websocket);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to persist WebSocket config: %s", esp_err_to_name(ret));
                break;
            }
            info->has_websocket_config = true;
        } else {
            ESP_LOGW(TAG, "No websocket section found");
        }

        cJSON *server_time = cJSON_GetObjectItem(root, "server_time");
        if (cJSON_IsObject(server_time)) {
            cJSON *timestamp = cJSON_GetObjectItem(server_time, "timestamp");
            if (cJSON_IsNumber(timestamp)) {
#if CONFIG_XIAOZHI_SYNC_SYSTEM_TIME_FROM_SERVER
                struct timeval tv;
                double ts = timestamp->valuedouble;
                cJSON *timezone_offset = cJSON_GetObjectItem(server_time, "timezone_offset");
                if (cJSON_IsNumber(timezone_offset)) {
                    ts += (timezone_offset->valueint * 60 * 1000);
                }

                tv.tv_sec = (time_t)(ts / 1000);
                tv.tv_usec = (suseconds_t)((long long)ts % 1000) * 1000;
                settimeofday(&tv, NULL);
#endif
                info->has_server_time = true;
            }
        } else {
            ESP_LOGW(TAG, "No server_time section found!");
        }

        cJSON *firmware = cJSON_GetObjectItem(root, "firmware");
        if (cJSON_IsObject(firmware)) {
            cJSON *version = cJSON_GetObjectItem(firmware, "version");
            if (cJSON_IsString(version)) {
                ret = str_replace(&info->firmware_version, version->valuestring);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to store firmware version: %s", esp_err_to_name(ret));
                    break;
                }
            }

            cJSON *firmware_url = cJSON_GetObjectItem(firmware, "url");
            if (cJSON_IsString(firmware_url)) {
                ret = str_replace(&info->firmware_url, firmware_url->valuestring);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to store firmware URL: %s", esp_err_to_name(ret));
                    break;
                }
            }

            if (cJSON_IsString(version) && cJSON_IsString(firmware_url)) {
                info->has_new_version = esp_xiaozhi_chat_ota_available(info->current_version, info->firmware_version);
                if (info->has_new_version) {
                    ESP_LOGD(TAG, "New version available: %s", info->firmware_version);
                } else {
                    ESP_LOGD(TAG, "Current is the latest version");
                }

                cJSON *force = cJSON_GetObjectItem(firmware, "force");
                if (cJSON_IsNumber(force) && force->valueint == 1) {
                    info->has_new_version = true;
                }
            }
        } else {
            ESP_LOGW(TAG, "No firmware section found!");
        }
    } while (0);

    cJSON_Delete(root);

    return ret;
}

static esp_err_t esp_xiaozhi_chat_http_event(esp_http_client_event_t *evt)
{
    esp_xiaozhi_chat_http_ctx_t *ctx = (esp_xiaozhi_chat_http_ctx_t *)evt->user_data;
    if (ctx == NULL) {
        return ESP_OK;
    }

    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        if (ctx->result != ESP_OK) {
            return ctx->result;
        }
        ctx->result = esp_xiaozhi_chat_http_ctx_append(ctx, (const char *)evt->data, evt->data_len);
        return ctx->result;
    case HTTP_EVENT_ON_FINISH:
        break;  /* Parse only after checking the final HTTP status. */
    case HTTP_EVENT_ERROR:
        if (ctx->result == ESP_OK) {
            ctx->result = ESP_FAIL;
        }
        break;
    default:
        ESP_LOGD(TAG, "HTTP_EVENT_UNKNOWN: %d", evt->event_id);
        break;
    }
    return ESP_OK;
}

static bool esp_xiaozhi_info_valid_header(const char *value)
{
    return !value || !strpbrk(value, "\r\n");
}

static bool esp_xiaozhi_info_valid_serial(const char *serial)
{
    if (!serial || !serial[0] || strlen(serial) > 32) {
        return false;
    }
    bool nonblank = false;
    for (const unsigned char *c = (const unsigned char *)serial; *c; c++) {
        if (*c < 0x20 || *c > 0x7e) {
            return false;
        }
        nonblank |= *c != ' ';
    }
    return nonblank;
}

static bool esp_xiaozhi_info_is_valid_url(const char *url)
{
    if (!url) {
        return false;
    }
    size_t prefix = !strncmp(url, "https://", 8) ? 8 : !strncmp(url, "http://", 7) ? 7 : 0;
    if (!prefix || !url[prefix] || strchr("/?#", url[prefix]) || strpbrk(url, "?#")) {
        return false;
    }
    for (const unsigned char *c = (const unsigned char *)url; *c; c++) {
        if (isspace(*c) || iscntrl(*c)) {
            return false;
        }
    }
    return true;
}

static esp_err_t esp_xiaozhi_info_get_url(const esp_xiaozhi_chat_bootstrap_config_t *config, char *url, size_t capacity)
{
    esp_err_t ret = ESP_OK;
    if (config && config->url) {
        if (strlen(config->url) >= capacity) {
            return ESP_ERR_INVALID_SIZE;
        }
        strcpy(url, config->url);
    } else {
        esp_xiaozhi_chat_keystore_t store = {0};
        ret = esp_xiaozhi_chat_keystore_init(&store, "wifi", false);
        if (ret == ESP_OK) {
            ret = esp_xiaozhi_chat_keystore_get_string(&store, "ota_url", CONFIG_XIAOZHI_OTA_URL, url, capacity);
            esp_xiaozhi_chat_keystore_deinit(&store);
            if (ret == ESP_OK && !esp_xiaozhi_info_is_valid_url(url)) {
                ESP_LOGW(TAG, "Ignoring invalid saved OTA URL; using default");
                if (strlen(CONFIG_XIAOZHI_OTA_URL) >= capacity) {
                    return ESP_ERR_INVALID_SIZE;
                }
                strcpy(url, CONFIG_XIAOZHI_OTA_URL);
            }
        } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
            if (strlen(CONFIG_XIAOZHI_OTA_URL) >= capacity) {
                return ESP_ERR_INVALID_SIZE;
            }
            strcpy(url, CONFIG_XIAOZHI_OTA_URL);
            ret = ESP_OK;
        }
    }
    if (ret == ESP_OK && !esp_xiaozhi_info_is_valid_url(url)) {
        return ESP_ERR_INVALID_ARG;
    }
    return ret;
}

static esp_err_t esp_xiaozhi_info_perform(const char *url, const char *payload,
                                          const esp_xiaozhi_chat_board_info_t *board,
                                          const char *serial, const esp_xiaozhi_chat_bootstrap_config_t *config,
                                          esp_xiaozhi_chat_http_ctx_t *response, int *status)
{
    const esp_app_desc_t *app = esp_app_get_description();
    char user_agent[128];
    snprintf(user_agent, sizeof(user_agent), "%s/%s", CONFIG_IDF_TARGET, app->version);
    const char *client_id = config && config->client_id ? config->client_id : board->uuid;
    const char *device_id = config && config->device_id ? config->device_id : board->mac_address;
    const char *agent = config && config->user_agent ? config->user_agent : user_agent;
    const char *language = config && config->language ? config->language : "zh-CN";
    if (!client_id[0] || !device_id[0] || !esp_xiaozhi_info_valid_header(client_id) ||
            !esp_xiaozhi_info_valid_header(device_id) || !esp_xiaozhi_info_valid_header(agent) ||
            !esp_xiaozhi_info_valid_header(language)) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_http_client_config_t http_config = {
        .url = url, .event_handler = esp_xiaozhi_chat_http_event, .user_data = response,
        .crt_bundle_attach = esp_crt_bundle_attach, .method = HTTP_METHOD_POST,
        .timeout_ms = CONFIG_XIAOZHI_INFO_TIMEOUT_MS,
        .disable_auto_redirect = true,
    };
    esp_http_client_handle_t http = esp_http_client_init(&http_config);
    if (!http) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t ret = esp_http_client_set_header(http, "Client-Id", client_id);
    if (ret == ESP_OK) {
        ret = esp_http_client_set_header(http, "Device-Id", device_id);
    }
    if (ret == ESP_OK) {
        ret = esp_http_client_set_header(http, "User-Agent", agent);
    }
    if (ret == ESP_OK) {
        ret = esp_http_client_set_header(http, "Accept-Language", language);
    }
    if (ret == ESP_OK) {
        ret = esp_http_client_set_header(http, "Content-Type", "application/json");
    }
    if (ret == ESP_OK) {
        ret = esp_http_client_set_header(http, "Activation-Version", serial ? "2" : "1");
    }
    if (ret == ESP_OK && serial) {
        ret = esp_http_client_set_header(http, "Serial-Number", serial);
    }
    if (ret == ESP_OK) {
        ret = esp_http_client_set_post_field(http, payload, strlen(payload));
    }
    if (ret == ESP_OK) {
        ret = esp_http_client_perform(http);
        *status = esp_http_client_get_status_code(http);
    }
    if (ret == ESP_OK) {
        ret = response->result;
    }
    esp_http_client_cleanup(http);
    return ret;
}

static esp_err_t esp_xiaozhi_info_append_protocol(char **buffer_ptr)
{
    char *buffer = *buffer_ptr;
    char *start = buffer;
    while (*start && isspace((unsigned char) * start)) {
        start++;
    }
    size_t len = strlen(buffer);
    while (len > 0 && isspace((unsigned char)buffer[len - 1])) {
        len--;
    }
    if (*start != '{' || len == 0 || buffer[len - 1] != '}') {
        return ESP_ERR_INVALID_RESPONSE;
    }
    const char *first_prop = start + 1;
    while (first_prop < buffer + len - 1 && isspace((unsigned char) * first_prop)) {
        first_prop++;
    }
    bool empty_obj = (first_prop == buffer + len - 1);

    cJSON *protocol = cJSON_CreateObject();
    if (!protocol) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t ret = esp_xiaozhi_protocol_add_capabilities(protocol, false);
    char *proto_json = (ret == ESP_OK) ? cJSON_PrintUnformatted(protocol) : NULL;
    cJSON_Delete(protocol);
    if (ret != ESP_OK) {
        cJSON_free(proto_json);
        return ret;
    }
    if (!proto_json) {
        return ESP_ERR_NO_MEM;
    }

    size_t insert_pos = len - 1;
    size_t extra = (empty_obj ? 0 : 1) + strlen("\"protocol\":") + strlen(proto_json) + 1;
    char *grown = realloc(buffer, insert_pos + extra + 1);
    if (!grown) {
        cJSON_free(proto_json);
        return ESP_ERR_NO_MEM;
    }
    *buffer_ptr = grown;
    snprintf(grown + insert_pos, extra + 1, "%s\"protocol\":%s}", empty_obj ? "" : ",", proto_json);
    cJSON_free(proto_json);
    return ESP_OK;
}

esp_err_t esp_xiaozhi_chat_get_info_with_config(esp_xiaozhi_chat_info_t *info,
                                                const esp_xiaozhi_chat_bootstrap_config_t *config)
{
    ESP_RETURN_ON_FALSE(info, ESP_ERR_INVALID_ARG, TAG, "Invalid info");
    if (config && config->serial_number && !esp_xiaozhi_info_valid_serial(config->serial_number)) {
        return ESP_ERR_INVALID_ARG;
    }
    char *url = calloc(1, 1024);
    ESP_RETURN_ON_FALSE(url, ESP_ERR_NO_MEM, TAG, "No URL buffer");
    esp_err_t ret = esp_xiaozhi_info_get_url(config, url, 1024);
    if (ret != ESP_OK) {
        free(url);
        ESP_LOGE(TAG, "Invalid bootstrap endpoint: %s", esp_err_to_name(ret));
        return ret;
    }
    esp_xiaozhi_chat_board_info_t board = {0};
    ret = esp_xiaozhi_chat_get_board_info(&board);
    if (ret != ESP_OK) {
        free(url);
        ESP_LOGE(TAG, "Failed to collect device identity: %s", esp_err_to_name(ret));
        return ret;
    }
    char *buffer = calloc(1, CONFIG_XIAOZHI_INFO_BUFFER_SIZE);
    if (!buffer) {
        free(url);
        ESP_LOGE(TAG, "No bootstrap buffer");
        return ESP_ERR_NO_MEM;
    }
    ret = config && config->board_json ? config->board_json(config->ctx, buffer, CONFIG_XIAOZHI_INFO_BUFFER_SIZE) :
          esp_xiaozhi_chat_get_board_json(&board, buffer, CONFIG_XIAOZHI_INFO_BUFFER_SIZE);
    cJSON *root = NULL;
    char *payload = NULL;
    const char *send_payload = NULL;
    esp_xiaozhi_chat_http_ctx_t response = {.result = ESP_OK};
    if (ret == ESP_OK && !memchr(buffer, '\0', CONFIG_XIAOZHI_INFO_BUFFER_SIZE)) {
        ret = ESP_ERR_INVALID_SIZE;
    }
    if (ret == ESP_OK) {
        if (config && (config->board_json || config->client_id || config->device_id)) {
            root = cJSON_ParseWithOpts(buffer, NULL, true);
            free(buffer);
            buffer = NULL;
            if (!cJSON_IsObject(root)) {
                ret = ESP_ERR_INVALID_RESPONSE;
            } else {
                const char *client_id = config->client_id ? config->client_id : board.uuid;
                const char *device_id = config->device_id ? config->device_id : board.mac_address;
                cJSON_DeleteItemFromObjectCaseSensitive(root, "uuid");
                cJSON_DeleteItemFromObjectCaseSensitive(root, "mac_address");
                cJSON_DeleteItemFromObjectCaseSensitive(root, "protocol");
                if (!cJSON_AddStringToObject(root, "uuid", client_id) ||
                        !cJSON_AddStringToObject(root, "mac_address", device_id)) {
                    ret = ESP_ERR_NO_MEM;
                } else {
                    cJSON *protocol = cJSON_AddObjectToObject(root, "protocol");
                    ret = protocol ? esp_xiaozhi_protocol_add_capabilities(protocol, false) : ESP_ERR_NO_MEM;
                }
            }
            if (ret == ESP_OK) {
                payload = cJSON_PrintUnformatted(root);
                if (!payload) {
                    ret = ESP_ERR_NO_MEM;
                }
                send_payload = payload;
            }
            cJSON_Delete(root);
        } else {
            ret = esp_xiaozhi_info_append_protocol(&buffer);
            send_payload = buffer;
        }
    }
    if (ret == ESP_OK) {
        ret = str_replace(&info->current_version, esp_app_get_description()->version);
    }
    if (ret == ESP_OK) {
        str_free(&info->serial_number);
        info->has_serial_number = config && config->serial_number;
        if (info->has_serial_number) {
            ret = str_replace(&info->serial_number, config->serial_number);
        }
    }
    if (ret == ESP_OK) {
        int status = 0;
        ret = esp_xiaozhi_info_perform(url, send_payload, &board, info->serial_number, config, &response, &status);
        if (ret == ESP_OK) {
            ret = status == 200 && response.output_len ?
                  esp_xiaozhi_info_parse_response(response.output_buffer, response.output_len, info) : ESP_ERR_INVALID_RESPONSE;
        }
        if (ret == ESP_OK && config && config->on_response) {
            config->on_response(config->ctx, response.output_buffer, response.output_len);
        }
    }
    free(url);
    free(buffer);
    cJSON_free(payload);
    esp_xiaozhi_chat_http_ctx_cleanup(&response);
    return ret;
}

esp_err_t esp_xiaozhi_chat_get_info(esp_xiaozhi_chat_info_t *info)
{
    return esp_xiaozhi_chat_get_info_with_config(info, NULL);
}

esp_err_t esp_xiaozhi_info_build_activation(const esp_xiaozhi_chat_info_t *info,
                                            const esp_xiaozhi_chat_bootstrap_config_t *config, char **payload)
{
    if (!info || !payload || !info->has_activation_challenge || !info->activation_challenge) {
        return ESP_ERR_INVALID_ARG;
    }
    *payload = NULL;
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t ret = ESP_OK;
    const char *serial = config && config->serial_number ? config->serial_number :
                         info->has_serial_number ? info->serial_number : NULL;
    if (serial) {
        if (!esp_xiaozhi_info_valid_serial(serial)) {
            ret = ESP_ERR_INVALID_ARG;
        } else if (!config || !config->sign_challenge) {
            ret = ESP_ERR_NOT_SUPPORTED;
        } else {
            uint8_t hmac[32] = {0};
            ret = config->sign_challenge(config->ctx, (const uint8_t *)info->activation_challenge,
                                         strlen(info->activation_challenge), hmac);
            if (ret == ESP_OK) {
                char hex[65];
                for (size_t i = 0; i < sizeof(hmac); i++) {
                    snprintf(hex + i * 2, 3, "%02x", hmac[i]);
                }
                if (!cJSON_AddStringToObject(root, "algorithm", "hmac-sha256") ||
                        !cJSON_AddStringToObject(root, "serial_number", serial) ||
                        !cJSON_AddStringToObject(root, "challenge", info->activation_challenge) ||
                        !cJSON_AddStringToObject(root, "hmac", hex)) {
                    ret = ESP_ERR_NO_MEM;
                }
            }
            memset(hmac, 0, sizeof(hmac));
        }
    }
    if (ret == ESP_OK) {
        *payload = cJSON_PrintUnformatted(root);
        ret = *payload ? ESP_OK : ESP_ERR_NO_MEM;
    }
    cJSON_Delete(root);
    return ret;
}

esp_err_t esp_xiaozhi_chat_activate(const esp_xiaozhi_chat_info_t *info,
                                    const esp_xiaozhi_chat_bootstrap_config_t *config)
{
    char *payload = NULL;
    esp_err_t ret = esp_xiaozhi_info_build_activation(info, config, &payload);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to build activation request");
    char *url = calloc(1, 1024);
    if (!url) {
        cJSON_free(payload);
        return ESP_ERR_NO_MEM;
    }
    ret = esp_xiaozhi_info_get_url(config, url, 1024 - sizeof("/activate"));
    esp_xiaozhi_chat_board_info_t board = {0};
    if (ret == ESP_OK) {
        size_t length = strlen(url);
        strcat(url, url[length - 1] == '/' ? "activate" : "/activate");
        ret = esp_xiaozhi_chat_get_board_info(&board);
    }
    esp_xiaozhi_chat_http_ctx_t response = {.result = ESP_OK};
    if (ret == ESP_OK) {
        const char *serial = config && config->serial_number ? config->serial_number :
                             info->has_serial_number ? info->serial_number : NULL;
        int status = 0;
        ret = esp_xiaozhi_info_perform(url, payload, &board, serial, config, &response, &status);
        if (ret == ESP_OK) {
            ret = status == 200 ? ESP_OK : status == 202 ? ESP_ERR_TIMEOUT : ESP_FAIL;
        }
    }
    free(url);
    cJSON_free(payload);
    esp_xiaozhi_chat_http_ctx_cleanup(&response);
    return ret;
}

esp_err_t esp_xiaozhi_chat_free_info(esp_xiaozhi_chat_info_t *info)
{
    ESP_RETURN_ON_FALSE(info, ESP_ERR_INVALID_ARG, TAG, "Invalid info");

    str_free(&info->current_version);
    str_free(&info->firmware_version);
    str_free(&info->firmware_url);
    str_free(&info->serial_number);
    str_free(&info->activation_code);
    str_free(&info->activation_challenge);
    str_free(&info->activation_message);

    return ESP_OK;
}
