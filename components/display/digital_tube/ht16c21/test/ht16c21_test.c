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
#include "ht16c21.h"
#include "i2c_bus.h"
#include "unity.h"

#define I2C_MASTER_NUM     0
#define I2C_MASTER_SDA_IO  21
#define I2C_MASTER_SCL_IO  19
#define I2C_MASTER_FREQ_HZ 100000


TEST_CASE("I2C HT16C21 test", "[ht16c21][iot][led]")
{
    i2c_bus_handle_t i2c_bus = NULL;
    ht16c21_handle_t seg = NULL;
    uint8_t lcd_data[8] = { 0x10, 0x20, 0x30, 0x50, 0x60, 0x70, 0x80 };
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
    seg = ht16c21_create(i2c_bus, HT16C21_I2C_ADDRESS_DEFAULT);
    TEST_ASSERT_NOT_NULL(seg);
    
    ht16c21_config_t ht16c21_conf = {
        .duty_bias = HT16C21_4DUTY_3BIAS,
        .oscillator_display = HT16C21_OSCILLATOR_ON_DISPLAY_ON,
        .frame_frequency = HT16C21_FRAME_160HZ,
        .blinking_frequency = HT16C21_BLINKING_OFF,
        .pin_and_voltage = HT16C21_VLCD_PIN_VOL_ADJ_ON,
        .adjustment_voltage = 0,
    };
    TEST_ASSERT(ESP_OK == ht16c21_param_config(seg, &ht16c21_conf));
    ht16c21_ram_write(seg, 0x00, lcd_data, 8);

    ht16c21_delete(seg);
    i2c_bus_delete(&i2c_bus);
}

