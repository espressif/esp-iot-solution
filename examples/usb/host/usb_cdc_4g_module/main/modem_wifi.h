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

#pragma once

#include "esp_err.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct {
    wifi_mode_t mode;
    char ssid[32];
    char password[64];
    size_t channel;
    size_t max_connection;
    size_t ssid_hidden;
    wifi_auth_mode_t authmode;
    wifi_bandwidth_t bandwidth;
    wifi_interface_t ifx;
} modem_wifi_config_t;

/**
 * @brief  Broadcast SSID or not, default 0, broadcast the SSID 
 * 
 */
#define MODEM_WIFI_DEFAULT_CONFIG()       \
{                                         \
    .mode = WIFI_MODE_AP,                 \
    .ssid = CONFIG_EXAMPLE_WIFI_SSID,              \
    .password = CONFIG_EXAMPLE_WIFI_PASSWORD,               \
    .channel = CONFIG_EXAMPLE_WIFI_CHANNEL,                        \
    .max_connection = CONFIG_EXAMPLE_MAX_STA_CONN,                 \
    .ssid_hidden = 0,                     \
    .authmode = WIFI_AUTH_WPA_WPA2_PSK,   \
    .bandwidth = WIFI_BW_HT20             \
}

esp_err_t modem_netif_get_sta_list(esp_netif_sta_list_t *netif_sta_list);
esp_err_t modem_wifi_ap_init(void);
esp_err_t modem_wifi_set(modem_wifi_config_t *config);
esp_err_t modem_wifi_napt_enable();
esp_err_t modem_wifi_sta_connected(uint32_t wait_ms);
esp_err_t modem_wifi_set_dhcps(esp_netif_t *netif, uint32_t addr);
esp_netif_t *modem_wifi_init(wifi_mode_t mode); //will abandoned

#ifdef __cplusplus
}
#endif