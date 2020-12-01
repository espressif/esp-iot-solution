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
#include <string.h>
#include "driver/i2c.h"
#include "esp_log.h"
#include "iot_ch450.h"
#include "iot_i2c_bus.h"

#define	CH450_I2C_ADDR0		0x40
#define	CH450_I2C_MASK		0x3E
#define CH450_GET_KEY	    0x0700

#define CH450_WRITE_BIT    0x00
#define CH450_READ_BIT     0x01
#define CH450_ACK_CHECK_EN 1

typedef void* ch450_handle_t;

typedef struct {
    i2c_bus_handle_t bus;
} ch450_dev_t;

void ch450_init(ch450_handle_t dev)
{
    iot_ch450_write(dev, CH450_SYS, 0x01);
}

ch450_handle_t iot_ch450_create(i2c_bus_handle_t bus)
{
    ch450_dev_t* seg = (ch450_dev_t*) calloc(1, sizeof(ch450_dev_t));
    seg->bus = bus;
    ch450_init(seg);
    return (ch450_handle_t) seg;
}

esp_err_t iot_ch450_delete(ch450_handle_t dev, bool del_bus)
{
    ch450_dev_t* seg = (ch450_dev_t*) dev;
    if(del_bus) {
        iot_i2c_bus_delete(seg->bus);
        seg->bus = NULL;
    }
    free(seg);
    return ESP_OK;
}

esp_err_t iot_ch450_write(ch450_handle_t dev, ch450_cmd_t ch450_cmd, uint8_t val)
{
    ch450_dev_t* seg = (ch450_dev_t*) dev;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (uint8_t) ch450_cmd, CH450_ACK_CHECK_EN);
    i2c_master_write_byte(cmd, val, CH450_ACK_CHECK_EN);
    i2c_master_stop(cmd);
    int ret = iot_i2c_bus_cmd_begin(seg->bus, cmd, portMAX_DELAY);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t iot_ch450_write_num(ch450_handle_t dev, uint8_t seg_idx, uint8_t val)
{
    ch450_cmd_t seg_cmd = CH450_SEG_2 + seg_idx * 2;
    uint8_t wr_val;
    switch(val) {
        case 1:
            wr_val = 0x28;
            break;
        case 2:
            wr_val = 0xcd;
            break;
        case 3:
            wr_val = 0x6d;
            break;
        case 4:
            wr_val = 0x2b;
            break;
        case 5:
            wr_val = 0x67;
            break;
        case 6:
            wr_val = 0xe7;
            break;
        case 7:
            wr_val = 0x2c;
            break;
        case 8:
            wr_val = 0xef;
            break;
        case 9:
            wr_val = 0x6f;
            break;
        case 0:
            wr_val = 0xee;
            break;
        default:
            wr_val = 0;
            break;

    }
    return iot_ch450_write(dev, seg_cmd, wr_val);
}

