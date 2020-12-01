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
#include "iot_ht16c21.h"
#include "iot_i2c_bus.h"

typedef struct {
    i2c_bus_handle_t bus;
    uint16_t dev_addr;
} ht16c21_dev_t;

ht16c21_handle_t iot_ht16c21_create(i2c_bus_handle_t bus, uint8_t dev_addr)
{
    ht16c21_dev_t* seg = (ht16c21_dev_t*) calloc(1, sizeof(ht16c21_dev_t));
    seg->bus = bus;
    seg->dev_addr = dev_addr;
    return (ht16c21_handle_t) seg;
}

esp_err_t iot_ht16c21_delete(ht16c21_handle_t dev, bool del_bus)
{
    ht16c21_dev_t* seg = (ht16c21_dev_t*) dev;
    if (del_bus) {
        iot_i2c_bus_delete(seg->bus);
        seg->bus = NULL;
    }
    free(seg);
    return ESP_OK;
}

esp_err_t iot_ht16c21_write_cmd(ht16c21_handle_t dev, ht16c21_cmd_t hd16c21_cmd, uint8_t val)
{
    ht16c21_dev_t* seg = (ht16c21_dev_t*) dev;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (seg->dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, (uint8_t) hd16c21_cmd, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, val, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    int ret = iot_i2c_bus_cmd_begin(seg->bus, cmd, portMAX_DELAY);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t iot_ht16c21_ram_write_byte(ht16c21_handle_t dev, uint8_t address, uint8_t buf)
{
    return iot_ht16c21_ram_write(dev, address, &buf, 1);
}

esp_err_t iot_ht16c21_ram_write(ht16c21_handle_t dev, uint8_t address, uint8_t *buf, uint8_t len)
{
    ht16c21_dev_t* seg = (ht16c21_dev_t*) dev;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (seg->dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, HT16C21_CMD_IOOUT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, address, ACK_CHECK_EN);
    i2c_master_write(cmd, buf, len, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    int ret = iot_i2c_bus_cmd_begin(seg->bus, cmd, portMAX_DELAY);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t iot_ht16c21_ram_read_byte(ht16c21_handle_t dev, uint8_t address, uint8_t *data)
{
    return iot_ht16c21_ram_read(dev, address, data, 1);
}

esp_err_t iot_ht16c21_ram_read(ht16c21_handle_t dev, uint8_t address, uint8_t *buf, uint8_t len)
{
    ht16c21_dev_t* seg = (ht16c21_dev_t*) dev;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (seg->dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, HT16C21_CMD_IOOUT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, address, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_read(cmd, buf,len, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    int ret = iot_i2c_bus_cmd_begin(seg->bus, cmd, portMAX_DELAY);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t iot_ht16c21_init(ht16c21_handle_t dev, ht16c21_config_t*  ht16c21_conf)
{
    iot_ht16c21_write_cmd(dev,HT16C21_CMD_DRIMO, ht16c21_conf->duty_bias);
    iot_ht16c21_write_cmd(dev,HT16C21_CMD_SYSMO, ht16c21_conf->oscillator_display);
    iot_ht16c21_write_cmd(dev,HT16C21_CMD_FRAME, ht16c21_conf->frame_frequency);
    iot_ht16c21_write_cmd(dev,HT16C21_CMD_BLINK, ht16c21_conf->blinking_frequency);
    return iot_ht16c21_write_cmd(dev,HT16C21_CMD_IVA, ht16c21_conf->pin_and_voltage|ht16c21_conf->adjustment_voltage);
}
