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
#include "iot_is31fl3736.h"

CIs31fl3736::CIs31fl3736(CI2CBus *p_i2c_bus, gpio_num_t rst_io, is31fl3736_addr_pin_t addr1, is31fl3736_addr_pin_t addr2,
                         uint8_t cur_val)
{
    bus = p_i2c_bus;
    m_led_handle = iot_is31fl3736_create(p_i2c_bus->get_bus_handle(),  rst_io, addr1, addr2, cur_val);
}

CIs31fl3736::~CIs31fl3736()
{
    iot_is31fl3736_delete(m_led_handle, false);
    m_led_handle = NULL;
}

esp_err_t CIs31fl3736::fill_matrix(uint8_t duty, uint8_t* buf)
{
    return iot_is31fl3736_fill_buf(m_led_handle, duty, buf);
}

esp_err_t CIs31fl3736::write_reg(uint8_t reg_addr, uint8_t *data, uint8_t data_num)
{
    return iot_is31fl3736_write(m_led_handle, reg_addr, data, data_num);
}

esp_err_t CIs31fl3736::set_page(uint8_t page_num)
{
    return iot_is31fl3736_write_page(m_led_handle, page_num);
}
