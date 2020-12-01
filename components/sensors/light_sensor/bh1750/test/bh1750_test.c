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
#include "iot_bh1750.h"
#include "iot_i2c_bus.h"
#include "esp_log.h"

#define I2C_MASTER_SCL_IO           21          /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO           15          /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_1   /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */

static i2c_bus_handle_t i2c_bus = NULL;
static bh1750_handle_t sens = NULL;

/**
 * @brief i2c master initialization
 */
void i2c_bus_init()
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

void bh1750_init()
{
    i2c_bus_init();
    sens = iot_bh1750_create(i2c_bus, 0x23);
}

void bh1750_test_task(void* pvParameters)
{
    int ret;
    bh1750_cmd_measure_t cmd_measure;
    float bh1750_data;
    int cnt = 2;
    while (cnt--) {
        iot_bh1750_power_on(sens);
        cmd_measure = BH1750_ONETIME_4LX_RES;
        iot_bh1750_set_measure_mode(sens, cmd_measure);
        vTaskDelay(30 / portTICK_RATE_MS);
        ret = iot_bh1750_get_data(sens, &bh1750_data);
        if (ret == ESP_OK) {
            printf("bh1750 val(one time mode): %f\n", bh1750_data);
        } else {
            printf("No ack, sensor not connected...\n");
        }
        cmd_measure = BH1750_CONTINUE_4LX_RES;
        iot_bh1750_set_measure_mode(sens, cmd_measure);
        vTaskDelay(30 / portTICK_RATE_MS);
        ret = iot_bh1750_get_data(sens, &bh1750_data);
        if (ret == ESP_OK) {
            printf("bh1750 val(continuously mode): %f\n", bh1750_data);
        } else {
            printf("No ack, sensor not connected...\n");
        }
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
    iot_bh1750_delete(sens, true);
    vTaskDelete(NULL);
}

void bh1750_test()
{
    bh1750_init();
    xTaskCreate(bh1750_test_task, "bh1750_test_task", 1024 * 2, NULL, 10, NULL);
}

TEST_CASE("Sensor BH1750 test", "[bh1750][iot][sensor]")
{
    bh1750_test();
}
