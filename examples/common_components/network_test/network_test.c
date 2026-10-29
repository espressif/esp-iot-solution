/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_timer.h"
#include "esp_check.h"
#include "ping/ping_sock.h"
#include "network_test.h"

static const char *TAG = "Network Test";

static esp_ping_handle_t s_ping = NULL;
static TaskHandle_t s_ping_task = NULL;
static volatile bool s_ping_task_running = false;

static void on_ping_success(esp_ping_handle_t hdl, void *args)
{
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    ESP_LOGI(TAG, "%"PRIu32" bytes from %s icmp_seq=%u ttl=%u time=%"PRIu32" ms\n", recv_len, ipaddr_ntoa(&target_addr), seqno, ttl, elapsed_time);
}

static void on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    uint16_t seqno;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    ESP_LOGW(TAG, "From %s icmp_seq=%u timeout\n", ipaddr_ntoa(&target_addr), seqno);
}

static esp_ping_handle_t ping_create()
{
    ip_addr_t target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    char *ping_addr_s = NULL;
    ping_addr_s = "8.8.8.8";
    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ipaddr_aton(ping_addr_s, &target_addr);
    ping_config.target_addr = target_addr;
    ping_config.timeout_ms = 2000;
    ping_config.task_stack_size = 4096;
    ping_config.count = 1;

    /* set callback functions */
    esp_ping_callbacks_t cbs = {
        .on_ping_success = on_ping_success,
        .on_ping_timeout = on_ping_timeout,
        .on_ping_end = NULL,
        .cb_args = NULL,
    };
    esp_ping_handle_t ping;
    esp_ping_new_session(&ping_config, &cbs, &ping);
    return ping;
}

// Periodic ping using FreeRTOS task. Ping starts only when IP is available.
static void ping_task(void *pvParameters)
{
    const TickType_t delay_ticks = pdMS_TO_TICKS(5000);

    while (s_ping_task_running) {
        if (s_ping) {
            esp_ping_start(s_ping);
            ESP_LOGI(TAG, "Network is connected, starting ping...");
        }
        vTaskDelay(delay_ticks);
    }

    s_ping_task = NULL;
    ESP_LOGI(TAG, "Ping task stopped");
    vTaskDelete(NULL);
}

esp_err_t start_ping_timer(void)
{
    if (s_ping == NULL) {
        s_ping = ping_create();
        ESP_RETURN_ON_FALSE(s_ping != NULL, ESP_FAIL, TAG, "Failed to create ping");
    }
    if (s_ping_task == NULL) {
        s_ping_task_running = true;
        BaseType_t ok = xTaskCreate(ping_task, "ping_periodic", 4096, NULL, 5, &s_ping_task);
        ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_FAIL, TAG, "Failed to create ping task");
        ESP_LOGI(TAG, "Ping task started");
    }
    return ESP_OK;
}

esp_err_t stop_ping_timer(void)
{
    if (s_ping_task) {
        s_ping_task_running = false;
        // We not wait the task to be deleted here
    }
    if (s_ping) {
        // Stop ongoing ping session if any
        esp_ping_stop(s_ping);
    }
    return ESP_OK;
}

static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static char *response_buffer = NULL;
    static int output_len = 0; // Total bytes read

    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        if (!esp_http_client_is_chunked_response(evt->client)) {
            response_buffer = realloc(response_buffer, output_len + evt->data_len + 1);
            memcpy(response_buffer + output_len, evt->data, evt->data_len);
            output_len += evt->data_len;
            response_buffer[output_len] = 0;
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        if (response_buffer) {
            ESP_LOGI(TAG, "Public IP: %s", response_buffer);
            free(response_buffer);
            response_buffer = NULL;
            output_len = 0;
        }
        break;
    case HTTP_EVENT_DISCONNECTED:
        if (response_buffer) {
            free(response_buffer);
            response_buffer = NULL;
            output_len = 0;
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

esp_err_t test_query_public_ip(void)
{
    esp_http_client_config_t config = {
        .url = "http://ifconfig.me/ip",
        .event_handler = _http_event_handler,
        .timeout_ms = 3000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    ESP_LOGI(TAG, "Querying public IP...");
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP GET status = %d", esp_http_client_get_status_code(client));
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }

    return esp_http_client_cleanup(client);
}

/**
 * @brief Download speed test context
 */
typedef struct {
    size_t total_size;
    size_t downloaded_size;
    uint64_t start_time;
    uint64_t last_update_time;
    size_t last_downloaded_size;
    int update_counter;
} speed_test_ctx_t;

/**
 * @brief HTTP event handler for speed test
 */
static esp_err_t speed_test_http_event_handler(esp_http_client_event_t *evt)
{
    speed_test_ctx_t *ctx = (speed_test_ctx_t *)evt->user_data;

    switch (evt->event_id) {
    case HTTP_EVENT_ON_CONNECTED:
        ctx->start_time = esp_timer_get_time();
        ctx->last_update_time = ctx->start_time;
        ctx->last_downloaded_size = 0;
        ctx->update_counter = 0;
        ESP_LOGI(TAG, "Connected, starting download...");
        break;

    case HTTP_EVENT_ON_HEADER:
        if (strcasecmp(evt->header_key, "Content-Length") == 0) {
            ctx->total_size = atoll(evt->header_value);
            ESP_LOGI(TAG, "File size: %zu bytes (%.2f MB)", ctx->total_size, ctx->total_size / (1024.0f * 1024.0f));
        }
        break;

    case HTTP_EVENT_ON_DATA:
        if (evt->data_len > 0) {
            ctx->downloaded_size += evt->data_len;
            ctx->update_counter++;

            if (ctx->update_counter % 100 == 0) {
                uint64_t current_time = esp_timer_get_time();
                uint64_t elapsed_us = current_time - ctx->last_update_time;

                if (elapsed_us > 500000) {
                    size_t bytes_diff = ctx->downloaded_size - ctx->last_downloaded_size;
                    float speed_mbs = bytes_diff / (elapsed_us / 1000000.0f) / (1024.0f * 1024.0f);
                    float progress = ctx->total_size > 0 ? (float)ctx->downloaded_size / ctx->total_size * 100.0f : 0.0f;

                    ESP_LOGI(TAG, "Progress: %zu/%zu KB (%.1f%%) | ↓Speed: %.2f MB/s",
                             ctx->downloaded_size / 1024, ctx->total_size / 1024, progress, speed_mbs);

                    ctx->last_update_time = current_time;
                    ctx->last_downloaded_size = ctx->downloaded_size;
                }
            }
        }
        break;

    case HTTP_EVENT_ON_FINISH: {
        uint64_t total_time_us = esp_timer_get_time() - ctx->start_time;
        float total_time_s = total_time_us / 1000000.0f;
        float avg_speed_mbs = ctx->downloaded_size / total_time_s / (1024.0f * 1024.0f);

        ESP_LOGI(TAG, "Completed: %zu bytes, %.2f s, avg speed: %.2f MB/s",
                 ctx->downloaded_size, total_time_s, avg_speed_mbs);
        break;
    }

    case HTTP_EVENT_ERROR:
        ESP_LOGE(TAG, "Download error occurred");
        return ESP_FAIL;

    default:
        break;
    }

    return ESP_OK;
}

esp_err_t test_download_speed(const char *url)
{
    if (!url) {
        ESP_LOGE(TAG, "URL is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    bool is_https = (strncmp(url, "https://", 8) == 0);
    ESP_LOGI(TAG, "Speed test: %s", url);

    speed_test_ctx_t ctx = {0};
    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 60000,
        .event_handler = speed_test_http_event_handler,
        .user_data = &ctx,
        .keep_alive_enable = true,
        .buffer_size = 16 * 1024,
    };

    if (is_https) {
        config.crt_bundle_attach = esp_crt_bundle_attach;
    }

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK && esp_http_client_get_status_code(client) != 200) {
        err = ESP_FAIL;
    }

    esp_http_client_cleanup(client);
    return err;
}

esp_err_t test_upload_speed(const char *url)
{
    if (!url) {
        ESP_LOGE(TAG, "URL is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    bool is_https = (strncmp(url, "https://", 8) == 0);
    ESP_LOGI(TAG, "Upload speed test: %s", url);

    // 20MB test data size
    const size_t test_data_size = 20 * 1024 * 1024;
    // Use 64KB buffer for streaming upload
    const size_t buffer_size = 16 * 1024;
    uint8_t *buffer = malloc(buffer_size);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        return ESP_ERR_NO_MEM;
    }
    memset(buffer, 0xAA, buffer_size);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_PUT,
        .timeout_ms = 300000, // 5 minutes for 200MB
        .keep_alive_enable = true,
        .buffer_size = 16 * 1024,
    };

    if (is_https) {
        config.crt_bundle_attach = esp_crt_bundle_attach;
    }

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        free(buffer);
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/octet-stream");

    uint64_t start_time = esp_timer_get_time();
    uint64_t last_update_time = start_time;
    size_t uploaded_size = 0;
    size_t last_uploaded_size = 0;
    int update_counter = 0;

    ESP_LOGI(TAG, "Starting upload: %zu bytes (%.2f MB)", test_data_size, test_data_size / (1024.0f * 1024.0f));

    esp_err_t err = esp_http_client_open(client, test_data_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP client: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        free(buffer);
        return err;
    }

    // Stream upload data in chunks
    while (uploaded_size < test_data_size) {
        size_t to_write = (test_data_size - uploaded_size > buffer_size) ? buffer_size : (test_data_size - uploaded_size);
        int written = esp_http_client_write(client, (const char *)buffer, to_write);

        if (written < 0) {
            ESP_LOGE(TAG, "Failed to write data");
            err = ESP_FAIL;
            break;
        }

        uploaded_size += written;
        update_counter++;

        // Update progress every 100 writes or every 500ms
        if (update_counter % 100 == 0) {
            uint64_t current_time = esp_timer_get_time();
            uint64_t elapsed_us = current_time - last_update_time;

            if (elapsed_us > 500000) {
                size_t bytes_diff = uploaded_size - last_uploaded_size;
                float speed_mbs = bytes_diff / (elapsed_us / 1000000.0f) / (1024.0f * 1024.0f);
                float progress = (float)uploaded_size / test_data_size * 100.0f;

                ESP_LOGI(TAG, "Progress: %zu/%zu MB (%.1f%%) | ↑Speed: %.2f MB/s",
                         uploaded_size / (1024 * 1024), test_data_size / (1024 * 1024), progress, speed_mbs);

                last_update_time = current_time;
                last_uploaded_size = uploaded_size;
            }
        }
    }

    if (err == ESP_OK) {
        // Fetch response headers to get status code
        int content_length = esp_http_client_fetch_headers(client);
        if (content_length < 0) {
            ESP_LOGW(TAG, "Failed to fetch headers, content_length: %d", content_length);
        }

        int status_code = esp_http_client_get_status_code(client);

        if (status_code == 200 || status_code == 201 || status_code == 204) {
            uint64_t total_time_us = esp_timer_get_time() - start_time;
            float total_time_s = total_time_us / 1000000.0f;
            float avg_speed_mbs = uploaded_size / total_time_s / (1024.0f * 1024.0f);

            ESP_LOGI(TAG, "Completed: %zu bytes, %.2f s, avg speed: %.2f MB/s",
                     uploaded_size, total_time_s, avg_speed_mbs);
        } else {
            ESP_LOGE(TAG, "HTTP status: %d", status_code);
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "Upload failed: %s", esp_err_to_name(err));
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    free(buffer);
    return err;
}
