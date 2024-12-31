/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "common/base_classes/Sensor.h"
#include "driver/gpio.h"
#include "i2c_bus.h"

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
    i2c_bus_handle_t _i2c_bus;
    i2c_bus_device_handle_t _i2c_device;
    i2c_port_t _i2c_port;
    gpio_num_t _scl_io;
    gpio_num_t _sda_io;
    bool _is_installed;
    uint16_t _raw_angle;
    float _angle;
};
