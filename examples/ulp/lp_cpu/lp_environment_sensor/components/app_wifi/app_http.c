/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "protocol_examples_utils.h"
#include "app_http.h"

static const char *TAG = "app-http";

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer;  // Buffer to store response of http request from event handler
    static int output_len;       // Stores number of bytes read
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        // Clean the buffer in case of a new request
        if (output_len == 0 && evt->user_data) {
            memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
        }
        int copy_len = 0;
        if (evt->user_data) {
            char *user_buffer = (char *)evt->user_data;
            /*
             * Reserve one byte for the NUL terminator.  Returning an error
             * on overflow prevents truncated JSON from being parsed.
             */
            if (output_len < 0 || output_len >= MAX_HTTP_OUTPUT_BUFFER) {
                ESP_LOGE(TAG, "HTTP response buffer state is invalid");
                return ESP_ERR_NO_MEM;
            }
            int available = MAX_HTTP_OUTPUT_BUFFER - 1 - output_len;
            if (available < 0) {
                available = 0;
            }
            copy_len = evt->data_len < available ? evt->data_len : available;
            if (copy_len > 0) {
                memcpy(user_buffer + output_len, evt->data, copy_len);
            }
            output_len += copy_len;
            user_buffer[output_len] = '\0';
            if (copy_len != evt->data_len) {
                ESP_LOGE(TAG, "HTTP response exceeds the configured buffer");
                return ESP_ERR_NO_MEM;
            }
        } else {
            int content_len = esp_http_client_get_content_length(evt->client);
            if (content_len < 0 || content_len > MAX_HTTP_OUTPUT_BUFFER - 1) {
                ESP_LOGE(TAG, "HTTP response length is invalid or too large");
                return ESP_ERR_NO_MEM;
            }
            if (output_buffer == NULL) {
                output_buffer = (char *)calloc((size_t)content_len + 1, sizeof(char));
                output_len = 0;
                if (output_buffer == NULL) {
                    ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                    return ESP_ERR_NO_MEM;
                }
            }
            if (output_len < 0 || output_len > content_len) {
                ESP_LOGE(TAG, "HTTP response buffer state is invalid");
                return ESP_ERR_NO_MEM;
            }
            int available = content_len - output_len;
            copy_len = evt->data_len < available ? evt->data_len : available;
            if (copy_len > 0) {
                memcpy(output_buffer + output_len, evt->data, copy_len);
            }
            output_len += copy_len;
            output_buffer[output_len] = '\0';
            if (copy_len != evt->data_len) {
                ESP_LOGE(TAG, "HTTP response exceeds the configured buffer");
                return ESP_ERR_NO_MEM;
            }
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        if (output_buffer != NULL) {
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        int mbedtls_err = 0;
        esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
        if (err != 0) {
            ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
            ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
        }
        if (output_buffer != NULL) {
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        esp_http_client_set_header(evt->client, "From", "user@example.com");
        esp_http_client_set_header(evt->client, "Accept", "text/html");
        esp_http_client_set_redirection(evt->client);
        break;
    }
    return ESP_OK;
}

static esp_err_t copy_json_string(char *destination, size_t destination_size, const cJSON *item)
{
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        return ESP_FAIL;
    }
    if (strlen(item->valuestring) >= destination_size) {
        return ESP_ERR_NO_MEM;
    }
    memcpy(destination, item->valuestring, strlen(item->valuestring) + 1);
    return ESP_OK;
}

esp_err_t get_http_weather_data_with_url(http_weather_data_t *weather_data)
{
    esp_err_t ret = ESP_OK;
    char *local_response_buffer = NULL;
    char weather_http_url[256] = {0};
    http_weather_data_t parsed_weather = {0};
    esp_http_client_handle_t client = NULL;
    cJSON *root = NULL;

    if (weather_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    local_response_buffer = calloc(MAX_HTTP_OUTPUT_BUFFER + 1, sizeof(*local_response_buffer));
    if (local_response_buffer == NULL) {
        return ESP_ERR_NO_MEM;
    }
    int district_len = snprintf(parsed_weather.district_id, sizeof(parsed_weather.district_id), "%s",
                                CONFIG_CITY_DISTRICT_ID);
    if (district_len < 0 || district_len >= (int)sizeof(parsed_weather.district_id)) {
        ret = ESP_ERR_INVALID_SIZE;
        goto cleanup;
    }
#ifdef CONFIG_REGION_INTERNATIONAL
    int url_len = snprintf(weather_http_url, sizeof(weather_http_url),
                           "https://api.map.baidu.com/weather_abroad/v1/?district_id=%s"
                           "&data_type=all&language=en&ak=uKBj61fKPRDbXzv5w3ecFaVove3ZqwlT",
                           parsed_weather.district_id);
#elif CONFIG_REGION_CHINA
    int url_len = snprintf(weather_http_url, sizeof(weather_http_url),
                           "https://api.map.baidu.com/weather/v1/?district_id=%s"
                           "&data_type=all&ak=uKBj61fKPRDbXzv5w3ecFaVove3ZqwlT",
                           parsed_weather.district_id);
#else
    int url_len = -1;
#endif
    if (url_len < 0 || url_len >= (int)sizeof(weather_http_url)) {
        ret = ESP_ERR_INVALID_ARG;
        goto cleanup;
    }

    esp_http_client_config_t config = {
        .url = weather_http_url,
        .event_handler = _http_event_handler,
        .user_data = local_response_buffer,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .disable_auto_redirect = true,
        .timeout_ms = 15000,
    };
    client = esp_http_client_init(&config);
    if (client == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    ret = esp_http_client_perform(client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    if (esp_http_client_get_status_code(client) != 200) {
        ESP_LOGE(TAG, "HTTP GET request returned status %d",
                 esp_http_client_get_status_code(client));
        ret = ESP_FAIL;
        goto cleanup;
    }

    root = cJSON_Parse(local_response_buffer);
    if (!cJSON_IsObject(root)) {
        ret = ESP_ERR_INVALID_RESPONSE;
        goto cleanup;
    }
    cJSON *status = cJSON_GetObjectItem(root, "status");
    if (!cJSON_IsNumber(status) || status->valueint != 0) {
        ret = ESP_ERR_INVALID_RESPONSE;
        goto cleanup;
    }
    cJSON *result = cJSON_GetObjectItem(root, "result");
    if (!cJSON_IsObject(result)) {
        ret = ESP_FAIL;
        goto cleanup;
    }
    cJSON *location = cJSON_GetObjectItem(result, "location");
    cJSON *now = cJSON_GetObjectItem(result, "now");
    cJSON *forecasts = cJSON_GetObjectItem(result, "forecasts");
    if (!cJSON_IsObject(location) || !cJSON_IsObject(now) || !cJSON_IsArray(forecasts) ||
        cJSON_GetArraySize(forecasts) < 1) {
        ret = ESP_FAIL;
        goto cleanup;
    }
    cJSON *first_forecast = cJSON_GetArrayItem(forecasts, 0);
    if (!cJSON_IsObject(first_forecast)) {
        ret = ESP_FAIL;
        goto cleanup;
    }

    ret = copy_json_string(parsed_weather.city_name, sizeof(parsed_weather.city_name),
                           cJSON_GetObjectItem(location, "city"));
    if (ret != ESP_OK) {
        goto cleanup;
    }
    ret = copy_json_string(parsed_weather.district_name, sizeof(parsed_weather.district_name),
                           cJSON_GetObjectItem(location, "name"));
    if (ret != ESP_OK) {
        goto cleanup;
    }
    ret = copy_json_string(parsed_weather.text, sizeof(parsed_weather.text),
                           cJSON_GetObjectItem(now, "text"));
    if (ret != ESP_OK) {
        goto cleanup;
    }
    ret = copy_json_string(parsed_weather.wind_class, sizeof(parsed_weather.wind_class),
                           cJSON_GetObjectItem(now, "wind_class"));
    if (ret != ESP_OK) {
        goto cleanup;
    }
    ret = copy_json_string(parsed_weather.wind_dir, sizeof(parsed_weather.wind_dir),
                           cJSON_GetObjectItem(now, "wind_dir"));
    if (ret != ESP_OK) {
        goto cleanup;
    }
    ret = copy_json_string(parsed_weather.uptime, sizeof(parsed_weather.uptime),
                           cJSON_GetObjectItem(now, "uptime"));
    if (ret != ESP_OK) {
        goto cleanup;
    }
    ret = copy_json_string(parsed_weather.week, sizeof(parsed_weather.week),
                           cJSON_GetObjectItem(first_forecast, "week"));
    if (ret != ESP_OK) {
        goto cleanup;
    }

    cJSON *high = cJSON_GetObjectItem(first_forecast, "high");
    cJSON *low = cJSON_GetObjectItem(first_forecast, "low");
    if (!cJSON_IsNumber(high) || !cJSON_IsNumber(low)) {
        ret = ESP_FAIL;
        goto cleanup;
    }
    parsed_weather.temp_high = high->valueint;
    parsed_weather.temp_low = low->valueint;

cleanup:
    if (root != NULL) {
        cJSON_Delete(root);
    }
    if (client != NULL) {
        esp_err_t cleanup_ret = esp_http_client_cleanup(client);
        if (ret == ESP_OK && cleanup_ret != ESP_OK) {
            ESP_LOGE(TAG, "HTTP cleanup failed: %s", esp_err_to_name(cleanup_ret));
            ret = cleanup_ret;
        }
    }
    free(local_response_buffer);
    if (ret == ESP_OK) {
        *weather_data = parsed_weather;
    }
    return ret;
}
