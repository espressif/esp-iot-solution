/*
  * ESPRESSIF MIT License
  *
  * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
  *
  * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
  * it is free of charge, to any person obtaining a copy of this software and associated
  * documentation files (the "Software"), to deal in the Software without restriction, including
  * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
  * to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in all copies or
  * substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  *
  */

#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "iot_i2c_bus.h"
#include "iot_lis2dh12.h"

CLis2dh12::CLis2dh12(CI2CBus *p_i2c_bus, uint8_t addr)
{
    bus = p_i2c_bus;
    m_sensor_handle = iot_lis2dh12_create(bus->get_bus_handle(), addr);

    lis2dh12_config_t  lis2dh12_config;
    iot_lis2dh12_get_config(m_sensor_handle, &lis2dh12_config);
    lis2dh12_config.temp_enable = LIS2DH12_TEMP_DISABLE;
    lis2dh12_config.odr = LIS2DH12_ODR_1HZ;
    lis2dh12_config.opt_mode = LIS2DH12_OPT_NORMAL;
    lis2dh12_config.z_enable = LIS2DH12_ENABLE;
    lis2dh12_config.y_enable = LIS2DH12_ENABLE;
    lis2dh12_config.x_enable = LIS2DH12_ENABLE;
    lis2dh12_config.bdu_status = LIS2DH12_DISABLE;
    lis2dh12_config.fs = LIS2DH12_FS_16G;
    iot_lis2dh12_set_config(m_sensor_handle, &lis2dh12_config);
}

CLis2dh12::~CLis2dh12()
{
    iot_lis2dh12_delete(m_sensor_handle, false);
    m_sensor_handle = NULL;
}

uint8_t CLis2dh12::id()
{
    uint8_t id;
    iot_lis2dh12_get_deviceid(m_sensor_handle, &id);
    return id;
}

uint16_t CLis2dh12::ax()
{
    uint16_t acc_val;
    iot_lis2dh12_get_x_acc(m_sensor_handle, &acc_val);
    return acc_val;
}

uint16_t CLis2dh12::ay()
{
    uint16_t acc_val;
    iot_lis2dh12_get_y_acc(m_sensor_handle, &acc_val);
    return acc_val;
}

uint16_t CLis2dh12::az()
{
    uint16_t acc_val;
    iot_lis2dh12_get_z_acc(m_sensor_handle, &acc_val);
    return acc_val;
}




