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

