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
    esp_err_t (*set_mediator)(iot_eth_driver_t *driver, iot_eth_mediator_t *eth);    /*!< Set mediator for driver */
    esp_err_t (*init)(iot_eth_driver_t *driver);                                     /*!< Initialize driver */
    esp_err_t (*deinit)(iot_eth_driver_t *driver);                                   /*!< Deinitialize driver */
    esp_err_t (*start)(iot_eth_driver_t *driver);                                    /*!< Start driver */
    esp_err_t (*stop)(iot_eth_driver_t *driver);                                     /*!< Stop driver */
    esp_err_t (*transmit)(iot_eth_driver_t *driver, uint8_t *data, size_t len);      /*!< Transmit TCP/IP packet */
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
    IOT_ETH_STAGE_LLINIT,    /*!< Low level init done */
    IOT_ETH_STAGE_DEINIT,    /*!< Deinit stage */
    IOT_ETH_STAGE_GET_MAC,   /*!< Get MAC address stage */
    IOT_ETH_STAGE_LINK,      /*!< Link status changed */
    IOT_ETH_STATE_PAUSE,     /*!< Pause state */
} iot_eth_stage_t;

/**
 * @brief Ethernet mediator structure
 */
struct iot_eth_mediator_s {
    esp_err_t (*stack_input)(iot_eth_mediator_t *mediator, uint8_t *data, size_t len);   /*!< Input function for stack */
    esp_err_t (*on_stage_changed)(iot_eth_mediator_t *mediator, iot_eth_stage_t stage, void *arg);  /*!< Stage change callback */
};

#ifdef __cplusplus
}
#endif
