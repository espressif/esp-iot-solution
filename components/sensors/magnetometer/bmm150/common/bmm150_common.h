/**
 * Copyright (C) 2023 Bosch Sensortec GmbH.
 * Copyright (C) 2025-2026 Espressif Systems (Shanghai) CO LTD.
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "bmm150.h"
#include "i2c_bus.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  @brief Function for reading the sensor's registers through I2C bus.
 *
 *  @param[in] reg_addr     : Register address.
 *  @param[out] reg_data    : Pointer to the data buffer to store the read data.
 *  @param[in] length       : No of bytes to read.
 *  @param[in] intf_ptr     : Interface pointer
 *
 *  @return Status of execution
 *  @retval = BMM150_INTF_RET_SUCCESS -> Success
 *  @retval != BMM150_INTF_RET_SUCCESS  -> Failure Info
 */
BMM150_INTF_RET_TYPE bmm150_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr);

/**
 *  @brief Function for writing the sensor's registers through I2C bus.
 *
 *  @param[in] reg_addr     : Register address.
 *  @param[in] reg_data     : Pointer to the data buffer whose value is to be written.
 *  @param[in] length       : No of bytes to write.
 *  @param[in] intf_ptr     : Interface pointer
 *
 *  @return Status of execution
 *  @retval = BMM150_INTF_RET_SUCCESS -> Success
 *  @retval != BMM150_INTF_RET_SUCCESS  -> Failure Info
 */
BMM150_INTF_RET_TYPE bmm150_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr);

/**
 * @brief This function provides the delay for required time (Microsecond) as per the input provided in some of the
 * APIs.
 *
 *  @param[in] period_us    : The required wait time in microsecond.
 *  @param[in] intf_ptr     : Interface pointer
 *
 *  @return void.
 */
void bmm150_delay(uint32_t period_us, void *intf_ptr);

/**
 * @brief Bind a BMM150 device to a native I2C master bus.
 *
 * The caller owns the bus. Release the device with
 * bmm150_interface_deinit_device().
 *
 * @param[out] dev BMM150 device.
 * @param[in] bus Native I2C master bus.
 * @param[in] i2c_addr 7-bit I2C address.
 * @param[in] scl_speed_hz Device clock in Hz.
 *
 * @return ESP_OK on success, otherwise an ESP error code.
 */
esp_err_t bmm150_interface_init_from_master_bus(struct bmm150_dev *dev, i2c_master_bus_handle_t bus,
                                                uint8_t i2c_addr, uint32_t scl_speed_hz);

/**
 * @brief Bind a BMM150 device to an `i2c_bus` handle.
 *
 * The device uses the bus clock. Release it with
 * bmm150_interface_deinit_device().
 *
 * @param[out] dev BMM150 device.
 * @param[in] bus I2C bus.
 * @param[in] i2c_addr 7-bit I2C address.
 *
 * @return ESP_OK on success, otherwise an ESP error code.
 */
esp_err_t bmm150_interface_init_from_i2c_bus(struct bmm150_dev *dev, i2c_bus_handle_t bus, uint8_t i2c_addr);

/**
 * @brief Release the I2C device of one sensor. The bus is not deleted.
 *
 * On a removal error the context is kept and transfers stay blocked, so the
 * call can be retried. `dev` is only cleared once removal succeeds.
 *
 * @param[in,out] dev Structure instance of bmm150_dev.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG when `dev` has no context,
 *         or the device-removal error.
 */
esp_err_t bmm150_interface_deinit_device(struct bmm150_dev *dev);

/**
 *  @brief Function to select the I2C interface and bind ESP platform callbacks.
 *
 *  Uses the bus and address of the file-level setters below, so it drives a
 *  single sensor. A later call replaces that device, including after `dev` is
 *  zeroed to probe another address. `dev` must stay valid until
 *  bmm150_interface_deinit() or that later call. For several sensors, use
 *  bmm150_interface_init_from_master_bus() or
 *  bmm150_interface_init_from_i2c_bus() instead.
 *
 *  @param[in] dev : Structure instance of bmm150_dev
 *
 *  @return Status of execution
 *  @retval 0 -> Success
 *  @retval < 0 -> Failure Info
 */
int8_t bmm150_interface_init(struct bmm150_dev *dev);

/**
 *  @brief Prints the execution status of the APIs.
 *
 *  @param[in] api_name : Name of the API whose execution status has to be printed.
 *  @param[in] rslt     : Error code returned by the API whose execution status has to be printed.
 *
 *  @return void.
 */
void bmm150_error_codes_print_result(const char api_name[], int8_t rslt);

/**
 * @brief Deinitializes the ESP peripheral driver.
 *
 * - For I2C: removes the device created by bmm150_interface_init() and clears
 *   the bus handle, without deinitializing the I2C bus. On a removal error the
 *   context is kept and a later init or deinit retries it.
 *
 * Sensors created through the instance APIs are not affected.
 *
 * @return void
 */
void bmm150_interface_deinit(void);

/**
 *  @brief Set I2C bus handle for bmm150_interface_init()
 *
 *  Takes effect on the next bmm150_interface_init().
 *
 *  @param[in] bus_handle : I2C bus handle
 *
 *  @return void.
 */
void bmm150_set_i2c_bus_handle(i2c_bus_handle_t bus_handle);

/**
 *  @brief Set I2C device address (0x10 / 0x11 / 0x12 / 0x13)
 *
 *  Must be called before bmm150_interface_init(). If the address changes after
 *  init, call bmm150_interface_init() again to recreate the I2C device.
 *
 *  @param[in] i2c_addr : 7-bit I2C address
 *
 *  @return void.
 */
void bmm150_set_i2c_address(uint8_t i2c_addr);

#ifdef __cplusplus
}
#endif /*__cplusplus */
