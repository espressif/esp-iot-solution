/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_WIFI_BANDWIFTH_40
#define MODEM_WIFI_BANDWIFTH WIFI_BW_HT40
#else
#define MODEM_WIFI_BANDWIFTH WIFI_BW_HT20
#endif

/**
 * @brief  Broadcast SSID or not, default 0, broadcast the SSID
 *
 */
#define MODEM_WIFI_DEFAULT_CONFIG()                \
{                                                  \
    .mode = WIFI_MODE_AP,                          \
    .ap_ssid = CONFIG_ESP_WIFI_AP_SSID,            \
    .ap_password = CONFIG_ESP_WIFI_AP_PASSWORD,    \
    .sta_ssid = CONFIG_ESP_WIFI_STA_SSID,          \
    .sta_password = CONFIG_ESP_WIFI_STA_PASSWORD,  \
    .channel = CONFIG_ESP_WIFI_AP_CHANNEL,         \
    .max_connection = CONFIG_MODEM_WIFI_MAX_STA,   \
    .ssid_hidden = 0,                              \
    .authmode = WIFI_AUTH_WPA_WPA2_PSK,            \
    .bandwidth = MODEM_WIFI_BANDWIFTH,             \
}

typedef struct {
    wifi_mode_t mode;              /*!< Wi-Fi Work mode */
    char ap_ssid[32];                 /*!< Wi-Fi SSID of AP mode */
    char ap_password[64];             /*!< Wi-Fi password of AP mode */
    char sta_ssid[32];             /*!< Wi-Fi SSID for station mode*/
    char sta_password[64];         /*!< Wi-Fi password for station mode*/
    char dns[16];                  /*!< Wi-Fi SoftAP DNS address */
    size_t channel;                /*!< Wi-Fi channel of the mode */
    size_t max_connection;         /*!< Wi-Fi max connections of the softap mode */
    size_t ssid_hidden;            /*!< If hide ssid in softap mode */
    wifi_auth_mode_t authmode;     /*!< Wi-Fi authenticate  mode */
    wifi_bandwidth_t bandwidth;    /*!< Wi-Fi bandwidth 20MHz or 40MHz */
} modem_wifi_config_t;

/**
 * @brief Wi-Fi configuration main function
 *
 * @param config Wi-Fi configuration
 *
 */
void app_wifi_main(const modem_wifi_config_t *config);

/**
 * @brief Get the wifi station netif
 *
 * @return The station netif pointer
 */
esp_netif_t *app_wifi_get_sta_netif(void);

/**
 * @brief Get the wifi softap netif
 *
 * @return The softap netif pointer
 */
esp_netif_t *app_wifi_get_ap_netif(void);

#ifdef __cplusplus
}
#endif
