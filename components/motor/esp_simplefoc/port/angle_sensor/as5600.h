/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "common/base_classes/Sensor.h"
#include "driver/i2c.h"
#include "driver/gpio.h"

class AS5600 : public Sensor {
public:
    /**
     * @brief Construct a new as5600 object
     *
     * @param i2c_port
     * @param scl_io
     * @param sda_io
     */
    AS5600(i2c_port_t i2c_port, gpio_num_t scl_io, gpio_num_t sda_io);

    /**
     * @brief Destroy the as5600 object
     *
     */
    ~AS5600();

    /**
     * @brief Init i2c for as5600
     *
     */
    void init();

    /**
     * @brief Deinit i2c for as5600
     *
     */
    void deinit();

    /**
     * @brief Get the output of as5600
     *
     * @return float
     */
    float getSensorAngle();

private:
    i2c_port_t _i2c_port;
    gpio_num_t _scl_io;
    gpio_num_t _sda_io;
    bool is_installed;
    uint8_t device_addr = 0x36;
    uint8_t raw_angle_addr = 0x0C;
    uint8_t raw_angle_buf[2] = {0};
    uint16_t raw_angle;
    float angle;
};
