/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "esp_err.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct {
    wifi_mode_t mode;              /*!< Wi-Fi Work mode */
    char ssid[32];                 /*!< Wi-Fi SSID of the mode */
    char password[64];             /*!< Wi-Fi password of the mode */
    char dns[16];                  /*!< Wi-Fi SoftAP DNS address */
    size_t channel;                /*!< Wi-Fi channel of the mode */
    size_t max_connection;         /*!< Wi-Fi max connections of the softap mode */
    size_t ssid_hidden;            /*!< If hide ssid in softap mode */
    wifi_auth_mode_t authmode;     /*!< Wi-Fi authenticate  mode */
    wifi_bandwidth_t bandwidth;    /*!< Wi-Fi bandwidth 20MHz or 40MHz */
} modem_wifi_config_t;

#ifdef CONFIG_WIFI_BANDWIFTH_40
#define MODEM_WIFI_BANDWIFTH WIFI_BW_HT40
#else
#define MODEM_WIFI_BANDWIFTH WIFI_BW_HT20
#endif

/**
 * @brief  Broadcast SSID or not, default 0, broadcast the SSID
 *
 */
#define MODEM_WIFI_DEFAULT_CONFIG()               \
{                                                 \
    .mode = WIFI_MODE_AP,                         \
    .ssid = CONFIG_MODEM_WIFI_SSID,               \
    .password = CONFIG_MODEM_WIFI_PASSWORD,       \
    .channel = CONFIG_MODEM_WIFI_CHANNEL,         \
    .max_connection = CONFIG_MODEM_WIFI_MAX_STA,\
    .ssid_hidden = 0,                             \
    .authmode = WIFI_AUTH_WPA_WPA2_PSK,           \
    .bandwidth = MODEM_WIFI_BANDWIFTH,                    \
}

/**
 * @brief Initializes the ESP32's Wi-Fi
 *
 * @param mode Wi-Fi work mode
 * @return pointer to an esp_netif_t object.
 */
esp_netif_t *modem_wifi_init(wifi_mode_t mode);

/**
 * @brief Initializes the ESP32's Wi-Fi in AP mode
 *
 * @return pointer to an esp_netif_t object.
 */
esp_netif_t *modem_wifi_ap_init(void);

/**
 * @brief Config Wi-Fi param in runtime
 *
 * @param config
 * @return ** esp_err_t
 */
esp_err_t modem_wifi_set(modem_wifi_config_t *config);

/**
 * @brief Enable AP interface NAPT function
 *
 * @return ** esp_err_t
 */
esp_err_t modem_wifi_napt_enable(bool enable);

/**
 * @brief Config DNS IP address of Wi-Fi interface
 *
 * @param netif net interface
 * @param addr
 * @return ** esp_err_t
 */
esp_err_t modem_wifi_set_dns(esp_netif_t *netif, uint32_t addr);

#ifdef __cplusplus
}
#endif
