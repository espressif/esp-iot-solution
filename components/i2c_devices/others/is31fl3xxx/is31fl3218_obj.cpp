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

