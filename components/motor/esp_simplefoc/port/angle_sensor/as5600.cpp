/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "as5600.h"
#include "esp_check.h"

#define PI 3.14159265358979f
static const char *TAG = "AS5600";

AS5600::AS5600(i2c_port_t i2c_port, gpio_num_t scl_io, gpio_num_t sda_io)
{
    _i2c_port = i2c_port;
    _scl_io = scl_io;
    _sda_io = sda_io;
    _is_installed = false;
}

AS5600::~AS5600()
{
    if (_is_installed) {
        deinit();
    }
}

void AS5600::init()
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = _sda_io;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = _scl_io;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 400 * 1000;
    conf.clk_flags = 0;

    _i2c_bus = i2c_bus_create(_i2c_port, &conf);
    ESP_RETURN_ON_FALSE(_i2c_bus != NULL,, TAG, "I2C bus create fail");
    _i2c_device = i2c_bus_device_create(_i2c_bus, 0x36, 0);
    ESP_RETURN_ON_FALSE(_i2c_device != NULL,, TAG, "AS5600 device create fail");
    _is_installed = true;
}

void AS5600::deinit()
{
    esp_err_t ret = ESP_FAIL;
    i2c_bus_device_delete(&_i2c_device);
    ESP_RETURN_ON_FALSE(_i2c_device == NULL,, TAG, "AS5600 device delete fail");
    ret = i2c_bus_delete(&_i2c_bus);
    ESP_RETURN_ON_FALSE(ret == ESP_OK,, TAG, "I2C bus delete fail");
    _is_installed = false;
}

float AS5600::getSensorAngle()
{
    uint8_t raw_angle_buf[2] = {0};
    if (i2c_bus_read_bytes(_i2c_device, 0x0C, 2, raw_angle_buf) != ESP_OK) {
        return -1.0f;
    }
    _raw_angle = (uint16_t)(raw_angle_buf[0] << 8 | raw_angle_buf[1]);
    _angle = (((int)_raw_angle & 0b0000111111111111) * 360.0f / 4096.0f) * (PI / 180.0f);
    return _angle;
}
