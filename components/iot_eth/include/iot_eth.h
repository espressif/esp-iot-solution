/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "esp_event.h"
#include "iot_eth_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
* @brief iot_eth event declarations
*
*/
typedef enum {
    IOT_ETH_EVENT_START,        /*!< IOT_ETH driver start */
    IOT_ETH_EVENT_STOP,         /*!< IOT_ETH driver stop */
    IOT_ETH_EVENT_CONNECTED,    /*!< IOT_ETH got a valid link */
    IOT_ETH_EVENT_DISCONNECTED, /*!< IOT_ETH lost a valid link */
} iot_eth_event_t;

/**
* @brief iot_eth event base declaration
*
*/
/** @cond **/
ESP_EVENT_DECLARE_BASE(IOT_ETH_EVENT);
/** @endcond **/

/**
 * @brief Ethernet handle type
 *
 * This is a handle to an Ethernet instance, used for managing Ethernet operations.
 */
typedef void *iot_eth_handle_t;

/**
 * @brief Static input callback function type
 *
 * This type defines a function pointer for a static input callback.
 * It takes an Ethernet handle, a pointer to data, the length of the data,
 * and a user data pointer as arguments.
 */
typedef esp_err_t (*static_input_cb_t)(iot_eth_handle_t handle, uint8_t *data, size_t len, void *user_data);

/**
 * @brief Ethernet configuration structure
 *
 * This structure holds the configuration for initializing an Ethernet instance.
 */
typedef struct {
    iot_eth_driver_t *driver; /*!< Pointer to the Ethernet driver */
    static_input_cb_t stack_input; /*!< Function pointer for stack input */
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
esp_err_t iot_eth_install(const iot_eth_config_t *config, iot_eth_handle_t *handle);

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
esp_err_t iot_eth_transmit(iot_eth_handle_t handle, void *data, size_t len);

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

/*
 * @brief Update the input path for Ethernet data
 *
 * This function updates the stack input callback function and user data pointer
 * that will be used when receiving Ethernet packets.
 *
 * @param[in] handle Ethernet handle
 * @param[in] stack_input Function pointer for stack input callback
 * @param[in] user_data User data to be passed to stack input callback
 *
 * @return
 *      - ESP_OK: Successfully updated input path
 *      - ESP_ERR_INVALID_ARG: Invalid handle argument
 */
esp_err_t iot_eth_update_input_path(iot_eth_handle_t handle, static_input_cb_t stack_input, void *user_data);

#ifdef __cplusplus
}
#endif
