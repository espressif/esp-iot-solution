/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C"
{
#endif

ESP_EVENT_DECLARE_BASE(MODEM_BOARD_EVENT);

typedef enum {
    MODEM_EVENT_DTE_CONN = 0,        /*!< Modem DTE Connected */
    MODEM_EVENT_DTE_DISCONN,         /*!< Modem DTE Disconnected */
    MODEM_EVENT_SIMCARD_CONN,        /*!< Modem SIM CARD Connected */
    MODEM_EVENT_SIMCARD_DISCONN,     /*!< Modem SIM CARD Disconnected */
    MODEM_EVENT_DTE_RESTART,         /*!< Modem DTE Reset to restart */
    MODEM_EVENT_DTE_RESTART_DONE,    /*!< Modem DTE Restart done */
    MODEM_EVENT_NET_CONN,            /*!< Modem Net Connected */
    MODEM_EVENT_NET_DISCONN,         /*!< Modem Net Disconnected */
    MODEM_EVENT_WIFI_STA_CONN,       /*!< Modem Wi-Fi new station Connected */
    MODEM_EVENT_WIFI_STA_DISCONN,    /*!< Modem Wi-Fi station disconnected */
    MODEM_EVENT_UNKNOWN              /*!< Modem Unknown Response */
} modem_event_t;

#define MODEM_DEFAULT_CONFIG()\
    {                                \
        .rx_buffer_size = 1024*15,   \
        .tx_buffer_size = 1024*15,   \
        .line_buffer_size = 1600,    \
        .event_task_priority = CONFIG_USBH_TASK_BASE_PRIORITY + 1,\
        .event_task_stack_size = 3072\
    }

#define MODEM_FLAGS_INIT_NOT_FORCE_RESET   (1UL<< 1)   /*!< If set, will not reset 4g modem using reset pin during init */
#define MODEM_FLAGS_INIT_NOT_ENTER_PPP     (1UL<< 2)   /*!< If set, will not enter ppp mode during init */
#define MODEM_FLAGS_INIT_NOT_BLOCK         (1UL<< 3)   /*!< If set, will not wait until ppp got ip before modem_board_init return */

typedef struct {
    int rx_buffer_size;             /*!< USB RX Buffer Size */
    int tx_buffer_size;             /*!< USB TX Buffer Size */
    int line_buffer_size;           /*!< Line buffer size for command mode */
    int event_task_priority;        /*!< USB Event/Data Handler Task Priority*/
    int event_task_stack_size;      /*!< USB Event/Data Handler Task Stack Size*/
    esp_event_handler_t handler;    /*!< Modem event handler */
    void *handler_arg;              /*!< Modem event handler arg */
    int flags;                      /*!< Modem config flag bits */
} modem_config_t;

/**
 * @brief Init all about the modem object
 *
 * @param config modem board config value
 * @return ** esp_err_t
 */
esp_err_t modem_board_init(modem_config_t *config);

/**
 * @brief Get the DNS information of modem ppp interface
 *
 * @param  type Type of DNS Server to get: ESP_NETIF_DNS_MAIN, ESP_NETIF_DNS_BACKUP
 * @param dns  DNS Server result is written here on success
 * @return ** esp_err_t
 */
esp_err_t modem_board_get_dns_info(esp_netif_dns_type_t type, esp_netif_dns_info_t *dns);

/**
 * @brief Get 4g signal quality value
 *
 * @param rssi received signal strength indication <rssi>, 2..30: -109..-53 dBm, 99 means not known or not detectable
 * @param ber channel bit error rate (in percent)
 * @return ** esp_err_t
 */
esp_err_t modem_board_get_signal_quality(int *rssi, int *ber);

/**
 * @brief Check if SIM Card ready
 *
 * @param if_ready output true if ready
 * @return ** esp_err_t
 */
esp_err_t modem_board_get_sim_cart_state(int *if_ready);

/**
 * @brief Force reset modem through reset pin
 *
 * @return ** esp_err_t
 */
esp_err_t modem_board_force_reset(void);

/**
 * @brief This function is used to get the operator's name, otherwise return ESP_FAIL if the network is not registered.
 *
 * @param buf buffer to store name
 * @param buf_size buffer size
 * @return ** esp_err_t
 */
esp_err_t modem_board_get_operator_state(char *buf, size_t buf_size);

/**
 * @brief Set APN, force_enable true will try to re-dialup if not will be enabled in next dialup
 *
 * @param new_apn APN string
 * @param force_enable if force enable
 * @return ** esp_err_t
 */
esp_err_t modem_board_set_apn(const char *new_apn, bool force_enable);

/**
 * @brief If enable modem network auto-connect, true will re-dialup if PPP/usb lost
 *
 * @return ** esp_err_t
 */
esp_err_t modem_board_ppp_auto_connect(bool enable);

/**
 * @brief If enter network mode, if enabled network AT command will not response
 * if need AT response during network mode, the modem must support secondary AT port
 *
 * @return ** esp_err_t
 */
esp_err_t modem_board_ppp_start(uint32_t timeout_ms);

/**
 * @brief If exit network mode, AT command can be response after ppp stop
 *
 * @return ** esp_err_t
 */
esp_err_t modem_board_ppp_stop(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
