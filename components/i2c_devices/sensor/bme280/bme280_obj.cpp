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
#include "iot_bme280.h"

CBme280::CBme280(CI2CBus *p_i2c_bus, uint8_t addr)
{
    bus = p_i2c_bus;
    m_dev_handle = iot_bme280_create(bus->get_bus_handle(), addr);
}

CBme280::~CBme280()
{
    iot_bme280_delete(m_dev_handle, false);
    m_dev_handle = NULL;
}

esp_err_t CBme280::init(void)
{
    return iot_bme280_init(m_dev_handle);
}

esp_err_t CBme280::read_coefficients(void)
{
    return iot_bme280_read_coefficients(m_dev_handle);
}

esp_err_t CBme280::set_sampling(bme280_sensor_mode mode,
        bme280_sensor_sampling tempsampling,
        bme280_sensor_sampling presssampling,
        bme280_sensor_sampling humsampling, bme280_sensor_filter filter,
        bme280_standby_duration duration)
{
    return iot_bme280_set_sampling(m_dev_handle, mode, tempsampling,
            presssampling, humsampling, filter, duration);
}

esp_err_t CBme280::take_forced_measurement(void)
{
    return iot_bme280_take_forced_measurement(m_dev_handle);
}

float CBme280::temperature()
{
    return iot_bme280_read_temperature(m_dev_handle);
}

float CBme280::pressure()
{
    return iot_bme280_read_pressure(m_dev_handle);
}

float CBme280::humidity()
{
    return iot_bme280_read_humidity(m_dev_handle);
}

float CBme280::altitude(float seaLevel)
{
    return iot_bme280_read_altitude(m_dev_handle, seaLevel);
}

float CBme280::calculates_pressure(float altitude, float atmospheric)
{
    return iot_bme280_calculates_pressure(m_dev_handle, altitude, atmospheric);
}

