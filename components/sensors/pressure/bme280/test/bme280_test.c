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
#include "iot_bme280.h"
#include "iot_i2c_bus.h"

#define I2C_MASTER_SCL_IO           21          /*!< gpio number for I2C master clock IO21*/
#define I2C_MASTER_SDA_IO           15          /*!< gpio number for I2C master data  IO15*/
#define I2C_MASTER_NUM              I2C_NUM_1   /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */

static i2c_bus_handle_t i2c_bus = NULL;
static bme280_handle_t dev = NULL;

/**
 * @brief i2c master initialization
 */
static void i2c_bus_init()
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_bus = iot_i2c_bus_create(I2C_MASTER_NUM, &conf);
}

void bme280_init()
{
    i2c_bus_init();
    dev = iot_bme280_create(i2c_bus, BME280_I2C_ADDRESS_DEFAULT);
    ESP_LOGI("BME280:", "iot_bme280_init:%d", iot_bme280_init(dev));
}

void bme280_test_task(void* pvParameters)
{
    int cnt = 100;
    while (cnt--) {
        ESP_LOGI("BME280:", "temperature:%f\n",
                iot_bme280_read_temperature(dev));
        vTaskDelay(300 / portTICK_RATE_MS);
        ESP_LOGI("BME280:", "humidity:%f\n", iot_bme280_read_humidity(dev));
        vTaskDelay(300 / portTICK_RATE_MS);
        ESP_LOGI("BME280:", "pressure:%f\n", iot_bme280_read_pressure(dev));
        vTaskDelay(300 / portTICK_RATE_MS);
    }
    vTaskDelete(NULL);
}

void bme280_test()
{
    bme280_init();
    xTaskCreate(bme280_test_task, "bme280_test_task", 1024 * 2, NULL, 10, NULL);
}

TEST_CASE("Device bme280 test", "[bme280][iot][device]")
{
    bme280_test();
}
