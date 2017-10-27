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
#include "i2c_bus.h"
#include "at24c02.h"

typedef struct {
    i2c_bus_handle_t bus;
    uint16_t dev_addr;
} at24c02_dev_t;

at24c02_handle_t dev_at24c02_create(i2c_bus_handle_t bus, uint16_t dev_addr)
{
    at24c02_dev_t* dev = (at24c02_dev_t*) calloc(1, sizeof(at24c02_dev_t));
    dev->bus = bus;
    dev->dev_addr = dev_addr;
    return (at24c02_handle_t) dev;
}

esp_err_t dev_at24c02_delete(at24c02_handle_t dev, bool del_bus)
{
    at24c02_dev_t* device = (at24c02_dev_t*) dev;
    if (del_bus) {
        i2c_bus_delete(device->bus);
        device->bus = NULL;
    }
    free(device);
    return ESP_OK;
}

esp_err_t at24c02_write_byte(at24c02_handle_t dev, uint8_t addr,
        uint8_t data)
{
    //start-device_addr-word_addr-data-stop
    esp_err_t ret;
    at24c02_dev_t* device = (at24c02_dev_t*) dev;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device->dev_addr << 1) | WRITE_BIT,
            ACK_CHECK_EN);
    i2c_master_write_byte(cmd, addr, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, data, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = i2c_bus_cmd_begin(device->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t at24c02_write(at24c02_handle_t dev, uint8_t start_addr,
        uint8_t write_num, uint8_t *data_buf)
{
    uint32_t i = 0;
    i2c_cmd_handle_t cmd;
    esp_err_t ret = ESP_FAIL;
    uint32_t writeNum = write_num;
    at24c02_dev_t* device = (at24c02_dev_t*) dev;
    if (data_buf != NULL) {
        for (uint32_t j = 0; j < write_num; j += 8) {
            cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (device->dev_addr << 1) | WRITE_BIT,
                    ACK_CHECK_EN);
            i2c_master_write_byte(cmd, start_addr, ACK_CHECK_EN);
            for (i = j; i < ((writeNum >= 8) ? 8 : writeNum); i++) {
                i2c_master_write_byte(cmd, data_buf[i], ACK_CHECK_EN);
                vTaskDelay(100 / portTICK_RATE_MS);
            }
            i2c_master_stop(cmd);
            ret = i2c_bus_cmd_begin(device->bus, cmd, 1000 / portTICK_RATE_MS);
            i2c_cmd_link_delete(cmd);

            writeNum -= 8;              //write num count
            if (ret == ESP_FAIL) {
                return ret;
            }
        }
    }
    return ret;
}

esp_err_t at24c02_read_byte(at24c02_handle_t dev, uint8_t addr,
        uint8_t *data)
{
    //start-device_addr-word_addr-start-device_addr-data-stop; no_ack of end data
    esp_err_t ret;
    at24c02_dev_t* device = (at24c02_dev_t*) dev;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device->dev_addr << 1) | WRITE_BIT,
            ACK_CHECK_EN);
    i2c_master_write_byte(cmd, addr, ACK_CHECK_EN);

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device->dev_addr << 1) | READ_BIT,
            ACK_CHECK_EN);
    i2c_master_read_byte(cmd, data, NACK_VAL);
    i2c_master_stop(cmd);
    ret = i2c_bus_cmd_begin(device->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t at24c02_read(at24c02_handle_t dev, uint8_t start_addr,
        uint8_t read_num, uint8_t *data_buf)
{
    uint32_t i = 0;
    esp_err_t ret = ESP_FAIL;
    if (data_buf != NULL) {
        at24c02_dev_t* device = (at24c02_dev_t*) dev;
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (device->dev_addr << 1) | WRITE_BIT,
                ACK_CHECK_EN);
        i2c_master_write_byte(cmd, start_addr, ACK_CHECK_EN);

        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (device->dev_addr << 1) | READ_BIT,
                ACK_CHECK_EN);
        for (i = 0; i < read_num - 1; i++) {
            i2c_master_read_byte(cmd, &data_buf[i], ACK_VAL);
        }
        i2c_master_read_byte(cmd, &data_buf[i], NACK_VAL);
        i2c_master_stop(cmd);
        ret = i2c_bus_cmd_begin(device->bus, cmd, 1000 / portTICK_RATE_MS);
        i2c_cmd_link_delete(cmd);
    }
    return ret;
}

