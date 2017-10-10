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
#include "i2c_bus.h"
#include "bh1750.h"

CBh1750::CBh1750(CI2CBus *p_i2c_bus, uint8_t addr)
{
    bus = p_i2c_bus;
    m_sensor_handle = sensor_bh1750_create(bus->get_bus_handle(), addr);

}

CBh1750::~CBh1750()
{
    sensor_bh1750_delete(m_sensor_handle, false);
    m_sensor_handle = NULL;
}

float CBh1750::read()
{
    float val;
    bh1750_get_data(m_sensor_handle, &val);
    return val;
}

esp_err_t CBh1750::on()
{
    return bh1750_power_on(m_sensor_handle);
}
esp_err_t CBh1750::off()
{
    return bh1750_power_down(m_sensor_handle);
}

esp_err_t CBh1750::set_mode(bh1750_cmd_measure_t cmd_measure)
{
    return bh1750_set_measure_mode(m_sensor_handle, cmd_measure);
}

