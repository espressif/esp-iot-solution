/**
 * Copyright (C) 2025 Bosch Sensortec GmbH.
 * Copyright (C) 2025-2026 Espressif Systems (Shanghai) CO LTD.
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "i2c_bus.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "bme69x.h"
#include "bme69x_defs.h"

/******************************************************************************/
/*!                 Macro definitions                                         */

/* For Debug */
#ifndef CONFIG_LOG_MAXIMUM_LEVEL
#define CONFIG_LOG_MAXIMUM_LEVEL ESP_LOG_INFO
#define CONFIG_BME690_I2C_TIMEOUT_VALUE_MS 50
#define CONFIG_BME690_AMBIENT_TEMP 25
#endif

/*! TAG for ESP_LOG */
#define TAG "BME69X"

/******************************************************************************/
/*!                Static variable definition                                 */

/*! I2C device address (default SDO low = 0x76) or SPI chip selection */
static uint8_t dev_addr = BME69X_I2C_ADDR_LOW;
static i2c_bus_handle_t i2c_bus = NULL;
static i2c_bus_device_handle_t i2c_dev = NULL;
static spi_device_handle_t spi_dev = NULL;

/******************************************************************************/
/*!                User interface functions                                   */

/*!
 * I2C read function map to ESP32 platform
 */
BME69X_INTF_RET_TYPE bme69x_i2c_read(uint8_t reg_addr, uint8_t *reg_data,
                                     uint32_t len, void *intf_ptr)
{
    esp_err_t ret;
    (void)intf_ptr;

    if (i2c_dev == NULL) {
        ESP_LOGE(TAG, "I2C bus not initialized");
        return BME69X_E_COM_FAIL;
    }

    ESP_LOGD(TAG, "I2C read reg 0x%02" PRIx8 " len %" PRIu32, reg_addr, len);
    // Read data from register address
    ret = i2c_bus_read_bytes(i2c_dev, reg_addr, len, reg_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C read data failed: %s", esp_err_to_name(ret));
        return BME69X_E_COM_FAIL;
    }
    return BME69X_INTF_RET_SUCCESS;
}

/*!
 * I2C write function map to ESP32 platform
 */
BME69X_INTF_RET_TYPE bme69x_i2c_write(uint8_t reg_addr, const uint8_t *reg_data,
                                      uint32_t len, void *intf_ptr)
{
    esp_err_t ret;
    (void)intf_ptr;

    if (i2c_dev == NULL) {
        ESP_LOGE(TAG, "I2C bus not initialized");
        return BME69X_E_COM_FAIL;
    }

    ESP_LOGD(TAG, "I2C write reg 0x%02" PRIx8 " len %" PRIu32, reg_addr, len);

    // Write data to device register
    ret = i2c_bus_write_bytes(i2c_dev, reg_addr, len, reg_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C write data failed: %s", esp_err_to_name(ret));
        return BME69X_E_COM_FAIL;
    }

    return BME69X_INTF_RET_SUCCESS;
}

/*!
 * SPI read function map to ESP32 platform
 */
BME69X_INTF_RET_TYPE bme69x_spi_read(uint8_t reg_addr, uint8_t *reg_data,
                                     uint32_t len, void *intf_ptr)
{
    esp_err_t ret;
    (void)intf_ptr;

    if (spi_dev == NULL) {
        ESP_LOGE(TAG, "SPI device not initialized");
        return BME69X_E_COM_FAIL;
    }

    if (len == 0 || reg_data == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return BME69X_E_NULL_PTR;
    }

    ESP_LOGD(TAG, "SPI read reg 0x%02" PRIx8 " len %" PRIu32, reg_addr, len);

    // For SPI read, set bit 7 of register address to 1
    uint8_t tx_addr = reg_addr | BME69X_SPI_RD_MSK;

    // SPI transaction: first send address byte, then read data
    spi_transaction_t trans = {
        .flags = 0,
        .cmd = 0,
        .addr = 0,
        .length = (1 + len) * 8,  // Total length: address + data (in bits)
        .rxlength = (1 + len) * 8,  // Receive same amount
        .tx_buffer = NULL,
        .rx_buffer = NULL
    };

    // Allocate buffers for full-duplex transaction
    uint8_t *tx_buf = malloc(1 + len);
    uint8_t *rx_buf = malloc(1 + len);
    if (tx_buf == NULL || rx_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate transaction buffers");
        free(tx_buf);
        free(rx_buf);
        return BME69X_E_COM_FAIL;
    }

    // Prepare TX buffer: address byte followed by dummy bytes
    tx_buf[0] = tx_addr;
    memset(&tx_buf[1], 0xFF, len);  // Dummy bytes for reading

    trans.tx_buffer = tx_buf;
    trans.rx_buffer = rx_buf;

    ret = spi_device_transmit(spi_dev, &trans);
    if (ret == ESP_OK) {
        // Copy received data (skip the first byte which is dummy)
        memcpy(reg_data, &rx_buf[1], len);
    }

    free(tx_buf);
    free(rx_buf);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI read failed: %s", esp_err_to_name(ret));
        return BME69X_E_COM_FAIL;
    }

    return BME69X_INTF_RET_SUCCESS;
}

/*!
 * SPI write function map to ESP32 platform
 */
BME69X_INTF_RET_TYPE bme69x_spi_write(uint8_t reg_addr, const uint8_t *reg_data,
                                      uint32_t len, void *intf_ptr)
{
    esp_err_t ret;
    (void)intf_ptr;

    if (spi_dev == NULL) {
        ESP_LOGE(TAG, "SPI device not initialized");
        return BME69X_E_COM_FAIL;
    }

    if (len == 0 || reg_data == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return BME69X_E_NULL_PTR;
    }

    if (len >= UINT32_MAX - 1) {
        ESP_LOGE(TAG, "Length is too long");
        return BME69X_E_INVALID_LENGTH;
    }

    ESP_LOGD(TAG, "SPI write reg 0x%02" PRIx8 " len %" PRIu32, reg_addr, len);

    // For SPI write, clear bit 7 of register address (write operation)
    uint8_t tx_addr = reg_addr & BME69X_SPI_WR_MSK;

    // Allocate buffer for address + data
    uint8_t *write_buf = malloc(len + 1);
    if (write_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate write buffer");
        return BME69X_E_COM_FAIL;
    }

    write_buf[0] = tx_addr;
    memcpy(&write_buf[1], reg_data, len);

    spi_transaction_t trans = {
        .flags = 0,
        .cmd = 0,
        .addr = 0,
        .length = (len + 1) * 8,  // Total length in bits (address + data)
        .rxlength = 0,
        .tx_buffer = write_buf,
        .rx_buffer = NULL
    };

    ret = spi_device_transmit(spi_dev, &trans);
    free(write_buf);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI write failed: %s", esp_err_to_name(ret));
        return BME69X_E_COM_FAIL;
    }

    return BME69X_INTF_RET_SUCCESS;
}

void bme69x_delay_us(uint32_t period, void *intf_ptr)
{
    (void)intf_ptr;

    if (period == 0) {
        return;
    }

    int64_t start_us = esp_timer_get_time();
    uint64_t full_ticks = ((uint64_t)period * configTICK_RATE_HZ) / 1000000ULL;

    /*
     * vTaskDelay() wakes on tick boundaries and can therefore block for up
     * to one tick less than expected. Measure the actual elapsed time below
     * and busy-wait only for the remaining microseconds.
     */
    if (full_ticks > 0) {
        vTaskDelay((TickType_t)full_ticks);
    }

    int64_t elapsed_us = esp_timer_get_time() - start_us;
    if (elapsed_us < period) {
        esp_rom_delay_us(period - (uint32_t)elapsed_us);
    }
}

int8_t bme69x_interface_init(struct bme69x_dev *bme, uint8_t intf)
{
    if (bme == NULL) {
        return BME69X_E_NULL_PTR;
    }

    if (intf == BME69X_I2C_INTF) {
        ESP_LOGI(TAG, "I2C Interface, dev_addr=0x%02" PRIx8, dev_addr);

        if (i2c_bus == NULL) {
            ESP_LOGE(TAG, "I2C bus handle is NULL, please call bme69x_set_i2c_bus_handle first");
            return BME69X_E_COM_FAIL;
        }

        bme->read = bme69x_i2c_read;
        bme->write = bme69x_i2c_write;
        bme->intf = BME69X_I2C_INTF;

        /* Allow re-init by replacing the previous device handle */
        if (i2c_dev != NULL) {
            i2c_bus_device_delete(&i2c_dev);
            i2c_dev = NULL;
        }

        /* clk_speed 0: inherit the bus clock, same as BMM150/BMM350 glue */
        i2c_dev = i2c_bus_device_create(i2c_bus, dev_addr, 0);
        if (i2c_dev == NULL) {
            ESP_LOGE(TAG, "i2c_bus_device_create failed");
            return BME69X_E_COM_FAIL;
        }
        ESP_LOGI(TAG, "I2C device created at address 0x%02" PRIx8, dev_addr);
    } else if (intf == BME69X_SPI_INTF) {
        ESP_LOGI(TAG, "SPI Interface");

        if (spi_dev == NULL) {
            ESP_LOGE(TAG, "SPI device handle is NULL, please call bme69x_set_spi_device_handle first");
            return BME69X_E_COM_FAIL;
        }

        dev_addr = 0;
        bme->read = bme69x_spi_read;
        bme->write = bme69x_spi_write;
        bme->intf = BME69X_SPI_INTF;

        ESP_LOGI(TAG, "SPI device configured successfully");
    } else {
        ESP_LOGE(TAG, "Invalid interface type");
        return BME69X_E_COM_FAIL;
    }

    bme->intf_ptr = &dev_addr;
    bme->delay_us = bme69x_delay_us;
    bme->amb_temp = CONFIG_BME690_AMBIENT_TEMP;

    return BME69X_OK;
}

void bme69x_check_rslt(const char api_name[], int8_t rslt)
{
    switch (rslt) {
    case BME69X_OK:
        /* Do nothing */
        break;
    case BME69X_E_NULL_PTR:
        ESP_LOGE(TAG, "API [%s] Error [%" PRIi8 "] : Null pointer", api_name, rslt);
        break;
    case BME69X_E_COM_FAIL:
        ESP_LOGE(TAG, "API [%s] Error [%" PRIi8 "] : Communication failure", api_name, rslt);
        break;
    case BME69X_E_INVALID_LENGTH:
        ESP_LOGE(TAG, "API [%s] Error [%" PRIi8 "] : Incorrect length parameter", api_name, rslt);
        break;
    case BME69X_E_DEV_NOT_FOUND:
        ESP_LOGE(TAG, "API [%s] Error [%" PRIi8 "] : Device not found", api_name, rslt);
        break;
    case BME69X_E_SELF_TEST:
        ESP_LOGE(TAG, "API [%s] Error [%" PRIi8 "] : Self test error", api_name, rslt);
        break;
    case BME69X_W_NO_NEW_DATA:
        ESP_LOGW(TAG, "API [%s] Warning [%" PRIi8 "] : No new data found", api_name, rslt);
        break;
    default:
        ESP_LOGE(TAG, "API [%s] Error [%" PRIi8 "] : Unknown error code", api_name, rslt);
        break;
    }
}

void bme69x_set_i2c_bus_handle(i2c_bus_handle_t bus_handle)
{
    i2c_bus = bus_handle;
    ESP_LOGI(TAG, "I2C bus handle set");
}

void bme69x_set_i2c_address(uint8_t i2c_addr)
{
    dev_addr = i2c_addr;
    ESP_LOGI(TAG, "I2C address set to 0x%02" PRIx8, i2c_addr);
}

void bme69x_set_spi_device_handle(spi_device_handle_t device_handle)
{
    spi_dev = device_handle;
    ESP_LOGI(TAG, "SPI device handle set");
}

void bme69x_interface_deinit(void)
{
    ESP_LOGI(TAG, "BME69X ESP32 deinit");

    if (i2c_dev) {
        i2c_bus_device_delete(&i2c_dev);
        i2c_dev = NULL;
    }
    /* Clear the bus handle, but do not delete the I2C bus */
    i2c_bus = NULL;
    dev_addr = BME69X_I2C_ADDR_LOW;
    spi_dev = NULL;
}
