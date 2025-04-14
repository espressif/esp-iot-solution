/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
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
 * @brief Ethernet handle type
 *
 * This is a handle to an Ethernet instance, used for managing Ethernet operations.
 */
typedef void *iot_eth_handle_t;

/**
 * @brief Ethernet configuration structure
 *
 * This structure holds the configuration for initializing an Ethernet instance.
 */
typedef struct {
    iot_eth_driver_t *driver; /*!< Pointer to the Ethernet driver */
    esp_err_t (*on_lowlevel_init_done)(iot_eth_handle_t handle); /*!< Callback for low-level initialization completion */
    esp_err_t (*on_lowlevel_deinit)(iot_eth_handle_t handle); /*!< Callback for low-level deinitialization */
    esp_err_t (*stack_input)(iot_eth_handle_t handle, uint8_t *data, size_t len, void *user_data); /*!< Function pointer for stack input */
    void *user_data; /*!< User data for callbacks */
} iot_eth_config_t;

/*
 * @brief Install Ethernet driver
 *
 * This function initializes the Ethernet driver and network interface.
 *
 * @param config Ethernet configuration
 * @param handle Pointer to the Ethernet handle
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if arguments are invalid
 *      - ESP_ERR_NO_MEM if memory allocation fails
 *      - Other error codes from initialization functions
 */
esp_err_t iot_eth_install(iot_eth_config_t *config, iot_eth_handle_t *handle);

/*
 * @brief Uninstall Ethernet driver
 *
 * This function deinitializes the Ethernet driver and frees resources.
 *
 * @param handle Ethernet handle
 *
 * @return
 *      - ESP_OK on success
 *      - Error code from driver deinitialization
 */
esp_err_t iot_eth_uninstall(iot_eth_handle_t handle);

/*
 * @brief Start Ethernet driver
 *
 * This function starts the Ethernet driver.
 *
 * @param handle Ethernet handle
 *
 * @return
 *      - ESP_OK on success
 *      - Error code from driver start function
 */
esp_err_t iot_eth_start(iot_eth_handle_t handle);

/*
 * @brief Stop Ethernet driver
 *
 * This function stops the Ethernet driver.
 *
 * @param handle Ethernet handle
 *
 * @return
 *      - ESP_OK on success
 *      - Error code from driver stop function
 */
esp_err_t iot_eth_stop(iot_eth_handle_t handle);

/*
 * @brief Transmit data over Ethernet
 *
 * This function sends data through the Ethernet driver.
 *
 * @param handle Ethernet handle
 * @param data Pointer to the data to be sent
 * @param len Length of the data to be sent
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if arguments are invalid
 *      - ESP_ERR_INVALID_STATE if Ethernet link is down
 */
esp_err_t iot_eth_transmit(iot_eth_handle_t handle, uint8_t *data, size_t len);

/*
 * @brief Get Ethernet MAC address
 *
 * This function retrieves the MAC address of the Ethernet interface.
 *
 * @param handle Ethernet handle
 * @param mac Pointer to the MAC address buffer
 *
 * @return
 *      - ESP_OK on success
 *      - Error code from driver get address function
 */
esp_err_t iot_eth_get_addr(iot_eth_handle_t handle, uint8_t *mac);

#ifdef __cplusplus
}
#endif
