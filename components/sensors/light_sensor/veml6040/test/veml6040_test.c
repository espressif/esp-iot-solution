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
#include "string.h"
#include "veml6040.h"
#include "unity.h"

#define VEML6040_I2C_MASTER_SCL_IO               (gpio_num_t)22  /*!< gpio number for I2C master clock */
#define VEML6040_I2C_MASTER_SDA_IO               (gpio_num_t)21  /*!< gpio number for I2C master data  */

#define VEML6040_I2C_MASTER_NUM              	 I2C_NUM_1   /*!< I2C port number for master dev */
#define VEML6040_I2C_MASTER_TX_BUF_DISABLE       0           /*!< I2C master do not need buffer */
#define VEML6040_I2C_MASTER_RX_BUF_DISABLE   	 0           /*!< I2C master do not need buffer */
#define VEML6040_I2C_MASTER_FREQ_HZ         	 100000      /*!< I2C master clock frequency */

static i2c_bus_handle_t i2c_bus = NULL;
static veml6040_handle_t veml6040 = NULL;

static void veml6040_test_init()
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = VEML6040_I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = VEML6040_I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = VEML6040_I2C_MASTER_FREQ_HZ,
    };
    i2c_bus = i2c_bus_create(VEML6040_I2C_MASTER_NUM, &conf);
    veml6040 = veml6040_create(i2c_bus, VEML6040_I2C_ADDRESS);
}

static void veml6040_test_deinit()
{
    veml6040_delete(&veml6040);
    i2c_bus_delete(&i2c_bus);
}

static void veml6040_test_set_mode()
{
    veml6040_config_t veml6040_info;
    memset(&veml6040_info, 0, sizeof(veml6040_info));
    veml6040_get_mode(veml6040, &veml6040_info);
    printf("integration_time:%d \n", veml6040_info.integration_time);
    printf("mode:%d \n", veml6040_info.mode);
    printf("trigger:%d \n", veml6040_info.trigger);
    printf("switch_en:%d \n", veml6040_info.switch_en);
    veml6040_info.integration_time = VEML6040_INTEGRATION_TIME_160MS;
    veml6040_info.mode = VEML6040_MODE_AUTO;
    veml6040_info.trigger = VEML6040_TRIGGER_DIS;
    veml6040_info.switch_en = VEML6040_SWITCH_EN;
    veml6040_set_mode(veml6040, &veml6040_info);
    vTaskDelay(500 / portTICK_RATE_MS);
    veml6040_get_mode(veml6040, &veml6040_info);
    printf("integration_time:%d \n", veml6040_info.integration_time);
    printf("mode:%d \n", veml6040_info.mode);
    printf("trigger:%d \n", veml6040_info.trigger);
    printf("switch_en:%d \n", veml6040_info.switch_en);

}

static void veml6040_test_get_date()
{
    int cnt = 10;
    while (cnt--) {
        printf("red:%d ", veml6040_get_red(veml6040));
        printf("green:%d ", veml6040_get_green(veml6040));
        printf("blue:%d ", veml6040_get_blue(veml6040));
        printf("lux:%f\n", veml6040_get_lux(veml6040));
        vTaskDelay(1000  / portTICK_RATE_MS);
     }
}

TEST_CASE("Sensor veml6040 test", "[veml6040][iot][sensor]")
{
    veml6040_test_init();
    veml6040_test_set_mode();
    veml6040_test_get_date();
    veml6040_test_deinit();
}

