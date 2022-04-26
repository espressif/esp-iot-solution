/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/queue.h>
#include "esp_eth.h"
#include "esp_netif.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "esp_tls_crypto.h"
#include "esp_vfs.h"
#include "json_parser.h"
#include "modem_wifi.h"
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
static const char* TAG = "4g_router_server";
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
 * @brief Basic Settings：ssid, password, if_hide_ssid, auth_mode
 *
 */
static char user_ssid[36] = CONFIG_EXAMPLE_WIFI_SSID;
static char user_password[36] = CONFIG_EXAMPLE_WIFI_PASSWORD;
static char user_hide_ssid[8] = "false";
static char user_auth_mode[16] = "WPA_WPA2_PSK";
static size_t user_ssid_size = sizeof(user_ssid);
static size_t user_password_size = sizeof(user_password);
static size_t user_hide_ssid_size = sizeof(user_hide_ssid);
static size_t user_auth_mode_size = sizeof(user_auth_mode);
/**
 * @brief Advanced Settings: Bandwidth, Signal Channel
 *
 */
static char user_channel[4] = "";
static size_t user_channel_size = sizeof(user_channel);
#ifdef CONFIG_WIFI_BANDWIFTH_20
static char user_bandwidth[4] = "20";
#else
static char user_bandwidth[4] = "40";
#endif
static size_t user_bandwidth_size = sizeof(user_bandwidth);

/**
 * @brief Store the currently connected sta
 *
 */
#define STA_CHECK(a, str, ret)                                                 \
    if (!(a)) {                                                                \
        ESP_LOGE(TAG, "%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        return (ret);                                                          \
    }
#define STA_CHECK_GOTO(a, str, lable)                                          \
    if (!(a)) {                                                                \
        ESP_LOGE(TAG, "%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        goto lable;                                                            \
    }

#define STA_NODE_MUTEX_TICKS_TO_WAIT 200
static SemaphoreHandle_t s_sta_node_mutex = NULL;
static SLIST_HEAD(modem_http_list_head_t,
    modem_http_sta_list_t) modem_http_sta_list_head = SLIST_HEAD_INITIALIZER(modem_http_sta_list_head);
// static modem_http_sta_list_t modem_http_sta_list ;

/**
 * @brief admin password
 *
 */
static char admin_password[36] = "espressif";
static size_t admin_password_size = sizeof(admin_password);

static void restart()
{
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}

static void delete_char(char* str, char target)
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
typedef struct
{
    char* username;
    char* password;
} basic_auth_info_t;

typedef struct
{
    rest_server_context_t* rest_context;
    basic_auth_info_t* basic_auth_info;
} ctx_info_t;

#define HTTPD_401 "401 UNAUTHORIZED" /*!< HTTP Response 401 */

static char* http_auth_basic(const char* username, const char* password)
{
    int out;
    char* user_info = NULL;
    char* digest = NULL;
    size_t n = 0;
    asprintf(&user_info, "%s:%s", username, password);
    if (!user_info) {
        ESP_LOGE(TAG, "No enough memory for user information");
        return NULL;
    }
    esp_crypto_base64_encode(NULL, 0, &n, (const unsigned char*)user_info, strlen(user_info));

    /* 6: The length of the "Basic " string
     * n: Number of bytes for a base64 encode format
     * 1: Number of bytes for a reserved which be used to fill zero
     */
    digest = calloc(1, 6 + n + 1);
    if (digest) {
        strcpy(digest, "Basic ");
        esp_crypto_base64_encode((unsigned char*)digest + 6, n, (size_t*)&out, (const unsigned char*)user_info,
            strlen(user_info));
    }
    free(user_info);
    return digest;
}

static esp_err_t basic_auth_get(httpd_req_t* req)
{
    char* buf = NULL;
    size_t buf_len = 0;
    ctx_info_t* ctx_info = req->user_ctx;
    basic_auth_info_t* basic_auth_info = ctx_info->basic_auth_info;

    buf_len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;
    if (buf_len > 1) {
        buf = calloc(1, buf_len);
        if (!buf) {
            ESP_LOGE(TAG, "No enough memory for basic authorization");
            return ESP_ERR_NO_MEM;
        }

        if (httpd_req_get_hdr_value_str(req, "Authorization", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Authorization: %s", buf);
        } else {
            ESP_LOGE(TAG, "No auth value received");
        }

        char* auth_credentials = http_auth_basic(basic_auth_info->username, basic_auth_info->password);
        if (!auth_credentials) {
            ESP_LOGE(TAG, "No enough memory for basic authorization credentials");
            free(buf);
            return ESP_ERR_NO_MEM;
        } else {
            ESP_LOGI(TAG, "auth_credentials : %s", auth_credentials);
        }

        if (strncmp(auth_credentials, buf, buf_len)) {
            ESP_LOGE(TAG, "Not authenticated");
            httpd_resp_set_status(req, HTTPD_401);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "keep-alive");
            httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"router\"");
            httpd_resp_send(req, NULL, 0);
            free(auth_credentials);
            free(buf);
            return ESP_FAIL;
        } else {
            ESP_LOGI(TAG, "Authenticated!");
            char* basic_auth_resp = NULL;
            httpd_resp_set_status(req, HTTPD_200);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "keep-alive");
            free(basic_auth_resp);

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

static void nvs_get_str_log(esp_err_t err, char* key, char* value)
{
    switch (err) {
    case ESP_OK:
        ESP_LOGI(TAG, "%s = %s\n", key, value);
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGI(TAG, "%s is not initialized yet!\n", key);
        break;
    default:
        ESP_LOGI(TAG, "Error (%s) reading!\n", esp_err_to_name(err));
    }
}

static esp_err_t form_nvs_set_value(char* key, char* value)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("memory", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return ESP_FAIL;
    } else {
        err = nvs_set_str(my_handle, key, value);
        printf("set %s is %s!,the err is %d\n", key, (err == ESP_OK) ? "succeed" : "failed", err);
        nvs_close(my_handle);
        printf("nvs_close Done\n");
    }
    return ESP_OK;
}

static esp_err_t form_nvs_get_value(char* key, char* value, size_t* size)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("memory", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return ESP_FAIL;
    } else {
        err = nvs_get_str(my_handle, key, value, size);
        nvs_get_str_log(err, key, value);
        nvs_close(my_handle);
    }
    return err;
}

static esp_err_t sta_add_node(modem_netif_sta_info_t* sta)
{
    STA_CHECK(sta != NULL, "sta pointer can not be NULL", ESP_ERR_INVALID_ARG);
    modem_http_sta_list_t* node = calloc(1, sizeof(modem_http_sta_list_t));
    STA_CHECK(node != NULL, "calloc node failed", ESP_ERR_NO_MEM);
    STA_CHECK_GOTO(pdTRUE == xSemaphoreTake(s_sta_node_mutex, STA_NODE_MUTEX_TICKS_TO_WAIT), "take semaphore timeout",
        cleanupnode);
    node->sta = sta;
    SLIST_INSERT_HEAD(&modem_http_sta_list_head, node, next);
    STA_CHECK_GOTO(pdTRUE == xSemaphoreGive(s_sta_node_mutex), "give semaphore failed", cleanupnode);
    return ESP_OK;

cleanupnode:
    free(node);
    return ESP_FAIL;
}

static esp_err_t sta_remove_node(modem_netif_sta_info_t* sta)
{
    STA_CHECK(sta != NULL, "sensor pointer can not be NULL", ESP_ERR_INVALID_ARG);
    modem_http_sta_list_t* node;
    STA_CHECK(pdTRUE == xSemaphoreTake(s_sta_node_mutex, STA_NODE_MUTEX_TICKS_TO_WAIT), "take semaphore timeout",
        ESP_ERR_TIMEOUT);
    SLIST_FOREACH(node, &modem_http_sta_list_head, next)
    {
        if (!memcmp(node->sta->mac, sta->mac, 6)) {
            ESP_LOGI(TAG, "remove mac is " MACSTR ",ip is " IPSTR ", start_time is %lld ", MAC2STR(node->sta->mac),
                IP2STR(&node->sta->ip), node->sta->start_time);
            SLIST_REMOVE(&modem_http_sta_list_head, node, modem_http_sta_list_t, next);
            free(node);
            break;
        }
    }
    STA_CHECK(pdTRUE == xSemaphoreGive(s_sta_node_mutex), "give semaphore failed", ESP_FAIL);
    return ESP_OK;
}

static esp_err_t sta_print_nodes()
{
    modem_http_sta_list_t* node;
    SLIST_FOREACH(node, &modem_http_sta_list_head, next)
    {
        ESP_LOGI(TAG, "mac is " MACSTR ",ip is " IPSTR ", start_time is %lld ", MAC2STR(node->sta->mac),
            IP2STR(&node->sta->ip), node->sta->start_time);
    }
    return ESP_OK;
}

esp_err_t modem_http_sta_list_updata(esp_netif_sta_list_t* netif_sta_list)
{
    modem_http_sta_list_t* node;
    modem_netif_sta_info_t* sta=NULL;
    for (int i = 0; i < netif_sta_list->num; i++) {
        sta = (modem_netif_sta_info_t*)calloc(1, sizeof(modem_netif_sta_info_t));
        bool not_find = true;
        char mac_addr[18] = "";
        memcpy(sta->mac, netif_sta_list->sta[i].mac, 6); // mac
        size_t name_size = sizeof(sta->name);
        sprintf(mac_addr, "%02x%02x%02x%02x%02x%02x", sta->mac[0], sta->mac[1], sta->mac[2], sta->mac[3], sta->mac[4],
            sta->mac[5]);
        form_nvs_get_value(mac_addr, &sta->name, &name_size); // name
        sta->ip.addr = netif_sta_list->sta[i].ip.addr; // ip
        sta->start_time = esp_timer_get_time(); // time
        SLIST_FOREACH(node, &modem_http_sta_list_head, next)
        {
            if (!memcmp(node->sta->mac, sta->mac, 6)) {
                if (memcmp(node->sta->name, sta->name, name_size)) {
                    memcpy(node->sta->name, sta->name, name_size);
                    ESP_LOGI(TAG, "name of mac: %s changed,is %s ", mac_addr, node->sta->name);
                }
                not_find = false;
                break;
            }
        }
        if (not_find) {
            ESP_LOGI(TAG, "add one ip");
            ESP_ERROR_CHECK(sta_add_node(sta));
            sta_print_nodes();
        } else {
            free(sta);
            sta=NULL;
        }
    }
    node=NULL;
    SLIST_FOREACH(node, &modem_http_sta_list_head, next)
    {
        bool not_find = true;
        sta = (modem_netif_sta_info_t*)calloc(1, sizeof(modem_netif_sta_info_t));
        memcpy(sta->mac, node->sta->mac, 6); // mac
        sta->ip.addr = node->sta->ip.addr; // ip
        sta->start_time = esp_timer_get_time();
        for (int i = 0; i < netif_sta_list->num; i++) {
            if (!memcmp(node->sta->mac, netif_sta_list->sta[i].mac, 6)) {
                not_find = false;
                break;
            }
        }
        if (not_find) {
            ESP_LOGI(TAG, "remove one ip");
            ESP_ERROR_CHECK(sta_remove_node(sta));
            sta_print_nodes(); // pirnt sta has now
            free(sta);
            sta=NULL;
            break;
        }
        free(sta);
        sta=NULL;
    }
    return ESP_OK;
}

static esp_err_t wlan_general_get_handler(httpd_req_t* req)
{
    esp_err_t err = basic_auth_get(req);
    REST_CHECK(err == ESP_OK, "not login yet", end);

    char* json_str = NULL;
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

static esp_err_t wlan_general_options_handler(httpd_req_t* req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "*");
    httpd_resp_send(req, NULL, 0); // Response body can be empty
    return ESP_OK;
}

static esp_err_t wlan_advance_options_handler(httpd_req_t* req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "*");
    httpd_resp_send(req, NULL, 0); // Response body can be empty
    return ESP_OK;
}

static esp_err_t wlan_general_post_handler(httpd_req_t* req)
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
        printf("Parser failed\n");
        return ESP_FAIL;
    }

    char str_val[64];

    if (json_obj_get_string(&jctx, "ssid", str_val, sizeof(str_val)) == OS_SUCCESS) {
        snprintf(user_ssid, sizeof(user_ssid), "%.*s", sizeof(user_ssid) - 1, str_val);
        printf("ssid %s\n", user_ssid);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid post");
        return ESP_FAIL;
    }

    if (json_obj_get_string(&jctx, "if_hide_ssid", str_val, sizeof(str_val)) == OS_SUCCESS) {
        snprintf(user_hide_ssid, sizeof(user_hide_ssid), "%.*s", sizeof(user_hide_ssid) - 1, str_val);
        printf("if_hide_ssid %s\n", user_hide_ssid);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid post");
        return ESP_FAIL;
    }

    if (json_obj_get_string(&jctx, "auth_mode", str_val, sizeof(str_val)) == OS_SUCCESS) {
        snprintf(user_auth_mode, sizeof(user_auth_mode), "%.*s", sizeof(user_auth_mode) - 1, str_val);
        printf("auth_mode %s\n", user_auth_mode);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid post");
        return ESP_FAIL;
    }

    if (json_obj_get_string(&jctx, "password", str_val, sizeof(str_val)) == OS_SUCCESS) {
        snprintf(user_password, sizeof(user_password), "%.*s", sizeof(user_password) - 1, str_val);
        printf("password %s\n", user_password);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid post");
        return ESP_FAIL;
    }

    char* json_str = NULL;
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

    ESP_ERROR_CHECK(form_nvs_set_value("user_ssid", user_ssid));
    ESP_ERROR_CHECK(form_nvs_set_value("user_hide_ssid", user_hide_ssid));
    ESP_ERROR_CHECK(form_nvs_set_value("user_auth_mode", user_auth_mode));
    ESP_ERROR_CHECK(form_nvs_set_value("user_password", user_password));

    restart();
    return ESP_OK;
end:
    return ESP_OK;
}

static esp_err_t wlan_advance_get_handler(httpd_req_t* req)
{
    esp_err_t err = basic_auth_get(req);
    REST_CHECK(err == ESP_OK, "not login yet", end);

    char* json_str = NULL;
    size_t size = 0;
    size = asprintf(&json_str, "{\"status\":\"200\", \"bandwidth\":\"%s\", \"channel\":\"%s\"}", user_bandwidth,
        user_channel);
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

static esp_err_t wlan_advance_post_handler(httpd_req_t* req)
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
        printf("Parser failed\n");
        return ESP_FAIL;
    }

    char str_val[64];

    if (json_obj_get_string(&jctx, "bandwidth", str_val, sizeof(str_val)) == OS_SUCCESS) {
        snprintf(user_bandwidth, sizeof(user_bandwidth), "%.*s", sizeof(user_bandwidth) - 1, str_val);
        printf("bandwidth: %s\n", user_bandwidth);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid post");
        return ESP_FAIL;
    }

    if (json_obj_get_string(&jctx, "channel", str_val, sizeof(str_val)) == OS_SUCCESS) {
        snprintf(user_channel, sizeof(user_channel), "%.*s", sizeof(user_channel) - 1, str_val);
        printf("channel: %s\n", user_channel);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid post");
        return ESP_FAIL;
    }

    char* json_str = NULL;
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

    ESP_ERROR_CHECK(form_nvs_set_value("user_channel", user_channel));
    ESP_ERROR_CHECK(form_nvs_set_value("user_bandwidth", user_bandwidth));
    restart();

    return ESP_OK;
end:
    return ESP_OK;
}

static esp_err_t login_get_handler(httpd_req_t* req)
{
    char* json_str = NULL;
    size_t size = 0;
    size = asprintf(&json_str, "{\"status\":\"200\", \"bandwidth\":\"%s\", \"channel\":\"%s\"}", user_bandwidth,
        user_channel);
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
}

static esp_err_t login_post_handler(httpd_req_t* req)
{
    return ESP_OK;
}

static esp_err_t system_station_get_handler(httpd_req_t* req)
{
    esp_err_t err = basic_auth_get(req);
    REST_CHECK(err == ESP_OK, "not login yet", end);

    char* json_str = NULL;
    char* json_str_old = NULL;
    size_t size = 0;
    modem_http_sta_list_t* node;
    size = asprintf(&json_str, "{\"station_list\":[");
    SLIST_FOREACH(node, &modem_http_sta_list_head, next)
    {
        asprintf(&json_str_old, "%s", json_str);
        free(json_str);
        json_str = NULL;
        size = asprintf(&json_str,
            "%s{\"name_str\":\"%s\",\"mac_str\":\"" MACSTR "\",\"ip_str\":\"" IPSTR
            "\",\"online_time_s\":\"%lld\"}%c",
            json_str_old, node->sta->name, MAC2STR(node->sta->mac), IP2STR(&node->sta->ip),
            node->sta->start_time, node->next.sle_next ? ',' : '\0');
        free(json_str_old);
        json_str_old = NULL;
    }
    size = asprintf(&json_str_old, "%s],\"now_time\":\"%lld\"}",json_str,esp_timer_get_time());

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
    ESP_LOGI(TAG,"%s",json_str_old);
    free(json_str);
    free(json_str_old);
    ESP_ERROR_CHECK(ret);
    return ESP_OK;
end:
    return ESP_OK;
}

static esp_err_t system_station_delete_device_post_handler(httpd_req_t* req)
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
        printf("Parser failed\n");
        return ESP_FAIL;
    }

    char str_val[64];

    if (json_obj_get_string(&jctx, "mac_str", str_val, sizeof(str_val)) == OS_SUCCESS) {
        snprintf(mac_str, sizeof(mac_str), "%.*s", sizeof(mac_str) - 1, str_val);
        printf("mac_str: %s\n", mac_str);
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
    ESP_LOGI(TAG,"remove aid is %d",aid);
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

static esp_err_t system_station_change_name_post_handler(httpd_req_t* req)
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
        printf("Parser failed\n");
        return ESP_FAIL;
    }

    char str_val[64];

    char name_str[36] = "";
    char mac_str[18] = "";

    if (json_obj_get_string(&jctx, "name_str", str_val, sizeof(str_val)) == OS_SUCCESS) {
        snprintf(name_str, sizeof(name_str), "%.*s", sizeof(name_str) - 1, str_val);
        printf("name_str: %s\n", name_str);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid post");
        return ESP_FAIL;
    }

    if (json_obj_get_string(&jctx, "mac_str", str_val, sizeof(str_val)) == OS_SUCCESS) {
        snprintf(mac_str, sizeof(mac_str), "%.*s", sizeof(mac_str) - 1, str_val);
        printf("mac_str: %s\n", mac_str);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "invalid post");
        return ESP_FAIL;
    }
    /**
     * @brief The length of the control key is within 15 bytes
     *    00:01:02:03:04:05 --> 000102030405
     */
    delete_char(&mac_str, ':');

    ESP_ERROR_CHECK(form_nvs_set_value(mac_str, name_str));
    ESP_ERROR_CHECK(modem_netif_updata_sta_list());

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

static esp_err_t set_content_type_from_file(httpd_req_t* req, const char* filepath)
{
    const char* type = "text/plain";
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

static esp_err_t rest_common_get_handler(httpd_req_t* req)
{
    esp_err_t err = basic_auth_get(req);
    ESP_LOGE(TAG, "%d", err);
    REST_CHECK(err == ESP_OK, "not login yet", end);

    ESP_LOGW(TAG, "%s", __func__);
    ESP_LOGI(TAG, "%s", req->uri);
    char filepath[FILE_PATH_MAX];

    //rest_server_context_t* rest_context = (rest_server_context_t*)req->user_ctx->rest_context;
    ctx_info_t* ctx_info = (ctx_info_t*)req->user_ctx;
    rest_server_context_t* rest_context = ctx_info->rest_context;
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

    char* chunk = rest_context->scratch;
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
    ESP_LOGI(TAG, "File sending complete");
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

static httpd_uri_t wlan_general_options = {
    .uri = "/wlan_general",
    .method = HTTP_OPTIONS,
    .handler = wlan_general_options_handler,
    /* Let's pass response string in user context to demonstrate it's usage */
    .user_ctx = NULL
};

static httpd_uri_t wlan_advance_options = {
    .uri = "/wlan_advance",
    .method = HTTP_OPTIONS,
    .handler = wlan_advance_options_handler,
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

static httpd_uri_t system_station_chanege_name_post = {
    .uri = "/system/station_state/change_name",
    .method = HTTP_POST,
    .handler = system_station_change_name_post_handler,
    /* Let's pass response string in user context to demonstrate it's usage */
    .user_ctx = NULL
};

static httpd_handle_t start_webserver(const char* base_path)
{
    ctx_info_t* ctx_info = calloc(1, sizeof(ctx_info_t));
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
        wlan_general_options.user_ctx = ctx_info;
        httpd_register_uri_handler(server, &wlan_general_options);
        wlan_advance_options.user_ctx = ctx_info;
        httpd_register_uri_handler(server, &wlan_advance_options);
        system_station_get.user_ctx = ctx_info;
        httpd_register_uri_handler(server, &system_station_get);
        system_station_delete_device_post.user_ctx = ctx_info;
        httpd_register_uri_handler(server, &system_station_delete_device_post);
        system_station_chanege_name_post.user_ctx = ctx_info;
        httpd_register_uri_handler(server, &system_station_chanege_name_post);
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

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
err:
    return ESP_FAIL;
}

static void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*)arg;

    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        stop_webserver(*server);
        *server = NULL;
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*)arg;

    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver(CONFIG_EXAMPLE_WEB_MOUNT_POINT);
    }
}

static esp_err_t init_nvs(void)
{
    printf("Reading restart counter from NVS ... ");
    form_nvs_get_value("user_password", &user_password, &user_password_size);
    form_nvs_get_value("user_hide_ssid", &user_hide_ssid, &user_hide_ssid_size);
    form_nvs_get_value("user_auth_mode", &user_auth_mode, &user_auth_mode_size);
    form_nvs_get_value("user_channel", &user_channel, &user_channel_size);
    form_nvs_get_value("user_ssid", &user_ssid, &user_ssid_size);
    form_nvs_get_value("user_bandwidth", &user_bandwidth, &user_bandwidth_size);
    return ESP_OK;
}

esp_err_t init_fs(void)
{
    esp_vfs_spiffs_conf_t conf = { .base_path = CONFIG_EXAMPLE_WEB_MOUNT_POINT,
        .partition_label = NULL,
        /*Maximum files that could be open at the same time.*/
        .max_files = 5,
        .format_if_mount_failed = false };
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

void modem_http_config_init(void)
{
    static httpd_handle_t server = NULL;
    sprintf(user_channel, "%d", CONFIG_EXAMPLE_WIFI_CHANNEL);
    /* Pulling configuration files from flash */
    ESP_ERROR_CHECK(init_nvs());
    /* Start the server for the first time */
    ESP_ERROR_CHECK(init_fs());
    server = start_webserver(CONFIG_EXAMPLE_WEB_MOUNT_POINT);
    s_sta_node_mutex = xSemaphoreCreateMutex();
    STA_CHECK(s_sta_node_mutex != NULL, "sensor_node xSemaphoreCreateMutex failed", ESP_FAIL);
}
