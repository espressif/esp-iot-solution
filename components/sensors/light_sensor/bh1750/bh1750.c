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
#include "iot_bh1750.h"
#include "iot_i2c_bus.h"

#define WRITE_BIT  I2C_MASTER_WRITE  /*!< I2C master write */
#define READ_BIT   I2C_MASTER_READ   /*!< I2C master read */
#define ACK_CHECK_EN   0x1           /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  0x0           /*!< I2C master will not check ack from slave */
#define ACK_VAL    0x0               /*!< I2C ack value */
#define NACK_VAL   0x1               /*!< I2C nack value */

#define BH_1750_MEASUREMENT_ACCURACY    1.2    /*!< the typical measurement accuracy of  BH1750 sensor */

#define BH1750_POWER_DOWN        0x00    /*!< Command to set Power Down*/
#define BH1750_POWER_ON          0x01    /*!< Command to set Power On*/
#define BH1750_DATA_REG_RESET    0x07    /*!< Command to reset data register, not acceptable in power down mode*/

typedef struct {
    i2c_bus_handle_t bus;
    uint16_t dev_addr;
} bh1750_dev_t;

bh1750_handle_t iot_bh1750_create(i2c_bus_handle_t bus, uint16_t dev_addr)
{
    bh1750_dev_t* sensor = (bh1750_dev_t*) calloc(1, sizeof(bh1750_dev_t));
    sensor->bus = bus;
    sensor->dev_addr = dev_addr;
    return (bh1750_handle_t) sensor;
}

esp_err_t iot_bh1750_delete(bh1750_handle_t sensor, bool del_bus)
{
    bh1750_dev_t* sens = (bh1750_dev_t*) sensor;
    if(del_bus) {
        iot_i2c_bus_delete(sens->bus);
        sens->bus = NULL;
    }
    free(sens);
    return ESP_OK;
}

esp_err_t iot_bh1750_power_down(bh1750_handle_t sensor)
{    
    bh1750_dev_t* sens = (bh1750_dev_t*) sensor;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (sens->dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, BH1750_POWER_DOWN, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    int ret = iot_i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t iot_bh1750_power_on(bh1750_handle_t sensor)
{    
    bh1750_dev_t* sens = (bh1750_dev_t*) sensor;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (sens->dev_addr) << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, BH1750_POWER_ON, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    int ret = iot_i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t iot_bh1750_reset_data_register(bh1750_handle_t sensor)
{    
    bh1750_dev_t* sens = (bh1750_dev_t*) sensor;
    iot_bh1750_power_on(sensor);
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (sens->dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, BH1750_DATA_REG_RESET, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    int ret = iot_i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t iot_bh1750_change_measure_time(bh1750_handle_t sensor, uint8_t measure_time)
{
    bh1750_dev_t* sens = (bh1750_dev_t*) sensor;
    uint32_t i=0;
    uint8_t buf[2]={0x40, 0x60};
    buf[0] |= measure_time >> 5;
    buf[1] |= measure_time & 0x1F;
    for(i=0; i<2; i++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (sens->dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
        i2c_master_write_byte(cmd, buf[i], ACK_CHECK_EN);
        i2c_master_stop(cmd);
        int ret = iot_i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
        i2c_cmd_link_delete(cmd);
        if (ret == ESP_FAIL) {
            return ret;
        }
    } 
    return ESP_OK;
}

esp_err_t iot_bh1750_set_measure_mode(bh1750_handle_t sensor, bh1750_cmd_measure_t cmd_measure)
{
    bh1750_dev_t* sens = (bh1750_dev_t*) sensor;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (sens->dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, cmd_measure, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    int ret = iot_i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t iot_bh1750_get_data(bh1750_handle_t sensor, float* data)
{
    bh1750_dev_t* sens = (bh1750_dev_t*) sensor;
    uint8_t bh1750_data_h, bh1750_data_l;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (sens->dev_addr << 1) | READ_BIT, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, &bh1750_data_h, ACK_VAL);
    i2c_master_read_byte(cmd, &bh1750_data_l, NACK_VAL);
    i2c_master_stop(cmd);
    int ret = iot_i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_FAIL) {
        return ESP_FAIL;
    }
    *data = (( bh1750_data_h << 8 | bh1750_data_l ) / BH_1750_MEASUREMENT_ACCURACY);
    return ESP_OK;
}

esp_err_t iot_bh1750_get_light_intensity(bh1750_handle_t sensor, bh1750_cmd_measure_t cmd_measure, float* data)
{
    bh1750_dev_t* sens = (bh1750_dev_t*) sensor;
    uint8_t bh1750_data_h, bh1750_data_l;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (sens->dev_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, cmd_measure, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    int ret = iot_i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        return ret;
    }
    if((cmd_measure == BH1750_CONTINUE_4LX_RES)||(cmd_measure == BH1750_ONETIME_4LX_RES)) {
        vTaskDelay(30 / portTICK_RATE_MS);
    } else {
        vTaskDelay(180 / portTICK_RATE_MS);
    }
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (sens->dev_addr << 1) | READ_BIT, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, &bh1750_data_h, ACK_VAL);
    i2c_master_read_byte(cmd, &bh1750_data_l, NACK_VAL);
    i2c_master_stop(cmd);
    ret = iot_i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        return ret;
    }
    *data = (( bh1750_data_h << 8 | bh1750_data_l ) / BH_1750_MEASUREMENT_ACCURACY);
    return ESP_OK;
}
