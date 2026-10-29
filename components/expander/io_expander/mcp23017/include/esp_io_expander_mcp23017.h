/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ESP IO expander: MCP23017
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "esp_io_expander.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create an MCP23017 IO expander object
 *
 * @note The MCP23017 only has internal pull-up resistors. Calling
 *       `esp_io_expander_set_pullupdown()` with `IO_EXPANDER_PULL_DOWN`
 *       returns `ESP_ERR_NOT_SUPPORTED`.
 *
 * @param[in]  i2c_bus    I2C bus handle. Obtained from `i2c_new_master_bus()`
 * @param[in]  dev_addr   I2C device address of chip. Can be `ESP_IO_EXPANDER_I2C_MCP23017_ADDRESS_XXX`.
 * @param[out] handle_ret Handle to created IO expander object
 *
 * @return
 *      - ESP_OK: Success, otherwise returns ESP_ERR_xxx
 */
esp_err_t esp_io_expander_new_i2c_mcp23017(i2c_master_bus_handle_t i2c_bus, uint32_t dev_addr,
                                           esp_io_expander_handle_t *handle_ret);

/**
 * @brief I2C address of the MCP23017
 *
 * The 8-bit address format is as follows:
 *
 *                (Slave Address)
 *     +---+---+---+---+----+----+----+-----+
 *     | 0 | 1 | 0 | 0 | A2 | A1 | A0 | R/W |
 *     +---+---+---+---+----+----+----+-----+
 *
 * The 7-bit slave address is the most important data for users.
 * For example, if A0, A1 and A2 are connected to GND, the 7-bit slave address is 0x20.
 */
#define ESP_IO_EXPANDER_I2C_MCP23017_ADDRESS_000    (0x20)
#define ESP_IO_EXPANDER_I2C_MCP23017_ADDRESS_001    (0x21)
#define ESP_IO_EXPANDER_I2C_MCP23017_ADDRESS_010    (0x22)
#define ESP_IO_EXPANDER_I2C_MCP23017_ADDRESS_011    (0x23)
#define ESP_IO_EXPANDER_I2C_MCP23017_ADDRESS_100    (0x24)
#define ESP_IO_EXPANDER_I2C_MCP23017_ADDRESS_101    (0x25)
#define ESP_IO_EXPANDER_I2C_MCP23017_ADDRESS_110    (0x26)
#define ESP_IO_EXPANDER_I2C_MCP23017_ADDRESS_111    (0x27)

#ifdef __cplusplus
}
#endif
