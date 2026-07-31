/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "airmouse_server.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define CONFIG_DONE_BIT    BIT2
#define CONFIG_BODY_MAX_LEN    2048
#define HTTP_SERVER_STOP_DRAIN_DELAY_MS  120
#define WIFI_DISCONNECT_DRAIN_DELAY_MS   60
#define WIFI_STOP_DRAIN_DELAY_MS         120
#define RUNTIME_NOTICE_MAX_LEN           192
#define RUNTIME_NOTICE_MODE_MAX_LEN      32
#define RUNTIME_NOTICE_LEVEL_MAX_LEN     16

static EventGroupHandle_t s_wifi_event_group = NULL;
static EventGroupHandle_t s_config_event_group = NULL;
static httpd_handle_t s_http_server = NULL;
static const char *TAG = "AIRMOUSE server";
static int s_retry_num = 0;
static esp_netif_t *s_wifi_netif = NULL;
static esp_event_handler_instance_t s_wifi_event_instance_any_id;
static esp_event_handler_instance_t s_ip_event_instance_got_ip;
static bool s_wifi_handlers_registered = false;
static bool s_wifi_started = false;
static bool s_wifi_initialized = false;
static bool s_runtime_notice_active = false;
static char s_runtime_notice_mode[RUNTIME_NOTICE_MODE_MAX_LEN] = "";
static char s_runtime_notice_level[RUNTIME_NOTICE_LEVEL_MAX_LEN] = "";
static char s_runtime_notice_message[RUNTIME_NOTICE_MAX_LEN] = "";

static airmouse_config_t *s_airmouse_config = NULL;

extern const unsigned char index_html_start[] asm("_binary_index_html_start");
extern const unsigned char index_html_end[] asm("_binary_index_html_end");

extern const unsigned char app_js_start[] asm("_binary_app_js_start");
extern const unsigned char app_js_end[] asm("_binary_app_js_end");

extern const unsigned char style_css_start[] asm("_binary_style_css_start");
extern const unsigned char style_css_end[] asm("_binary_style_css_end");

static esp_err_t airmouse_http_root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);

}

static esp_err_t airmouse_http_app_js_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/javascript");
    return httpd_resp_send(req,
                           (const char *)app_js_start,
                           app_js_end - app_js_start);
}

static esp_err_t airmouse_http_style_css_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/css");
    return httpd_resp_send(req,
                           (const char *)style_css_start,
                           style_css_end - style_css_start);
}

static esp_err_t airmouse_config_get_handler(httpd_req_t *req)
{
    char *json = NULL;
    esp_err_t ret = ESP_OK;

    if (s_airmouse_config == NULL) {
        httpd_resp_send_err(req,
                            HTTPD_500_INTERNAL_SERVER_ERROR,
                            "config is null");
        return ESP_FAIL;
    }

    ret = airmouse_config_export_json(s_airmouse_config, &json);
    if (ret != ESP_OK || json == NULL) {
        httpd_resp_send_err(req,
                            HTTPD_500_INTERNAL_SERVER_ERROR,
                            "failed to export config");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    ret = httpd_resp_sendstr(req, json);
    free(json);
    return ret;
}

static esp_err_t airmouse_runtime_state_get_handler(httpd_req_t *req)
{
    char response[320];
    int written = snprintf(response,
                           sizeof(response),
                           "{\"active\":%s,\"mode\":\"%s\",\"level\":\"%s\",\"message\":\"%s\"}",
                           s_runtime_notice_active ? "true" : "false",
                           s_runtime_notice_mode,
                           s_runtime_notice_level,
                           s_runtime_notice_message);
    if (written < 0 || written >= (int)sizeof(response)) {
        httpd_resp_send_err(req,
                            HTTPD_500_INTERNAL_SERVER_ERROR,
                            "failed to build runtime state");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, response);
}

static esp_err_t airmouse_config_post_handler(httpd_req_t *req)
{
    int remaining = req->content_len;
    int total_received = 0;
    char *buf = NULL;

    if (s_airmouse_config == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "config is null");
        return ESP_FAIL;
    }

    if (remaining <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty config");
        return ESP_FAIL;
    }

    if (remaining >= CONFIG_BODY_MAX_LEN) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "config too large");
        return ESP_FAIL;
    }

    buf = (char *)calloc((size_t)remaining + 1, sizeof(char));
    if (buf == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no memory for config");
        return ESP_FAIL;
    }

    while (remaining > 0) {
        int received = httpd_req_recv(req, buf + total_received, remaining);
        if (received == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }

        if (received <= 0) {
            ESP_LOGE(TAG, "Failed to receive config body: %d", received);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "failed to receive config");
            free(buf);
            return ESP_FAIL;
        }

        total_received += received;
        remaining -= received;
    }

    buf[total_received] = '\0';

    esp_err_t ret = airmouse_config_save(buf, s_airmouse_config);
    free(buf);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid config");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    ret = httpd_resp_sendstr(req, "{\"ok\":true}");
    if (ret != ESP_OK) {
        return ret;
    }

    xEventGroupSetBits(s_config_event_group, CONFIG_DONE_BIT);
    return ESP_OK;
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *sta_disconnect_evt = (wifi_event_sta_disconnected_t*)event_data;
        ESP_LOGI(TAG, "wifi disconnect reason: %d", sta_disconnect_evt->reason);
        if (s_retry_num < 3) {
            esp_wifi_connect();
            s_retry_num++;
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static esp_err_t airmouse_wifi_cleanup_driver(bool log_errors)
{
    esp_err_t ret = ESP_OK;

    if (s_wifi_started || s_wifi_initialized) {
        esp_err_t disconnect_ret = esp_wifi_disconnect();
        if (disconnect_ret == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(WIFI_DISCONNECT_DRAIN_DELAY_MS));
        } else if (disconnect_ret != ESP_ERR_WIFI_NOT_INIT &&
                   disconnect_ret != ESP_ERR_WIFI_NOT_STARTED &&
                   log_errors) {
            ESP_LOGW(TAG, "wifi cleanup: disconnect failed: 0x%x",
                     (unsigned int)disconnect_ret);
            ret = disconnect_ret;
        }

        esp_err_t stop_ret = esp_wifi_stop();
        if (stop_ret == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(WIFI_STOP_DRAIN_DELAY_MS));
        } else if (stop_ret != ESP_ERR_WIFI_NOT_INIT &&
                   stop_ret != ESP_ERR_WIFI_NOT_STARTED &&
                   stop_ret != ESP_ERR_WIFI_STATE &&
                   log_errors) {
            ESP_LOGW(TAG, "wifi cleanup: stop failed: 0x%x",
                     (unsigned int)stop_ret);
            ret = stop_ret;
        }

        esp_err_t deinit_ret = esp_wifi_deinit();
        if (deinit_ret != ESP_OK &&
                deinit_ret != ESP_ERR_WIFI_NOT_INIT &&
                log_errors) {
            ESP_LOGW(TAG, "wifi cleanup: deinit failed: 0x%x",
                     (unsigned int)deinit_ret);
            ret = deinit_ret;
        }
    }

    s_wifi_started = false;
    s_wifi_initialized = false;
    return ret;
}

static void airmouse_wifi_cleanup_state(void)
{
    if (s_wifi_handlers_registered) {
        (void)esp_event_handler_instance_unregister(
            WIFI_EVENT, ESP_EVENT_ANY_ID, s_wifi_event_instance_any_id);
        (void)esp_event_handler_instance_unregister(
            IP_EVENT, IP_EVENT_STA_GOT_IP, s_ip_event_instance_got_ip);
        s_wifi_handlers_registered = false;
    }

    if (s_wifi_netif != NULL) {
        esp_netif_destroy(s_wifi_netif);
        s_wifi_netif = NULL;
    }

    if (s_wifi_event_group != NULL) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }

    s_retry_num = 0;
}

esp_err_t airmouse_wifi_init_sta(void)
{
    if (s_wifi_started) {
        return ESP_OK;
    }

    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK) {
        airmouse_wifi_cleanup_state();
        return ret;
    }

    s_wifi_netif = esp_netif_create_default_wifi_sta();
    if (s_wifi_netif == NULL) {
        airmouse_wifi_cleanup_state();
        return ESP_ERR_NO_MEM;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_LOGI(TAG, "WiFi init request: free heap before esp_wifi_init = %u",
             (unsigned int)esp_get_free_heap_size());
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s (0x%x)",
                 esp_err_to_name(ret), (unsigned int)ret);
        (void)airmouse_wifi_cleanup_driver(true);
        airmouse_wifi_cleanup_state();
        return ret;
    }
    s_wifi_initialized = true;

    ret = esp_event_handler_instance_register(
              WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL,
              &s_wifi_event_instance_any_id);
    if (ret != ESP_OK) {
        (void)airmouse_wifi_cleanup_driver(true);
        airmouse_wifi_cleanup_state();
        return ret;
    }

    ret = esp_event_handler_instance_register(
              IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL,
              &s_ip_event_instance_got_ip);
    if (ret != ESP_OK) {
        (void)airmouse_wifi_cleanup_driver(true);
        airmouse_wifi_cleanup_state();
        return ret;
    }
    s_wifi_handlers_registered = true;

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_AIRMOUSE_WIFI_SSID,
            .password = CONFIG_AIRMOUSE_WIFI_PASS,
        },
    };
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        (void)airmouse_wifi_cleanup_driver(true);
        airmouse_wifi_cleanup_state();
        return ret;
    }

    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        (void)airmouse_wifi_cleanup_driver(true);
        airmouse_wifi_cleanup_state();
        return ret;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        (void)airmouse_wifi_cleanup_driver(true);
        airmouse_wifi_cleanup_state();
        return ret;
    }
    s_wifi_started = true;
    EventBits_t bits = xEventGroupWaitBits(
                           s_wifi_event_group,
                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                           pdFALSE,
                           pdFALSE,
                           portMAX_DELAY
                       );

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi connected");
        return ESP_OK;
    }

    if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "WiFi connection failed");
        return ESP_FAIL;
    }

    return ESP_ERR_TIMEOUT;
}

esp_err_t airmouse_wifi_deinit_sta(void)
{
    esp_err_t ret = airmouse_wifi_cleanup_driver(true);
    airmouse_wifi_cleanup_state();
    ESP_LOGI(TAG, "WiFi deinitialized");
    return ret;
}

esp_err_t airmouse_http_server_start(airmouse_config_t *airmouse_config)
{
    if (airmouse_config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_LWIP_MAX_SOCKETS < (CONFIG_AIRMOUSE_HTTP_SERVER_MAX_OPEN_SOCKETS + 3)
#error "CONFIG_LWIP_MAX_SOCKETS must be at least CONFIG_AIRMOUSE_HTTP_SERVER_MAX_OPEN_SOCKETS + 3"
#endif

    s_airmouse_config = airmouse_config;

    if (s_config_event_group == NULL) {
        s_config_event_group = xEventGroupCreate();
        if (s_config_event_group == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (s_http_server != NULL) {
        return ESP_OK;
    }

    xEventGroupClearBits(s_config_event_group, CONFIG_DONE_BIT);

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 2;
    httpd_handle_t server = NULL;
    esp_err_t ret = httpd_start(&server, &config);
    if (ret != ESP_OK) {
        return ret;
    }

    const httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = airmouse_http_root_get_handler,
        .user_ctx = NULL,
    };

    ret = httpd_register_uri_handler(server, &root_uri);
    if (ret != ESP_OK) {
        httpd_stop(server);
        return ret;
    }

    const httpd_uri_t app_js_uri = {
        .uri = "/app.js",
        .method = HTTP_GET,
        .handler = airmouse_http_app_js_get_handler,
        .user_ctx = NULL,
    };

    ret = httpd_register_uri_handler(server, &app_js_uri);
    if (ret != ESP_OK) {
        httpd_stop(server);
        return ret;
    }

    const httpd_uri_t style_css_uri = {
        .uri = "/style.css",
        .method = HTTP_GET,
        .handler = airmouse_http_style_css_get_handler,
        .user_ctx = NULL,
    };

    ret = httpd_register_uri_handler(server, &style_css_uri);
    if (ret != ESP_OK) {
        httpd_stop(server);
        return ret;
    }

    const httpd_uri_t config_uri = {
        .uri = "/config",
        .method = HTTP_POST,
        .handler = airmouse_config_post_handler,
        .user_ctx = NULL,
    };

    ret = httpd_register_uri_handler(server, &config_uri);
    if (ret != ESP_OK) {
        httpd_stop(server);
        return ret;
    }

    const httpd_uri_t config_get_uri = {
        .uri = "/config",
        .method = HTTP_GET,
        .handler = airmouse_config_get_handler,
        .user_ctx = NULL,
    };

    ret = httpd_register_uri_handler(server, &config_get_uri);
    if (ret != ESP_OK) {
        httpd_stop(server);
        return ret;
    }

    const httpd_uri_t runtime_state_get_uri = {
        .uri = "/runtime-state",
        .method = HTTP_GET,
        .handler = airmouse_runtime_state_get_handler,
        .user_ctx = NULL,
    };

    ret = httpd_register_uri_handler(server, &runtime_state_get_uri);
    if (ret != ESP_OK) {
        httpd_stop(server);
        return ret;
    }

    s_http_server = server;
    ESP_LOGI(TAG, "HTTP server started");
    return ESP_OK;
}

esp_err_t airmouse_http_server_stop(void)
{
    if (s_http_server == NULL) {
        return ESP_OK;
    }

    esp_err_t ret = httpd_stop(s_http_server);
    if (ret == ESP_OK) {
        s_http_server = NULL;
        ESP_LOGI(TAG, "HTTP server stopped");
        vTaskDelay(pdMS_TO_TICKS(HTTP_SERVER_STOP_DRAIN_DELAY_MS));
    }

    return ret;
}

esp_err_t airmouse_http_server_wait_config_done(TickType_t ticks_to_wait)
{
    if (s_config_event_group == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    EventBits_t bits = xEventGroupWaitBits(
                           s_config_event_group,
                           CONFIG_DONE_BIT,
                           pdTRUE,
                           pdFALSE,
                           ticks_to_wait
                       );

    if (bits & CONFIG_DONE_BIT) {
        return ESP_OK;
    }

    return ESP_ERR_TIMEOUT;
}

void airmouse_http_runtime_notice_set(const char *mode,
                                      const char *level,
                                      const char *message,
                                      bool active)
{
    s_runtime_notice_active = active;

    snprintf(s_runtime_notice_mode,
             sizeof(s_runtime_notice_mode),
             "%s",
             mode != NULL ? mode : "");
    snprintf(s_runtime_notice_level,
             sizeof(s_runtime_notice_level),
             "%s",
             level != NULL ? level : "");
    snprintf(s_runtime_notice_message,
             sizeof(s_runtime_notice_message),
             "%s",
             message != NULL ? message : "");
}
