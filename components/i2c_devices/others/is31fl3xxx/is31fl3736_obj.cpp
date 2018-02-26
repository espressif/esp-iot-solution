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
