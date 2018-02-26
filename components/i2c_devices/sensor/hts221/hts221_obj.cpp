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
