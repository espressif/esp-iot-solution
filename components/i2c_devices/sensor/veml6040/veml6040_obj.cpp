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
#include "iot_veml6040.h"

CVeml6040::CVeml6040(CI2CBus *p_i2c_bus, uint8_t addr)
{
    bus = p_i2c_bus;
    m_sensor_handle = iot_veml6040_create(bus->get_bus_handle(), addr);
}

CVeml6040::~CVeml6040()
{
    iot_veml6040_delete(m_sensor_handle, false);
    m_sensor_handle = NULL;
}

esp_err_t CVeml6040::set_mode(veml6040_config_t * device_info)
{

    return iot_veml6040_set_mode(m_sensor_handle, device_info);
}

int CVeml6040::red()
{
    return iot_veml6040_get_red(m_sensor_handle);
}

int CVeml6040::green()
{
    return iot_veml6040_get_green(m_sensor_handle);
}

int CVeml6040::blue()
{
    return iot_veml6040_get_blue(m_sensor_handle);
}

int CVeml6040::white()
{
    return iot_veml6040_get_white(m_sensor_handle);
}

float CVeml6040::lux()
{
    return iot_veml6040_get_lux(m_sensor_handle);
}

uint16_t CVeml6040::cct(float offset)
{
    return iot_veml6040_get_cct(m_sensor_handle, offset);
}

