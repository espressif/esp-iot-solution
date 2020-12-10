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
#include "at24c02.h"
#include "esp_log.h"

static const char *TAG = "at24c02";

typedef struct {
    i2c_bus_device_handle_t i2c_dev;
    uint8_t dev_addr;
} at24c02_dev_t;

at24c02_handle_t at24c02_create(i2c_bus_handle_t bus, uint8_t dev_addr)
{
    if (bus == NULL) {
        return NULL;
    }
    at24c02_dev_t *dev = (at24c02_dev_t *) calloc(1, sizeof(at24c02_dev_t));
    dev->i2c_dev = i2c_bus_device_create(bus, dev_addr, i2c_bus_get_current_clk_speed(bus));
    if (dev->i2c_dev == NULL) {
        free(dev);
        return NULL;
    }
    dev->dev_addr = dev_addr;
    return (at24c02_handle_t) dev;
}

esp_err_t at24c02_delete(at24c02_handle_t *device)
{
    if ( device == NULL || *device == NULL) {
        return ESP_FAIL;
    }
    at24c02_dev_t *dev = (at24c02_dev_t *)(*device);
    i2c_bus_device_delete(&dev->i2c_dev);
    free(dev);
    *device = NULL;
    return ESP_OK;
}

esp_err_t at24c02_write_byte(at24c02_handle_t device, uint8_t addr, uint8_t data)
{
    at24c02_dev_t *dev = (at24c02_dev_t *) device;
    return i2c_bus_write_byte(dev->i2c_dev, addr, data);
}

esp_err_t at24c02_write(at24c02_handle_t device, uint8_t start_addr, uint8_t write_num, uint8_t *data_buf)
{
    at24c02_dev_t *dev = (at24c02_dev_t *) device;

    if (write_num > 8) {
        ESP_LOGW(TAG, "must write_num <= 8, extra bytes will overwrite from byte 0 in this 8B page");
    }

    return i2c_bus_write_bytes(dev->i2c_dev, start_addr, write_num, data_buf);
}

esp_err_t at24c02_read_byte(at24c02_handle_t device, uint8_t addr, uint8_t *data)
{
    at24c02_dev_t *dev = (at24c02_dev_t *) device;
    return i2c_bus_read_byte(dev->i2c_dev, addr, data);
}

esp_err_t at24c02_read(at24c02_handle_t device, uint8_t start_addr, uint8_t read_num, uint8_t *data_buf)
{
    at24c02_dev_t *dev = (at24c02_dev_t *) device;
    return i2c_bus_read_bytes(dev->i2c_dev, start_addr, read_num, data_buf);
}

esp_err_t at24c02_write_bit(at24c02_handle_t device, uint8_t addr, uint8_t bit_num, uint8_t data)
{
    at24c02_dev_t *dev = (at24c02_dev_t *) device;
    return i2c_bus_write_bit(dev->i2c_dev, addr, bit_num, data);
}

esp_err_t at24c02_write_bits(at24c02_handle_t device, uint8_t addr, uint8_t bit_start, uint8_t length, uint8_t data)
{
    at24c02_dev_t *dev = (at24c02_dev_t *) device;
    return i2c_bus_write_bits(dev->i2c_dev, addr, bit_start, length, data);
}

esp_err_t at24c02_read_bit(at24c02_handle_t device, uint8_t addr, uint8_t bit_num, uint8_t *data)
{
    at24c02_dev_t *dev = (at24c02_dev_t *) device;
    return i2c_bus_read_bit(dev->i2c_dev, addr, bit_num, data);
}

esp_err_t at24c02_read_bits(at24c02_handle_t device, uint8_t addr, uint8_t bit_start, uint8_t length, uint8_t *data)
{
    at24c02_dev_t *dev = (at24c02_dev_t *) device;
    return i2c_bus_read_bits(dev->i2c_dev, addr, bit_start, length, data);
}
