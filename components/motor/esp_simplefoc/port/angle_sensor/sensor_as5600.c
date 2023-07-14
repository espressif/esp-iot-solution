/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sensor_as5600.h"
#include "driver/gpio.h"
#include "driver/i2c.h"

#define PI 3.14159265358979f
#define AS5600_ADDR 0x36
#define RAW_ANGLE_ADDR 0x0C


/**
 * @description: sensor as5600 init.
 * @return {*}
 */
void sensor_as5600_init(int i2c_num, int scl_io, int sda_io)
{
    esp_err_t result;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_io,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = scl_io,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 20 * 1000,
    };

    result = i2c_param_config(i2c_num, &conf);
    if (result != ESP_OK)
    {
        return;
    }

    // 安装驱动
    i2c_driver_install(i2c_num, conf.mode, 0, 0, 0);
}

/**
 * @description: sensor as5600 gets raw angle.
 * @return {*}
 */
float sensor_as5600_getAngle(int i2c_num)
{
    uint8_t data[2];
    uint8_t reg_addr = RAW_ANGLE_ADDR;
    uint16_t result = 0;
    float angle = 0.0f;
    i2c_master_write_read_device(i2c_num, AS5600_ADDR, &reg_addr, 1, data, 2, 1000 / portTICK_PERIOD_MS);

    result = (uint16_t)(data[0] << 8 | data[1]);
    angle = (((int)result & 0b0000111111111111) * 360.0f / 4096.0f) * (PI / 180.0f);

    return angle;
}