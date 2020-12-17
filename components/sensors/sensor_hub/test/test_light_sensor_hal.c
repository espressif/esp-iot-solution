// Copyright 2020-2021 Espressif Systems (Shanghai) PTE LTD
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
#include "hal/light_sensor_hal.h"
#include "esp_system.h"

#define I2C_MASTER_SCL_IO           22          /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO           21          /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_1   /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */

static i2c_bus_handle_t i2c_bus = NULL;
static sensor_light_handle_t light_sensor_handle = NULL;
static light_sensor_id_t LIGHT_SENSOR_ID_TEST = BH1750_ID;
//static light_sensor_id_t LIGHT_SENSOR_ID_TEST = VEML6040_ID;
//static light_sensor_id_t LIGHT_SENSOR_ID_TEST = VEML6075_ID;

static void light_sensor_test_get_data()
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
    light_sensor_handle = light_sensor_create(i2c_bus, LIGHT_SENSOR_ID_TEST);
    TEST_ASSERT(NULL != light_sensor_handle);
    vTaskDelay(100 / portTICK_RATE_MS);
    TEST_ASSERT(ESP_OK == light_sensor_test(light_sensor_handle));

    while (cnt--) {
        float light = 0.0;
        uv_t uv = {0};
        rgbw_t rgbw = {0};
        printf("\n************* light sensor ************\n");
        if ( ESP_OK == light_sensor_acquire_light(light_sensor_handle, &light))
        printf("light illuminance:%.2f\n", light);
        if ( ESP_OK == light_sensor_acquire_rgbw(light_sensor_handle, &rgbw))
        printf("red:%.2f green:%.2f blue:%.2f white:%.2f\n", rgbw.r, rgbw.g, rgbw.b, rgbw.w);
        if ( ESP_OK == light_sensor_acquire_uv(light_sensor_handle, &uv))
        printf("UV:%.2f UVA:%.2f UVB:%.2f\n", uv.uv, uv.uva, uv.uvb);
        printf("**************************************************\n");
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
    light_sensor_delete(&light_sensor_handle);
    TEST_ASSERT(NULL == light_sensor_handle);
    i2c_bus_delete(&i2c_bus);
    TEST_ASSERT(NULL == i2c_bus);
}

TEST_CASE("Sensor light sensor hal test get data [1000ms]", "[light_sensor_hal][iot][sensor_hal]")
{
    light_sensor_test_get_data();
}
