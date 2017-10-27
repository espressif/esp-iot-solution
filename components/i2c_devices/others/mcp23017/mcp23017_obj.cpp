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
#include "mcp23017.h"

CMCP23017::CMCP23017(CI2CBus *p_i2c_bus, uint8_t addr)
{
    bus = p_i2c_bus;
    m_dev_handle = dev_mcp23017_create(bus->get_bus_handle(), addr);
}

CMCP23017::~CMCP23017()
{
    dev_mcp23017_delete(m_dev_handle, false);
    m_dev_handle = NULL;
}

esp_err_t CMCP23017::enable_interrupt_pins(uint16_t pins, bool isDefault,
        uint16_t defaultValue)
{
    return mcp23017_enable_interrupt_pins(m_dev_handle, pins, isDefault,
            defaultValue);
}

esp_err_t CMCP23017::disable_interrupt_pins(uint16_t pins)
{
    return mcp23017_disable_interrupt_pins(m_dev_handle, pins);
}

uint16_t CMCP23017::get_intpin_values()
{
    return mcp23017_get_intpin_values(m_dev_handle);
}

uint16_t CMCP23017::get_intflag_values()
{
    return mcp23017_get_intflag_values(m_dev_handle);
}

esp_err_t CMCP23017::check_device_present()
{
    return mcp23017_check_device_present(m_dev_handle);
}

esp_err_t CMCP23017::set_pullups(uint16_t pins)
{
    return mcp23017_set_pullups(m_dev_handle, pins);
}

esp_err_t CMCP23017::set_iodirection(uint8_t value, mcp23017_gpio_t gpio)
{
    return mcp23017_set_iodirection(m_dev_handle, value, gpio);
}

esp_err_t CMCP23017::write_ioport(uint8_t value, mcp23017_gpio_t gpio)
{
    return mcp23017_write_ioport(m_dev_handle, value, gpio);
}

uint8_t CMCP23017::read_ioport(mcp23017_gpio_t gpio)
{
    return mcp23017_read_ioport(m_dev_handle, gpio);
}

esp_err_t CMCP23017::mirror_interrupt(uint8_t mirror, mcp23017_gpio_t gpio)
{
    return mcp23017_mirror_interrupt(m_dev_handle, mirror, gpio);
}

