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
#include "ch450.h"
#include "unity.h"

#define I2C_MASTER_NUM 0
#define I2C_MASTER_SDA_IO 22
#define I2C_MASTER_SCL_IO 23
#define I2C_MASTER_FREQ_HZ 100000


TEST_CASE("I2C CH450 test", "[ch450][iot][led]")
{
    i2c_bus_handle_t i2c_bus = NULL;
    ch450_handle_t seg = NULL;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    TEST_ASSERT_NOT_NULL(i2c_bus);
    seg = ch450_create(i2c_bus);
    TEST_ASSERT_NOT_NULL(seg);

    for (size_t i = 0; i < 10; i++) {
        for (size_t index = 0; index < 6; index++) {
            ch450_write_num(seg, index, i);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    ch450_delete(seg);
    i2c_bus_delete(&i2c_bus);
}

