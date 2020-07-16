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
#include "freertos/queue.h"
#include <stdio.h>
#include "lwip/apps/sntp.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "string.h"
#include "unity.h"
#include "driver/gpio.h"
#include "iot_veml6040.h"


#define VEML6040_I2C_MASTER_SCL_IO               (gpio_num_t)26  /*!< gpio number for I2C master clock */
#define VEML6040_I2C_MASTER_SDA_IO               (gpio_num_t)25  /*!< gpio number for I2C master data  */

#define VEML6040_I2C_MASTER_NUM              	 I2C_NUM_1   /*!< I2C port number for master dev */
#define VEML6040_I2C_MASTER_TX_BUF_DISAB    	 0           /*!< I2C master do not need buffer */                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                LE   0           /*!< I2C master do not need buffer */
#define VEML6040_I2C_MASTER_RX_BUF_DISABLE   	 0           /*!< I2C master do not need buffer */
#define VEML6040_I2C_MASTER_FREQ_HZ         	 100000      /*!< I2C master clock frequency */

static i2c_bus_handle_t i2c_bus = NULL;
static veml6040_handle_t veml6040 = NULL;
static const char *TAG = "veml6040";
/**
 * @brief i2c master initialization
 */
static void i2c_master_init()
{
    i2c_port_t i2c_master_port = VEML6040_I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = VEML6040_I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = VEML6040_I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = VEML6040_I2C_MASTER_FREQ_HZ;
    i2c_bus = iot_i2c_bus_create(i2c_master_port, &conf);
}

void veml6040_init()
{
    i2c_master_init();
    veml6040 = iot_veml6040_create(i2c_bus, VEML6040_I2C_ADDRESS);
    veml6040_config_t veml6040_info;
    memset(&veml6040_info, 0, sizeof(veml6040_info));
    veml6040_info.integration_time = VEML6040_INTEGRATION_TIME_160MS;
    veml6040_info.mode = VEML6040_MODE_AUTO;
    veml6040_info.trigger = VEML6040_TRIGGER_DIS;
    veml6040_info.switch_en = VEML6040_SWITCH_EN;
    iot_veml6040_set_mode(veml6040, &veml6040_info);
}

void veml6040_test_task(void* pvParameters)
{
    veml6040_init();
    while (1) {
        ESP_LOGI(TAG, "red:%d", iot_veml6040_get_red(veml6040));
        ESP_LOGI(TAG, "green:%d", iot_veml6040_get_green(veml6040));
        ESP_LOGI(TAG, "blue:%d", iot_veml6040_get_blue(veml6040));
        ESP_LOGI(TAG, "lux:%f", iot_veml6040_get_lux(veml6040));
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
    vTaskDelete(NULL);
}

void veml6040_test()
{
    xTaskCreate(veml6040_test_task, "veml6040_test_task", 4096, NULL, 10, NULL);
}

TEST_CASE("Sensor veml6040 test", "[veml6040][iot][sensor]")
{
    veml6040_test();
}

