// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
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

