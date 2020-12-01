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
#include "iot_i2c_bus.h"
#include "iot_mpu6050.h"
#include "esp_system.h"

#define I2C_MASTER_SCL_IO           26          /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO           25          /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_1   /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */

static i2c_bus_handle_t i2c_bus = NULL;
static mpu6050_handle_t mpu6050 = NULL;

/**
 * @brief i2c master initialization
 */
void i2c_sensor_mpu6050_init()
{
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_bus = iot_i2c_bus_create(i2c_master_port, &conf);
    mpu6050 = iot_mpu6050_create(i2c_bus, MPU6050_I2C_ADDRESS);
}

void mpu6050_test_task(void* pvParameters)
{
    uint8_t mpu6050_deviceid;
    mpu6050_acce_value_t acce;
    mpu6050_gyro_value_t gyro;
    
    iot_mpu6050_get_deviceid(mpu6050, &mpu6050_deviceid);
    printf("mpu6050 device ID is: 0x%02x\n", mpu6050_deviceid);
    
    iot_mpu6050_wake_up(mpu6050);
    iot_mpu6050_set_acce_fs(mpu6050, ACCE_FS_4G);
    iot_mpu6050_set_gyro_fs(mpu6050, GYRO_FS_500DPS);
    while(1){
        printf("\n************* MPU6050 MOTION SENSOR ************\n");
        iot_mpu6050_get_acce(mpu6050, &acce);
        printf("acce_x:%.2f, acce_y:%.2f, acce_z:%.2f\n", acce.acce_x, acce.acce_y, acce.acce_z);
        iot_mpu6050_get_gyro(mpu6050, &gyro);
        printf("gyro_x:%.2f, gyro_y:%.2f, gyro_z:%.2f\n", gyro.gyro_x, gyro.gyro_y, gyro.gyro_z);
        printf("**************************************************\n");
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

void mpu6050_test()
{
    i2c_sensor_mpu6050_init();
    vTaskDelay(1000 / portTICK_RATE_MS);
    xTaskCreate(mpu6050_test_task, "mpu6050_test_task", 1024 * 2, NULL, 10, NULL);
}

TEST_CASE("Sensor mpu6050 test", "[mpu6050][iot][sensor]")
{
    mpu6050_test();
}

