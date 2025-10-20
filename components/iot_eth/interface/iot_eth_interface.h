/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Forward declarations for driver and mediator structures
 */
typedef struct iot_eth_driver_s iot_eth_driver_t;
typedef struct iot_eth_mediator_s iot_eth_mediator_t;

/**
 * @brief Ethernet driver structure
 */
struct iot_eth_driver_s {
    const char *name;                                                                /*!< Driver name */

    /**
     * @brief [Required] Set mediator for driver, driver can report event to iot_eth layer by this function.
     *
     * @param driver Ethernet driver handle
     * @param eth Ethernet mediator handle
     * @return esp_err_t
     *      - ESP_OK: Set mediator successfully
     *      - ESP_ERR_INVALID_ARG: Set mediator failed because of invalid argument
     */
    esp_err_t (*set_mediator)(iot_eth_driver_t *driver, iot_eth_mediator_t *eth);

    /**
     * @brief [Required] Initialize driver
     *
     * @param driver Ethernet driver handle
     * @return esp_err_t
     *      - ESP_OK: Initialize driver successfully
     */
    esp_err_t (*init)(iot_eth_driver_t *driver);

    /**
     * @brief [Required] Deinitialize driver
     *
     * @param driver Ethernet driver handle
     * @return esp_err_t
     *      - ESP_OK: Deinitialize driver successfully
     */
    esp_err_t (*deinit)(iot_eth_driver_t *driver);

    /**
     * @brief [Required] Start driver
     *
     * @param driver Ethernet driver handle
     * @return esp_err_t
     *      - ESP_OK: Start driver successfully
     */
    esp_err_t (*start)(iot_eth_driver_t *driver);

    /**
     * @brief [Required] Stop driver
     *
     * @param driver Ethernet driver handle
     * @return esp_err_t
     *      - ESP_OK: Stop driver successfully
     */
    esp_err_t (*stop)(iot_eth_driver_t *driver);

    /**
     * @brief [Required] Transmit data to lower layer
     *
     * @param driver Ethernet driver handle
     * @param data Data to transmit
     * @param len Data length
     * @return esp_err_t
     *      - ESP_OK: Transmit successfully
     */
    esp_err_t (*transmit)(iot_eth_driver_t *driver, uint8_t *data, size_t len);

    /**
     * @brief Get eth device MAC address
     *
     * @param driver Ethernet driver handle
     * @param mac_address Buffer to store MAC address
     * @return esp_err_t
     *      - ESP_OK: Get MAC address successfully
     *      - ESP_ERR_INVALID_ARG: Get MAC address failed because of invalid argument
     *      - ESP_FAIL: Get MAC address failed because some other error occurred
     */
    esp_err_t (*get_addr)(iot_eth_driver_t *driver, uint8_t *mac_address);
};

/**
 * @brief Ethernet driver stage
 */
typedef enum {
    IOT_ETH_STAGE_LL_INIT,      /*!< Low level init done */
    IOT_ETH_STAGE_LL_DEINIT,    /*!< Low level deinit done */
    IOT_ETH_STAGE_LINK,         /*!< Link status changed */
} iot_eth_stage_t;

/**
 * @brief Ethernet mediator structure
 */
struct iot_eth_mediator_s {
    esp_err_t (*stack_input)(iot_eth_mediator_t *mediator, uint8_t *data, size_t len);  /*!< Input function for stack */
    esp_err_t (*on_stage_changed)(iot_eth_mediator_t *mediator, iot_eth_stage_t stage, void *arg);  /*!< Stage change callback */
};

#ifdef __cplusplus
}
#endif
