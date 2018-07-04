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
#include "iot_i2c_bus.h"
#include "iot_ft5x06.h"
#include "esp_log.h"
#include "string.h"

esp_err_t iot_ft5x06_read(ft5x06_handle_t dev, uint8_t start_addr,
        uint8_t read_num, uint8_t *data_buf)
{
    esp_err_t ret = ESP_FAIL;

    if (data_buf != NULL) {
        i2c_cmd_handle_t cmd = NULL;
        ft5x06_dev_t* device = (ft5x06_dev_t*) dev;
        cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (device->dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
        i2c_master_write_byte(cmd, start_addr, ACK_CHECK_EN);
        i2c_master_stop(cmd);
        ret = iot_i2c_bus_cmd_begin(device->bus, cmd, 1000 / portTICK_RATE_MS);
        i2c_cmd_link_delete(cmd);
        if(ret != ESP_OK) {
            return ESP_FAIL;
        }
        cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (device->dev_addr << 1) | READ_BIT, ACK_CHECK_EN);
        if(read_num > 1) {
            i2c_master_read(cmd, data_buf, read_num-1, ACK_VAL);
        }
        i2c_master_read_byte(cmd, &data_buf[read_num-1], NACK_VAL);
        i2c_master_stop(cmd);
        ret = iot_i2c_bus_cmd_begin(device->bus, cmd, 1000 / portTICK_RATE_MS);
        i2c_cmd_link_delete(cmd);
    }
    return ret;
}

esp_err_t iot_ft5x06_write(ft5x06_handle_t dev, uint8_t start_addr,
        uint8_t write_num, uint8_t *data_buf)
{
    esp_err_t ret = ESP_FAIL;
    if (data_buf != NULL) {
        ft5x06_dev_t* device = (ft5x06_dev_t*) dev;
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (device->dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
        i2c_master_write_byte(cmd, start_addr, ACK_CHECK_EN);
        i2c_master_write(cmd, data_buf, write_num, ACK_CHECK_EN);
        i2c_master_stop(cmd);
        ret = iot_i2c_bus_cmd_begin(device->bus, cmd, 1000 / portTICK_RATE_MS);
        i2c_cmd_link_delete(cmd);
    }
    return ret;
}

esp_err_t iot_ft5x06_touch_report(ft5x06_handle_t device, touch_info_t* ifo)
{
    if(device == NULL || ifo == NULL) {
        return ESP_FAIL;
    }
    ft5x06_dev_t* dev = (ft5x06_dev_t*) device;
    uint8_t data[32];
    memset(data, 0, sizeof(data));
    if(iot_ft5x06_read(dev, 0x00, 32, data) == ESP_OK) {
        ifo->touch_point = data[2] & 0x7;
        if(ifo->touch_point > 0 && ifo->touch_point <=5) {
            if(!(dev->xy_swap)) {
                ifo->curx[0] = dev->x_size -(((uint16_t)(data[0x03] & 0x0f) << 8) | data[0x04]);
                ifo->cury[0] = ((uint16_t)(data[0x05] & 0x0f) << 8) | data[0x06];
                ifo->curx[1] = dev->x_size -(((uint16_t)(data[0x09] & 0x0f) << 8) | data[0x0a]);
                ifo->cury[1] = ((uint16_t)(data[0x0b] & 0x0f) << 8) | data[0x0c];
                ifo->curx[2] = dev->x_size -(((uint16_t)(data[0x0f] & 0x0f) << 8) | data[0x10]);
                ifo->cury[2] = ((uint16_t)(data[0x11] & 0x0f) << 8) | data[0x12];
                ifo->curx[3] = dev->x_size -(((uint16_t)(data[0x15] & 0x0f) << 8) | data[0x16]);
                ifo->cury[3] = ((uint16_t)(data[0x17] & 0x0f) << 8) | data[0x18];
                ifo->curx[4] = dev->x_size -(((uint16_t)(data[0x1b] & 0x0f) << 8) | data[0x1c]);
                ifo->cury[4] = ((uint16_t)(data[0x1d] & 0x0f) << 8) | data[0x1e];
            } else {
                ifo->cury[0] = dev->x_size - (((uint16_t)(data[0x03] & 0x0f) << 8) | data[0x04]);
                ifo->curx[0] = ((uint16_t)(data[0x05] & 0x0f) << 8) | data[0x06];
                ifo->cury[1] = dev->x_size - (((uint16_t)(data[0x09] & 0x0f) << 8) | data[0x0a]);
                ifo->curx[1] = ((uint16_t)(data[0x0b] & 0x0f) << 8) | data[0x0c];
                ifo->cury[2] = dev->x_size - (((uint16_t)(data[0x0f] & 0x0f) << 8) | data[0x10]);
                ifo->curx[2] = ((uint16_t)(data[0x11] & 0x0f) << 8) | data[0x12];
                ifo->cury[3] = dev->x_size - (((uint16_t)(data[0x15] & 0x0f) << 8) | data[0x16]);
                ifo->curx[3] = ((uint16_t)(data[0x17] & 0x0f) << 8) | data[0x18];
                ifo->cury[4] = dev->x_size - (((uint16_t)(data[0x1b] & 0x0f) << 8) | data[0x1c]);
                ifo->curx[4] = ((uint16_t)(data[0x1d] & 0x0f) << 8) | data[0x1e];
            }
            ifo->touch_event = TOUCH_EVT_PRESS;
        } else {
            ifo->touch_event = TOUCH_EVT_RELEASE;
            ifo->touch_point = 0;
        }
    } else {
        ifo->touch_event = TOUCH_EVT_RELEASE;
        ifo->touch_point = 0;
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t iot_ft5x06_init(ft5x06_handle_t dev, ft5x06_cfg_t * cfg)
{
    return ESP_OK;
}

ft5x06_handle_t iot_ft5x06_create(i2c_bus_handle_t bus, uint16_t dev_addr)
{
    ft5x06_dev_t* dev = (ft5x06_dev_t*) calloc(1, sizeof(ft5x06_dev_t));
    if(dev == NULL) {
        ESP_LOGI("FT5X06:","dev handle calloc fail\n");
        return NULL;
    }
    dev->bus = bus;
    dev->dev_addr = dev_addr;
    dev->x_size = SCREEN_XSIZE;
    dev->y_size = SCREEN_YSIZE;
    dev->xy_swap = false;
    return (ft5x06_handle_t) dev;
}