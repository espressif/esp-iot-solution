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
#include "driver/i2c.h"
#include "esp_system.h"
#include "esp_log.h"
#include "mpu6050.h"

static const char *TAG = "MPU6050";

#define WRITE_BIT  I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT   I2C_MASTER_READ  /*!< I2C master read */
#define ACK_CHECK_EN   0x1     /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  0x0     /*!< I2C master will not check ack from slave */
#define ACK_VAL    0x0         /*!< I2C ack value */
#define NACK_VAL   0x1         /*!< I2C nack value */

#define ALPHA 0.99             /*!< Weight for gyroscope */
#define RAD_TO_DEG 57.27272727 /*!< Radians to degrees */

typedef struct {
    i2c_bus_device_handle_t i2c_dev;
    uint8_t dev_addr;
    uint32_t counter;
    float dt;  /*!< delay time between twice measurement, dt should be small (ms level) */
    struct timeval *timer;
} mpu6050_dev_t;

mpu6050_handle_t mpu6050_create(i2c_bus_handle_t bus, uint8_t dev_addr)
{
    if (bus == NULL) {
        return NULL;
    }

    mpu6050_dev_t *sens = (mpu6050_dev_t *) calloc(1, sizeof(mpu6050_dev_t));
    sens->i2c_dev = i2c_bus_device_create(bus, dev_addr, i2c_bus_get_current_clk_speed(bus));
    if (sens->i2c_dev == NULL) {
        free(sens);
        return NULL;
    }
    sens->dev_addr = dev_addr;
    sens->counter = 0;
    sens->dt = 0;
    sens->timer = (struct timeval *) calloc(1, sizeof(struct timeval));
    return (mpu6050_handle_t) sens;
}

esp_err_t mpu6050_delete(mpu6050_handle_t *sensor)
{
    if (*sensor == NULL) {
        return ESP_OK;
    }

    mpu6050_dev_t *sens = (mpu6050_dev_t *)(*sensor);
    i2c_bus_device_delete(&sens->i2c_dev);
    free(sens->timer);
    free(sens);
    *sensor = NULL;
    return ESP_OK;
}

esp_err_t mpu6050_get_deviceid(mpu6050_handle_t sensor, uint8_t *deviceid)
{
    mpu6050_dev_t *sens = (mpu6050_dev_t *) sensor;
    esp_err_t ret;
    uint8_t tmp;
    ret = i2c_bus_read_byte(sens->i2c_dev, MPU6050_WHO_AM_I, &tmp);
    *deviceid = tmp;
    return ret;
}

esp_err_t mpu6050_wake_up(mpu6050_handle_t sensor)
{
    mpu6050_dev_t *sens = (mpu6050_dev_t *) sensor;
    esp_err_t ret;
    uint8_t tmp;
    ret = i2c_bus_read_byte(sens->i2c_dev, MPU6050_PWR_MGMT_1, &tmp);

    if (ret != ESP_OK) {
        return ret;
    }

    tmp &= (~BIT6);
    ret = i2c_bus_write_byte(sens->i2c_dev, MPU6050_PWR_MGMT_1, tmp);
    return ret;
}

esp_err_t mpu6050_sleep(mpu6050_handle_t sensor)
{
    mpu6050_dev_t *sens = (mpu6050_dev_t *) sensor;
    esp_err_t ret;
    uint8_t tmp;
    ret = i2c_bus_read_byte(sens->i2c_dev, MPU6050_PWR_MGMT_1, &tmp);

    if (ret != ESP_OK) {
        return ret;
    }

    tmp |= BIT6;
    ret = i2c_bus_write_byte(sens->i2c_dev, MPU6050_PWR_MGMT_1, tmp);
    return ret;
}

esp_err_t mpu6050_set_acce_fs(mpu6050_handle_t sensor, mpu6050_acce_fs_t acce_fs)
{
    mpu6050_dev_t *sens = (mpu6050_dev_t *) sensor;
    esp_err_t ret;
    uint8_t tmp;
    ret = i2c_bus_read_byte(sens->i2c_dev, MPU6050_ACCEL_CONFIG, &tmp);

    if (ret != ESP_OK) {
        return ret;
    }

    tmp &= (~BIT3);
    tmp &= (~BIT4);
    tmp |= (acce_fs << 3);
    ret = i2c_bus_write_byte(sens->i2c_dev, MPU6050_ACCEL_CONFIG, tmp);
    return ret;
}

esp_err_t mpu6050_set_gyro_fs(mpu6050_handle_t sensor, mpu6050_gyro_fs_t gyro_fs)
{
    mpu6050_dev_t *sens = (mpu6050_dev_t *) sensor;
    esp_err_t ret;
    uint8_t tmp;
    ret = i2c_bus_read_byte(sens->i2c_dev, MPU6050_GYRO_CONFIG, &tmp);

    if (ret != ESP_OK) {
        return ret;
    }

    tmp &= (~BIT3);
    tmp &= (~BIT4);
    tmp |= (gyro_fs << 3);
    ret = i2c_bus_write_byte(sens->i2c_dev, MPU6050_GYRO_CONFIG, tmp);
    return ret;
}

esp_err_t mpu6050_get_acce_fs(mpu6050_handle_t sensor, mpu6050_acce_fs_t *acce_fs)
{
    mpu6050_dev_t *sens = (mpu6050_dev_t *) sensor;
    esp_err_t ret;
    uint8_t tmp;
    ret = i2c_bus_read_byte(sens->i2c_dev, MPU6050_ACCEL_CONFIG, &tmp);
    tmp = (tmp >> 3) & 0x03;
    *acce_fs = tmp;
    return ret;
}

esp_err_t mpu6050_get_gyro_fs(mpu6050_handle_t sensor, mpu6050_gyro_fs_t *gyro_fs)
{
    mpu6050_dev_t *sens = (mpu6050_dev_t *) sensor;
    esp_err_t ret;
    uint8_t tmp;
    ret = i2c_bus_read_byte(sens->i2c_dev, MPU6050_GYRO_CONFIG, &tmp);
    tmp = (tmp >> 3) & 0x03;
    *gyro_fs = tmp;
    return ret;
}

esp_err_t mpu6050_get_acce_sensitivity(mpu6050_handle_t sensor, float *acce_sensitivity)
{
    mpu6050_dev_t *sens = (mpu6050_dev_t *) sensor;
    esp_err_t ret;
    uint8_t acce_fs;
    ret = i2c_bus_read_byte(sens->i2c_dev, MPU6050_ACCEL_CONFIG, &acce_fs);
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

esp_err_t mpu6050_get_gyro_sensitivity(mpu6050_handle_t sensor, float *gyro_sensitivity)
{
    mpu6050_dev_t *sens = (mpu6050_dev_t *) sensor;
    esp_err_t ret;
    uint8_t gyro_fs;
    ret = i2c_bus_read_byte(sens->i2c_dev, MPU6050_ACCEL_CONFIG, &gyro_fs);
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

esp_err_t mpu6050_get_raw_acce(mpu6050_handle_t sensor, mpu6050_raw_acce_value_t *raw_acce_value)
{
    mpu6050_dev_t *sens = (mpu6050_dev_t *) sensor;
    uint8_t data_rd[6] = {0};
    esp_err_t ret = i2c_bus_read_bytes(sens->i2c_dev, MPU6050_ACCEL_XOUT_H, 6, data_rd);
    raw_acce_value->raw_acce_x = (int16_t)((data_rd[0] << 8) + (data_rd[1]));
    raw_acce_value->raw_acce_y = (int16_t)((data_rd[2] << 8) + (data_rd[3]));
    raw_acce_value->raw_acce_z = (int16_t)((data_rd[4] << 8) + (data_rd[5]));
    return ret;
}

esp_err_t mpu6050_get_raw_gyro(mpu6050_handle_t sensor, mpu6050_raw_gyro_value_t *raw_gyro_value)
{
    mpu6050_dev_t *sens = (mpu6050_dev_t *) sensor;
    uint8_t data_rd[6] = {0};
    esp_err_t ret = i2c_bus_read_bytes(sens->i2c_dev, MPU6050_GYRO_XOUT_H, 6, data_rd);
    raw_gyro_value->raw_gyro_x = (int16_t)((data_rd[0] << 8) + (data_rd[1]));
    raw_gyro_value->raw_gyro_y = (int16_t)((data_rd[2] << 8) + (data_rd[3]));
    raw_gyro_value->raw_gyro_z = (int16_t)((data_rd[4] << 8) + (data_rd[5]));
    return ret;
}

esp_err_t mpu6050_get_acce(mpu6050_handle_t sensor, mpu6050_acce_value_t *acce_value)
{
    esp_err_t ret;
    float acce_sensitivity;
    mpu6050_raw_acce_value_t raw_acce;
    ret = mpu6050_get_acce_sensitivity(sensor, &acce_sensitivity);

    if (ret != ESP_OK) {
        return ret;
    }

    ret = mpu6050_get_raw_acce(sensor, &raw_acce);

    if (ret != ESP_OK) {
        return ret;
    }

    acce_value->acce_x = raw_acce.raw_acce_x / acce_sensitivity;
    acce_value->acce_y = raw_acce.raw_acce_y / acce_sensitivity;
    acce_value->acce_z = raw_acce.raw_acce_z / acce_sensitivity;
    return ESP_OK;
}

esp_err_t mpu6050_get_gyro(mpu6050_handle_t sensor, mpu6050_gyro_value_t *gyro_value)
{
    esp_err_t ret;
    float gyro_sensitivity;
    mpu6050_raw_gyro_value_t raw_gyro;
    ret = mpu6050_get_gyro_sensitivity(sensor, &gyro_sensitivity);

    if (ret != ESP_OK) {
        return ret;
    }

    ret = mpu6050_get_raw_gyro(sensor, &raw_gyro);

    if (ret != ESP_OK) {
        return ret;
    }

    gyro_value->gyro_x = raw_gyro.raw_gyro_x / gyro_sensitivity;
    gyro_value->gyro_y = raw_gyro.raw_gyro_y / gyro_sensitivity;
    gyro_value->gyro_z = raw_gyro.raw_gyro_z / gyro_sensitivity;
    return ESP_OK;
}

esp_err_t mpu6050_complimentory_filter(mpu6050_handle_t sensor, mpu6050_acce_value_t *acce_value,
        mpu6050_gyro_value_t *gyro_value, complimentary_angle_t *complimentary_angle)
{
    float acce_angle[2];
    float gyro_angle[2];
    float gyro_rate[2];
    mpu6050_dev_t *sens = (mpu6050_dev_t *) sensor;
    sens->counter++;

    if (sens->counter == 1) {
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
    sens->dt = (float)(dt_t.tv_sec) + (float)dt_t.tv_usec / 1000000;
    gettimeofday(sens->timer, NULL);
    acce_angle[0] = (atan2(acce_value->acce_y, acce_value->acce_z) * RAD_TO_DEG);
    acce_angle[1] = (atan2(acce_value->acce_x, acce_value->acce_z) * RAD_TO_DEG);
    gyro_rate[0] = gyro_value->gyro_x;
    gyro_rate[1] = gyro_value->gyro_y;
    gyro_angle[0] = gyro_rate[0] * sens->dt;
    gyro_angle[1] = gyro_rate[1] * sens->dt;
    complimentary_angle->roll = (ALPHA * (complimentary_angle->roll + gyro_angle[0])) + ((1 - ALPHA) * acce_angle[0]);
    complimentary_angle->pitch = (ALPHA * (complimentary_angle->pitch + gyro_angle[1])) + ((1 - ALPHA) * acce_angle[1]);
    return ESP_OK;
}

/***sensors hal interface****/
#ifdef CONFIG_SENSOR_IMU_INCLUDED_MPU6050

static mpu6050_handle_t mpu6050 = NULL;
static bool is_init = false;

esp_err_t imu_mpu6050_init(i2c_bus_handle_t i2c_bus)
{
    if (is_init || !i2c_bus) {
        return ESP_FAIL;
    }

    mpu6050 = mpu6050_create(i2c_bus, MPU6050_I2C_ADDRESS);

    if (!mpu6050) {
        return ESP_FAIL;
    }

    uint8_t mpu6050_deviceid;
    mpu6050_get_deviceid(mpu6050, &mpu6050_deviceid);
    ESP_LOGI(TAG, "mpu6050 device address is: 0x%02x\n", mpu6050_deviceid);
    esp_err_t ret = mpu6050_wake_up(mpu6050);
    ret = mpu6050_set_acce_fs(mpu6050, ACCE_FS_4G);
    ret = mpu6050_set_gyro_fs(mpu6050, GYRO_FS_500DPS);

    if (ret == ESP_OK) {
        is_init = true;
    }

    return ret;
}

esp_err_t imu_mpu6050_deinit(void)
{
    if (!is_init) {
        return ESP_FAIL;
    }

    esp_err_t ret = mpu6050_sleep(mpu6050);
    ret = mpu6050_delete(&mpu6050);

    if (ret == ESP_OK) {
        is_init = false;
    }

    return ret;
}

esp_err_t imu_mpu6050_sleep(void)
{
    if (!is_init) {
        return ESP_FAIL;
    }

    return mpu6050_sleep(mpu6050);
}

esp_err_t imu_mpu6050_wakeup(void)
{
    if (!is_init) {
        return ESP_FAIL;
    }

    return mpu6050_wake_up(mpu6050);
}

esp_err_t imu_mpu6050_test(void)
{
    if (!is_init) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t imu_mpu6050_acquire_gyro(float *gyro_x, float *gyro_y, float *gyro_z)
{
    if (!is_init) {
        return ESP_FAIL;
    }

    mpu6050_gyro_value_t gyro = {0, 0, 0};

    if (gyro_x != NULL && gyro_y != NULL && gyro_z != NULL) {
        if (ESP_OK == mpu6050_get_gyro(mpu6050, &gyro)) {
            *gyro_x = gyro.gyro_x;
            *gyro_y = gyro.gyro_y;
            *gyro_z = gyro.gyro_z;
            return ESP_OK;
        }
    }

    *gyro_x = 0;
    *gyro_y = 0;
    *gyro_z = 0;
    return ESP_FAIL;
}

esp_err_t imu_mpu6050_acquire_acce(float *acce_x, float *acce_y, float *acce_z)
{
    if (!is_init) {
        return ESP_FAIL;
    }

    mpu6050_acce_value_t acce = {0, 0, 0};

    if (acce_x != NULL && acce_y != NULL && acce_z != NULL) {
        if (ESP_OK == mpu6050_get_acce(mpu6050, &acce)) {
            *acce_x = acce.acce_x;
            *acce_y = acce.acce_y;
            *acce_z = acce.acce_z;
            return ESP_OK;
        }
    }

    *acce_x = 0;
    *acce_y = 0;
    *acce_z = 0;
    return ESP_FAIL;
}

#endif