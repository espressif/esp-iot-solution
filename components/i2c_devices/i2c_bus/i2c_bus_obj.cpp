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

CI2CBus::CI2CBus(i2c_port_t i2c_port, gpio_num_t scl_io, gpio_num_t sda_io, int clk_hz, i2c_mode_t i2c_mode)
{
    i2c_config_t conf;
    conf.mode = i2c_mode;
    conf.scl_io_num = scl_io;
    conf.sda_io_num = sda_io;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = clk_hz;
    m_i2c_bus_handle = iot_i2c_bus_create(i2c_port, &conf);
}

CI2CBus::~CI2CBus()
{
    iot_i2c_bus_delete(m_i2c_bus_handle);
    m_i2c_bus_handle = NULL;
}

esp_err_t CI2CBus::send(i2c_cmd_handle_t cmd, portBASE_TYPE ticks_to_wait)
{
    return iot_i2c_bus_cmd_begin(m_i2c_bus_handle, cmd, ticks_to_wait);
}

i2c_bus_handle_t CI2CBus::get_bus_handle()
{
    return m_i2c_bus_handle;
}
