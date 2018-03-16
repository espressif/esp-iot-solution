/* Touch Sensor Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdio.h>
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "iot_i2c_bus.h"
#include "iot_ch450.h"
#include "evb.h"

static i2c_bus_handle_t i2c_bus = NULL;
static ch450_handle_t seg = NULL;

void ch450_dev_init()
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_bus = iot_i2c_bus_create(I2C_MASTER_NUM, &conf);
    if (seg == NULL) {
        seg = iot_ch450_create(i2c_bus);
    }
    for (int i = 0; i<6;i++) {
        ch450_write_dig(i, -1);
    }
}

void ch450_write_led(int mask, int on_off)
{
    static uint8_t bit_mask = 0;

    int seg_idx = 2;

    uint8_t val = 0;
    if (mask & BIT(3)) {
        val |= BIT(0);
    }
    if (mask & BIT(4)) {
        val |= BIT(1);
    }
    if (mask & BIT(2)) {
        val |= BIT(4);
    }
    if (mask & BIT(1)) {
        val |= BIT(5);
    }
    if (mask & BIT(0)) {
        val |= BIT(6);
    }
    if (mask & BIT(5)) {
        val |= BIT(7);
    }
    if (on_off) {
        bit_mask |= val;
    } else {
        bit_mask &= (~val);
    }
    ch450_cmd_t seg_cmd = CH450_SEG_2 + seg_idx * 2;
    iot_ch450_write(seg, seg_cmd, bit_mask);
}

void ch450_write_dig_reverse(int idx, int num)
{
    uint8_t val = 0;
    switch(num) {
        case 1:
            val = 0x82;
            break;
        case 2:
            val = 0xcd;
            break;
        case 3:
            val = 0xc7;
            break;
        case 4:
            val = 0xa3;
            break;
        case 5:
            val = 0x67;
            break;
        case 6:
            val = 0x6f;
            break;
        case 7:
            val = 0xc2;
            break;
        case 8:
            val = 0xef;
            break;
        case 9:
            val = 0xe7;
            break;
        case 0:
            val = 0xee;
            break;
        default:
            val = 0;
            break;
    }
    ch450_cmd_t seg_cmd = CH450_SEG_2 + idx * 2;
    iot_ch450_write(seg, seg_cmd, val);
}

void ch450_write_dig(int idx, int val)
{
    ch450_write_dig_reverse(idx, val);
}

