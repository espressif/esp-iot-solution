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
#include "driver/i2c.h"
#include "iot_ch450.h"
#include "iot_i2c_bus.h"
#include "unity.h"
#include "driver/gpio.h"

#define I2C_MASTER_NUM 0
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_SCL_IO 19
#define I2C_MASTER_FREQ_HZ 100000

static i2c_bus_handle_t i2c_bus = NULL;
static ch450_handle_t seg = NULL;

void ch450_test()
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_bus = iot_i2c_bus_create(I2C_MASTER_NUM, &conf);
    if(seg ==NULL) {
        seg = iot_ch450_create(i2c_bus);
    }
    int _idx = 0, _val = 0;
    while (1) {
        int idx = ((_idx ++) % 6);
        int val = ((_val ++) % 10);
        iot_ch450_write_num(seg, idx, val);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}


TEST_CASE("I2C CH450 test", "[ch450][iot][led]")
{
    ch450_test();
}

