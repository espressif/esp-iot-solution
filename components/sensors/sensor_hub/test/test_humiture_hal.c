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
#include "hal/humiture_hal.h"
#include "esp_system.h"

#define I2C_MASTER_SCL_IO           22          /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO           21          /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_1   /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */

static i2c_bus_handle_t i2c_bus = NULL;
static sensor_humiture_handle_t humiture_handle = NULL;
static humiture_id_t HUMITURE_ID_TEST = SHT3X_ID;
//static humiture_id_t HUMITURE_ID_TEST = HTS221_ID;

static void humiture_test_get_data()
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
    humiture_handle = humiture_create(i2c_bus, HUMITURE_ID_TEST);
    TEST_ASSERT(NULL != humiture_handle);
    vTaskDelay(100 / portTICK_RATE_MS);
    TEST_ASSERT(ESP_OK == humiture_test(humiture_handle));

    while (cnt--) {
        float humidity = 0.0;
        float temperature = 0.0;
        printf("\n************* humiture sensor ************\n");
        humiture_acquire_humidity(humiture_handle, &humidity);
        printf("humidity:%.2f\n", humidity);
        humiture_acquire_temperature(humiture_handle, &temperature);
        printf("temperature:%.2f\n", temperature);
        printf("**************************************************\n");
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
    humiture_delete(&humiture_handle);
    TEST_ASSERT(NULL == humiture_handle);
    i2c_bus_delete(&i2c_bus);
    TEST_ASSERT(NULL == i2c_bus);
}

TEST_CASE("Sensor humidity hal test get data [1000ms]", "[humiture_hal][iot][sensor_hal]")
{
    humiture_test_get_data();
}
