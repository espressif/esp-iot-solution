/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "iot_usbh_cdc.h"
#include "esp_modem_dte.h"
#include "esp_modem_dte_types.h"
#include "iot_eth_interface.h"
#include "iot_eth_types.h"
#include "at_3gpp_ts_27_007.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ESP Modem DTE Configuration
 *
 */
typedef struct {
    const usb_modem_id_t *modem_id_list; /*!< USB Modem ID list, must be end with empty entry*/

    struct {
        void (*connect)(iot_eth_driver_t *dte_drv, void *user_data);    /*!< USB connect callback, set NULL if use */
        void (*disconnect)(iot_eth_driver_t *dte_drv, void *user_data); /*!< USB disconnect callback, set NULL if not use */
        void *user_data;                        /*!< User data to pass to the connect/disconnect callbacks */
    } cbs;

    size_t at_tx_buffer_size;   /*!< AT command tx buffer size */
    size_t at_rx_buffer_size;   /*!< AT command rx buffer size */
} esp_modem_dte_config_t;

typedef enum {
    ESP_MODEM_UNKOWN_MODE,      /*!< Unknown mode */
    ESP_MODEM_COMMAND_MODE,     /*!< Command Mode */
    ESP_MODEM_DATA_MODE,         /*!< DATA Mode */
} modem_port_mode_t;

/**
 * @brief Create and initialize a Modem DTE driver
 *
 * This function creates the modem DTE driver according to the given configuration
 * and returns an Ethernet driver handle that exposes the modem as an `iot_eth_driver_t`.
 *
 * @param[in]  config       Pointer to DTE configuration
 * @param[out] ret_handle   Output pointer where the created driver handle will be stored
 *
 * @return
 *  - ESP_OK: Driver created successfully
 *  - ESP_ERR_INVALID_ARG: Null argument or invalid configuration
 *  - ESP_ERR_NO_MEM: Memory allocation failed
 *  - ESP_FAIL: Creation failed due to internal error
 */
esp_err_t esp_modem_dte_new(const esp_modem_dte_config_t *config, iot_eth_driver_t **ret_handle);

/**
 * @brief Synchronize with the modem using a basic AT probe
 *
 * Sends a simple AT command to verify modem responsiveness and align states.
 *
 * @param[in] dte_drv DTE driver handle
 *
 * @return
 *  - ESP_OK: Modem responded correctly
 *  - ESP_ERR_INVALID_ARG: Invalid handle
 *  - ESP_FAIL: Synchronization failed
 */
esp_err_t esp_modem_dte_sync(iot_eth_driver_t *dte_drv);

/**
 * @brief Start PPP dial-up connection
 *
 * Configure the modem and start PPP session establishment.
 *
 * @param[in] dte_drv DTE driver handle
 *
 * @return
 *  - ESP_OK: PPP started (or already connected)
 *  - ESP_ERR_INVALID_STATE: DTE not ready or wrong port mode
 *  - ESP_FAIL: Dial-up failed
 */
esp_err_t esp_modem_dte_dial_up(iot_eth_driver_t *dte_drv);

/**
 * @brief Hang up PPP connection
 *
 * Stops the ongoing PPP session and restores command mode if applicable.
 *
 * @param[in] dte_drv DTE driver handle
 *
 * @return
 *  - ESP_OK: PPP stopped successfully
 *  - ESP_ERR_INVALID_STATE: PPP not running
 *  - ESP_FAIL: Operation failed
 */
esp_err_t esp_modem_dte_hang_up(iot_eth_driver_t *dte_drv);

/**
 * @brief Change modem port operating mode
 *
 * Switch between command mode and data mode. Optionally emits required AT commands
 * to request the transition.
 *
 * @param[in] dte_drv     DTE driver handle
 * @param[in] new_mode    Target mode
 * @param[in] send_at_cmd Whether to send AT command(s) to request mode switching
 *
 * @return
 *  - ESP_OK: Mode changed successfully (or already in requested mode)
 *  - ESP_ERR_INVALID_ARG: Invalid parameter
 *  - ESP_ERR_INVALID_STATE: Operation not allowed in current state
 *  - ESP_FAIL: Mode switch failed
 */
esp_err_t esp_modem_dte_change_port_mode(iot_eth_driver_t *dte_drv, modem_port_mode_t new_mode, bool send_at_cmd);

/**
 * @brief Get current modem port operating mode
 *
 * @param[in] dte_drv DTE driver handle
 *
 * @return Current port mode
 */
modem_port_mode_t esp_modem_dte_get_port_mode(iot_eth_driver_t *dte_drv);

/**
 * @brief Get the AT command parser handle associated with the DTE
 *
 * @param[in] dte_drv DTE driver handle
 *
 * @return AT parser handle (`at_handle_t`), or NULL if unavailable
 */
at_handle_t esp_modem_dte_get_atparser(iot_eth_driver_t *dte_drv);

/**
 * @brief Check whether PPP link is up
 *
 * @param[in] dte_drv DTE driver handle
 *
 * @return true if PPP is connected, false otherwise
 */
bool esp_modem_dte_ppp_is_connect(iot_eth_driver_t *dte_drv);

/**
 * @brief Notify DTE of link stage change
 *
 * Used by upper layers to inform the DTE about link up/down transitions.
 *
 * @param[in] dte_drv DTE driver handle
 * @param[in] link    Link stage
 *
 * @return
 *  - ESP_OK: Stage handled
 *  - ESP_ERR_INVALID_ARG: Invalid argument
 *  - ESP_FAIL: Processing failed
 */
esp_err_t esp_modem_dte_on_stage_changed(iot_eth_driver_t *dte_drv, iot_eth_link_t link);

/**
 * @brief Check whether the modem is physically connected over USB
 *
 * @param[in] dte_drv DTE driver handle
 *
 * @return true if device is connected, false otherwise
 */
bool esp_modem_dte_is_connected(iot_eth_driver_t *dte_drv);

#ifdef __cplusplus
}
#endif
