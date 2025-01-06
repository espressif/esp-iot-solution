/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "as5048a.h"
#include "esp_log.h"
#include "esp_check.h"

#define AS5048A_REG_ANGLE   0x3FFF
#define PI                  3.14159265358979f

static const char *TAG = "AS5048a";

AS5048a::AS5048a(spi_host_device_t spi_host, gpio_num_t sclk_io, gpio_num_t miso_io, gpio_num_t mosi_io, gpio_num_t cs_io)
{
    _spi_host = spi_host;
    _sclk_io = sclk_io;
    _miso_io = miso_io;
    _mosi_io = mosi_io;
    _cs_io = cs_io;
    _is_installed = false;
}

AS5048a::~AS5048a()
{
    if (_is_installed) {
        deinit();
    }
}

void AS5048a::deinit()
{
    esp_err_t ret;
    ret = spi_bus_remove_device(_spi_device);
    ESP_RETURN_ON_FALSE(ret == ESP_OK,, TAG, "SPI remove device fail");
    ret = spi_bus_free(_spi_host);
    ESP_RETURN_ON_FALSE(ret == ESP_OK,, TAG, "SPI free fail");
    _is_installed = false;
}

void AS5048a::init()
{
    esp_err_t ret;

    /*!< Configuration for the spi bus */
    spi_bus_config_t buscfg = {
        .mosi_io_num = _mosi_io,
        .miso_io_num = _miso_io,
        .sclk_io_num = _sclk_io,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 1000,
    };
    ret = spi_bus_initialize(_spi_host, &buscfg, SPI_DMA_CH_AUTO);
    ESP_RETURN_ON_FALSE(ret == ESP_OK,, TAG, "SPI bus init fail");

    spi_device_interface_config_t devcfg = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .mode = 1,
        .cs_ena_pretrans = 1,
        .clock_speed_hz = 4 * 1000 * 1000,
        .spics_io_num = _cs_io,
        .queue_size = 1,
    };

    ret = spi_bus_add_device(_spi_host, &devcfg, &_spi_device);
    ESP_RETURN_ON_FALSE(ret == ESP_OK,, TAG, "SPI bus add device fail");

    _is_installed = true;
}

uint8_t AS5048a::calculateParity(uint16_t value)
{
    uint8_t count = 0;
    for (int i = 0; i < 16; i++) {
        if (value & 0x1) {
            count++;
        }
        value >>= 1;
    }
    return count & 0x1;
}

uint16_t AS5048a::readRegister(uint16_t reg_address)
{
    uint16_t command = 1 << 14; // PAR=0 R/W=R (Read command)
    command |= reg_address; // Command to read angle
    command |= ((uint16_t)calculateParity(command) << 15); // Adding parity bit

    uint8_t cmd_high = (command >> 8) & 0xFF;
    uint8_t cmd_low = command & 0xFF;
    uint8_t tx_buffer[2] = {cmd_high, cmd_low};
    spi_transaction_t t = {};
    t.length = 16; // 16 bits
    t.tx_buffer = tx_buffer;
    t.rx_buffer = NULL;
    spi_device_transmit(_spi_device, &t);

    uint8_t rx_buffer[2] = {};
    t.length = 16; // 16 bits
    t.tx_buffer = NULL;
    t.rxlength = 16; // 16 bits
    t.rx_buffer = rx_buffer;
    spi_device_transmit(_spi_device, &t);

    uint16_t reg_value = (rx_buffer[0] << 8) | rx_buffer[1];
    return reg_value & 0x3FFF; // Masking to get 14 bits angle value
}

uint16_t AS5048a::readRawPosition()
{
    return readRegister(AS5048A_REG_ANGLE);
}

float AS5048a::getSensorAngle()
{
    uint16_t raw_angle = readRawPosition();
    float angle = (((int)raw_angle & 0b0011111111111111) * 360.0f / 16383.0f) * (PI / 180.0f);
    return angle;
}
