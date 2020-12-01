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
#include "iot_mvh3004d.h"

CMvh3004d::CMvh3004d(CI2CBus *p_i2c_bus, uint8_t addr)
{
    bus = p_i2c_bus;
    m_sensor_handle = iot_mvh3004d_create(bus->get_bus_handle(), addr);

}

CMvh3004d::~CMvh3004d()
{
    iot_mvh3004d_delete(m_sensor_handle, false);
    m_sensor_handle = NULL;
}

float CMvh3004d::read_temperature()
{
    float val;
    iot_mvh3004d_get_temperature(m_sensor_handle, &val);
    return val;
}

float CMvh3004d::read_humidity()
{
    float val;
    iot_mvh3004d_get_huminity(m_sensor_handle, &val);
    return val;
}

