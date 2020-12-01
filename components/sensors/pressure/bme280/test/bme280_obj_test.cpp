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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "unity.h"
#include "iot_i2c_bus.h"
#include "iot_bme280.h"

#define I2C_MASTER_SCL_IO           (gpio_num_t)21          /*!< gpio number for I2C master clock IO21*/
#define I2C_MASTER_SDA_IO           (gpio_num_t)15          /*!< gpio number for I2C master data  IO15*/
#define I2C_MASTER_NUM              I2C_NUM_1               /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0           			/*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0           			/*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ          100000      			/*!< I2C master clock frequency */

extern "C" void bme280_obj_test()
{
    int cnt = 100;
    CI2CBus i2c_bus(I2C_MASTER_NUM, I2C_MASTER_SCL_IO, I2C_MASTER_SDA_IO);
    CBme280 bme280(&i2c_bus);
    bme280.init();

    while (cnt--) {
        ESP_LOGI("BME280:", "temperature:%f\n", bme280.temperature());
        vTaskDelay(300 / portTICK_RATE_MS);
        ESP_LOGI("BME280:", "humidity:%f\n", bme280.humidity());
        vTaskDelay(300 / portTICK_RATE_MS);
    }
    printf("heap: %d\n", esp_get_free_heap_size());
    vTaskDelete(NULL);
}

TEST_CASE("Device bme280 obj test", "[bme280_cpp][iot][device]")
{
    bme280_obj_test();
}
