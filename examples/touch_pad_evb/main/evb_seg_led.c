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
    int seg_idx = 4;

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

void ch450_write_dig(int idx, int val)
{
#if CONFIG_TOUCH_EB_V2
    if (idx > 3) {
        return;
    }
#endif
    iot_ch450_write_num(seg, idx, val);
}

