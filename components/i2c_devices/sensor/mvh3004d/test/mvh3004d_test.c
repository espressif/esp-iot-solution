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
  

#include <stdio.h>
#include "unity.h"
#include "driver/i2c.h"
#include "mvh3004d.h"
#include "i2c_bus.h"

#define I2C_MASTER_SCL_IO           17          /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO           16          /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_1   /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */

static i2c_bus_handle_t i2c_bus = NULL;
static mvh3004d_handle_t sens = NULL;

/**
 * @brief i2c master initialization
 */
static void i2c_bus_init()
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

void mvh3004d_init()
{
    i2c_bus_init();
    sens = sensor_mvh3004d_create(i2c_bus, MVH3004D_SLAVE_ADDR, false);
}

void mvh3004d_test_task(void* pvParameters)
{
    int cnt = 0;
    float tp, rh;
    while(1){
        mvh3004d_get_data(sens, &tp, &rh);
        printf("test[%d]: tp: %.02f; rh: %.02f %%\n", cnt++, tp, rh);
        vTaskDelay(10 / portTICK_RATE_MS);
    }
}

void mvh3004d_test()
{
    mvh3004d_init();
    xTaskCreate(mvh3004d_test_task, "mvh3004d_test_task", 1024 * 2, NULL, 10, NULL);
}

TEST_CASE("Sensor MVH3004D test", "[mvh3004d][iot][sensor]")
{
    mvh3004d_test();
}


