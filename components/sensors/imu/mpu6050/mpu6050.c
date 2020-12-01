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
#include "iot_mpu6050.h"

#define WRITE_BIT  I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT   I2C_MASTER_READ  /*!< I2C master read */
#define ACK_CHECK_EN   0x1     /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  0x0     /*!< I2C master will not check ack from slave */
#define ACK_VAL    0x0         /*!< I2C ack value */
#define NACK_VAL   0x1         /*!< I2C nack value */

#define ALPHA 0.99             /*!< Weight for gyroscope */
#define RAD_TO_DEG 57.27272727 /*!< Radians to degrees */

typedef struct {
    i2c_bus_handle_t bus;
    uint16_t dev_addr;
    uint32_t counter;
    float dt;  /*!< delay time between twice measurement, dt should be small (ms level) */
    struct timeval *timer;
} mpu6050_dev_t;

esp_err_t iot_mpu6050_write_byte(mpu6050_handle_t sensor, uint8_t reg_addr, uint8_t data)
{
    mpu6050_dev_t* sens = (mpu6050_dev_t*) sensor;
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

esp_err_t iot_mpu6050_write(mpu6050_handle_t sensor, uint8_t reg_start_addr, uint8_t reg_num, uint8_t *data_buf)
{
    uint32_t i = 0;
    if (data_buf != NULL) {
        for(i=0; i<reg_num; i++) {
            iot_mpu6050_write_byte(sensor, reg_start_addr+i, data_buf[i]);
        }
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t iot_mpu6050_read_byte(mpu6050_handle_t sensor, uint8_t reg, uint8_t *data)
{
    mpu6050_dev_t* sens = (mpu6050_dev_t*) sensor;
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

esp_err_t iot_mpu6050_read(mpu6050_handle_t sensor, uint8_t reg_start_addr, uint8_t reg_num, uint8_t *data_buf)
{
    uint32_t i = 0;
    uint8_t data_t = 0;
    if (data_buf != NULL) {
        for(i=0; i<reg_num; i++){
            iot_mpu6050_read_byte(sensor, reg_start_addr+i, &data_t);
            data_buf[i] = data_t;
        }
        return ESP_OK;
    } 
    return ESP_FAIL;  
}

mpu6050_handle_t iot_mpu6050_create(i2c_bus_handle_t bus, uint16_t dev_addr)
{
    mpu6050_dev_t* sensor = (mpu6050_dev_t*) calloc(1, sizeof(mpu6050_dev_t));
    sensor->bus = bus;
    sensor->dev_addr = dev_addr;
    sensor->counter = 0;
    sensor->dt = 0;
    sensor->timer = (struct timeval *) calloc(1, sizeof(struct timeval));
    return (mpu6050_handle_t) sensor;
}

esp_err_t iot_mpu6050_delete(mpu6050_handle_t sensor, bool del_bus)
{
    mpu6050_dev_t* sens = (mpu6050_dev_t*) sensor;
    if(del_bus) {
        iot_i2c_bus_delete(sens->bus);
        sens->bus = NULL;
    }
    free(sens);
    return ESP_OK;
}

esp_err_t iot_mpu6050_get_deviceid(mpu6050_handle_t sensor, uint8_t* deviceid)
{
    esp_err_t ret;
    uint8_t tmp;
    ret = iot_mpu6050_read_byte(sensor, MPU6050_WHO_AM_I, &tmp);
    *deviceid = tmp;
    return ret;
}

esp_err_t iot_mpu6050_wake_up(mpu6050_handle_t sensor)
{
    esp_err_t ret;
    uint8_t tmp;
    ret = iot_mpu6050_read_byte(sensor, MPU6050_PWR_MGMT_1, &tmp);
    if (ret == ESP_FAIL) {
        return ret;
    }
    tmp &= (~BIT6);
    ret = iot_mpu6050_write_byte(sensor, MPU6050_PWR_MGMT_1, tmp);
    return ret;
}

esp_err_t iot_mpu6050_sleep(mpu6050_handle_t sensor)
{
    esp_err_t ret;
    uint8_t tmp;
    ret = iot_mpu6050_read_byte(sensor, MPU6050_PWR_MGMT_1, &tmp);
    if (ret == ESP_FAIL) {
        return ret;
    }
    tmp |= BIT6;
    ret = iot_mpu6050_write_byte(sensor, MPU6050_PWR_MGMT_1, tmp);
    return ret;
}

esp_err_t iot_mpu6050_set_acce_fs(mpu6050_handle_t sensor, mpu6050_acce_fs_t acce_fs)
{
    esp_err_t ret;
    uint8_t tmp;
    ret = iot_mpu6050_read_byte(sensor, MPU6050_ACCEL_CONFIG, &tmp);
    if (ret == ESP_FAIL) {
        return ret;
    }
    tmp &= (~BIT3);
    tmp &= (~BIT4);
    tmp |= (acce_fs << 3);
    ret = iot_mpu6050_write_byte(sensor, MPU6050_ACCEL_CONFIG, tmp);
    return ret;
}

esp_err_t iot_mpu6050_set_gyro_fs(mpu6050_handle_t sensor, mpu6050_gyro_fs_t gyro_fs)
{
    esp_err_t ret;
    uint8_t tmp;
    ret = iot_mpu6050_read_byte(sensor, MPU6050_GYRO_CONFIG, &tmp);
    if (ret == ESP_FAIL) {
        return ret;
    }
    tmp &= (~BIT3);
    tmp &= (~BIT4);
    tmp |= (gyro_fs << 3);
    ret = iot_mpu6050_write_byte(sensor, MPU6050_GYRO_CONFIG, tmp);
    return ret;
}

esp_err_t iot_mpu6050_get_acce_fs(mpu6050_handle_t sensor, mpu6050_acce_fs_t *acce_fs)
{
    esp_err_t ret;
    uint8_t tmp;
    ret = iot_mpu6050_read_byte(sensor, MPU6050_ACCEL_CONFIG, &tmp);
    tmp = (tmp >> 3) & 0x03;
    *acce_fs = tmp;
    return ret;
}

esp_err_t iot_mpu6050_get_gyro_fs(mpu6050_handle_t sensor, mpu6050_gyro_fs_t *gyro_fs)
{
    esp_err_t ret;
    uint8_t tmp;
    ret = iot_mpu6050_read_byte(sensor, MPU6050_GYRO_CONFIG, &tmp);
    tmp = (tmp >> 3) & 0x03;
    *gyro_fs = tmp;
    return ret;
}

esp_err_t iot_mpu6050_get_acce_sensitivity(mpu6050_handle_t sensor, float *acce_sensitivity)
{
    esp_err_t ret;
    uint8_t acce_fs;
    ret = iot_mpu6050_read_byte(sensor, MPU6050_ACCEL_CONFIG, &acce_fs);
    acce_fs = (acce_fs >> 3) & 0x03;
    switch (acce_fs) {
        case ACCE_FS_2G:
            *acce_sensitivity = 16384;
            break;

        case ACCE_FS_4G:
            *acce_sensitivity = 8192;
            break;

        case ACCE_FS_8G:
            *acce_sensitivity = 4096;
            break;

        case ACCE_FS_16G:
            *acce_sensitivity = 2048;
            break;

        default:
            break;
    }
    return ret;
}

esp_err_t iot_mpu6050_get_gyro_sensitivity(mpu6050_handle_t sensor, float *gyro_sensitivity)
{
    esp_err_t ret;
    uint8_t gyro_fs;
    ret = iot_mpu6050_read_byte(sensor, MPU6050_ACCEL_CONFIG, &gyro_fs);
    gyro_fs = (gyro_fs >> 3) & 0x03;
    switch (gyro_fs) {
        case GYRO_FS_250DPS:
            *gyro_sensitivity = 131;
            break;

        case GYRO_FS_500DPS:
            *gyro_sensitivity = 65.5;
            break;

        case GYRO_FS_1000DPS:
            *gyro_sensitivity = 32.8;
            break;

        case GYRO_FS_2000DPS:
            *gyro_sensitivity = 16.4;
            break;

        default:
            break;
    }
    return ret;
}

esp_err_t iot_mpu6050_get_raw_acce(mpu6050_handle_t sensor, mpu6050_raw_acce_value_t *raw_acce_value)
{
    uint8_t data_rd[6] = {0};
    mpu6050_dev_t* sens = (mpu6050_dev_t*) sensor;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ( MPU6050_I2C_ADDRESS << 1 ) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, MPU6050_ACCEL_XOUT_H, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ( MPU6050_I2C_ADDRESS << 1 ) | READ_BIT, ACK_CHECK_EN);
    i2c_master_read(cmd, data_rd, 5, ACK_VAL);
    i2c_master_read(cmd, data_rd + 5, 1, NACK_VAL);
    i2c_master_stop(cmd);
    int ret = iot_i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    raw_acce_value->raw_acce_x = (int16_t)((data_rd[0] << 8) + (data_rd[1]));
    raw_acce_value->raw_acce_y = (int16_t)((data_rd[2] << 8) + (data_rd[3]));
    raw_acce_value->raw_acce_z = (int16_t)((data_rd[4] << 8) + (data_rd[5]));
    return ret;
}

esp_err_t iot_mpu6050_get_raw_gyro(mpu6050_handle_t sensor, mpu6050_raw_gyro_value_t *raw_gyro_value)
{
    uint8_t data_rd[6] = {0};
    mpu6050_dev_t* sens = (mpu6050_dev_t*) sensor;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ( MPU6050_I2C_ADDRESS << 1 ) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, MPU6050_GYRO_XOUT_H, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ( MPU6050_I2C_ADDRESS << 1 ) | READ_BIT, ACK_CHECK_EN);
    i2c_master_read(cmd, data_rd, 5, ACK_VAL);
    i2c_master_read(cmd, data_rd + 5, 1, NACK_VAL);
    i2c_master_stop(cmd);
    int ret = iot_i2c_bus_cmd_begin(sens->bus, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    raw_gyro_value->raw_gyro_x = (int16_t)((data_rd[0] << 8) + (data_rd[1]));
    raw_gyro_value->raw_gyro_y = (int16_t)((data_rd[2] << 8) + (data_rd[3]));
    raw_gyro_value->raw_gyro_z = (int16_t)((data_rd[4] << 8) + (data_rd[5]));
    return ret;
}

esp_err_t iot_mpu6050_get_acce(mpu6050_handle_t sensor, mpu6050_acce_value_t *acce_value)
{
    esp_err_t ret;
    float acce_sensitivity;
    mpu6050_raw_acce_value_t raw_acce;

    ret = iot_mpu6050_get_acce_sensitivity(sensor, &acce_sensitivity);
    if (ret != ESP_OK) return ret;
    ret = iot_mpu6050_get_raw_acce(sensor, &raw_acce);
    if (ret != ESP_OK) return ret;

    acce_value->acce_x = raw_acce.raw_acce_x / acce_sensitivity;
    acce_value->acce_y = raw_acce.raw_acce_y / acce_sensitivity;
    acce_value->acce_z = raw_acce.raw_acce_z / acce_sensitivity;
    return ESP_OK;
}

esp_err_t iot_mpu6050_get_gyro(mpu6050_handle_t sensor, mpu6050_gyro_value_t *gyro_value)
{
    esp_err_t ret;
    float gyro_sensitivity;
    mpu6050_raw_gyro_value_t raw_gyro;

    ret = iot_mpu6050_get_gyro_sensitivity(sensor, &gyro_sensitivity);
    if (ret != ESP_OK) return ret;
    ret = iot_mpu6050_get_raw_gyro(sensor, &raw_gyro);
    if (ret != ESP_OK) return ret;

    gyro_value->gyro_x = raw_gyro.raw_gyro_x / gyro_sensitivity;
    gyro_value->gyro_y = raw_gyro.raw_gyro_y / gyro_sensitivity;
    gyro_value->gyro_z = raw_gyro.raw_gyro_z / gyro_sensitivity;
    return ESP_OK;
}

esp_err_t iot_mpu6050_complimentory_filter(mpu6050_handle_t sensor, mpu6050_acce_value_t *acce_value, 
                                   mpu6050_gyro_value_t *gyro_value, complimentary_angle_t *complimentary_angle)
{
    float acce_angle[2];
    float gyro_angle[2];
    float gyro_rate[2];
    mpu6050_dev_t* sens = (mpu6050_dev_t*) sensor;
    
    sens->counter++;
    if(sens->counter == 1) {
        acce_angle[0] = (atan2(acce_value->acce_y, acce_value->acce_z) * RAD_TO_DEG);
        acce_angle[1] = (atan2(acce_value->acce_x, acce_value->acce_z) * RAD_TO_DEG);
        complimentary_angle->roll = acce_angle[0];
        complimentary_angle->pitch = acce_angle[1];
        gettimeofday(sens->timer, NULL);
        return ESP_OK;
    }

    struct timeval now, dt_t;
    gettimeofday(&now, NULL);
    timersub(&now, sens->timer, &dt_t);
    sens->dt = (float) (dt_t.tv_sec) + (float)dt_t.tv_usec / 1000000;
    gettimeofday(sens->timer, NULL);

    acce_angle[0] = (atan2(acce_value->acce_y, acce_value->acce_z) * RAD_TO_DEG);
    acce_angle[1] = (atan2(acce_value->acce_x, acce_value->acce_z) * RAD_TO_DEG);

    gyro_rate[0] = gyro_value->gyro_x;
    gyro_rate[1] = gyro_value->gyro_y;
    gyro_angle[0] = gyro_rate[0] * sens->dt;
    gyro_angle[1] = gyro_rate[1] * sens->dt;

    complimentary_angle->roll = (ALPHA * (complimentary_angle->roll + gyro_angle[0])) + ((1-ALPHA) * acce_angle[0]);
    complimentary_angle->pitch = (ALPHA * (complimentary_angle->pitch + gyro_angle[1])) + ((1-ALPHA) * acce_angle[1]);

    return ESP_OK;
}

