/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include  "spi_bus.h"

typedef void *mcp3201_handle_t;

/**
 * @brief Create and init sensor object and return a sensor handle
 *
 * @param spi_bus SPI bus handle
 * @param dev_cfg SPI device configuration
 *
 * @return
 *     - sht3x handle_t
 */
mcp3201_handle_t mcp3201_create(spi_bus_handle_t spi_bus, spi_device_config_t *dev_cfg);

/**
 * @brief Delete and release a sensor object
 *
 * @param sensor Point to object handle of mcp3201
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t mcp3201_delete(mcp3201_handle_t *sensor);

/**
 * @brief Get device data of MCP3201
 *
 * @param sensor Point to object handle of mcp3201
 * @param data adc data
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t mcp3201_get_data(mcp3201_handle_t sensor, int16_t *data);

#ifdef __cplusplus
}
#endif
