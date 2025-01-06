/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mt6701.h"
#include "math.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "MT6701";

/* Constants for CRC calculation */
static uint8_t tableCRC6[64] = {
    0x00, 0x03, 0x06, 0x05, 0x0C, 0x0F, 0x0A, 0x09,
    0x18, 0x1B, 0x1E, 0x1D, 0x14, 0x17, 0x12, 0x11,
    0x30, 0x33, 0x36, 0x35, 0x3C, 0x3F, 0x3A, 0x39,
    0x28, 0x2B, 0x2E, 0x2D, 0x24, 0x27, 0x22, 0x21,
    0x23, 0x20, 0x25, 0x26, 0x2F, 0x2C, 0x29, 0x2A,
    0x3B, 0x38, 0x3D, 0x3E, 0x37, 0x34, 0x31, 0x32,
    0x13, 0x10, 0x15, 0x16, 0x1F, 0x1C, 0x19, 0x1A,
    0x0B, 0x08, 0x0D, 0x0E, 0x07, 0x04, 0x01, 0x02
};

/* 32-bit input data, right alignment, Calculation over 18 bits (mult. of 6) */
static uint8_t CRC6_43_18bit(uint32_t w_InputData)
{
    uint8_t b_Index = 0;
    uint8_t b_CRC = 0;

    b_Index = (uint8_t)(((uint32_t)w_InputData >> 12u) & 0x0000003Fu);

    b_CRC = (uint8_t)(((uint32_t)w_InputData >> 6u) & 0x0000003Fu);
    b_Index = b_CRC ^ tableCRC6[b_Index];

    b_CRC = (uint8_t)((uint32_t)w_InputData & 0x0000003Fu);
    b_Index = b_CRC ^ tableCRC6[b_Index];

    b_CRC = tableCRC6[b_Index];

    return b_CRC;
}

MT6701::MT6701(spi_host_device_t spi_host, gpio_num_t sclk_io, gpio_num_t miso_io, gpio_num_t mosi_io, gpio_num_t cs_io)
{
    _spi_host = spi_host;
    _sclk_io = sclk_io;
    _miso_io = miso_io;
    _mosi_io = mosi_io;
    _cs_io = cs_io;
    _is_installed = false;
}

MT6701::MT6701(i2c_port_t i2c_port, gpio_num_t sclk_io, gpio_num_t miso_io)
{
    _i2c_port = i2c_port;
    _sclk_io = sclk_io;
    _miso_io = miso_io;
    _is_installed = false;
}

MT6701::~MT6701()
{
    if (_is_installed) {
        deinit();
    }
}

void MT6701::init()
{
    esp_err_t ret;

    if (_spi_host != SPI_HOST_MAX) {
        // Configuration for the spi bus
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

        spi_device_interface_config_t dev_cfg = {
            .command_bits = 0,
            .address_bits = 0,
            .dummy_bits = 0,
            .mode = 3,
            .clock_speed_hz = 4000000,
            .spics_io_num = _cs_io,
            .flags = 0,
            .queue_size = 1,
            .pre_cb = NULL,
            .post_cb = NULL,
        };

        ret = spi_bus_add_device(_spi_host, &dev_cfg, &_spi_device);
        ESP_RETURN_ON_FALSE(ret == ESP_OK,, TAG, "SPI bus add device fail");

        _is_installed = true;
    } else if (_i2c_port != I2C_NUM_MAX) {
        // Configuration for the i2c bus
        i2c_config_t i2c_config = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = _miso_io,
            .scl_io_num = _sclk_io,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
        };
        i2c_config.master.clk_speed = 400 * 1000;

        _i2c_bus = i2c_bus_create(_i2c_port, &i2c_config);
        ESP_RETURN_ON_FALSE(_i2c_bus != NULL,, TAG, "I2C bus create fail");
        _i2c_device = i2c_bus_device_create(_i2c_bus, 0x06, 0);
        ESP_RETURN_ON_FALSE(_i2c_device != NULL,, TAG, "MT6701 device create fail");

        _is_installed = true;
    } else {
        _is_installed = false;
    }

}

void MT6701::deinit()
{
    esp_err_t ret;

    if (_spi_host != SPI_HOST_MAX) {
        ret = spi_bus_remove_device(_spi_device);
        ESP_RETURN_ON_FALSE(ret == ESP_OK,, TAG, "SPI remove device fail");
        ret = spi_bus_free(_spi_host);
        ESP_RETURN_ON_FALSE(ret == ESP_OK,, TAG, "SPI free fail");
        _is_installed = false;
    } else if (_i2c_port != I2C_NUM_MAX) {
        i2c_bus_device_delete(&_i2c_device);
        ESP_RETURN_ON_FALSE(_i2c_device == NULL,, TAG, "MT6701 device delete fail");
        ret = i2c_bus_delete(&_i2c_bus);
        ESP_RETURN_ON_FALSE(ret == ESP_OK,, TAG, "I2C bus delete fail");
        _is_installed = false;
    }
}

float MT6701::getSensorAngle()
{
    esp_err_t ret;
    static float angle = 0.0;
    static float previous_angle = 0.0;

    if (_spi_host != SPI_HOST_MAX) {
        spi_transaction_t spi_transaction = {
            .flags = SPI_TRANS_USE_RXDATA,
            .length = 24,
            .rxlength = 24,
            .tx_buffer = NULL,
            .rx_buffer = NULL,
        };

        ret = spi_device_polling_transmit(_spi_device, &spi_transaction);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, 0.0, TAG, "SPI transaction failed: %s", esp_err_to_name(ret));

        uint32_t spi_32 = ((int32_t)spi_transaction.rx_data[0] << 16) | ((int32_t)spi_transaction.rx_data[1] << 8) | spi_transaction.rx_data[2];
        uint32_t angle_spi = spi_32 >> 10;

        uint8_t received_crc = spi_32 & 0x3F;
        uint8_t calculated_crc = CRC6_43_18bit(spi_32 >> 6);

        if (received_crc == calculated_crc) {
            angle = (float)angle_spi * 2 * M_PI / 16384;
            previous_angle = angle;
            ESP_LOGD(TAG, "Angle: %f", angle);
        } else {
            return previous_angle;
        }
        return angle;
    } else if (_i2c_port != I2C_NUM_MAX) {
        uint8_t raw_angle_buffer[2] = {0};
        if (i2c_bus_read_bytes(_i2c_device, 0x03, 2, raw_angle_buffer) != ESP_OK) {
            return -1;
        }
        angle = ((int)((raw_angle_buffer[0] << 6) | (raw_angle_buffer[1] >> 2))) * 0.00038349519f; /*!< Converts raw data into angle information in radians. */
        return angle;
    }

    return -1;
}
