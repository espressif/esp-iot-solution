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
  
#define BH1750_TEST_CODE 1
#if BH1750_TEST_CODE

#include <stdio.h>
#include "unity.h"
#include "driver/i2c.h"
#include "bh1750.h"
#include "i2c_bus.h"

#define I2C_MASTER_SCL_IO           19          /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO           18          /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_1   /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */


i2c_bus_handle_t i2c_bus = NULL;
bh1750_handle_t sens = NULL;

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
    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
}

void bh1750_init()
{
    i2c_bus_init();
    sens = sensor_bh1750_create(i2c_bus, 0x23, false);
}

void bh1750_test_task(void* pvParameters)
{
    int ret;
    bh1750_cmd_measure_t cmd_measure;
    float bh1750_data;
    while(1){
        uint32_t i;
        bh1750_power_on(sens);
        cmd_measure = BH1750_ONETIME_4LX_RES;
        bh1750_set_measure_mode(sens, cmd_measure);
        vTaskDelay(30 / portTICK_RATE_MS);
        for (i=0; i<10; i++) {
            ret = bh1750_get_data(sens, &bh1750_data);
            if (ret == ESP_OK) {
                printf("bh1750 val(one time mode): %f\n", bh1750_data);
            } else {
                printf("No ack, sensor not connected...\n");
            }
            vTaskDelay(1000 / portTICK_RATE_MS);
        }
        cmd_measure = BH1750_CONTINUE_4LX_RES;
        bh1750_set_measure_mode(sens, cmd_measure);
        vTaskDelay(30 / portTICK_RATE_MS);
        for (i=0; i<10; i++) {
            ret = bh1750_get_data(sens, &bh1750_data);
            if (ret == ESP_OK) {
                printf("bh1750 val(continuously mode): %f\n", bh1750_data);
            } else {
                printf("No ack, sensor not connected...\n");
            }
            vTaskDelay(1000 / portTICK_RATE_MS);
        }
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
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


#endif
