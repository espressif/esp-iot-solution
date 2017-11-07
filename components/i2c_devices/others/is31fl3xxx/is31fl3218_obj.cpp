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
#include "iot_is31fl3218.h"

CIs31fl3218::CIs31fl3218(CI2CBus *p_i2c_bus)
{
    bus = p_i2c_bus;
    m_led_handle = iot_is31fl3218_create(bus->get_bus_handle());
}

CIs31fl3218::~CIs31fl3218()
{
    iot_is31fl3218_delete(m_led_handle, false);
    m_led_handle = NULL;
}

esp_err_t CIs31fl3218::set_duty(uint32_t ch_bit, uint8_t duty)
{
    return iot_is31fl3218_channel_set(m_led_handle, ch_bit, duty);
}

esp_err_t CIs31fl3218::write_duty_regs(uint8_t* duty, int len)
{
    return iot_is31fl3218_write_pwm_regs(m_led_handle, duty, len);
}

