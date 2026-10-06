/**
 * Copyright (C) 2023 Bosch Sensortec GmbH.
 * Copyright (C) 2025-2026 Espressif Systems (Shanghai) CO LTD.
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "bmm350.h"
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
 *  @retval = BMM350_INTF_RET_SUCCESS -> Success
 *  @retval != BMM350_INTF_RET_SUCCESS  -> Failure Info
 */
BMM350_INTF_RET_TYPE bmm350_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr);

/**
 *  @brief Function for writing the sensor's registers through I2C bus.
 *
 *  @param[in] reg_addr     : Register address.
 *  @param[in] reg_data     : Pointer to the data buffer whose value is to be written.
 *  @param[in] length       : No of bytes to write.
 *  @param[in] intf_ptr     : Interface pointer
 *
 *  @return Status of execution
 *  @retval = BMM350_INTF_RET_SUCCESS -> Success
 *  @retval != BMM350_INTF_RET_SUCCESS  -> Failure Info
 */
BMM350_INTF_RET_TYPE bmm350_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr);

/**
 * @brief This function provides the delay for required time (Microsecond) as per the input provided in some of the
 * APIs.
 *
 *  @param[in] period_us    : The required wait time in microsecond.
 *  @param[in] intf_ptr     : Interface pointer
 *
 *  @return void.
 */
void bmm350_delay(uint32_t period_us, void *intf_ptr);

/**
 *  @brief Function to select the I2C interface and bind ESP platform callbacks.
 *
 *  @param[in] dev : Structure instance of bmm350_dev
 *
 *  @return Status of execution
 *  @retval 0 -> Success
 *  @retval < 0 -> Failure Info
 */
int8_t bmm350_interface_init(struct bmm350_dev *dev);

/**
 *  @brief Prints the execution status of the APIs.
 *
 *  @param[in] api_name : Name of the API whose execution status has to be printed.
 *  @param[in] rslt     : Error code returned by the API whose execution status has to be printed.
 *
 *  @return void.
 */
void bmm350_error_codes_print_result(const char api_name[], int8_t rslt);

/**
 * @brief Release the I2C device. The bus is not deleted.
 *
 * @return void
 */
void bmm350_interface_deinit(void);

/**
 * @brief Select an `i2c_bus` handle. NULL clears the selection.
 *
 * @param[in] bus_handle : I2C bus handle
 *
 * @return void.
 */
void bmm350_set_i2c_bus_handle(i2c_bus_handle_t bus_handle);

/**
 * @brief Select a native I2C master bus. NULL clears the selection.
 *
 * Adds a 100 kHz device with a 200 ms timeout after `bmm350_interface_init()`.
 *
 * @param[in] bus_handle Native bus handle, or NULL.
 * @return ESP_OK, a device-removal error, or ESP_ERR_NOT_SUPPORTED when
 *         CONFIG_I2C_BUS_BACKWARD_CONFIG is enabled.
 */
esp_err_t bmm350_set_i2c_master_bus_handle(i2c_master_bus_handle_t bus_handle);

/**
 *  @brief Set I2C device address (0x14 or 0x15)
 *
 *  Must be called before bmm350_interface_init(). If the address changes after
 *  init, call bmm350_interface_init() again to recreate the I2C device.
 *
 *  @param[in] i2c_addr : 7-bit I2C address
 *
 *  @return void.
 */
void bmm350_set_i2c_address(uint8_t i2c_addr);

#ifdef __cplusplus
}
#endif /*__cplusplus */
