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
#include "iot_mcp23017.h"

CMCP23017::CMCP23017(CI2CBus *p_i2c_bus, uint8_t addr)
{
    bus = p_i2c_bus;
    m_dev_handle = iot_mcp23017_create(bus->get_bus_handle(), addr);
}

CMCP23017::~CMCP23017()
{
    iot_mcp23017_delete(m_dev_handle, false);
    m_dev_handle = NULL;
}

esp_err_t CMCP23017::enable_interrupt_pins(uint16_t pins, bool isDefault,
        uint16_t defaultValue)
{
    return iot_mcp23017_interrupt_en(m_dev_handle, pins, isDefault,
            defaultValue);
}

esp_err_t CMCP23017::disable_interrupt_pins(uint16_t pins)
{
    return iot_mcp23017_interrupt_disable(m_dev_handle, pins);
}

uint16_t CMCP23017::get_intpin_values()
{
    return iot_mcp23017_get_int_pin(m_dev_handle);
}

uint16_t CMCP23017::get_intflag_values()
{
    return iot_mcp23017_get_int_flag(m_dev_handle);
}

esp_err_t CMCP23017::check_device_present()
{
    return iot_mcp23017_check_present(m_dev_handle);
}

esp_err_t CMCP23017::set_pullups(uint16_t pins)
{
    return iot_mcp23017_set_pullup(m_dev_handle, pins);
}

esp_err_t CMCP23017::set_iodirection(uint8_t value, mcp23017_gpio_t gpio)
{
    return iot_mcp23017_set_io_dir(m_dev_handle, value, gpio);
}

esp_err_t CMCP23017::write_ioport(uint8_t value, mcp23017_gpio_t gpio)
{
    return iot_mcp23017_write_io(m_dev_handle, value, gpio);
}

uint8_t CMCP23017::read_ioport(mcp23017_gpio_t gpio)
{
    return iot_mcp23017_read_io(m_dev_handle, gpio);
}

esp_err_t CMCP23017::mirror_interrupt(uint8_t mirror, mcp23017_gpio_t gpio)
{
    return iot_mcp23017_mirror_interrupt(m_dev_handle, mirror, gpio);
}

