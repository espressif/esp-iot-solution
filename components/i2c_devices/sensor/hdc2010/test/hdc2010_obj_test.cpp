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
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "unity.h"
#include "iot_i2c_bus.h"
#include "driver/i2c.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "iot_hdc2010.h"

#define HDC2010_I2C_MASTER_SCL_IO                (gpio_num_t)26  /*!< gpio number for I2C master clock */
#define HDC2010_I2C_MASTER_SDA_IO                (gpio_num_t)25  /*!< gpio number for I2C master data  */
#define HDC2010_I2C_MASTER_NUM              	 I2C_NUM_1       /*!< I2C port number for master dev */
#define HDC2010_I2C_MASTER_TX_BUF_DISAB    	     0               /*!< I2C master do not need buffer */
#define HDC2010_I2C_MASTER_RX_BUF_DISABLE   	 0               /*!< I2C master do not need buffer */
#define HDC2010_I2C_MASTER_FREQ_HZ         	     100000          /*!< I2C master clock frequency */

static const char *TAG = "hdc2010";

extern "C" void hdc2010_obj_test()
{
    CI2CBus i2c_bus(HDC2010_I2C_MASTER_NUM, HDC2010_I2C_MASTER_SCL_IO,
    HDC2010_I2C_MASTER_SDA_IO);
    CHdc2010 hdc(&i2c_bus, HDC2010_ADDR_PIN_SELECT_GND);
    int cnt = 10;
    while (cnt --) {
        ESP_LOGI(TAG, "temperature %f", hdc.temperature());
        ESP_LOGI(TAG, "humidity:%f", hdc.humidity());
        ESP_LOGI(TAG, "max temperature:%f", hdc.max_temperature());
        ESP_LOGI(TAG, "max humidity:%f", hdc.max_humidity());
        vTaskDelay(2000 / portTICK_RATE_MS);
    }
}

TEST_CASE("Sensor hdc2010 obj test", "[hdc2010_app][iot][Sensor]")
{
    hdc2010_obj_test();
}

