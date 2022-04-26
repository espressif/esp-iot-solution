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
    const char *ssid;
    const char *password;
    size_t channel;
    size_t max_connection;
    size_t ssid_hidden;
    wifi_auth_mode_t authmode;
    wifi_bandwidth_t bandwidth;
    wifi_interface_t ifx;
} modem_wifi_config_t;

#define MODEM_WIFI_DEFAULT_CONFIG()          \
    {                                           \
        .mode = WIFI_MODE_AP,                 \
        .ssid = "esp_4g_router",                  \
        .password = "12345678",                 \
        .channel = 13,                 \
        .max_connection = 10                 \
    }

esp_err_t modem_netif_get_sta_list(esp_netif_sta_list_t *netif_sta_list);
esp_err_t modem_wifi_ap_init(void);
esp_err_t modem_wifi_set(modem_wifi_config_t *config);
esp_err_t modem_wifi_napt_enable();
esp_err_t modem_wifi_sta_connected(uint32_t wait_ms);
esp_err_t modem_wifi_set_dhcps(esp_netif_t *netif, uint32_t addr);
esp_netif_t *modem_wifi_init(wifi_mode_t mode); //will abandoned
esp_err_t modem_netif_updata_sta_list();

#ifdef __cplusplus
}
#endif