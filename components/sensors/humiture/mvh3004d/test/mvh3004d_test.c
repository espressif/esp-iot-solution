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
#include "mvh3004d.h"
#include "i2c_bus.h"

#define I2C_MASTER_SCL_IO           22          /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO           21          /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_1   /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */

static i2c_bus_handle_t i2c_bus = NULL;
static mvh3004d_handle_t mvh3004d = NULL;

void mvh3004d_init_test()
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    mvh3004d = mvh3004d_create(i2c_bus, MVH3004D_SLAVE_ADDR);
}

void mvh3004d_deinit_test()
{
    i2c_bus_delete(&i2c_bus);
    mvh3004d_delete(&mvh3004d);
}

void mvh3004d_get_data_test()
{
    int cnt = 10;
    float tp = 0.0, rh = 0.0;
    while(cnt--){
        mvh3004d_get_data(mvh3004d, &tp, &rh);
        printf("mvh3004d[%d]: tp: %.02f; rh: %.02f %%\n", cnt, tp, rh);
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

TEST_CASE("Sensor MVH3004D test", "[mvh3004d][iot][sensor]")
{
    mvh3004d_init_test();
    mvh3004d_get_data_test();
    mvh3004d_deinit_test();
}


