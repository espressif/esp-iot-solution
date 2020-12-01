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
#include "iot_bh1750.h"

CBh1750::CBh1750(CI2CBus *p_i2c_bus, uint8_t addr)
{
    bus = p_i2c_bus;
    m_sensor_handle = iot_bh1750_create(bus->get_bus_handle(), addr);

}

CBh1750::~CBh1750()
{
    iot_bh1750_delete(m_sensor_handle, false);
    m_sensor_handle = NULL;
}

float CBh1750::read()
{
    float val;
    iot_bh1750_get_data(m_sensor_handle, &val);
    return val;
}

esp_err_t CBh1750::on()
{
    return iot_bh1750_power_on(m_sensor_handle);
}
esp_err_t CBh1750::off()
{
    return iot_bh1750_power_down(m_sensor_handle);
}

esp_err_t CBh1750::set_mode(bh1750_cmd_measure_t cmd_measure)
{
    return iot_bh1750_set_measure_mode(m_sensor_handle, cmd_measure);
}

