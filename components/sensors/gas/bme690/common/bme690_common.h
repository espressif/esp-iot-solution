/**
 * Copyright (C) 2025 Bosch Sensortec GmbH.
 * Copyright (C) 2025-2026 Espressif Systems (Shanghai) CO LTD.
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "bme69x.h"
#include "i2c_bus.h"
#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  @brief Function to select the interface between SPI and I2C.
 *
 *  @param[in] bme      : Structure instance of bme69x_dev
 *  @param[in] intf     : Interface selection parameter
 *
 *  @return Status of execution
 *  @retval 0 -> Success
 *  @retval < 0 -> Failure Info
 */
int8_t bme69x_interface_init(struct bme69x_dev *bme, uint8_t intf);

/**
 *  @brief Function for reading the sensor's registers through I2C bus.
 *
 *  @param[in] reg_addr     : Register address.
 *  @param[out] reg_data    : Pointer to the data buffer to store the read data.
 *  @param[in] len          : No of bytes to read.
 *  @param[in] intf_ptr     : Interface pointer
 *
 *  @return Status of execution
 *  @retval = BME69X_INTF_RET_SUCCESS -> Success
 *  @retval != BME69X_INTF_RET_SUCCESS  -> Failure Info
 *
 */
BME69X_INTF_RET_TYPE bme69x_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr);

/**
 *  @brief Function for writing the sensor's registers through I2C bus.
 *
 *  @param[in] reg_addr     : Register address.
 *  @param[in] reg_data     : Pointer to the data buffer whose value is to be written.
 *  @param[in] len          : No of bytes to write.
 *  @param[in] intf_ptr     : Interface pointer
 *
 *  @return Status of execution
 *  @retval = BME69X_INTF_RET_SUCCESS -> Success
 *  @retval != BME69X_INTF_RET_SUCCESS  -> Failure Info
 *
 */
BME69X_INTF_RET_TYPE bme69x_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr);

/**
 *  @brief Function for reading the sensor's registers through SPI bus.
 *
 *  @param[in] reg_addr     : Register address.
 *  @param[out] reg_data    : Pointer to the data buffer to store the read data.
 *  @param[in] len          : No of bytes to read.
 *  @param[in] intf_ptr     : Interface pointer
 *
 *  @return Status of execution
 *  @retval = BME69X_INTF_RET_SUCCESS -> Success
 *  @retval != BME69X_INTF_RET_SUCCESS  -> Failure Info
 *
 */
BME69X_INTF_RET_TYPE bme69x_spi_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr);

/**
 *  @brief Function for writing the sensor's registers through SPI bus.
 *
 *  @param[in] reg_addr     : Register address.
 *  @param[in] reg_data     : Pointer to the data buffer whose data has to be written.
 *  @param[in] len          : No of bytes to write.
 *  @param[in] intf_ptr     : Interface pointer
 *
 *  @return Status of execution
 *  @retval = BME69X_INTF_RET_SUCCESS -> Success
 *  @retval != BME69X_INTF_RET_SUCCESS  -> Failure Info
 *
 */
BME69X_INTF_RET_TYPE bme69x_spi_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr);

/**
 * @brief This function provides the delay for required time (Microsecond) as per the input provided in some of the
 * APIs.
 *
 *  @param[in] period       : The required wait time in microsecond.
 *  @param[in] intf_ptr     : Interface pointer
 *
 *  @return void.
 *
 */
void bme69x_delay_us(uint32_t period, void *intf_ptr);

/**
 *  @brief Prints the execution status of the APIs.
 *
 *  @param[in] api_name : Name of the API whose execution status has to be printed.
 *  @param[in] rslt     : Error code returned by the API whose execution status has to be printed.
 *
 *  @return void.
 */
void bme69x_check_rslt(const char api_name[], int8_t rslt);

/**
 * @brief Deinitializes the ESP peripheral driver.
 *
 * - For I2C: removes the device from the bus and clears the bus handle,
 *   without deinitializing the I2C bus.
 * - For SPI: clears the device handle, without deinitializing the SPI bus.
 *
 * @return void
 */
void bme69x_interface_deinit(void);

/**
 *  @brief Set I2C bus handle for ESP32 platform
 *
 *  @param[in] bus_handle : I2C bus handle
 *
 *  @return void.
 */
void bme69x_set_i2c_bus_handle(i2c_bus_handle_t bus_handle);

/**
 *  @brief Set SPI device handle for ESP32 platform
 *
 *  @param[in] device_handle : SPI device handle
 *
 *  @return void.
 */
void bme69x_set_spi_device_handle(spi_device_handle_t device_handle);

#ifdef __cplusplus
}
#endif /*__cplusplus */
