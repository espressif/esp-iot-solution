/* SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "rom/ets_sys.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/ip_addr.h"

/* The examples use WiFi configuration that you can set via 'make menuconfig'.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID       CONFIG_ESP_WIFI_ROUTE_SSID
#define EXAMPLE_ESP_WIFI_PASS       CONFIG_ESP_WIFI_ROUTE_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY   CONFIG_ESP_MAXIMUM_RETRY
#define EXAMPLE_ESP_WIFI_AP_SSID    CONFIG_ESP_WIFI_SOFTAP_SSID
#define EXAMPLE_ESP_WIFI_AP_PASS    CONFIG_ESP_WIFI_SOFTAP_PASSWORD
#define EXAMPLE_MAX_STA_CONN        CONFIG_ESP_WIFI_SOFTAP_MAX_STA
#define EXAMPLE_IP_ADDR             CONFIG_SERVER_IP
#define EXAMPLE_ESP_WIFI_AP_CHANNEL CONFIG_ESP_WIFI_AP_CHANNEL

#define EXAMPLE_SSID_LEN            32
#define EXAMPLE_PASSWORD_LEN        64

static const char *TAG = "wifi";

static int s_retry_num = 0;

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
    }
}

static void wifi_init_softap(esp_netif_t *wifi_netif, char *ssid, char *password)
{
    if (strcmp(EXAMPLE_IP_ADDR, "192.168.4.1")) {
        esp_netif_ip_info_t ip;
        memset(&ip, 0, sizeof(esp_netif_ip_info_t));
        ip.ip.addr = ipaddr_addr(EXAMPLE_IP_ADDR);
        ip.gw.addr = ipaddr_addr(EXAMPLE_IP_ADDR);
        ip.netmask.addr = ipaddr_addr("255.255.255.0");
        ESP_ERROR_CHECK(esp_netif_dhcps_stop(wifi_netif));
        ESP_ERROR_CHECK(esp_netif_set_ip_info(wifi_netif, &ip));
        ESP_ERROR_CHECK(esp_netif_dhcps_start(wifi_netif));
    }
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config_t));
    snprintf((char *)wifi_config.ap.ssid, EXAMPLE_SSID_LEN, "%s", ssid);
    wifi_config.ap.ssid_len = strlen((char *)wifi_config.ap.ssid);
    snprintf((char *)wifi_config.ap.password, EXAMPLE_PASSWORD_LEN, "%s", password);
    wifi_config.ap.max_connection = EXAMPLE_MAX_STA_CONN;
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    if (strlen(password) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    if (EXAMPLE_ESP_WIFI_AP_CHANNEL > 0) {
        int channel = EXAMPLE_ESP_WIFI_AP_CHANNEL;
        wifi_config.ap.channel = channel;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));

    ESP_LOGI(TAG, "wifi_init_softap finished.SSID:%s password:%s",
             ssid, password);
}

static void wifi_init_sta(esp_netif_t *wifi_netif, char *ssid, char *password)
{
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config_t));
    snprintf((char *)wifi_config.sta.ssid, EXAMPLE_SSID_LEN, "%s", ssid);
    snprintf((char *)wifi_config.sta.password, EXAMPLE_PASSWORD_LEN, "%s", password);

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s",
             ssid, password);
}

static void nvs_get_str_log(esp_err_t err, char *key, char *value)
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

esp_err_t app_wifi_set_wifi(char *mode, char *ap_ssid, char *ap_passwd, char *sta_ssid, char *sta_passwd)
{
    if (!strcmp(mode, "ap")) {
        from_nvs_set_value("wifimode", "ap");
    } else if (!strcmp(mode, "sta")) {
        from_nvs_set_value("wifimode", "sta");
    } else if (!strcmp(mode, "apsta")) {
        from_nvs_set_value("wifimode", "apsta");
    } else {
        return ESP_FAIL;
    }

    from_nvs_set_value("apssid", ap_ssid);
    from_nvs_set_value("appasswd", ap_passwd);
    from_nvs_set_value("stassid", sta_ssid);
    from_nvs_set_value("stapasswd", sta_passwd);
    return ESP_OK;
}

void app_wifi_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    // Initialize networking stack
    ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop needed by the  main app
    esp_event_loop_create_default();

    wifi_mode_t mode = WIFI_MODE_NULL;
    esp_netif_t *wifi_ap_netif = NULL;
    esp_netif_t *wifi_sta_netif = NULL;

    char str[EXAMPLE_PASSWORD_LEN] = "";
    size_t str_size = sizeof(str);
    char ap_ssid[EXAMPLE_SSID_LEN] = "";
    char ap_passwd[EXAMPLE_PASSWORD_LEN] = "";
    char sta_ssid[EXAMPLE_SSID_LEN] = "";
    char sta_passwd[EXAMPLE_PASSWORD_LEN] = "";

    ret = from_nvs_get_value("wifimode", str, &str_size);
    if (ret == ESP_OK) {
        if (!strcmp(str, "ap")) {
            mode = WIFI_MODE_AP;
        } else if (!strcmp(str, "sta")) {
            mode = WIFI_MODE_STA;
        } else if (!strcmp(str, "apsta")) {
            mode = WIFI_MODE_APSTA;
        }
    } else {
        strcpy(ap_ssid, EXAMPLE_ESP_WIFI_AP_SSID);
        strcpy(ap_passwd, EXAMPLE_ESP_WIFI_AP_PASS);
        strcpy(sta_ssid, EXAMPLE_ESP_WIFI_SSID);
        strcpy(sta_passwd, EXAMPLE_ESP_WIFI_PASS);
    }

    if (mode & WIFI_MODE_AP) {
        str_size = sizeof(str);
        ret = from_nvs_get_value("apssid", str, &str_size);
        if (ret == ESP_OK) {
            strcpy(ap_ssid, str);
        }

        str_size = sizeof(str);
        ret = from_nvs_get_value("appasswd", str, &str_size);
        if (ret == ESP_OK) {
            strcpy(ap_passwd, str);
        }
    }

    if (mode & WIFI_MODE_STA) {
        str_size = sizeof(str);
        ret = from_nvs_get_value("stassid", str, &str_size);
        if (ret == ESP_OK) {
            strcpy(sta_ssid, str);
        }

        str_size = sizeof(str);
        ret = from_nvs_get_value("stapasswd", str, &str_size);
        if (ret == ESP_OK) {
            strcpy(sta_passwd, str);
        }
    }

    if (strlen(ap_ssid) && strlen(sta_ssid)) {
        mode = WIFI_MODE_APSTA;
        wifi_ap_netif = esp_netif_create_default_wifi_ap();
        wifi_sta_netif = esp_netif_create_default_wifi_sta();
    } else if (strlen(ap_ssid)) {
        mode = WIFI_MODE_AP;
        wifi_ap_netif = esp_netif_create_default_wifi_ap();
    } else if (strlen(sta_ssid)) {
        mode = WIFI_MODE_STA;
        wifi_sta_netif = esp_netif_create_default_wifi_sta();
    }

    if (mode == WIFI_MODE_NULL) {
        ESP_LOGW(TAG, "Neither AP or STA have been configured. WiFi will be off.");
        return;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(mode));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    if (mode & WIFI_MODE_AP) {
        wifi_init_softap(wifi_ap_netif, ap_ssid, ap_passwd);
    }

    if (mode & WIFI_MODE_STA) {
        wifi_init_sta(wifi_sta_netif, sta_ssid, sta_passwd);
    }
    ESP_ERROR_CHECK(esp_wifi_start());
}
