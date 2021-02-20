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
#include "esp_log.h"
#include "driver/i2c.h"
#include "bme280.h"
#include "i2c_bus.h"

#define I2C_MASTER_SCL_IO           22          /*!< gpio number for I2C master clock IO21*/
#define I2C_MASTER_SDA_IO           21          /*!< gpio number for I2C master data  IO15*/
#define I2C_MASTER_NUM              I2C_NUM_1   /*!< I2C port number for master bme280 */
#define I2C_MASTER_TX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */

static i2c_bus_handle_t i2c_bus = NULL;
static bme280_handle_t bme280 = NULL;

void bme280_test_init()
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
    bme280 = bme280_create(i2c_bus, BME280_I2C_ADDRESS_DEFAULT);
    ESP_LOGI("BME280:", "bme280_default_init:%d", bme280_default_init(bme280));
}

void bme280_test_deinit()
{
    bme280_delete(&bme280);
    i2c_bus_delete(&i2c_bus);
}

void bme280_test_getdata()
{
    int cnt = 10;
    while (cnt--) {
        float temperature = 0.0, humidity = 0.0, pressure = 0.0;
        if (ESP_OK == bme280_read_temperature(bme280, &temperature))
        ESP_LOGI("BME280", "temperature:%f ", temperature);
        vTaskDelay(300 / portTICK_RATE_MS);
        if (ESP_OK == bme280_read_humidity(bme280, &humidity))
        ESP_LOGI("BME280", "humidity:%f ", humidity);
        vTaskDelay(300 / portTICK_RATE_MS);
        if (ESP_OK == bme280_read_pressure(bme280, &pressure))
        ESP_LOGI("BME280", "pressure:%f\n", pressure);
        vTaskDelay(300 / portTICK_RATE_MS);
    }
}

TEST_CASE("Device bme280 test", "[bme280][iot][device]")
{
    bme280_test_init();
    bme280_test_getdata();
    bme280_test_deinit();
}
