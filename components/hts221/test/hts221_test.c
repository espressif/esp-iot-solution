/*
  * ESPRESSIF MIT License
  *
  * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
  *
  * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
  * it is free of charge, to any person obtaining a copy of this software and associated
  * documentation files (the "Software"), to deal in the Software without restriction, including
  * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
  * to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in all copies or
  * substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  *
  */
  
#define HTS221_TEST_CODE 1
#if HTS221_TEST_CODE

#include <stdio.h>
#include "unity.h"
#include "driver/i2c.h"
#include "i2c_bus.h"
#include "hts221.h"
#include "esp_system.h"

#define I2C_MASTER_SCL_IO           19          /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO           18          /*!< gpio number for I2C master data  */
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
    i2c_bus = i2c_bus_create(i2c_master_port, &conf);
    hts221 = sensor_hts221_create(i2c_bus, HTS221_I2C_ADDRESS, false);
}

void hts221_test_task(void* pvParameters)
{
    uint8_t hts221_deviceid;
    int16_t temperature;
    int16_t humidity;
    
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
    while(1){
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
#endif

