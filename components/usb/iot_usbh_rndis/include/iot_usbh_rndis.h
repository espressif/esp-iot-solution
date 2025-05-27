/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "iot_eth_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief USB Host Ethernet ECM Configuration
 */
typedef struct {
    bool auto_detect;                         /*!< Auto detect RNDIS device */
    TickType_t auto_detect_timeout;           /*!< Auto detect timeout in ticks, used when auto_detect is true */
    uint16_t vid;                             /*!< USB device vendor ID, used when auto_detect is false */
    uint16_t pid;                             /*!< USB device product ID, used when auto_detect is false */
    int itf_num;                              /*!< interface numbers, used when auto_detect is false */
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

#ifdef __cplusplus
}
#endif
