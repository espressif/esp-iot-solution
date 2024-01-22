/*
 * SPDX-FileCopyrightText: 2019-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

/* 16 Bit TX Power Service UUID */
#define BLE_TPS_UUID16                                      0x1804

/* 16 Bit TX Power Service Characteristic UUIDs */
#define BLE_TPS_CHR_UUID16_TX_POWER_LEVEL                   0x2A07

/**
 * @brief Get the TX Power Level of the device.
 *
 * @return: The TX Power Level of the device
 */
int8_t esp_ble_tps_get_tx_power_level(void);

/**
 * @brief Set the TX Power Level of the device.
 *
 * @param[in]  tx_power_level The TX Power Level of the device
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong TX Power Level
 */
esp_err_t esp_ble_tps_set_tx_power_level(int8_t tx_power_level);

/**
 * @brief Initialization TX Power Service
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 *  - ESP_FAIL on error
 */
esp_err_t esp_ble_tps_init(void);

#ifdef __cplusplus
}
#endif
