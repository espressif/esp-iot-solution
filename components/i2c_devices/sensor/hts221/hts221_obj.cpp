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
#include "iot_hts221.h"

CHts221::CHts221(CI2CBus *p_i2c_bus, uint8_t addr)
{
    bus = p_i2c_bus;
    m_sensor_handle = iot_hts221_create(bus->get_bus_handle(), addr);
    hts221_config_t hts221_config;
    iot_hts221_get_config(m_sensor_handle, &hts221_config);
    hts221_config.avg_h = HTS221_AVGH_32;
    hts221_config.avg_t = HTS221_AVGT_16;
    hts221_config.odr = HTS221_ODR_1HZ;
    hts221_config.bdu_status = HTS221_DISABLE;
    hts221_config.heater_status = HTS221_DISABLE;
    iot_hts221_set_config(m_sensor_handle, &hts221_config);
    iot_hts221_set_activate(m_sensor_handle);
}

CHts221::~CHts221()
{
    iot_hts221_delete(m_sensor_handle, false);
    m_sensor_handle = NULL;
}

float CHts221::read_temperature()
{
    int16_t temperature;
    iot_hts221_get_temperature(m_sensor_handle, &temperature);
    return (float) temperature / 10;
}

float CHts221::read_humidity()
{
    int16_t humidity;
    iot_hts221_get_humidity(m_sensor_handle, &humidity);
    return (float) humidity / 10;
}

uint8_t CHts221::id()
{
    uint8_t id;
    iot_hts221_get_deviceid(m_sensor_handle, &id);
    return id;
}
