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
#include "unity.h"
#include "driver/i2c.h"
#include "i2c_bus.h"
#include "hal/imu_hal.h"
#include "esp_system.h"

#define I2C_MASTER_SCL_IO           22          /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO           21          /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_1   /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */

static i2c_bus_handle_t i2c_bus = NULL;
static sensor_imu_handle_t imu_handle = NULL;
static imu_id_t IMU_ID_TEST = MPU6050_ID;
//static imu_id_t IMU_ID_TEST = LIS2DH12_ID;

static void imu_test_get_data()
{
    int cnt = 10;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    TEST_ASSERT(NULL != i2c_bus);
    imu_handle = imu_create(i2c_bus, IMU_ID_TEST);
    TEST_ASSERT(NULL != imu_handle);
    vTaskDelay(100 / portTICK_RATE_MS);
    TEST_ASSERT(ESP_OK == imu_test(imu_handle));

    while (cnt--) {
        axis3_t acce = {0};
        axis3_t gyro = {0};
        printf("\n************* imu sensor************\n");
        imu_acquire_acce(imu_handle, &acce);
        printf("acce_x:%.2f, acce_y:%.2f, acce_z:%.2f\n", acce.x, acce.y, acce.z);
        imu_acquire_gyro(imu_handle, &gyro);
        printf("gyro_x:%.2f, gyro_y:%.2f, gyro_z:%.2f\n", gyro.x, gyro.y, gyro.z);
        printf("**************************************************\n");
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
    imu_delete(&imu_handle);
    TEST_ASSERT(NULL == imu_handle);
    i2c_bus_delete(&i2c_bus);
    TEST_ASSERT(NULL == i2c_bus);
}

TEST_CASE("Sensor imu hal test get data [1000ms]", "[imu_hal][iot][sensor_hal]")
{
    imu_test_get_data();
}
