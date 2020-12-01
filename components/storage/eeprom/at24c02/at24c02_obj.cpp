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
#include "iot_at24c02.h"

CAT24C02::CAT24C02(CI2CBus *p_i2c_bus, uint8_t addr)
{
    bus = p_i2c_bus;
    m_dev_handle = iot_at24c02_create(bus->get_bus_handle(), addr);

}

CAT24C02::~CAT24C02()
{
    iot_at24c02_delete(m_dev_handle, false);
    m_dev_handle = NULL;
}

esp_err_t CAT24C02::write_byte(uint8_t addr, uint8_t data)
{
    return iot_at24c02_write_byte(m_dev_handle, addr, data);
}

esp_err_t CAT24C02::read_byte(uint8_t addr, uint8_t *data)
{
    return iot_at24c02_read_byte(m_dev_handle, addr, data);
}

esp_err_t CAT24C02::write(uint8_t start_addr, uint8_t write_num,
        uint8_t *data_buf)
{
    return iot_at24c02_write(m_dev_handle, start_addr, write_num, data_buf);
}

esp_err_t CAT24C02::read(uint8_t start_addr, uint8_t read_num,
        uint8_t *data_buf)
{
    return iot_at24c02_read(m_dev_handle, start_addr, read_num, data_buf);
}
