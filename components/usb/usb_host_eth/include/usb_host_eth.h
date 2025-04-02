/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include "esp_err.h"
#include "esp_netif.h"
#include "usb/cdc_acm_host.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief USB Host Ethernet ECM Configuration
 */
typedef struct {
    uint16_t vid;                      /*!< USB device vendor ID */
    uint16_t pid;                      /*!< USB device product ID */
    uint16_t out_buffer_size;          /*!< Size of the USB OUT buffer */
    uint16_t in_buffer_size;           /*!< Size of the USB IN buffer */
    uint32_t connection_timeout_ms;    /*!< Connection timeout in milliseconds */
    void (*on_connection_cb)(bool connected); /*!< Connection status change callback */
} usb_host_eth_config_t;

/**
 * @brief Default USB Host Ethernet ECM Configuration
 */
#define USB_HOST_ETH_DEFAULT_CONFIG() { \
    .vid = 0x1a86, \
    .pid = 0x5397, \
    .out_buffer_size = 1600, \
    .in_buffer_size = 1600, \
    .connection_timeout_ms = 2000, \
    .on_connection_cb = NULL, \
}

/**
 * @brief Initialize USB Host Ethernet driver
 *
 * This function initializes ESP-NETIF, creates network interface
 * and sets up event handlers.
 *
 * @param[in] config Configuration of the USB ECM device
 * @return
 *      - ESP_OK: Successfully initialized the USB ECM driver
 *      - ESP_ERR_INVALID_ARG: Invalid argument
 *      - ESP_ERR_NO_MEM: Failed to allocate resources
 *      - ESP_FAIL: Failed to initialize for other reasons
 */
esp_err_t usb_host_eth_init(const usb_host_eth_config_t *config);

/**
 * @brief Deinitialize USB Host Ethernet driver
 *
 * @return
 *      - ESP_OK: Successfully deinitialized the USB ECM driver
 *      - ESP_ERR_INVALID_STATE: Driver not initialized
 */
esp_err_t usb_host_eth_deinit(void);

/**
 * @brief Connect to a USB ECM device
 *
 * This function attempts to connect to a USB ECM device with the configured VID/PID.
 * If successful, it will set up the device with proper settings.
 *
 * @param[out] disconnected_sem Semaphore to be signaled when device disconnects (optional, can be NULL)
 * @return
 *      - ESP_OK: Successfully connected to a USB ECM device
 *      - ESP_ERR_NOT_FOUND: No matching USB ECM device found
 *      - ESP_ERR_INVALID_STATE: Driver not initialized
 *      - ESP_FAIL: Failed to connect for other reasons
 */
esp_err_t usb_host_eth_connect(SemaphoreHandle_t disconnected_sem);

/**
 * @brief Disconnect from the USB ECM device
 *
 * @return
 *      - ESP_OK: Successfully disconnected from the USB ECM device
 *      - ESP_ERR_INVALID_STATE: Not connected
 */
esp_err_t usb_host_eth_disconnect(void);

/**
 * @brief Get the ESP-NETIF instance used by the USB Host Ethernet driver
 *
 * @return ESP-NETIF instance handle or NULL if not initialized
 */
esp_netif_t *usb_host_eth_get_netif(void);

/**
 * @brief Get the connection status of the USB Host Ethernet driver
 *
 * @return true if connected, false otherwise
 */
bool usb_host_eth_is_connected(void);

#ifdef __cplusplus
}
#endif
