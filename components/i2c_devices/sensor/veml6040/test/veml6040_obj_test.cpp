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
#include "iot_veml6040.h"

#define VEML6040_I2C_MASTER_SCL_IO               (gpio_num_t)26  /*!< gpio number for I2C master clock */
#define VEML6040_I2C_MASTER_SDA_IO               (gpio_num_t)25  /*!< gpio number for I2C master data  */

#define VEML6040_I2C_MASTER_NUM              	 I2C_NUM_1   /*!< I2C port number for master dev */
#define VEML6040_I2C_MASTER_TX_BUF_DISAB    	 0			 /*!< I2C master do not need buffer */
#define VEML6040_I2C_MASTER_RX_BUF_DISABLE   	 0           /*!< I2C master do not need buffer */
#define VEML6040_I2C_MASTER_FREQ_HZ         	 100000      /*!< I2C master clock frequency */

static const char *TAG = "veml6040";

extern "C" void veml6040_obj_test()
{
    CI2CBus i2c_bus(VEML6040_I2C_MASTER_NUM, VEML6040_I2C_MASTER_SCL_IO, VEML6040_I2C_MASTER_SDA_IO);
    CVeml6040 veml(&i2c_bus);
    veml6040_config_t veml6040_info;
    memset(&veml6040_info, 0, sizeof(veml6040_info));
    veml6040_info.integration_time = VEML6040_INTEGRATION_TIME_160MS;
    veml6040_info.mode = VEML6040_MODE_AUTO;
    veml6040_info.trigger = VEML6040_TRIGGER_DIS;
    veml6040_info.switch_en = VEML6040_SWITCH_EN;
    // Initialize veml6040 (configure I2C and initial values)
    if (veml.set_mode(&veml6040_info)) {
        ESP_LOGI(TAG, "Device initialized!");
    } else {
        ESP_LOGI(TAG, "Device false!");
    }
    ESP_LOGI(TAG, "red:%d", veml.red());
    ESP_LOGI(TAG, "green:%d", veml.green());
    ESP_LOGI(TAG, "blue:%d", veml.blue());
    ESP_LOGI(TAG, "lux:%f", veml.lux());
}

TEST_CASE("Sensor veml6040 obj test", "[veml6040_cpp][iot][Sensor]")
{
    veml6040_obj_test();
}

