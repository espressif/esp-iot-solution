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

typedef void *iot_eth_handle_t;

typedef struct {
    iot_eth_driver_t *driver;
    esp_err_t (*on_lowlevel_init_done)(iot_eth_handle_t handle);
    esp_err_t (*on_lowlevel_deinit)(iot_eth_handle_t handle);
    esp_err_t (*stack_input)(iot_eth_handle_t handle, uint8_t *data, size_t len, void *user_data);
    void *user_data;
} iot_eth_config_t;

esp_err_t iot_eth_install(iot_eth_config_t *config, iot_eth_handle_t *handle);

esp_err_t iot_eth_uninstall(iot_eth_handle_t handle);

esp_err_t iot_eth_start(iot_eth_handle_t handle);

esp_err_t iot_eth_stop(iot_eth_handle_t handle);

esp_err_t iot_eth_transmit(iot_eth_handle_t handle, uint8_t *data, size_t len);

esp_err_t iot_eth_get_addr(iot_eth_handle_t handle, uint8_t *mac);

#ifdef __cplusplus
}
#endif
