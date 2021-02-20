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
#include "hts221.h"
#include "esp_system.h"

#define I2C_MASTER_SCL_IO           22          /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO           21          /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_1   /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */

static i2c_bus_handle_t i2c_bus = NULL;
static hts221_handle_t hts221 = NULL;

void hts221_init_test()
{
    uint8_t hts221_deviceid;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    hts221 = hts221_create(i2c_bus, HTS221_I2C_ADDRESS);
    hts221_get_deviceid(hts221, &hts221_deviceid);
    printf("hts221 device ID is: %02x\n", hts221_deviceid);
    hts221_config_t hts221_config;    
    hts221_get_config(hts221, &hts221_config);
    printf("avg_h is: %02x\n", hts221_config.avg_h);
    printf("avg_t is: %02x\n", hts221_config.avg_t);
    printf("odr is: %02x\n", hts221_config.odr);
    printf("bdu_status is: %02x\n", hts221_config.bdu_status);
    printf("heater_status is: %02x\n", hts221_config.heater_status);
    printf("irq_level is: %02x\n", hts221_config.irq_level);
    printf("irq_output_type is: %02x\n", hts221_config.irq_output_type);
    printf("irq_enable is: %02x\n", hts221_config.irq_enable);

    hts221_config.avg_h = HTS221_AVGH_32;
    hts221_config.avg_t = HTS221_AVGT_16;
    hts221_config.odr = HTS221_ODR_1HZ;
    hts221_config.bdu_status = HTS221_DISABLE;
    hts221_config.heater_status = HTS221_DISABLE;
    hts221_set_config(hts221, &hts221_config);
    hts221_set_activate(hts221);
}

void hts221_deinit_test()
{
    hts221_delete(&hts221);
    i2c_bus_delete(&i2c_bus);
}

void hts221_get_data_test()
{
    int16_t temperature;
    int16_t humidity;
    int cnt = 10;
    while(cnt--){
        printf("\n********HTS221 HUMIDITY&TEMPERATURE SENSOR********\n");
        hts221_get_humidity(hts221, &humidity);
        printf("humidity value is: %2.2f\n", (float)humidity / 10);
        hts221_get_temperature(hts221, &temperature);
        printf("temperature value is: %2.2f\n", (float)temperature / 10);
        printf("**************************************************\n");
        vTaskDelay(1000 / portTICK_RATE_MS);
        printf("heap: %d\n", esp_get_free_heap_size());
    }
}

TEST_CASE("Sensor hts221 test", "[hts221][iot][sensor]")
{
    hts221_init_test();
    vTaskDelay(1000 / portTICK_RATE_MS);
    hts221_get_data_test();
    hts221_deinit_test();
}
