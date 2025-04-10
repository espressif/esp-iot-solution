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

typedef struct iot_eth_driver_s iot_eth_driver_t;
typedef struct iot_eth_mediator_s iot_eth_mediator_t;

struct iot_eth_driver_s {
    const char *name;
    esp_err_t (*set_mediator)(iot_eth_driver_t *driver, iot_eth_mediator_t *eth);
    esp_err_t (*init)(iot_eth_driver_t *driver);
    esp_err_t (*deinit)(iot_eth_driver_t *driver);
    esp_err_t (*start)(iot_eth_driver_t *driver);
    esp_err_t (*stop)(iot_eth_driver_t *driver);
    esp_err_t (*transmit)(iot_eth_driver_t *driver, uint8_t *data, size_t len);
    /**
     * @brief Get MAC address
     *
     * @param driver
     * @param mac_address
     * @return esp_err_t
     */
    esp_err_t (*get_addr)(iot_eth_driver_t *driver, uint8_t *mac_address);
};

typedef enum {
    IOT_ETH_STAGE_LLINIT,
    IOT_ETH_STAGE_DEINIT,
    IOT_ETH_STAGE_GET_MAC,
    IOT_ETH_STAGE_LINK,
    IOT_ETH_STATE_PAUSE,
} iot_eth_stage_t;

struct iot_eth_mediator_s {
    esp_err_t (*stack_input)(iot_eth_mediator_t *mediator, uint8_t *data, size_t len);
    // TODO: args 可能不需要，让 driver 层传递给 eth 层
    esp_err_t (*on_stage_changed)(iot_eth_mediator_t *mediator, iot_eth_stage_t stage, void *arg);
};

#ifdef __cplusplus
}
#endif
