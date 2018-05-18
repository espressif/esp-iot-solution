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
#include <stdlib.h>
#include <math.h>
#include "driver/i2c.h"
#include "iot_i2c_bus.h"
#include "iot_hdc2010.h"

CHdc2010::CHdc2010(CI2CBus *p_i2c_bus, uint8_t addr)
{
    bus = p_i2c_bus;
    m_sensor_handle = iot_hdc2010_create(bus->get_bus_handle(), addr);
}

CHdc2010::~CHdc2010()
{
    iot_hdc2010_delete(m_sensor_handle, false);
    m_sensor_handle = NULL;
}

float CHdc2010::temperature()
{
    return iot_hdc2010_get_temperature(m_sensor_handle);
}

float CHdc2010::humidity()
{
    return iot_hdc2010_get_humidity(m_sensor_handle);
}

esp_err_t CHdc2010::interrupt_info(hdc2010_interrupt_info_t * info)
{
    return iot_hdc2010_get_interrupt_info(m_sensor_handle, info);
}

esp_err_t CHdc2010::set_interrupt_config(hdc2010_interrupt_config_t * config)
{
    return iot_hdc2010_set_interrupt_config(m_sensor_handle, config);
}

float CHdc2010::max_temperature()
{
    return iot_hdc2010_get_max_temperature(m_sensor_handle);
}

float CHdc2010::max_humidity()
{
    return iot_hdc2010_get_max_humidity(m_sensor_handle);
}

esp_err_t CHdc2010::set_temperature_offset(uint8_t offset_data)
{

    return iot_hdc2010_set_temperature_offset(m_sensor_handle, offset_data);
}

esp_err_t CHdc2010::set_humidity_offset(uint8_t offset_data)
{
    return iot_hdc2010_set_humidity_offset(m_sensor_handle, offset_data);
}

esp_err_t CHdc2010::set_temperature_threshold(float temperature_data)
{
    return iot_hdc2010_set_temperature_threshold(m_sensor_handle,
            temperature_data);
}

esp_err_t CHdc2010::set_humidity_threshold(float humidity_data)
{
    return iot_hdc2010_set_humidity_threshold(m_sensor_handle, humidity_data);
}

esp_err_t CHdc2010::set_reset_and_drdy(hdc2010_reset_and_drdy_t * config)
{
    return iot_hdc2010_set_reset_and_drdy(m_sensor_handle, config);
}
esp_err_t CHdc2010::set_measurement_config(
        hdc2010_measurement_config_t * config)
{
    return iot_hdc2010_set_measurement_config(m_sensor_handle, config);
}

int CHdc2010::manufacturer_id(void)
{
    return iot_hdc2010_get_manufacturer_id(m_sensor_handle);
}

int CHdc2010::device_id(void)
{
    return iot_hdc2010_get_device_id(m_sensor_handle);
}

