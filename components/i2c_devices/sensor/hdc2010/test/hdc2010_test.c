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
#include "iot_i2c_bus.h"
#include "esp_system.h"
#include "iot_hdc2010.h"

#define HDC2010_I2C_MASTER_SCL_IO           (gpio_num_t)26         /*!< gpio number for I2C master clock */
#define HDC2010_I2C_MASTER_SDA_IO           (gpio_num_t)25         /*!< gpio number for I2C master data  */
#define HDC2010_I2C_MASTER_NUM              I2C_NUM_1              /*!< I2C port number for master dev */
#define HDC2010_I2C_MASTER_TX_BUF_DISABLE   0                      /*!< I2C master do not need buffer */
#define HDC2010_I2C_MASTER_RX_BUF_DISABLE   0                      /*!< I2C master do not need buffer */
#define HDC2010_I2C_MASTER_FREQ_HZ          100000                 /*!< I2C master clock frequency */

static i2c_bus_handle_t i2c_bus = NULL;
static hdc2010_handle_t hdc2010 = NULL;
static const char *TAG = "hdc2010";
/**
 * @brief i2c master initialization
 */
static void i2c_master_init()
{
    int i2c_master_port = HDC2010_I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = HDC2010_I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = HDC2010_I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = HDC2010_I2C_MASTER_FREQ_HZ;
    i2c_bus = iot_i2c_bus_create(i2c_master_port, &conf);
}

void hdc2010_init()
{
    i2c_master_init();
    hdc2010 = iot_hdc2010_create(i2c_bus, HDC2010_ADDR_PIN_SELECT_GND);
}

void hdc2010_test()
{
    hdc2010_init();
    int cnt = 10;
    while (cnt--) {
        ESP_LOGI(TAG, "temperature %f", iot_hdc2010_get_temperature(hdc2010));
        ESP_LOGI(TAG, "humidity:%f", iot_hdc2010_get_humidity(hdc2010));
        ESP_LOGI(TAG, "max temperature:%f", iot_hdc2010_get_max_temperature(hdc2010));
        ESP_LOGI(TAG, "max humidity:%f", iot_hdc2010_get_max_humidity(hdc2010));
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

TEST_CASE("Sensor hdc2010 test", "[hdc2010][iot][sensor]")
{
    hdc2010_test();
}

