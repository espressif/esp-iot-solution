// Copyright 2020-2022 Espressif Systems (Shanghai) PTE LTD
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

#include <sys/queue.h>
#include "esp_err.h"
#include "modem_wifi.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Structure for recording the connected sta
 * 
 */
struct modem_netif_sta_info {
    SLIST_ENTRY(modem_netif_sta_info) field;
    uint8_t mac[6];
    char name[32];
    esp_ip4_addr_t ip;
    int64_t start_time;
};

typedef SLIST_HEAD(modem_http_list_head, modem_netif_sta_info) modem_http_list_head_t;

/**
 * @brief Deinit http server
 * 
 * @return esp_err_t 
 */
esp_err_t modem_http_deinit(httpd_handle_t server);

/**
 * @brief Initialize http server
 * 
 * @param wifi_config wifi Configure Information
 * @return esp_err_t 
 */
esp_err_t modem_http_init(modem_wifi_config_t *wifi_config);

/**
 * @brief Read the stored wifi configuration information from nsv and loaded into the structure wifi_config
 * 
 * @param wifi_config wifi Configure Information
 * @return esp_err_t 
 */
esp_err_t modem_http_get_nvs_wifi_config(modem_wifi_config_t *wifi_config);

/**
 * @brief Print information about the nodes in the chain table
 * 
 * @param head Link table header
 * @return esp_err_t 
 */
esp_err_t modem_http_print_nodes(modem_http_list_head_t *head);

/**
 * @brief Get managed list information
 * 
 * @return struct modem_http_list_head_t 
 */
modem_http_list_head_t *modem_http_get_stalist();

#ifdef __cplusplus
}
#endif