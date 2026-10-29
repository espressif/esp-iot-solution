/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "iot_eth_interface.h"
#include "iot_usbh_cdc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief USB Host Ethernet RNDIS Configuration
 */
typedef struct {
    const usb_device_match_id_t *match_id_list; /*!< USB device match ID for RNDIS */
} iot_usbh_rndis_config_t;

/**
 * @brief Create a new USB RNDIS Ethernet driver.
 *
 * This function initializes a new USB RNDIS Ethernet driver with the specified configuration.
 * It allocates memory for the driver, sets up the driver functions, and returns a handle to the driver.
 *
 * @param config Pointer to the RNDIS configuration structure.
 * @param ret_handle Pointer to a location where the handle to the new Ethernet driver will be stored.
 *
 * @return
 *     - ESP_OK: Driver created successfully
 *     - ESP_ERR_INVALID_ARG: Invalid argument (NULL config or ret_handle)
 *     - ESP_ERR_NO_MEM: Memory allocation failed
 */
esp_err_t iot_eth_new_usb_rndis(const iot_usbh_rndis_config_t *config, iot_eth_driver_t **ret_handle);

/**
 * @brief Get the CDC port handle of the RNDIS driver.
 *
 * @param[in] rndis_drv RNDIS Ethernet driver handle
 *
 * @return usbh_cdc_port_handle_t CDC port handle of the RNDIS driver
 */
usbh_cdc_port_handle_t usb_rndis_get_cdc_port_handle(const iot_eth_driver_t *rndis_drv);

#ifdef __cplusplus
}
#endif
