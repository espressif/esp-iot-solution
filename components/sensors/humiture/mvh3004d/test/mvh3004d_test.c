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
#include "iot_mvh3004d.h"
#include "iot_i2c_bus.h"

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
    i2c_bus = iot_i2c_bus_create(I2C_MASTER_NUM, &conf);
}

void mvh3004d_init()
{
    i2c_bus_init();
    sens = iot_mvh3004d_create(i2c_bus, MVH3004D_SLAVE_ADDR);
}

void mvh3004d_test_task(void* pvParameters)
{
    int cnt = 0;
    float tp, rh;
    while(1){
        iot_mvh3004d_get_data(sens, &tp, &rh);
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


