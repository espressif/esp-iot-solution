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
