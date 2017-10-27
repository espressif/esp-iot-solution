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
#include "at24c02.h"

CAT24C02::CAT24C02(CI2CBus *p_i2c_bus, uint8_t addr)
{
    bus = p_i2c_bus;
    m_dev_handle = dev_at24c02_create(bus->get_bus_handle(), addr);

}

CAT24C02::~CAT24C02()
{
    dev_at24c02_delete(m_dev_handle, false);
    m_dev_handle = NULL;
}

esp_err_t CAT24C02::write_byte(uint8_t addr, uint8_t data)
{
    return at24c02_write_byte(m_dev_handle, addr, data);
}

esp_err_t CAT24C02::read_byte(uint8_t addr, uint8_t *data)
{
    return at24c02_read_byte(m_dev_handle, addr, data);
}

esp_err_t CAT24C02::write(uint8_t start_addr, uint8_t write_num,
        uint8_t *data_buf)
{
    return at24c02_write(m_dev_handle, start_addr, write_num, data_buf);
}

esp_err_t CAT24C02::read(uint8_t start_addr, uint8_t read_num,
        uint8_t *data_buf)
{
    return at24c02_read(m_dev_handle, start_addr, read_num, data_buf);
}
