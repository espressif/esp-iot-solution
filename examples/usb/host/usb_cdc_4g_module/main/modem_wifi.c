// Copyright 2019-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_fat.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "lwip/opt.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/lwip_napt.h"
#include "modem_http_config.h"
#include "modem_wifi.h"

static const char* TAG = "modem_wifi";

esp_err_t modem_netif_updata_sta_list()
{
    esp_netif_sta_list_t netif_sta_list = { 0 };
    wifi_sta_list_t sta_list = { 0 };
    ESP_ERROR_CHECK(esp_wifi_ap_get_sta_list(&sta_list));
    ESP_ERROR_CHECK(esp_netif_get_sta_list(&sta_list, &netif_sta_list));
    for (int i = 0; i < netif_sta_list.num; i++) {
        ESP_LOGI(TAG, "netif_sta mac is " MACSTR ",ip is " IPSTR " ",
            MAC2STR(netif_sta_list.sta[i].mac),
            IP2STR(&netif_sta_list.sta[i].ip));
    }

    ESP_ERROR_CHECK(modem_http_sta_list_updata(&netif_sta_list));
    return ESP_OK;
}

/* Event handler for catching system events */
static void event_handler(void* arg, esp_event_base_t event_base,
    int event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d",
            MAC2STR(event->mac), event->aid);
        ESP_ERROR_CHECK(modem_netif_updata_sta_list());
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
            MAC2STR(event->mac), event->aid);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_AP_STAIPASSIGNED) {
        ESP_LOGI(TAG, "Get ip addr updata");
        ESP_ERROR_CHECK(modem_netif_updata_sta_list());
    }
}

esp_err_t modem_wifi_ap_init(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Register our event handler for Wi-Fi, IP and Provisioning related events */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));

    return esp_wifi_start();
}

esp_netif_t* modem_wifi_init(wifi_mode_t mode)
{
    esp_netif_t* wifi_netif = NULL;

    if (mode & WIFI_MODE_STA) {
        wifi_netif = esp_netif_create_default_wifi_sta();
    }

    if (mode & WIFI_MODE_AP) {
        wifi_netif = esp_netif_create_default_wifi_ap();
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Register our event handler for Wi-Fi, IP and Provisioning related events */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(mode));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_start());

    return wifi_netif;
}

esp_err_t modem_wifi_set(modem_wifi_config_t* config)
{
    if (config == NULL || config->ssid == NULL || config->password == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_config_t wifi_cfg = { 0 };

    if (config->mode & WIFI_MODE_STA) {
        strlcpy((char*)wifi_cfg.sta.ssid, config->ssid, sizeof(wifi_cfg.sta.ssid));
        strlcpy((char*)wifi_cfg.sta.password, config->password, sizeof(wifi_cfg.sta.password));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_cfg));

        ESP_LOGI(TAG, "sta ssid: %s password: %s", config->ssid, config->password);
    }

    if (config->mode & WIFI_MODE_AP) {
        wifi_cfg.ap.max_connection = config->max_connection;
        wifi_cfg.ap.authmode = config->authmode;
        wifi_cfg.ap.channel = config->channel;
        strlcpy((char*)wifi_cfg.ap.ssid, config->ssid, sizeof(wifi_cfg.ap.ssid));
        strlcpy((char*)wifi_cfg.ap.password, config->password, sizeof(wifi_cfg.ap.password));
        wifi_cfg.ap.ssid_hidden = config->ssid_hidden;
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_cfg));
        ESP_ERROR_CHECK(esp_wifi_set_bandwidth(ESP_IF_WIFI_AP, config->bandwidth));
        ESP_LOGI(TAG, "softap ssid: %s password: %s", config->ssid, config->password);
    }

    return ESP_OK;
}

esp_err_t modem_wifi_napt_enable()
{
    ip_napt_enable(_g_esp_netif_soft_ap_ip.ip.addr, 1);
    ESP_LOGI(TAG, "NAT is enabled");

    return ESP_OK;
}

esp_err_t modem_wifi_set_dhcps(esp_netif_t* netif, uint32_t addr)
{
    esp_netif_dns_info_t dns;
    dns.ip.u_addr.ip4.addr = addr;
    dns.ip.type = IPADDR_TYPE_V4;
    dhcps_offer_t dhcps_dns_value = OFFER_DNS;
    ESP_ERROR_CHECK(esp_netif_dhcps_option(netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &dhcps_dns_value, sizeof(dhcps_dns_value)));
    ESP_ERROR_CHECK(esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns));
    return ESP_OK;
}
