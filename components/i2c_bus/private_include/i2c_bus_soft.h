/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "i2c_bus.h"

struct i2c_master_soft_bus_t {
    gpio_num_t scl_io;      /*!< SCL GPIO PIN */
    gpio_num_t sda_io;      /*!< SDA GPIO PIN */
    uint32_t time_delay_us; /*!< Interval between SCL GPIO toggles in microseconds, determining the SCL frequency */
};

typedef struct i2c_master_soft_bus_t *i2c_master_soft_bus_handle_t;

/**
 * @brief Allocate an I2C master soft bus
 *
 * @param conf I2C master soft bus configuration
 * @param ret_soft_bus_handle I2C soft bus handle
 * @return
 *      - ESP_OK: I2C master soft bus initialized successfully
 *      - ESP_ERR_INVALID_ARG: I2C soft bus initialization failed because of invalid argument
 *      - ESP_ERR_NO_MEM: Create I2C soft bus failed because of out of memory
 */
esp_err_t i2c_new_master_soft_bus(const i2c_config_t *conf, i2c_master_soft_bus_handle_t *ret_soft_bus_handle);

/**
 * @brief Delete the I2C master soft bus
 *
 * @param bus_handle I2C soft bus handle
 * @return
 *      - ESP_OK: Delete I2C soft bus success
 *      - Otherwise: Some module delete failed
 */
esp_err_t i2c_del_master_soft_bus(i2c_master_soft_bus_handle_t bus_handle);

/**
 * @brief Change I2C soft bus frequency
 *
 * @param bus_handle I2C soft bus handle
 * @param frequency I2C bus frequency
 * @return
 *      - ESP_OK: Change I2C soft bus frequency success
 *      - ESP_ERR_INVALID_ARG: I2C soft bus change frequency failed because of invalid argument
 */
esp_err_t i2c_master_soft_bus_change_frequency(i2c_master_soft_bus_handle_t bus_handle, uint32_t frequency);

/**
 * @brief Probe I2C address, if address is correct and ACK is received, this function will return ESP_OK
 *
 * @param bus_handle I2C soft bus handle
 * @param address I2C device address that you want to probe
 * @return
 *      - ESP_OK: I2C device probe successfully
 *      - ESP_ERR_INVALID_ARG: I2C probe failed because of invalid argument
 *      - ESP_ERR_NOT_FOUND: I2C probe failed, doesn't find the device with specific address you gave
 */
esp_err_t i2c_master_soft_bus_probe(i2c_master_soft_bus_handle_t bus_handle, uint8_t address);

/**
 * @brief Write multiple byte to i2c device with 8-bit internal register/memory address
 *
 * @param bus_handle I2C soft bus handle
 * @param dev_addr I2C device address
 * @param mem_address The internal reg/mem address to write to, set to NULL_I2C_MEM_ADDR if no internal address
 * @param data_len Number of bytes to write
 * @param data Pointer to the bytes to write
 * @return
 *      - ESP_OK: I2C master write success
 *      - ESP_ERR_INVALID_ARG: I2C master write failed because of invalid argument
 *      - Otherwise: I2C master write failed
 */
esp_err_t i2c_master_soft_bus_write_reg8(i2c_master_soft_bus_handle_t bus_handle, uint8_t dev_addr, uint8_t mem_address, size_t data_len, const uint8_t *data);

/**
 * @brief Write multiple byte to i2c device with 16-bit internal register/memory address
 *
 * @param bus_handle I2C soft bus handle
 * @param dev_addr I2C device address
 * @param mem_address The internal reg/mem address to write to, set to NULL_I2C_MEM_16BIT_ADDR if no internal address
 * @param data_len Number of bytes to write
 * @param data Pointer to the bytes to write
 * @return
 *      - ESP_OK: I2C master write success
 *      - ESP_ERR_INVALID_ARG: I2C master write failed because of invalid argument
 *      - Otherwise: I2C master write failed
 */
esp_err_t i2c_master_soft_bus_write_reg16(i2c_master_soft_bus_handle_t bus_handle, uint8_t dev_addr, uint16_t mem_address, size_t data_len, const uint8_t *data);

/**
 * @brief Read multiple byte to i2c device with 8-bit internal register/memory address
 *
 * @param bus_handle I2C soft bus handle
 * @param dev_addr I2C device address
 * @param mem_address The internal reg/mem address to write to, set to NULL_I2C_MEM_ADDR if no internal address
 * @param data_len Number of bytes to read
 * @param data Pointer to the bytes to read
 * @return
 *      - ESP_OK: I2C master read success
 *      - ESP_ERR_INVALID_ARG: I2C master read failed because of invalid argument
 *      - Otherwise: I2C master read failed
 */
esp_err_t i2c_master_soft_bus_read_reg8(i2c_master_soft_bus_handle_t bus_handle, uint8_t dev_addr, uint8_t mem_address, size_t data_len, uint8_t *data);

/**
 * @brief Read multiple byte to i2c device with 16-bit internal register/memory address
 *
 * @param bus_handle I2C soft bus handle
 * @param dev_addr I2C device address
 * @param mem_address The internal reg/mem address to write to, set to NULL_I2C_MEM_16BIT_ADDR if no internal address
 * @param data_len Number of bytes to read
 * @param data Pointer to the bytes to read
 * @return
 *      - ESP_OK: I2C master read success
 *      - ESP_ERR_INVALID_ARG: I2C master read failed because of invalid argument
 *      - Otherwise: I2C master read failed
 */
esp_err_t i2c_master_soft_bus_read_reg16(i2c_master_soft_bus_handle_t bus_handle, uint8_t dev_addr, uint16_t mem_address, size_t data_len, uint8_t *data);
