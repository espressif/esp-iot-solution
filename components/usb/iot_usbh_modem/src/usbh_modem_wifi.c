/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "lwip/opt.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/lwip_napt.h"
#include "dhcpserver/dhcpserver.h"
#include "usbh_modem_wifi.h"
#include "usbh_modem_board.h"

static const char *TAG = "modem_wifi";
static int s_active_station_num = 0;
static esp_netif_t *s_wifi_netif[WIFI_MODE_MAX] = {NULL};

/* Event handler for catching system events */
static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_AP_STAIPASSIGNED) {
        ESP_LOGI(TAG, "Get IP addr");
    } else if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_AP_STACONNECTED:
            if (s_active_station_num == 0) {
                modem_wifi_napt_enable(true);
            }
            if (++s_active_station_num > 0) {
                esp_event_post(MODEM_BOARD_EVENT, MODEM_EVENT_WIFI_STA_CONN, (void *)s_active_station_num, 0, 0);
            }
            break;
        case WIFI_EVENT_AP_STADISCONNECTED:
            if (--s_active_station_num == 0) {
                esp_event_post(MODEM_BOARD_EVENT, MODEM_EVENT_WIFI_STA_DISCONN, (void *)s_active_station_num, 0, 0);
                //TODO: esp-lwip NAPT now have a bug, disable then enable NAPT will cause random table index
                //modem_wifi_napt_enable(false);
            }
            break;
        default:
            break;
        }
    }
}

esp_netif_t *modem_wifi_init(wifi_mode_t mode)
{
    esp_netif_t *wifi_netif = NULL;

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
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(mode));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Wi-Fi %s started", mode == WIFI_MODE_STA ? "STA" : "AP");
    s_wifi_netif[mode] = wifi_netif;
    return wifi_netif;
}

esp_netif_t *modem_wifi_ap_init(void)
{
    return modem_wifi_init(WIFI_MODE_AP);
}

esp_err_t modem_wifi_set(modem_wifi_config_t *config)
{
    if (config == NULL || config->ssid[0] == 0 || config->password[0] == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_config_t wifi_cfg = { 0 };

    if (config->mode & WIFI_MODE_STA) {
        strlcpy((char *)wifi_cfg.sta.ssid, config->ssid, sizeof(wifi_cfg.sta.ssid));
        strlcpy((char *)wifi_cfg.sta.password, config->password, sizeof(wifi_cfg.sta.password));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_cfg));

        ESP_LOGI(TAG, "sta ssid: %s password: %s", config->ssid, config->password);
    }

    if (config->mode & WIFI_MODE_AP) {
        wifi_cfg.ap.max_connection = config->max_connection;
        wifi_cfg.ap.authmode = config->authmode;
        wifi_cfg.ap.channel = config->channel;
        strlcpy((char *)wifi_cfg.ap.ssid, config->ssid, sizeof(wifi_cfg.ap.ssid));
        strlcpy((char *)wifi_cfg.ap.password, config->password, sizeof(wifi_cfg.ap.password));
        wifi_cfg.ap.ssid_hidden = config->ssid_hidden;
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_cfg));
        ESP_ERROR_CHECK(esp_wifi_set_bandwidth(ESP_IF_WIFI_AP, config->bandwidth));
        ESP_LOGI(TAG, "softap ssid: %s password: %s", config->ssid, config->password);
        if (config->dns[0]) {
            uint32_t ap_dns_addr = inet_addr(config->dns);
            ESP_ERROR_CHECK(modem_wifi_set_dns(s_wifi_netif[WIFI_MODE_AP], ap_dns_addr));
            ESP_LOGI(TAG, "ap dns addr(manual): %s", config->dns);
        } else {
            uint32_t ap_dns_addr = inet_addr(CONFIG_MODEM_WIFI_DEFAULT_DNS);
            ESP_ERROR_CHECK(modem_wifi_set_dns(s_wifi_netif[WIFI_MODE_AP], ap_dns_addr));
            ESP_LOGI(TAG, "ap dns addr(default): %s", CONFIG_MODEM_WIFI_DEFAULT_DNS);
        }
    }
    return ESP_OK;
}

esp_err_t modem_wifi_napt_enable(bool enable)
{
    ip_napt_enable(_g_esp_netif_soft_ap_ip.ip.addr, enable);
    ESP_LOGI(TAG, "NAT is %s", enable ? "enabled" : "disabled");
    return ESP_OK;
}

esp_err_t modem_wifi_set_dns(esp_netif_t *netif, uint32_t addr)
{
    esp_netif_dns_info_t dns;
    dns.ip.u_addr.ip4.addr = addr;
    dns.ip.type = IPADDR_TYPE_V4;
    dhcps_offer_t dhcps_dns_value = OFFER_DNS;
    esp_netif_dhcps_stop(netif);
    ESP_ERROR_CHECK(esp_netif_dhcps_option(netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &dhcps_dns_value, sizeof(dhcps_dns_value)));
    ESP_ERROR_CHECK(esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns));
    esp_netif_dhcps_start(netif);
    return ESP_OK;
}
