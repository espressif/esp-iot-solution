/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "common/base_classes/Sensor.h"
#include "driver/spi_master.h"
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
    spi_host_device_t _spi_host;
    gpio_num_t _sclk_io;
    gpio_num_t _miso_io;
    gpio_num_t _mosi_io;
    gpio_num_t _cs_io;
    spi_device_handle_t spi_device;
    bool is_installed;
};
