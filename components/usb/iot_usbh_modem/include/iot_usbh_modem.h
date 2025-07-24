/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_modem_dte_types.h"
#include "at_3gpp_ts_27_007.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief USB modem configuration structure
 */
typedef struct {
    const usb_modem_id_t *modem_id_list;    /*!< USB modem ID list */
    size_t at_tx_buffer_size;           /*!< AT command tx buffer size */
    size_t at_rx_buffer_size;           /*!< AT command rx buffer size */
} usbh_modem_config_t;

/**
 * @brief Install and initialize the USB modem subsystem
 *
 * Sets up the USB modem manager, creates internal resources and prepares the modem
 * for operation according to the provided configuration.
 *
 * @param[in] config Pointer to modem configuration
 *
 * @return
 *  - ESP_OK: Installed successfully
 *  - ESP_ERR_INVALID_ARG: Invalid argument
 *  - ESP_ERR_INVALID_STATE: Already installed
 *  - ESP_ERR_NO_MEM: Memory allocation failed
 */
esp_err_t usbh_modem_install(const usbh_modem_config_t *config);

/**
 * @brief Uninstall the USB modem subsystem
 *
 * Releases resources allocated during installation and stops related tasks.
 *
 * @return
 *  - ESP_OK: Uninstalled successfully
 *  - ESP_ERR_INVALID_STATE: Not installed
 */
esp_err_t usbh_modem_uninstall(void);

/**
 * @brief Get the ESP-NETIF of the modem
 *
 * Returns the network interface created for PPP networking.
 *
 * @return Pointer to `esp_netif_t` if available, otherwise NULL
 */
esp_netif_t *usbh_modem_get_netif();

/**
 * @brief Enable or disable PPP auto-connect
 *
 * When enabled, the modem will attempt to establish PPP automatically after
 * device connection or recoveries.
 *
 * @note PPP auto-connect is enabled by default.
 *
 * @param[in] enable true to enable, false to disable
 *
 * @return ESP_OK on success, error otherwise
 */
esp_err_t usbh_modem_ppp_auto_connect(bool enable);

/**
 * @brief Start PPP connection and wait for link up
 *
 * @param[in] timeout Maximum time to wait for PPP to become connected
 *
 * @return
 *  - ESP_OK: PPP connected within timeout
 *  - ESP_ERR_TIMEOUT: Timed out waiting for connection
 *  - ESP_ERR_INVALID_STATE: Modem not ready
 *  - ESP_FAIL: Failed to start PPP
 */
esp_err_t usbh_modem_ppp_start(TickType_t timeout);

/**
 * @brief Stop PPP connection
 *
 * @return
 *  - ESP_OK: Stopped successfully (or already stopped)
 *  - ESP_ERR_INVALID_STATE: Modem not ready
 *  - ESP_FAIL: Operation failed
 */
esp_err_t usbh_modem_ppp_stop();

/**
 * @brief Get the AT command parser handle associated with the modem
 *
 * @return AT parser handle (`at_handle_t`), or NULL if unavailable
 */
at_handle_t usbh_modem_get_atparser();

#ifdef __cplusplus
}
#endif
