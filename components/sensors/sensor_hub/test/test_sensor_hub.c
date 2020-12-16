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
#include "iot_sensor.h"

#define I2C_MASTER_SCL_IO           22          /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO           21          /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_1   /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */

static i2c_bus_handle_t i2c_bus = NULL;
static sensor_imu_handle_t imu_handle = NULL;
static sensor_id_t SENSOR_ID = SENSOR_MPU6050_ID;
//static sensor_id_t SENSOR_ID = SENSOR_MPU6050_ID;

static void imu_test_get_data()
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    vTaskDelay(100 / portTICK_RATE_MS);

    iot_sensor_config_t sensor_conf = {
        .bus = i2c_bus,
        .mode = MODE_POLLING,
        .min_delay = 200,
        .task_name = NULL                     /**< name of the event loop task; if NULL,a dedicated task is not created for sensor*/
    };

    TEST_ASSERT(ESP_OK == iot_sensor_create(SENSOR_ID, &sensor_conf, &imu_handle));
    TEST_ASSERT(ESP_OK == iot_sensor_start(imu_handle));
    vTaskDelay(10000 / portTICK_RATE_MS);
    TEST_ASSERT(ESP_OK == iot_sensor_stop(imu_handle));
    TEST_ASSERT(ESP_OK == iot_sensor_delete(&imu_handle));
    TEST_ASSERT(NULL == imu_handle);
    TEST_ASSERT(ESP_OK == i2c_bus_delete(&i2c_bus));
    TEST_ASSERT(NULL == i2c_bus);
}

TEST_CASE("SENSOR_HUB test get data [200ms]", "[imu_handle][iot][sensor]")
{
    imu_test_get_data();
}
