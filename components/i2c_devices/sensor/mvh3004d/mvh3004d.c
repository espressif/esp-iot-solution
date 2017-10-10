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
#include "esp_log.h"
#include "driver/i2c.h"
#include "mvh3004d.h"
#include "i2c_bus.h"

#define WRITE_BIT  I2C_MASTER_WRITE  /*!< I2C master write */
#define READ_BIT   I2C_MASTER_READ   /*!< I2C master read */
#define ACK_CHECK_EN   0x1           /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  0x0           /*!< I2C master will not check ack from slave */
#define ACK_VAL    0x0               /*!< I2C ack value */
#define NACK_VAL   0x1               /*!< I2C nack value */

static const char* TAG = "mvh3004d";
typedef struct {
    i2c_bus_handle_t bus;
    uint16_t dev_addr;
} mvh3004d_dev_t;

mvh3004d_handle_t sensor_mvh3004d_create(i2c_bus_handle_t bus, uint16_t dev_addr)
{
    mvh3004d_dev_t* sensor = (mvh3004d_dev_t*) calloc(1, sizeof(mvh3004d_dev_t));
    sensor->bus = bus;
    sensor->dev_addr = dev_addr;
    return (mvh3004d_handle_t) sensor;
}

esp_err_t sensor_mvh3004d_delete(mvh3004d_handle_t sensor, bool del_bus)
{
    mvh3004d_dev_t* sens = (mvh3004d_dev_t*) sensor;
    if(del_bus) {
        i2c_bus_delete(sens->bus);
        sens->bus = NULL;
    }
    free(sens);
    return ESP_OK;
}

esp_err_t mvh3004d_get_data(mvh3004d_handle_t sensor, float* tp, float* rh)
{
    mvh3004d_dev_t* sens = (mvh3004d_dev_t*) sensor;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (sens->dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    int ret = i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_FAIL) {
        ESP_LOGE(TAG, "SNED WRITE ERROR\n");
        return ESP_FAIL;
    }
    cmd = i2c_cmd_link_create();
    uint8_t data[4];
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (sens->dev_addr << 1) | READ_BIT, ACK_CHECK_EN);
    i2c_master_read(cmd, data, 3, ACK_VAL);
    i2c_master_read_byte(cmd, &data[3], NACK_VAL);
    i2c_master_stop(cmd);
    ret = i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    vTaskDelay(20 / portTICK_PERIOD_MS);

    if (ret == ESP_FAIL) {
        ESP_LOGE(TAG, "SEND READ ERROR \n");
        return ESP_FAIL;
    }

    if (rh) {
        *rh = (float) (((data[0] << 8) | data[1]) & 0x3fff) * 100 / (16384 - 1);
    }
    if (tp) {
        *tp = ((float) ((data[2] << 6) | data[3] >> 2)) * 165 / 16383 - 40;
    }
    return ESP_OK;
}

esp_err_t mvh3004d_get_huminity(mvh3004d_handle_t sensor, float* rh)
{
    return mvh3004d_get_data(sensor, NULL, rh);
}

esp_err_t mvh3004d_get_temperature(mvh3004d_handle_t sensor, float* tp)
{
    return mvh3004d_get_data(sensor, tp, NULL);
}



