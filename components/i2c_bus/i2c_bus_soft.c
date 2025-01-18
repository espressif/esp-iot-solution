/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_err.h"
#include "esp_check.h"
#include "i2c_bus_soft.h"
#include "driver/gpio.h"

static const char*TAG = "i2c_bus_soft";

static esp_err_t i2c_master_soft_bus_wait_ack(i2c_master_soft_bus_handle_t bus_handle)
{
    esp_rom_delay_us(bus_handle->time_delay_us);
    ESP_RETURN_ON_ERROR(gpio_set_level(bus_handle->scl_io, 1), TAG, "Failed to set SCL high");
    esp_rom_delay_us(bus_handle->time_delay_us);

    bool ack = !gpio_get_level(bus_handle->sda_io);                                                              /*!< SDA should be low for ACK */
    ESP_RETURN_ON_ERROR(gpio_set_level(bus_handle->scl_io, 0), TAG, "Failed to set SCL low");
    esp_rom_delay_us(bus_handle->time_delay_us);

    return ack ? ESP_OK : ESP_ERR_NOT_FOUND;
}

static esp_err_t i2c_master_soft_bus_send_ack(i2c_master_soft_bus_handle_t bus_handle, bool ack)
{
    ESP_RETURN_ON_ERROR(gpio_set_level(bus_handle->sda_io, ack ? 0 : 1), TAG, "Failed to set SDA for ACK/NACK"); /*!< Set SDA line to ACK (low) or NACK (high) */

    // Generate clock pulse for ACK/NACK
    ESP_RETURN_ON_ERROR(gpio_set_level(bus_handle->scl_io, 1), TAG, "Failed to set SCL high during ACK/NACK");
    esp_rom_delay_us(bus_handle->time_delay_us);
    ESP_RETURN_ON_ERROR(gpio_set_level(bus_handle->scl_io, 0), TAG, "Failed to set SCL low after ACK/NACK");
    esp_rom_delay_us(bus_handle->time_delay_us);

    return ESP_OK;
}

static esp_err_t i2c_master_soft_bus_start(i2c_master_soft_bus_handle_t bus_handle)
{
    ESP_RETURN_ON_ERROR(gpio_set_level(bus_handle->scl_io, 1), TAG, "Failed to set SCL high");
    ESP_RETURN_ON_ERROR(gpio_set_level(bus_handle->sda_io, 1), TAG, "Failed to set SDA high");
    esp_rom_delay_us(bus_handle->time_delay_us);
    ESP_RETURN_ON_ERROR(gpio_set_level(bus_handle->sda_io, 0), TAG, "Failed to set SDA low");
    esp_rom_delay_us(bus_handle->time_delay_us);
    ESP_RETURN_ON_ERROR(gpio_set_level(bus_handle->scl_io, 0), TAG, "Failed to set SCL low");
    esp_rom_delay_us(bus_handle->time_delay_us);
    return ESP_OK;
}

static esp_err_t i2c_master_soft_bus_stop(i2c_master_soft_bus_handle_t bus_handle)
{
    ESP_RETURN_ON_ERROR(gpio_set_level(bus_handle->sda_io, 0), TAG, "Failed to set SDA low");
    ESP_RETURN_ON_ERROR(gpio_set_level(bus_handle->scl_io, 1), TAG, "Failed to set SCL high");
    esp_rom_delay_us(bus_handle->time_delay_us);
    ESP_RETURN_ON_ERROR(gpio_set_level(bus_handle->sda_io, 1), TAG, "Failed to set SDA high");
    return ESP_OK;
}

static esp_err_t i2c_master_soft_bus_write_byte(i2c_master_soft_bus_handle_t bus_handle, uint8_t byte)
{
    for (int i = 0; i < 8; i++) {
        if (byte & 0x80) {
            ESP_RETURN_ON_ERROR(gpio_set_level(bus_handle->sda_io, 1), TAG, "Failed to set SDA high");
        } else {
            ESP_RETURN_ON_ERROR(gpio_set_level(bus_handle->sda_io, 0), TAG, "Failed to set SDA low");
        }
        esp_rom_delay_us(bus_handle->time_delay_us);
        ESP_RETURN_ON_ERROR(gpio_set_level(bus_handle->scl_io, 1), TAG, "Failed to set SCL high");
        esp_rom_delay_us(bus_handle->time_delay_us);
        ESP_RETURN_ON_ERROR(gpio_set_level(bus_handle->scl_io, 0), TAG, "Failed to set SCL low");

        if (i == 7) {
            ESP_RETURN_ON_ERROR(gpio_set_level(bus_handle->sda_io, 1), TAG, "Failed to release SDA");            /*!< Release SDA */
        }

        byte <<= 1;
    }
    return ESP_OK;
}

static esp_err_t i2c_master_soft_bus_read_byte(i2c_master_soft_bus_handle_t bus_handle, uint8_t *byte)
{
    uint8_t value = 0;
    ESP_RETURN_ON_ERROR(gpio_set_level(bus_handle->sda_io, 1), TAG, "Failed to release SDA");                    /*!< First release SDA */
    for (int i = 0; i < 8; i++) {
        value <<= 1;
        ESP_RETURN_ON_ERROR(gpio_set_level(bus_handle->scl_io, 1), TAG, "Failed to set SCL high");
        esp_rom_delay_us(bus_handle->time_delay_us);

        if (gpio_get_level(bus_handle->sda_io)) {
            value ++;
        }
        ESP_RETURN_ON_ERROR(gpio_set_level(bus_handle->scl_io, 0), TAG, "Failed to set SCL low");
        esp_rom_delay_us(bus_handle->time_delay_us);
    }

    *byte = value;
    return ESP_OK;
}

esp_err_t i2c_master_soft_bus_write_reg8(i2c_master_soft_bus_handle_t bus_handle, uint8_t dev_addr, uint8_t mem_address, size_t data_len, const uint8_t *data)
{
    ESP_RETURN_ON_FALSE(bus_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid I2C bus handle");
    ESP_RETURN_ON_ERROR(i2c_master_soft_bus_start(bus_handle), TAG, "Failed to initiate start signal");

    // Send device address with write bit (0)
    uint8_t address_byte = (dev_addr << 1) | 0;
    ESP_RETURN_ON_ERROR(i2c_master_soft_bus_write_byte(bus_handle, address_byte), TAG, "Failed to write device address");

    ESP_RETURN_ON_ERROR(i2c_master_soft_bus_wait_ack(bus_handle), TAG, "No ACK for device address");

    if (mem_address != NULL_I2C_MEM_ADDR) {
        ESP_RETURN_ON_ERROR(i2c_master_soft_bus_write_byte(bus_handle, mem_address), TAG, "Failed to write memory address");
        ESP_RETURN_ON_ERROR(i2c_master_soft_bus_wait_ack(bus_handle), TAG, "No ACK for device address");
    }

    // Write data
    for (size_t i = 0; i < data_len; i++) {
        ESP_RETURN_ON_ERROR(i2c_master_soft_bus_write_byte(bus_handle, data[i]), TAG, "Failed to write data byte");
        ESP_RETURN_ON_ERROR(i2c_master_soft_bus_wait_ack(bus_handle), TAG, "No ACK for data byte");
    }

    // Generate STOP condition
    ESP_RETURN_ON_ERROR(i2c_master_soft_bus_stop(bus_handle), TAG, "Failed to initiate stop signal");
    return ESP_OK;
}

esp_err_t i2c_master_soft_bus_write_reg16(i2c_master_soft_bus_handle_t bus_handle, uint8_t dev_addr, uint16_t mem_address, size_t data_len, const uint8_t *data)
{
    ESP_RETURN_ON_FALSE(bus_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid I2C bus handle");
    ESP_RETURN_ON_ERROR(i2c_master_soft_bus_start(bus_handle), TAG, "Failed to initiate start signal");

    // Send device address with write bit (0)
    uint8_t address_byte = (dev_addr << 1) | 0;
    ESP_RETURN_ON_ERROR(i2c_master_soft_bus_write_byte(bus_handle, address_byte), TAG, "Failed to write device address");
    ESP_RETURN_ON_ERROR(i2c_master_soft_bus_wait_ack(bus_handle), TAG, "No ACK for device address");

    if (mem_address != NULL_I2C_MEM_16BIT_ADDR) {
        ESP_RETURN_ON_ERROR(i2c_master_soft_bus_write_byte(bus_handle, (uint8_t)((mem_address >> 8) & 0x00FF)), TAG, "Failed to write memory address");
        ESP_RETURN_ON_ERROR(i2c_master_soft_bus_wait_ack(bus_handle), TAG, "No ACK for mem address");
        ESP_RETURN_ON_ERROR(i2c_master_soft_bus_write_byte(bus_handle, (uint8_t)(mem_address & 0x00FF)), TAG, "Failed to write memory address");
        ESP_RETURN_ON_ERROR(i2c_master_soft_bus_wait_ack(bus_handle), TAG, "No ACK for mem address");
    }

    // Write data
    for (size_t i = 0; i < data_len; i++) {
        ESP_RETURN_ON_ERROR(i2c_master_soft_bus_write_byte(bus_handle, data[i]), TAG, "Failed to write data byte");
        ESP_RETURN_ON_ERROR(i2c_master_soft_bus_wait_ack(bus_handle), TAG, "No ACK for data byte");
    }

    // Generate STOP condition
    ESP_RETURN_ON_ERROR(i2c_master_soft_bus_stop(bus_handle), TAG, "Failed to initiate stop signal");
    return ESP_OK;
}

esp_err_t i2c_master_soft_bus_read_reg8(i2c_master_soft_bus_handle_t bus_handle, uint8_t dev_addr, uint8_t mem_address, size_t data_len, uint8_t *data)
{
    ESP_RETURN_ON_FALSE(bus_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid I2C bus handle");

    // Send memory address
    if (mem_address != NULL_I2C_MEM_ADDR) {
        // Generate START condition
        ESP_RETURN_ON_ERROR(i2c_master_soft_bus_start(bus_handle), TAG, "Failed to initiate start signal");

        // Send device address with write bit (0) to write the memory address
        uint8_t write_address_byte = (dev_addr << 1) | 0;
        ESP_RETURN_ON_ERROR(i2c_master_soft_bus_write_byte(bus_handle, write_address_byte), TAG, "Failed to write device address");
        ESP_RETURN_ON_ERROR(i2c_master_soft_bus_wait_ack(bus_handle), TAG, "No ACK for device address");
        ESP_RETURN_ON_ERROR(i2c_master_soft_bus_write_byte(bus_handle, mem_address), TAG, "Failed to write memory address");
        ESP_RETURN_ON_ERROR(i2c_master_soft_bus_wait_ack(bus_handle), TAG, "No ACK for mem address");
    }

    // Generate RESTART condition
    ESP_RETURN_ON_ERROR(i2c_master_soft_bus_start(bus_handle), TAG, "Failed to initiate repeated start signal");

    // Send device address with read bit (1)
    uint8_t read_address_byte = (dev_addr << 1) | 1;
    ESP_RETURN_ON_ERROR(i2c_master_soft_bus_write_byte(bus_handle, read_address_byte), TAG, "Failed to write device address with read bit");
    ESP_RETURN_ON_ERROR(i2c_master_soft_bus_wait_ack(bus_handle), TAG, "No ACK for device address");

    // Read data bytes
    for (size_t i = 0; i < data_len; i++) {
        ESP_RETURN_ON_ERROR(i2c_master_soft_bus_read_byte(bus_handle, &data[i]), TAG, "Failed to read data byte");

        // Send ACK for all but the last byte
        ESP_RETURN_ON_ERROR(i2c_master_soft_bus_send_ack(bus_handle, i != data_len - 1), TAG, "Failed to send ACK/NACK");
    }

    // Generate STOP condition
    ESP_RETURN_ON_ERROR(i2c_master_soft_bus_stop(bus_handle), TAG, "Failed to initiate stop signal");

    return ESP_OK;
}

esp_err_t i2c_master_soft_bus_read_reg16(i2c_master_soft_bus_handle_t bus_handle, uint8_t dev_addr, uint16_t mem_address, size_t data_len, uint8_t *data)
{
    ESP_RETURN_ON_FALSE(bus_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid I2C bus handle");

    // Send memory address
    if (mem_address != NULL_I2C_MEM_16BIT_ADDR) {
        // Generate START condition
        ESP_RETURN_ON_ERROR(i2c_master_soft_bus_start(bus_handle), TAG, "Failed to initiate start signal");

        // Send device address with write bit (0) to write the memory address
        uint8_t write_address_byte = (dev_addr << 1) | 0;
        ESP_RETURN_ON_ERROR(i2c_master_soft_bus_write_byte(bus_handle, write_address_byte), TAG, "Failed to write device address");
        ESP_RETURN_ON_ERROR(i2c_master_soft_bus_wait_ack(bus_handle), TAG, "No ACK for device address");
        ESP_RETURN_ON_ERROR(i2c_master_soft_bus_write_byte(bus_handle, (uint8_t)((mem_address >> 8) & 0x00FF)), TAG, "Failed to write memory address");
        ESP_RETURN_ON_ERROR(i2c_master_soft_bus_wait_ack(bus_handle), TAG, "No ACK for mem address");
        ESP_RETURN_ON_ERROR(i2c_master_soft_bus_write_byte(bus_handle, (uint8_t)(mem_address & 0x00FF)), TAG, "Failed to write memory address");
        ESP_RETURN_ON_ERROR(i2c_master_soft_bus_wait_ack(bus_handle), TAG, "No ACK for mem address");
    }

    // Generate RESTART condition
    ESP_RETURN_ON_ERROR(i2c_master_soft_bus_start(bus_handle), TAG, "Failed to initiate repeated start signal");

    // Send device address with read bit (1)
    uint8_t read_address_byte = (dev_addr << 1) | 1;
    ESP_RETURN_ON_ERROR(i2c_master_soft_bus_write_byte(bus_handle, read_address_byte), TAG, "Failed to write device address with read bit");
    ESP_RETURN_ON_ERROR(i2c_master_soft_bus_wait_ack(bus_handle), TAG, "No ACK for device address during read");

    // Read data bytes
    for (size_t i = 0; i < data_len; i++) {
        ESP_RETURN_ON_ERROR(i2c_master_soft_bus_read_byte(bus_handle, &data[i]), TAG, "Failed to read data byte");

        // Send ACK for all but the last byte
        ESP_RETURN_ON_ERROR(i2c_master_soft_bus_send_ack(bus_handle, i != data_len - 1), TAG, "Failed to send ACK/NACK");
    }

    // Generate STOP condition
    ESP_RETURN_ON_ERROR(i2c_master_soft_bus_stop(bus_handle), TAG, "Failed to initiate stop signal");
    return ESP_OK;
}

esp_err_t i2c_master_soft_bus_probe(i2c_master_soft_bus_handle_t bus_handle, uint8_t address)
{
    ESP_RETURN_ON_FALSE(bus_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid I2C bus handle");
    ESP_RETURN_ON_ERROR(i2c_master_soft_bus_start(bus_handle), TAG, "Failed to initiate start signal");

    // Send device address with write bit (0)
    uint8_t address_byte = (address << 1) | 0;
    ESP_RETURN_ON_ERROR(i2c_master_soft_bus_write_byte(bus_handle, address_byte), TAG, "Failed to write address byte");

    // Wait for ACK
    ESP_RETURN_ON_ERROR(i2c_master_soft_bus_wait_ack(bus_handle), TAG, "Failed to wait for ACK");

    // Generate STOP condition
    ESP_RETURN_ON_ERROR(i2c_master_soft_bus_stop(bus_handle), TAG, "Failed to initiate stop signal");
    return ESP_OK;
}

esp_err_t i2c_new_master_soft_bus(const i2c_config_t *conf, i2c_master_soft_bus_handle_t *ret_soft_bus_handle)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(GPIO_IS_VALID_GPIO(conf->scl_io_num) && GPIO_IS_VALID_GPIO(conf->sda_io_num), ESP_ERR_INVALID_ARG, TAG, "Invalid SDA/SCL pin number");
    ESP_RETURN_ON_FALSE(conf->master.clk_speed > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid scl frequency");

    gpio_config_t scl_io_conf = {
        .mode = GPIO_MODE_OUTPUT_OD,
        .pull_up_en = conf->scl_pullup_en,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pin_bit_mask = (1ULL << conf->scl_io_num),
    };
    ESP_RETURN_ON_ERROR(gpio_config(&scl_io_conf), TAG, "Failed to configure scl gpio");

    gpio_config_t sda_io_conf = {
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = conf->sda_pullup_en,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pin_bit_mask = (1ULL << conf->sda_io_num),
    };
    ESP_RETURN_ON_ERROR(gpio_config(&sda_io_conf), TAG, "Failed to configure sda gpio");

    i2c_master_soft_bus_handle_t soft_bus_handle = calloc(1, sizeof(struct i2c_master_soft_bus_t));
    if (soft_bus_handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate soft bus handle");
        return ESP_ERR_NO_MEM;
    }

    soft_bus_handle->scl_io = conf->scl_io_num;
    soft_bus_handle->sda_io = conf->sda_io_num;
    soft_bus_handle->time_delay_us = (uint32_t)((1e6f / conf->master.clk_speed) / 2.0f + 0.5f);
    *ret_soft_bus_handle = soft_bus_handle;

    return ret;
}

esp_err_t i2c_master_soft_bus_change_frequency(i2c_master_soft_bus_handle_t bus_handle, uint32_t frequency)
{
    ESP_RETURN_ON_FALSE(bus_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid I2C bus handle");
    ESP_RETURN_ON_FALSE(frequency > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid scl frequency");
    bus_handle->time_delay_us = (uint32_t)((1e6f / frequency) / 2.0f + 0.5f);
    return ESP_OK;
}

esp_err_t i2c_del_master_soft_bus(i2c_master_soft_bus_handle_t bus_handle)
{
    ESP_RETURN_ON_FALSE(bus_handle, ESP_ERR_INVALID_ARG, TAG, "no memory for i2c master soft bus");
    free(bus_handle);
    return ESP_OK;
}
