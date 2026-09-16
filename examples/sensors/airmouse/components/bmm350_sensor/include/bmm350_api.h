/**
 * Copyright (c) 2024 Bosch Sensortec GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * @file  bmm350_api.h
 * @brief ESP-IDF I2C platform glue for the Bosch BMM350 SensorAPI.
 *
 * Adapted from the BMM350_SensorAPI "examples/common" HAL and from
 * Espressif's esp-dev-kits factory_demo Compass app, to run on top of
 * this repository's `i2c_bus` component instead of the COINES platform.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "bmm350.h"
#include "i2c_bus.h"

/**
 * @brief Bind a bmm350_dev instance to an I2C bus.
 *
 * Wires dev->read / dev->write / dev->delay_us to the I2C glue below and
 * points dev->intf_ptr at dev_addr_ref, so the caller controls which I2C
 * address (e.g. BMM350_I2C_ADSEL_SET_LOW / _HIGH) is used and can change it
 * between address probe attempts without re-running this function.
 *
 * @param[out] dev          bmm350 device instance to configure
 * @param[in]  i2c_bus      Shared I2C bus handle (not created or owned here)
 * @param[in]  dev_addr_ref Pointer to the I2C address byte to use for this
 *                           device; must outlive dev
 *
 * @return BMM350_OK on success, BMM350_E_NULL_PTR if dev or i2c_bus is NULL
 */
int8_t bmm350_interface_init(struct bmm350_dev *dev, i2c_bus_handle_t i2c_bus, uint8_t *dev_addr_ref);

/**
 * @brief Release the I2C device handle created for the current address.
 */
void bmm350_interface_deinit(void);

/*! I2C read function mapped onto i2c_bus_read_bytes */
BMM350_INTF_RET_TYPE bmm350_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr);

/*! I2C write function mapped onto i2c_bus_write_bytes */
BMM350_INTF_RET_TYPE bmm350_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr);

/*! Delay function mapped onto esp_rom_delay_us / vTaskDelay */
void bmm350_delay(uint32_t period_us, void *intf_ptr);

/*! Logs the meaning of a BMM350 SensorAPI result code, no-op on BMM350_OK */
void bmm350_error_codes_print_result(const char *api_name, int8_t rslt);

#ifdef __cplusplus
}
#endif
