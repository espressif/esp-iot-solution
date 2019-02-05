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
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include "esp_system.h"
#include "driver/i2c.h"
#include "iot_i2c_bus.h"
#include "iot_mag3110.h"


#define WRITE_BIT  I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT   I2C_MASTER_READ  /*!< I2C master read */
#define ACK_CHECK_EN   0x1     /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  0x0     /*!< I2C master will not check ack from slave */
#define ACK_VAL    0x0         /*!< I2C ack value */
#define NACK_VAL   0x1         /*!< I2C nack value */


typedef struct {
    i2c_bus_handle_t bus;
    uint16_t dev_addr;
} mag3110_dev_t;

mag3110_handle_t iot_mag3110_create(i2c_bus_handle_t bus, uint16_t dev_addr)
{

    mag3110_dev_t* sensor = (mag3110_dev_t*) calloc(1, sizeof(mag3110_dev_t));
    sensor->bus = bus;
    sensor->dev_addr = dev_addr;

    return (mag3110_handle_t) sensor;

}


esp_err_t iot_mag3110_get_deviceid(mag3110_handle_t sensor, uint8_t* deviceid)
{
    esp_err_t ret;
    uint8_t tmp;
    ret = iot_mag3110_read_byte(sensor, MAG3110_WHO_AM_I, &tmp);
    *deviceid = tmp;
    return ret;
}

esp_err_t iot_mag3110_mode_set(mag3110_handle_t sensor, mag3110_mode_sel_t mode_sel)
{
    esp_err_t ret;
    uint8_t tmp=0;
    ret = iot_mag3110_read_byte(sensor, MAG3110_CTRL_REG1, &tmp);
    if (ret == ESP_FAIL) {
        return ret;
    }

    tmp &= (~BIT0);
    tmp |= mode_sel;
    ret = iot_mag3110_write_byte(sensor, MAG3110_CTRL_REG1, tmp);
    return ret;


}


esp_err_t iot_mag3110_read_byte(mag3110_handle_t sensor, uint8_t reg, uint8_t *data)
{
    mag3110_dev_t* sens = (mag3110_dev_t*) sensor;
    esp_err_t ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (sens->dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = iot_i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_FAIL) {
        return ret;
    }

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (sens->dev_addr << 1) | READ_BIT, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, data, NACK_VAL);
    i2c_master_stop(cmd);
    ret = iot_i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t iot_mag3110_read(mag3110_handle_t sensor, uint8_t reg_start_addr, uint8_t reg_num, uint8_t *data_buf)
{
    uint32_t i = 0;
    uint8_t data_t = 0;
    if (data_buf != NULL) {
        for(i=0; i<reg_num; i++) {
            iot_mag3110_read_byte(sensor, reg_start_addr+i, &data_t);
            data_buf[i] = data_t;
        }
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t iot_mag3110_write_byte(mag3110_handle_t sensor, uint8_t reg_addr, uint8_t data)
{
    mag3110_dev_t* sens = (mag3110_dev_t*) sensor;
    esp_err_t  ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (sens->dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg_addr, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, data, ACK_CHECK_EN);
    ret = iot_i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_FAIL) {
        return ret;
    }
    return ESP_OK;
}

esp_err_t iot_mag3110_write(mag3110_handle_t sensor, uint8_t reg_start_addr, uint8_t reg_num, uint8_t *data_buf)
{
    uint32_t i = 0;
    if (data_buf != NULL) {
        for(i=0; i<reg_num; i++) {
            iot_mag3110_write_byte(sensor, reg_start_addr+i, data_buf[i]);
        }
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t iot_mag3110_get_magneticfield(mag3110_handle_t sensor, mag3110_raw_values_t* magneticfield)
{

    uint8_t data_rd[6] = {0};
    mag3110_dev_t* sens = (mag3110_dev_t*) sensor;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ( MAG3110_I2C_ADDRESS << 1 ) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, MAG3110_OUT_X_MSB, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ( MAG3110_I2C_ADDRESS << 1 ) | READ_BIT, ACK_CHECK_EN);
    i2c_master_read(cmd, data_rd, 5, ACK_VAL);
    i2c_master_read(cmd, data_rd + 5, 1, NACK_VAL);
    i2c_master_stop(cmd);
    int ret = iot_i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    magneticfield->magno_x = (int16_t)((data_rd[0] << 8) + (data_rd[1]));
    magneticfield->magno_y = (int16_t)((data_rd[2] << 8) + (data_rd[3]));
    magneticfield->magno_z = (int16_t)((data_rd[4] << 8) + (data_rd[5]));
    return ret;

}

