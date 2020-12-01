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
#include "iot_i2c_bus.h"
#include "iot_hts221.h"
#include "esp_system.h"

#define I2C_MASTER_SCL_IO           21          /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO           15          /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_1   /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */

static i2c_bus_handle_t i2c_bus = NULL;
static hts221_handle_t hts221 = NULL;
/**
 * @brief i2c master initialization
 */
void i2c_sensor_hts221_init()
{
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_bus = iot_i2c_bus_create(i2c_master_port, &conf);
    hts221 = iot_hts221_create(i2c_bus, HTS221_I2C_ADDRESS);
}

void hts221_test_task(void* pvParameters)
{
    uint8_t hts221_deviceid;
    int16_t temperature;
    int16_t humidity;
    
    iot_hts221_get_deviceid(hts221, &hts221_deviceid);
    printf("hts221 device ID is: %02x\n", hts221_deviceid);
        
    hts221_config_t hts221_config;    
    iot_hts221_get_config(hts221, &hts221_config);
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
    iot_hts221_set_config(hts221, &hts221_config);
    
    iot_hts221_set_activate(hts221);
    while(1){
        printf("\n********HTS221 HUMIDITY&TEMPERATURE SENSOR********\n");
        iot_hts221_get_humidity(hts221, &humidity);
        printf("humidity value is: %2.2f\n", (float)humidity / 10);
        iot_hts221_get_temperature(hts221, &temperature);
        printf("temperature value is: %2.2f\n", (float)temperature / 10);
        printf("**************************************************\n");
        vTaskDelay(1000 / portTICK_RATE_MS);
        printf("heap: %d\n", esp_get_free_heap_size());
    }
}

void hts221_test()
{
    i2c_sensor_hts221_init();
    vTaskDelay(1000 / portTICK_RATE_MS);
    xTaskCreate(hts221_test_task, "hts221_test_task", 1024 * 2, NULL, 10, NULL);
}

TEST_CASE("Sensor hts221 test", "[hts221][iot][sensor]")
{
    hts221_test();
}

