/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
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
    is_installed = false;
}

AS5600::~AS5600()
{
    if (is_installed) {
        deinit();
    }
}

void AS5600::init()
{
    esp_err_t ret;

    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = _sda_io;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = _scl_io;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 20 * 1000;
    conf.clk_flags = 0;

    ret = i2c_param_config(_i2c_port, &conf);
    ESP_RETURN_ON_FALSE(ret == ESP_OK,, TAG, "I2C config fail");
    ret = i2c_driver_install(_i2c_port, conf.mode, 0, 0, 0);
    ESP_RETURN_ON_FALSE(ret == ESP_OK,, TAG, "I2C install fail");
    is_installed = true;
}

void AS5600::deinit()
{
    esp_err_t ret;
    ret = i2c_driver_delete(_i2c_port);
    ESP_RETURN_ON_FALSE(ret == ESP_OK,, TAG, "I2C del fail");
    is_installed = false;
}

float AS5600::getSensorAngle()
{
    if (i2c_master_write_read_device(_i2c_port, device_addr, &raw_angle_addr, 1, raw_angle_buf, 2, 1000 / portTICK_PERIOD_MS) != ESP_OK) {
        return -1;
    }
    raw_angle = (uint16_t)(raw_angle_buf[0] << 8 | raw_angle_buf[1]);
    angle = (((int)raw_angle & 0b0000111111111111) * 360.0f / 4096.0f) * (PI / 180.0f);

    return angle;
}
