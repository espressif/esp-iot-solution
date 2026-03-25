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

static const char *TAG = "ESP_XIAOZHI_INFO";

typedef struct {
    esp_xiaozhi_chat_info_t *info;
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
            } else if (cJSON_IsNumber(item)) {
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
    if (!version || !count) {
        return NULL;
    }

    *count = 0;

    size_t dots = 0;
    for (const char *p = version; *p; p++) {
        if (*p == '.') {
            dots++;
        }
    }

    size_t components = dots + 1;
    int *version_numbers = malloc(components * sizeof(int));
    if (!version_numbers) {
        return NULL;
    }

    char *version_copy = str_dup(version);
    if (!version_copy) {
        free(version_numbers);
        return NULL;
    }

    char *token = strtok(version_copy, ".");
    size_t i = 0;

    while (token && i < components) {
        version_numbers[i] = atoi(token);
        token = strtok(NULL, ".");
        i++;
    }

    *count = i;
    free(version_copy);
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

static esp_err_t esp_xiaozhi_chat_http_data_handler(const char *data, size_t data_len, esp_xiaozhi_chat_info_t *info)
{
    ESP_RETURN_ON_FALSE(data != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid HTTP data");
    ESP_RETURN_ON_FALSE(info != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid info");

    esp_err_t ret = ESP_OK;
    cJSON *root = cJSON_ParseWithLength(data, data_len);
    ESP_RETURN_ON_FALSE(root, ESP_ERR_INVALID_RESPONSE, TAG, "Failed to parse JSON response");

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
            cJSON *timezone_offset = cJSON_GetObjectItem(server_time, "timezone_offset");

            if (cJSON_IsNumber(timestamp)) {
#if CONFIG_XIAOZHI_SYNC_SYSTEM_TIME_FROM_SERVER
                struct timeval tv;
                double ts = timestamp->valuedouble;
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
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        if (ctx->result != ESP_OK) {
            return ctx->result;
        }
        if (ctx->output_buffer == NULL || ctx->output_len == 0) {
            ctx->result = ESP_ERR_INVALID_RESPONSE;
            ESP_LOGE(TAG, "HTTP response body is empty");
        } else {
            ctx->result = esp_xiaozhi_chat_http_data_handler(ctx->output_buffer, ctx->output_len, ctx->info);
            if (ctx->result != ESP_OK) {
                ESP_LOGE(TAG, "Failed to handle HTTP response: %s", esp_err_to_name(ctx->result));
            }
        }
        return ctx->result;
    case HTTP_EVENT_ERROR:
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        int mbedtls_err = 0;
        esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
        if (err != 0) {
            ESP_LOGE(TAG, "Last esp error code: 0x%x", err);
            ESP_LOGE(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
        }
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        esp_http_client_set_redirection(evt->client);
        break;
    default:
        ESP_LOGD(TAG, "HTTP_EVENT_UNKNOWN: %d", evt->event_id);
        break;
    }
    return ESP_OK;
}

esp_err_t esp_xiaozhi_chat_get_info(esp_xiaozhi_chat_info_t *info)
{
    ESP_RETURN_ON_FALSE(info, ESP_ERR_INVALID_ARG, TAG, "Invalid info");

    esp_err_t ret = ESP_OK;
    esp_http_client_handle_t http_client = NULL;
    esp_xiaozhi_chat_board_info_t board_info;
    char *json_buffer = NULL;
    esp_xiaozhi_chat_http_ctx_t http_ctx = {
        .info = info,
        .result = ESP_OK,
    };

    do {
        ret = esp_xiaozhi_chat_get_board_info(&board_info);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize board: %s", esp_err_to_name(ret));
            break;
        }

        json_buffer = calloc(1, (size_t)CONFIG_XIAOZHI_INFO_BUFFER_SIZE);
        if (json_buffer == NULL) {
            ret = ESP_ERR_NO_MEM;
            ESP_LOGE(TAG, "Failed to allocate memory for JSON buffer");
            break;
        }

        ret = esp_xiaozhi_chat_get_board_json(&board_info, json_buffer, (uint16_t)CONFIG_XIAOZHI_INFO_BUFFER_SIZE);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get full JSON, status: %s", esp_err_to_name(ret));
            break;
        }

        esp_http_client_config_t http_client_cfg = {
            .url = CONFIG_XIAOZHI_OTA_URL,
            .event_handler = esp_xiaozhi_chat_http_event,
            .user_data = &http_ctx,
            .crt_bundle_attach = esp_crt_bundle_attach,
            .method = HTTP_METHOD_POST,
            .timeout_ms = CONFIG_XIAOZHI_INFO_TIMEOUT_MS,
        };

        http_client = esp_http_client_init(&http_client_cfg);
        if (!http_client) {
            ret = ESP_ERR_NO_MEM;
            ESP_LOGE(TAG, "Failed to initialize HTTP client");
            break;
        }

        esp_http_client_set_header(http_client, "Client-Id", board_info.uuid);
        esp_http_client_set_header(http_client, "Device-Id", board_info.mac_address);
        esp_http_client_set_header(http_client, "User-Agent", CONFIG_IDF_TARGET);
        esp_http_client_set_header(http_client, "Content-Type", "application/json");

        esp_http_client_set_post_field(http_client, json_buffer, strlen(json_buffer));
        while (1) {
            ret = esp_http_client_perform(http_client);
            if (ret != ESP_ERR_HTTP_EAGAIN) {
                break;
            }
        }
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(ret));
            break;
        }
        if (http_ctx.result != ESP_OK) {
            ret = http_ctx.result;
            ESP_LOGE(TAG, "Failed to handle HTTP response: %s", esp_err_to_name(ret));
            break;
        }
    } while (0);

    esp_http_client_cleanup(http_client);
    esp_xiaozhi_chat_http_ctx_cleanup(&http_ctx);
    free(json_buffer);
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
