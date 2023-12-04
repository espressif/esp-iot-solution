/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include "esp_eth.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "esp_tls_crypto.h"
#include "esp_vfs.h"
#include "json_parser.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "modem_http_config.h"

/* A simple example that demonstrates how to create GET and POST
 * handlers for the web server.
 */
static const char *TAG = "4g_router_server";
#define HTTPD_401 "401 UNAUTHORIZED" /*!< HTTP Response 401 */

#define REST_CHECK(a, str, goto_tag, ...)                                         \
    do {                                                                          \
        if (!(a)) {                                                               \
            ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto goto_tag;                                                        \
        }                                                                         \
    } while (0)

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (10240)

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

/**
 * @brief Store the currently connected sta
 *
 */
#define STA_CHECK(a, str, ret)                                                 \
    if (!(a)) {                                                                \
        ESP_LOGE(TAG, "%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        return (ret);                                                          \
    }
#define STA_CHECK_GOTO(a, str, label)                                          \
    if (!(a)) {                                                                \
        ESP_LOGE(TAG, "%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        goto label;                                                            \
    }

#define STA_NODE_MUTEX_TICKS_TO_WAIT 200
static SemaphoreHandle_t s_sta_node_mutex = NULL;
static modem_wifi_config_t *s_modem_wifi_config = NULL;
static modem_http_list_head_t s_sta_list_head = SLIST_HEAD_INITIALIZER(s_sta_list_head);

static void restart()
{
    ESP_LOGI(TAG, "Restarting now.\n");
    fflush(stdout);
    esp_restart();
}

static void delete_char(char *str, char target)
{
    int i, j;
    for (i = j = 0; str[i] != '\0'; i++) {
        if (str[i] != target) {
            str[j++] = str[i];
        }
    }
    str[j] = '\0';
}

typedef struct rest_server_context {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;
typedef struct {
    char *username;
    char *password;
} basic_auth_info_t;

typedef struct {
    rest_server_context_t *rest_context;
    basic_auth_info_t *basic_auth_info;
} ctx_info_t;

static char *http_auth_basic(const char *username, const char *password)
{
    int out;
    char *user_info = NULL;
    char *digest = NULL;
    size_t n = 0;
    asprintf(&user_info, "%s:%s", username, password);
    if (!user_info) {
        ESP_LOGE(TAG, "No enough memory for user information");
        return NULL;
    }
    esp_crypto_base64_encode(NULL, 0, &n, (const unsigned char *)user_info, strlen(user_info));

    /* 6: The length of the "Basic " string
     * n: Number of bytes for a base64 encode format
     * 1: Number of bytes for a reserved which be used to fill zero
     */
    digest = calloc(1, 6 + n + 1);
    if (digest) {
        strcpy(digest, "Basic ");
        esp_crypto_base64_encode((unsigned char *)digest + 6, n, (size_t *)&out, (const unsigned char *)user_info,
                                 strlen(user_info));
    }
    free(user_info);
    return digest;
}

static esp_err_t basic_auth_get(httpd_req_t *req)
{
    char *buf = NULL;
    size_t buf_len = 0;
    ctx_info_t *ctx_info = req->user_ctx;
    basic_auth_info_t *basic_auth_info = ctx_info->basic_auth_info;

    buf_len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;
    if (buf_len > 1) {
        buf = calloc(1, buf_len);
        if (!buf) {
            ESP_LOGE(TAG, "No enough memory for basic authorization");
            return ESP_ERR_NO_MEM;
        }

        if (httpd_req_get_hdr_value_str(req, "Authorization", buf, buf_len) == ESP_OK) {
            ESP_LOGD(TAG, "Found header => Authorization: %s", buf);
        } else {
            ESP_LOGE(TAG, "No auth value received");
        }

        char *auth_credentials = http_auth_basic(basic_auth_info->username, basic_auth_info->password);
        if (!auth_credentials) {
            ESP_LOGE(TAG, "No enough memory for basic authorization credentials");
            free(buf);
            return ESP_ERR_NO_MEM;
        } else {
            ESP_LOGD(TAG, "auth_credentials : %s", auth_credentials);
        }

        if (strncmp(auth_credentials, buf, buf_len)) {
            ESP_LOGE(TAG, "Not Authenticated");
            httpd_resp_set_status(req, HTTPD_401);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "keep-alive");
            httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"router\"");
            httpd_resp_send(req, NULL, 0);
            free(auth_credentials);
            free(buf);
            return ESP_FAIL;
        } else {
            ESP_LOGD(TAG, "Authenticated!");
            httpd_resp_set_status(req, HTTPD_200);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "keep-alive");
            free(auth_credentials);
            free(buf);
            return ESP_OK;
        }
    } else {
        ESP_LOGE(TAG, "No auth header received");
        httpd_resp_set_status(req, HTTPD_401);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Connection", "keep-alive");
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"router\"");
        httpd_resp_send(req, NULL, 0);
        return ESP_FAIL;
    }

    return ESP_OK;
}

void nvs_get_str_log(esp_err_t err, char *key, char *value)
{
    switch (err) {
    case ESP_OK:
        ESP_LOGI(TAG, "%s = %s", key, value);
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGI(TAG, "%s : Can't find in NVS!", key);
        break;
    default:
        ESP_LOGE(TAG, "Error (%s) reading!", esp_err_to_name(err));
    }
}

static esp_err_t from_nvs_set_value(char *key, char *value)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("memory", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return ESP_FAIL;
    } else {
        err = nvs_set_str(my_handle, key, value);
        ESP_LOGI(TAG, "set %s is %s!,the err is %d\n", key, (err == ESP_OK) ? "succeed" : "failed", err);
        nvs_close(my_handle);
        ESP_LOGI(TAG, "NVS close Done\n");
    }
    return ESP_OK;
}

static esp_err_t from_nvs_get_value(char *key, char *value, size_t *size)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("memory", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return ESP_FAIL;
    } else {
        err = nvs_get_str(my_handle, key, value, size);
        nvs_get_str_log(err, key, value);
        nvs_close(my_handle);
    }
    return err;
}

esp_err_t modem_http_print_nodes(modem_http_list_head_t *head)
{
    struct modem_netif_sta_info *node;
    SLIST_FOREACH(node, head, field) {
        ESP_LOGI(TAG, "MAC is " MACSTR ", IP is " IPSTR ", start_time is %lld ", MAC2STR(node->mac),
                 IP2STR(&node->ip), node->start_time);
    }
    return ESP_OK;
}

static esp_err_t stalist_update()
{
    if (pdTRUE == xSemaphoreTake(s_sta_node_mutex, STA_NODE_MUTEX_TICKS_TO_WAIT)) {
        struct modem_netif_sta_info *node;
        SLIST_FOREACH(node, &s_sta_list_head, field) {
            if (node->ip.addr == 0) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
                esp_netif_pair_mac_ip_t pair_mac_ip = { 0 };
                memcpy(pair_mac_ip.mac, node->mac, 6);
                esp_netif_dhcps_get_clients_by_mac(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), 1, &pair_mac_ip);
                node->ip = pair_mac_ip.ip;
#else
                dhcp_search_ip_on_mac(node->mac, (ip4_addr_t *)&node->ip);
#endif
            }
            char mac_addr[18] = "";
            size_t name_size = sizeof(node->name);
            sprintf(mac_addr, "%02x%02x%02x%02x%02x%02x", node->mac[0], node->mac[1], node->mac[2], node->mac[3], node->mac[4], node->mac[5]);
            from_nvs_get_value(mac_addr, node->name, &name_size);
        }
        if (!(pdTRUE == xSemaphoreGive(s_sta_node_mutex))) {
            ESP_LOGE(TAG, "give semaphore failed");
        };
    }
    modem_http_print_nodes(&s_sta_list_head);
    return ESP_OK;
}

modem_http_list_head_t *modem_http_get_stalist()
{
    return &s_sta_list_head;
}

static esp_err_t stalist_add_node(uint8_t mac[6])
{
    // STA_CHECK(sta != NULL, "sta pointer can not be NULL", ESP_ERR_INVALID_ARG);
    struct modem_netif_sta_info *node = calloc(1, sizeof(struct modem_netif_sta_info));
    STA_CHECK(node != NULL, "calloc node failed", ESP_ERR_NO_MEM);
    STA_CHECK_GOTO(pdTRUE == xSemaphoreTake(s_sta_node_mutex, STA_NODE_MUTEX_TICKS_TO_WAIT), "take semaphore timeout", cleanupnode);
    node->start_time = esp_timer_get_time();
    memcpy(node->mac, mac, 6);
    char mac_addr[18] = "";
    size_t name_size = sizeof(node->name);
    sprintf(mac_addr, "%02x%02x%02x%02x%02x%02x", node->mac[0], node->mac[1], node->mac[2], node->mac[3], node->mac[4], node->mac[5]);
    esp_err_t err = from_nvs_get_value(mac_addr, node->name, &name_size); // name
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        memcpy(node->name, mac_addr, 12);
    }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_netif_pair_mac_ip_t pair_mac_ip = { 0 };
    memcpy(pair_mac_ip.mac, node->mac, 6);
    esp_netif_dhcps_get_clients_by_mac(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), 1, &pair_mac_ip);
    node->ip = pair_mac_ip.ip;
#else
    dhcp_search_ip_on_mac(node->mac, (ip4_addr_t *)&node->ip);
#endif
    SLIST_INSERT_HEAD(&s_sta_list_head, node, field);
    STA_CHECK_GOTO(pdTRUE == xSemaphoreGive(s_sta_node_mutex), "give semaphore failed", cleanupnode);
    return ESP_OK;
cleanupnode:
    free(node);
    return ESP_FAIL;
}

static esp_err_t sta_remove_node(uint8_t mac[6])
{
    struct modem_netif_sta_info *node;
    STA_CHECK(pdTRUE == xSemaphoreTake(s_sta_node_mutex, STA_NODE_MUTEX_TICKS_TO_WAIT), "take semaphore timeout", ESP_ERR_TIMEOUT);
    SLIST_FOREACH(node, &s_sta_list_head, field) {
        if (!memcmp(node->mac, mac, 6)) {
            ESP_LOGI(TAG, "remove MAC is " MACSTR ", IP is " IPSTR ", start_time is %lld ", MAC2STR(node->mac),
                     IP2STR(&node->ip), node->start_time);
            SLIST_REMOVE(&s_sta_list_head, node, modem_netif_sta_info, field);
            free(node);
            break;
        }
    }
    STA_CHECK(pdTRUE == xSemaphoreGive(s_sta_node_mutex), "give semaphore failed", ESP_FAIL);
    return ESP_OK;
}

static esp_err_t wlan_general_get_handler(httpd_req_t *req)
{
    const char *user_ssid = s_modem_wifi_config->ssid;
    const char *user_password = s_modem_wifi_config->password;
    const char *user_hide_ssid ;
    if (s_modem_wifi_config->ssid_hidden == 0) {
        user_hide_ssid = "flase";
    } else {
        user_hide_ssid = "true";
    }

    const char *user_auth_mode;
    switch (s_modem_wifi_config->authmode) {
    case WIFI_AUTH_OPEN:
        user_auth_mode = "OPEN";
        break;

    case WIFI_AUTH_WEP:
        user_auth_mode = "WEP";
        break;

    case WIFI_AUTH_WPA2_PSK:
        user_auth_mode = "WAP2_PSK";
        break;

    case WIFI_AUTH_WPA_WPA2_PSK:
        user_auth_mode = "WPA_WPA2_PSK";
        break;

    default:
        user_auth_mode = "WPA_WPA2_PSK";
        break;
    }

    esp_err_t err = basic_auth_get(req);
    REST_CHECK(err == ESP_OK, "not login yet", end);

    char *json_str = NULL;
    size_t size = 0;
    size = asprintf(&json_str,
                    "{\"status\":\"200\", \"ssid\":\"%s\", \"if_hide_ssid\":\"%s\", "
                    "\"auth_mode\":\"%s\", \"password\":\"%s\"}",
                    user_ssid, user_hide_ssid, user_auth_mode, user_password);

    /**
     * @brief Set the HTTP status code
     */
    esp_err_t ret = httpd_resp_set_status(req, HTTPD_200);
    ESP_ERROR_CHECK(ret);
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "*");
    /**
     * @brief Set the HTTP content type
     */
    ret = httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    ESP_ERROR_CHECK(ret);

    /**
     * @brief Set some custom headers
     */

    ret = httpd_resp_send(req, json_str, size);
    free(json_str);
    ESP_ERROR_CHECK(ret);
    return ESP_OK;
end:
    return ESP_OK;
}

static esp_err_t wlan_general_post_handler(httpd_req_t *req)
{
    char user_ssid[32] = "";
    char user_password[64] = "";
    char user_hide_ssid[8] = "";
    char user_auth_mode[16] = "";

    esp_err_t err = basic_auth_get(req);
    REST_CHECK(err == ESP_OK, "not login yet", end);

    char buf[256] = { 0 };
    int len_ret, remaining = req->content_len;

    if (remaining > sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "string too long");
        return ESP_FAIL;
    }

    while (remaining > 0) {
        /* Read the data for the request */
        if ((len_ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
            if (len_ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }

            return ESP_FAIL;
        }

        remaining -= len_ret;

        /* Log data received */
        ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
        ESP_LOGI(TAG, "%.*s", len_ret, buf);
        ESP_LOGI(TAG, "====================================");
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "*");
    jparse_ctx_t jctx;
    int ps_ret = json_parse_start(&jctx, buf, strlen(buf));

    if (ps_ret != OS_SUCCESS) {
        ESP_LOGE(TAG, "Parser failed\n");
        return ESP_FAIL;
    }

    char str_val[64];

    if (json_obj_get_string(&jctx, "ssid", str_val, sizeof(str_val)) == OS_SUCCESS) {
        snprintf(user_ssid, sizeof(user_ssid), "%.*s", sizeof(user_ssid) - 1, str_val);
        ESP_LOGI(TAG, "ssid %s\n", user_ssid);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid post");
        return ESP_FAIL;
    }

    if (json_obj_get_string(&jctx, "if_hide_ssid", str_val, sizeof(str_val)) == OS_SUCCESS) {
        snprintf(user_hide_ssid, sizeof(user_hide_ssid), "%.*s", sizeof(user_hide_ssid) - 1, str_val);
        ESP_LOGI(TAG, "if_hide_ssid %s\n", user_hide_ssid);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid post");
        return ESP_FAIL;
    }

    if (json_obj_get_string(&jctx, "auth_mode", str_val, sizeof(str_val)) == OS_SUCCESS) {
        snprintf(user_auth_mode, sizeof(user_auth_mode), "%.*s", sizeof(user_auth_mode) - 1, str_val);
        ESP_LOGI(TAG, "auth_mode %s\n", user_auth_mode);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid post");
        return ESP_FAIL;
    }

    if (json_obj_get_string(&jctx, "password", str_val, sizeof(str_val)) == OS_SUCCESS) {
        snprintf(user_password, sizeof(user_password), "%.*s", sizeof(user_password) - 1, str_val);
        ESP_LOGI(TAG, "password %s\n", user_password);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid post");
        return ESP_FAIL;
    }

    char *json_str = NULL;
    size_t size = 0;
    size = asprintf(&json_str,
                    "{\"status\":\"200\", \"ssid\":\"%s\", \"if_hide_ssid\":\"%s\", "
                    "\"auth_mode\":\"%s\", \"password\":\"%s\"}",
                    user_ssid, user_hide_ssid, user_auth_mode, user_password);
    esp_err_t ret = httpd_resp_set_status(req, HTTPD_200);
    ESP_ERROR_CHECK(ret);
    ret = httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    ESP_ERROR_CHECK(ret);
    ret = httpd_resp_send(req, json_str, size);
    free(json_str);
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(from_nvs_set_value("ssid", user_ssid));
    ESP_ERROR_CHECK(from_nvs_set_value("hide_ssid", user_hide_ssid));
    ESP_ERROR_CHECK(from_nvs_set_value("auth_mode", user_auth_mode));
    ESP_ERROR_CHECK(from_nvs_set_value("password", user_password));

    restart();
    return ESP_OK;
end:
    return ESP_OK;
}

static esp_err_t wlan_advance_get_handler(httpd_req_t *req)
{
    esp_err_t err = basic_auth_get(req);
    REST_CHECK(err == ESP_OK, "not login yet", end);

    char *json_str = NULL;
    size_t size;
    size_t user_bandwidth;
    if (s_modem_wifi_config->bandwidth == WIFI_BW_HT20) {
        user_bandwidth = 20;
    } else {
        user_bandwidth = 40;
    }
    size = asprintf(&json_str, "{\"status\":\"200\", \"bandwidth\":\"%d\", \"channel\":\"%d\"}", user_bandwidth,
                    s_modem_wifi_config->channel);
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "*");
    /**
     * @brief Set the HTTP status code
     */
    esp_err_t ret = httpd_resp_set_status(req, HTTPD_200);
    ESP_ERROR_CHECK(ret);

    /**
     * @brief Set the HTTP content type
     */
    ret = httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    ESP_ERROR_CHECK(ret);

    /**
     * @brief Set some custom headers
     */

    ret = httpd_resp_send(req, json_str, size);
    free(json_str);
    ESP_ERROR_CHECK(ret);
    return ESP_OK;
end:
    return ESP_OK;
}

static esp_err_t wlan_advance_post_handler(httpd_req_t *req)
{
    char user_channel[4] = "";
    char user_bandwidth[4] = "";

    esp_err_t err = basic_auth_get(req);
    REST_CHECK(err == ESP_OK, "not login yet", end);

    char buf[256] = { 0 };
    int len_ret, remaining = req->content_len;

    if (remaining > sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "string too long");
        return ESP_FAIL;
    }

    while (remaining > 0) {
        /* Read the data for the request */
        if ((len_ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
            if (len_ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }

            return ESP_FAIL;
        }

        remaining -= len_ret;

        /* Log data received */
        ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
        ESP_LOGI(TAG, "%.*s", len_ret, buf);
        ESP_LOGI(TAG, "====================================");
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "*");
    jparse_ctx_t jctx;
    int ps_ret = json_parse_start(&jctx, buf, strlen(buf));

    if (ps_ret != OS_SUCCESS) {
        ESP_LOGE(TAG, "Parser failed\n");
        return ESP_FAIL;
    }

    char str_val[64];

    if (json_obj_get_string(&jctx, "bandwidth", str_val, sizeof(str_val)) == OS_SUCCESS) {
        snprintf(user_bandwidth, sizeof(user_bandwidth), "%.*s", sizeof(user_bandwidth) - 1, str_val);
        ESP_LOGI(TAG, "bandwidth: %s\n", user_bandwidth);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid post");
        return ESP_FAIL;
    }

    if (json_obj_get_string(&jctx, "channel", str_val, sizeof(str_val)) == OS_SUCCESS) {
        snprintf(user_channel, sizeof(user_channel), "%.*s", sizeof(user_channel) - 1, str_val);
        ESP_LOGI(TAG, "channel: %s\n", user_channel);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid post");
        return ESP_FAIL;
    }

    char *json_str = NULL;
    size_t size = 0;
    size = asprintf(&json_str, "{\"status\":\"200\", \"bandwidth\":\"%s\", \"channel\":\"%s\"}", user_bandwidth,
                    user_channel);
    esp_err_t ret = httpd_resp_set_status(req, HTTPD_200);
    ESP_ERROR_CHECK(ret);
    ret = httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    ESP_ERROR_CHECK(ret);
    ret = httpd_resp_send(req, json_str, size);
    free(json_str);
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(from_nvs_set_value("channel", user_channel));
    ESP_ERROR_CHECK(from_nvs_set_value("bandwidth", user_bandwidth));
    restart();

    return ESP_OK;
end:
    return ESP_OK;
}

static esp_err_t login_get_handler(httpd_req_t *req)
{

    char *json_str = NULL;
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "*");
    /**
     * @brief Set the HTTP status code
     */
    esp_err_t ret = httpd_resp_set_status(req, HTTPD_200);
    ESP_ERROR_CHECK(ret);

    /**
     * @brief Set the HTTP content type
     */
    ret = httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    ESP_ERROR_CHECK(ret);

    /**
     * @brief Set some custom headers
     */

    ret = httpd_resp_send(req, json_str, sizeof(json_str));
    free(json_str);
    ESP_ERROR_CHECK(ret);
    return ESP_OK;
}

static esp_err_t login_post_handler(httpd_req_t *req)
{
    return ESP_OK;
}

static esp_err_t system_station_get_handler(httpd_req_t *req)
{
    esp_err_t err = basic_auth_get(req);
    REST_CHECK(err == ESP_OK, "not login yet", end);
    char *json_str = NULL;
    char *json_str_old = NULL;
    size_t size = 0;
    struct modem_netif_sta_info *node;
    size = asprintf(&json_str, "{\"station_list\":[");
    SLIST_FOREACH(node, &s_sta_list_head, field) {
        asprintf(&json_str_old, "%s", json_str);
        free(json_str);
        json_str = NULL;
        size = asprintf(&json_str,
                        "%s{\"name_str\":\"%s\",\"mac_str\":\"" MACSTR "\",\"ip_str\":\"" IPSTR
                        "\",\"online_time_s\":\"%lld\"}%c",
                        json_str_old, node->name, MAC2STR(node->mac), IP2STR(&node->ip),
                        node->start_time, node->field.sle_next ? ',' : '\0');
        free(json_str_old);
        json_str_old = NULL;
    }
    size = asprintf(&json_str_old, "%s],\"now_time\":\"%lld\"}", json_str, esp_timer_get_time());

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "*");
    /**
     * @brief Set the HTTP status code
     */
    esp_err_t ret = httpd_resp_set_status(req, HTTPD_200);
    ESP_ERROR_CHECK(ret);

    /**
     * @brief Set the HTTP content type
     */
    ret = httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    ESP_ERROR_CHECK(ret);

    /**
     * @brief Set some custom headers
     */

    ret = httpd_resp_send(req, json_str_old, size);
    ESP_LOGD(TAG, "%s", json_str_old);
    free(json_str);
    free(json_str_old);
    ESP_ERROR_CHECK(ret);
    return ESP_OK;
end:
    return ESP_OK;
}

static esp_err_t system_station_delete_device_post_handler(httpd_req_t *req)
{
    esp_err_t err = basic_auth_get(req);
    REST_CHECK(err == ESP_OK, "not login yet", end);

    char buf[256] = { 0 };
    char mac_str[18] = "";
    int value[6] = { 0 };
    uint8_t mac_byte[6] = { 0 };
    uint16_t aid = 0;
    int len_ret, remaining = req->content_len;

    if (remaining > sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "string too long");
        return ESP_FAIL;
    }

    while (remaining > 0) {
        /* Read the data for the request */
        if ((len_ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
            if (len_ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return ESP_FAIL;
        }

        remaining -= len_ret;

        /* Log data received */
        ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
        ESP_LOGI(TAG, "%.*s", len_ret, buf);
        ESP_LOGI(TAG, "====================================");
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "*");
    jparse_ctx_t jctx;
    int ps_ret = json_parse_start(&jctx, buf, strlen(buf));

    if (ps_ret != OS_SUCCESS) {
        ESP_LOGE(TAG, "Parser failed\n");
        return ESP_FAIL;
    }

    char str_val[64];

    if (json_obj_get_string(&jctx, "mac_str", str_val, sizeof(str_val)) == OS_SUCCESS) {
        snprintf(mac_str, sizeof(mac_str), "%.*s", sizeof(mac_str) - 1, str_val);
        ESP_LOGI(TAG, "mac_str: %s\n", mac_str);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid post");
        return ESP_FAIL;
    }

    if (6 == sscanf(mac_str, "%x:%x:%x:%x:%x:%x%*c", &value[0], &value[1], &value[2], &value[3], &value[4], &value[5])) {
        for (size_t i = 0; i < 6; i++) {
            mac_byte[i] = (uint8_t)value[i];
        }
        ESP_LOGI(TAG, "trans mac_addr from str to uint8_t ok");
    } else {
        ESP_LOGE(TAG, "trans mac_addr from str to uint8_t fail");
    }

    ESP_ERROR_CHECK(esp_wifi_ap_get_sta_aid(mac_byte, &aid));
    ESP_LOGI(TAG, "remove aid is %d", aid);
    esp_err_t res_deauth_sta;
    res_deauth_sta = esp_wifi_deauth_sta(aid);
    if (res_deauth_sta == ESP_OK) {
        ESP_LOGI(TAG, "deauth OK");
    } else {
        ESP_LOGI(TAG, "deauth failed");
    }

    esp_err_t ret = httpd_resp_set_status(req, HTTPD_200);
    ESP_ERROR_CHECK(ret);
    ret = httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    ESP_ERROR_CHECK(ret);
    ret = httpd_resp_send(req, NULL, 0);
    ESP_ERROR_CHECK(ret);

    return ESP_OK;
end:
    return ESP_OK;
}

static esp_err_t system_station_change_name_post_handler(httpd_req_t *req)
{
    esp_err_t err = basic_auth_get(req);
    REST_CHECK(err == ESP_OK, "not login yet", end);

    char buf[256] = { 0 };
    int len_ret, remaining = req->content_len;

    if (remaining > sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "string too long");
        return ESP_FAIL;
    }

    while (remaining > 0) {
        /* Read the data for the request */
        if ((len_ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
            if (len_ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }

            return ESP_FAIL;
        }

        remaining -= len_ret;

        /* Log data received */
        ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
        ESP_LOGI(TAG, "%.*s", len_ret, buf);
        ESP_LOGI(TAG, "====================================");
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "*");
    jparse_ctx_t jctx;
    int ps_ret = json_parse_start(&jctx, buf, strlen(buf));

    if (ps_ret != OS_SUCCESS) {
        ESP_LOGE(TAG, "Parser failed\n");
        return ESP_FAIL;
    }

    char str_val[64];
    char name_str[36] = "";
    char mac_str[18] = "";

    if (json_obj_get_string(&jctx, "name_str", str_val, sizeof(str_val)) == OS_SUCCESS) {
        snprintf(name_str, sizeof(name_str), "%.*s", sizeof(name_str) - 1, str_val);
        ESP_LOGI(TAG, "name_str: %s\n", name_str);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid post");
        return ESP_FAIL;
    }

    if (json_obj_get_string(&jctx, "mac_str", str_val, sizeof(str_val)) == OS_SUCCESS) {
        snprintf(mac_str, sizeof(mac_str), "%.*s", sizeof(mac_str) - 1, str_val);
        ESP_LOGI(TAG, "mac_str: %s\n", mac_str);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid post");
        return ESP_FAIL;
    }
    /**
     * @brief The length of the control key is within 15 bytes
     *    00:01:02:03:04:05 --> 000102030405
     */
    delete_char(mac_str, ':');

    ESP_ERROR_CHECK(from_nvs_set_value(mac_str, name_str));

    esp_err_t ret = httpd_resp_set_status(req, HTTPD_200);
    ESP_ERROR_CHECK(ret);
    ret = httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    ESP_ERROR_CHECK(ret);
    ret = httpd_resp_send(req, NULL, 0);
    ESP_ERROR_CHECK(ret);
    stalist_update();
    return ESP_OK;
end:
    return ESP_OK;
}

static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath)
{
    const char *type = "text/plain";
    if (CHECK_FILE_EXTENSION(filepath, ".html")) {
        type = "text/html";
    } else if (CHECK_FILE_EXTENSION(filepath, ".js")) {
        type = "application/javascript";
    } else if (CHECK_FILE_EXTENSION(filepath, ".css")) {
        type = "text/css";
    } else if (CHECK_FILE_EXTENSION(filepath, ".png")) {
        type = "image/png";
    } else if (CHECK_FILE_EXTENSION(filepath, ".ico")) {
        type = "image/x-icon";
    } else if (CHECK_FILE_EXTENSION(filepath, ".svg")) {
        type = "text/xml";
    }
    return httpd_resp_set_type(req, type);
}

static esp_err_t rest_common_get_handler(httpd_req_t *req)
{
    esp_err_t err = basic_auth_get(req);
    REST_CHECK(err == ESP_OK, "not login yet", end);

    ESP_LOGD(TAG, "(%s) %s", __func__, req->uri);
    char filepath[FILE_PATH_MAX];

    //rest_server_context_t* rest_context = (rest_server_context_t*)req->user_ctx->rest_context;
    ctx_info_t *ctx_info = (ctx_info_t *)req->user_ctx;
    rest_server_context_t *rest_context = ctx_info->rest_context;
    strlcpy(filepath, rest_context->base_path, sizeof(filepath));
    if (req->uri[strlen(req->uri) - 1] == '/') {
        strlcat(filepath, "/index.html", sizeof(filepath));
    } else {
        strlcat(filepath, req->uri, sizeof(filepath));
    }
    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        ESP_LOGE(TAG, "Failed to open file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    set_content_type_from_file(req, filepath);

    char *chunk = rest_context->scratch;
    ssize_t read_bytes;
    do {
        /* Read file in chunks into the scratch buffer */
        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
        if (read_bytes == -1) {
            ESP_LOGE(TAG, "Failed to read file : %s", filepath);
        } else if (read_bytes > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
                close(fd);
                ESP_LOGE(TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);
    /* Close file after sending complete */
    close(fd);
    ESP_LOGD(TAG, "File sending complete");
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
end:
    return ESP_OK;
}

static httpd_uri_t wlan_general = {
    .uri = "/wlan_general",
    .method = HTTP_GET,
    .handler = wlan_general_get_handler,
    /* Let's pass response string in user context to demonstrate it's usage */
    .user_ctx = NULL
};

static httpd_uri_t wlan_general_post = {
    .uri = "/wlan_general",
    .method = HTTP_POST,
    .handler = wlan_general_post_handler,
    /* Let's pass response string in user context to demonstrate it's usage */
    .user_ctx = NULL
};

static httpd_uri_t wlan_advance = {
    .uri = "/wlan_advance",
    .method = HTTP_GET,
    .handler = wlan_advance_get_handler,
    /* Let's pass response string in user context to demonstrate it's usage */
    .user_ctx = NULL
};

static httpd_uri_t wlan_advance_post = {
    .uri = "/wlan_advance",
    .method = HTTP_POST,
    .handler = wlan_advance_post_handler,
    /* Let's pass response string in user context to demonstrate it's usage */
    .user_ctx = NULL
};

static httpd_uri_t system_station_get = {
    .uri = "/system/station_state",
    .method = HTTP_GET,
    .handler = system_station_get_handler,
    /* Let's pass response string in user context to demonstrate it's usage */
    .user_ctx = NULL
};

static httpd_uri_t login_get = {
    .uri = "/login",
    .method = HTTP_GET,
    .handler = login_get_handler,
    /* Let's pass response string in user context to demonstrate it's usage */
    .user_ctx = NULL
};

static httpd_uri_t login_post = {
    .uri = "/login",
    .method = HTTP_POST,
    .handler = login_post_handler,
    /* Let's pass response string in user context to demonstrate it's usage */
    .user_ctx = NULL
};

static httpd_uri_t system_station_delete_device_post = {
    .uri = "/system/station_state/delete_device",
    .method = HTTP_POST,
    .handler = system_station_delete_device_post_handler,
    /* Let's pass response string in user context to demonstrate it's usage */
    .user_ctx = NULL
};

static httpd_uri_t system_station_change_name_post = {
    .uri = "/system/station_state/change_name",
    .method = HTTP_POST,
    .handler = system_station_change_name_post_handler,
    /* Let's pass response string in user context to demonstrate it's usage */
    .user_ctx = NULL
};

static httpd_handle_t start_webserver(const char *base_path)
{
    ctx_info_t *ctx_info = calloc(1, sizeof(ctx_info_t));
    REST_CHECK(base_path, "wrong base path", err);
    ctx_info->rest_context = calloc(1, sizeof(rest_server_context_t));
    REST_CHECK(ctx_info->rest_context, "No memory for rest context", err);
    strlcpy(ctx_info->rest_context->base_path, base_path, sizeof(ctx_info->rest_context->base_path));

    ctx_info->basic_auth_info = calloc(1, sizeof(basic_auth_info_t));
    ctx_info->basic_auth_info->username = CONFIG_EXAMPLE_WEB_USERNAME;
    ctx_info->basic_auth_info->password = CONFIG_EXAMPLE_WEB_PASSWORD;

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 15;
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);

    if (httpd_start(&server, &config) == ESP_OK) {

        // Set URI handlers & Add user_ctx
        ESP_LOGI(TAG, "Registering URI handlers");
        wlan_general.user_ctx = ctx_info;
        httpd_register_uri_handler(server, &wlan_general);
        wlan_general_post.user_ctx = ctx_info;
        httpd_register_uri_handler(server, &wlan_general_post);
        wlan_advance.user_ctx = ctx_info;
        httpd_register_uri_handler(server, &wlan_advance);
        wlan_advance_post.user_ctx = ctx_info;
        httpd_register_uri_handler(server, &wlan_advance_post);
        system_station_get.user_ctx = ctx_info;
        httpd_register_uri_handler(server, &system_station_get);
        system_station_delete_device_post.user_ctx = ctx_info;
        httpd_register_uri_handler(server, &system_station_delete_device_post);
        system_station_change_name_post.user_ctx = ctx_info;
        httpd_register_uri_handler(server, &system_station_change_name_post);
        login_get.user_ctx = ctx_info;
        httpd_register_uri_handler(server, &login_get);
        login_post.user_ctx = ctx_info;
        httpd_register_uri_handler(server, &login_post);
        httpd_uri_t common_get_uri = {
            .uri = "/*",
            .method = HTTP_GET,
            .handler = rest_common_get_handler,
            .user_ctx = ctx_info
        };
        httpd_register_uri_handler(server, &common_get_uri);

        return server;
    }

    ESP_LOGE(TAG, "Error starting server!");
    return NULL;
err:
    return NULL;
}

static void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}

static bool s_if_init = false;

esp_err_t modem_http_get_nvs_wifi_config(modem_wifi_config_t *wifi_config)
{
    char str[64] = "";
    size_t str_size = sizeof(str);

    esp_err_t err = from_nvs_get_value("ssid", str, &str_size);
    if (err == ESP_OK) {
        strncpy(wifi_config->ssid, str, str_size);
    }
    str_size = sizeof(str);

    err = from_nvs_get_value("password", str, &str_size);
    if (err == ESP_OK) {
        strncpy(wifi_config->password, str, sizeof(wifi_config->password));
    }
    str_size = sizeof(str);

    err = from_nvs_get_value("auth_mode", str, &str_size);
    if (err == ESP_OK) {
        if (!strcmp(str, "OPEN")) {
            wifi_config->authmode = WIFI_AUTH_OPEN;
        } else if (!strcmp(str, "WEP")) {
            wifi_config->authmode = WIFI_AUTH_WEP;
        } else if (!strcmp(str, "WPA2_PSK")) {
            wifi_config->authmode = WIFI_AUTH_WPA2_PSK;
        } else if (!strcmp(str, "WPA_WPA2_PSK")) {
            wifi_config->authmode = WIFI_AUTH_WPA_WPA2_PSK;
        } else {
            ESP_LOGE(TAG, "auth_mode %s is not define", str);
        }
    }
    str_size = sizeof(str);

    err = from_nvs_get_value("channel", str, &str_size);
    if (err == ESP_OK) {
        wifi_config->channel = atoi(str);
    }
    str_size = sizeof(str);

    from_nvs_get_value("hide_ssid", str, &str_size);
    if (err == ESP_OK) {
        if (!strcmp(str, "true")) {
            wifi_config->ssid_hidden = 1;
        } else {
            wifi_config->ssid_hidden = 0;
        }
    }
    str_size = sizeof(str);

    err = from_nvs_get_value("bandwidth", str, &str_size);
    if (err == ESP_OK) {
        if (!strcmp(str, "40")) {
            wifi_config->bandwidth = WIFI_BW_HT40;
        } else {
            wifi_config->bandwidth = WIFI_BW_HT20;
        }
    }

    err = from_nvs_get_value("max_connection", str, &str_size);
    if (err == ESP_OK) {
        wifi_config->max_connection = atoi(str);
    }

    return ESP_OK;
}

static esp_err_t init_fs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = CONFIG_EXAMPLE_WEB_MOUNT_POINT,
        .partition_label = NULL,
        /*Maximum files that could be open at the same time.*/
        .max_files = 1,
        .format_if_mount_failed = true
    };
    /*Register and mount SPIFFS to VFS with given path prefix.*/
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return ESP_OK;
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
        sta_remove_node(event->mac);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
        stalist_add_node(event->mac);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_AP_STAIPASSIGNED) {
        stalist_update();
    }
}

static httpd_handle_t s_server = NULL;

esp_err_t modem_http_deinit(httpd_handle_t server)
{
    if (s_if_init == true) {
        s_modem_wifi_config = NULL;
        stop_webserver(server);
        s_if_init = false;
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t modem_http_init(modem_wifi_config_t *wifi_config)
{
    if (s_if_init == false) {
        s_modem_wifi_config = wifi_config;
        SLIST_INIT(&s_sta_list_head);
        /* Start the server for the first time */
        ESP_ERROR_CHECK(init_fs());
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &event_handler, NULL));
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
        s_server = start_webserver(CONFIG_EXAMPLE_WEB_MOUNT_POINT);
        ESP_LOGI(TAG, "Starting webserver");
        s_sta_node_mutex = xSemaphoreCreateMutex();
        STA_CHECK(s_sta_node_mutex != NULL, "sensor_node xSemaphoreCreateMutex failed", ESP_FAIL);
        s_if_init = true;
        return ESP_OK;
    }
    ESP_LOGI(TAG, "http server already initialized");
    return ESP_FAIL;
}
