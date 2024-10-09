/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "common/base_classes/Sensor.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "driver/gpio.h"

class MT6701 : public Sensor {
public:
    /**
     * @brief Construct a new mt6701 object
     *
     * @param spi_host
     * @param sclk_io
     * @param miso_io
     * @param mosi_io
     * @param cs_io
     */
    MT6701(spi_host_device_t spi_host, gpio_num_t sclk_io, gpio_num_t miso_io, gpio_num_t mosi_io, gpio_num_t cs_io);

    /**
     * @brief Construct a new MT6701 object
     *
     * @param i2c_port
     * @param sclk_io
     * @param miso_io
     */
    MT6701(i2c_port_t i2c_port, gpio_num_t sclk_io, gpio_num_t miso_io);

    /**
     * @brief Destroy the mt6701 object
     *
     */
    ~MT6701();

    /**
     * @brief Init spi for mt6701
     *
     */
    void init();

    /**
     * @brief Deinit spi for mt6701
     *
     */
    void deinit();

    /**
     * @brief Get the output of mt6701
     *
     * @return float
     */
    float getSensorAngle();

private:
    spi_host_device_t _spi_host = SPI_HOST_MAX;
    spi_device_handle_t spi_device;
    i2c_port_t _i2c_port = I2C_NUM_MAX;
    gpio_num_t _sclk_io;
    gpio_num_t _miso_io;
    gpio_num_t _mosi_io;
    gpio_num_t _cs_io;
    bool is_installed;
};
