/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "common/base_classes/Sensor.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

class AS5048a : public Sensor {
public:
    /**
         * @brief Construct a new AS5048a object
         *
         * @param spi_host
         * @param sclk_io
         * @param miso_io
         * @param mosi_io
         * @param cs_io
         */
    AS5048a(spi_host_device_t spi_host, gpio_num_t sclk_io, gpio_num_t miso_io, gpio_num_t mosi_io, gpio_num_t cs_io);
    /**
     * @brief Destroy the AS5048a object
     *
     */
    ~AS5048a();
    /**
     * @brief Init spi for AS5048a
     *
     */
    void        init();
    /**
     * @brief Deinit spi for AS5048a
     *
     */
    void        deinit();
    /**
     * @brief Get the output of AS5048a
     *
     * @return float
     */
    float       getSensorAngle();

private:
    uint16_t        readRegister(uint16_t reg_address);
    uint16_t        readRawPosition();
    static uint8_t  calculateParity(uint16_t value);

private:
    spi_host_device_t _spi_host;
    gpio_num_t _sclk_io;
    gpio_num_t _miso_io;
    gpio_num_t _mosi_io;
    gpio_num_t _cs_io;
    spi_device_handle_t _spi_device;
    bool _is_installed;
};
