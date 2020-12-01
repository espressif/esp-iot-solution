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
#include "iot_apds9960.h"
#include "esp_system.h"

#define APDS9960_I2C_MASTER_SCL_IO           (gpio_num_t)21          /*!< gpio number for I2C master clock */
#define APDS9960_I2C_MASTER_SDA_IO           (gpio_num_t)22          /*!< gpio number for I2C master data  */
#define APDS9960_I2C_MASTER_NUM              I2C_NUM_1   /*!< I2C port number for master dev */
#define APDS9960_I2C_MASTER_TX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define APDS9960_I2C_MASTER_RX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define APDS9960_I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */

i2c_bus_handle_t i2c_bus = NULL;
apds9960_handle_t apds9960 = NULL;

void gpio_init(void)
{
    gpio_config_t cfg;
    cfg.pin_bit_mask = BIT19;
    cfg.intr_type = 0;
    cfg.mode = GPIO_MODE_OUTPUT;
    cfg.pull_down_en = 0;
    cfg.pull_up_en = 0;

    gpio_config(&cfg);
    gpio_set_level(19, 0);
}

/**
 * @brief i2c master initialization
 */
static void i2c_sensor_apds9960_init()
{
    int i2c_master_port = APDS9960_I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = APDS9960_I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = APDS9960_I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = APDS9960_I2C_MASTER_FREQ_HZ;
    i2c_bus = iot_i2c_bus_create(i2c_master_port, &conf);
    apds9960 = iot_apds9960_create(i2c_bus, APDS9960_I2C_ADDRESS);
}

static void apds9960_test_func()
{
    int cnt = 0;
    while (cnt < 5) {
        uint8_t gesture = iot_apds9960_read_gesture(apds9960);
        if (gesture == APDS9960_DOWN) {
            printf("gesture APDS9960_DOWN*********************!\n");
        } else if (gesture == APDS9960_UP) {
            printf("gesture APDS9960_UP*********************!\n");
        } else if (gesture == APDS9960_LEFT) {
            printf("gesture APDS9960_LEFT*********************!\n");
            cnt++;
        } else if (gesture == APDS9960_RIGHT) {
            printf("gesture APDS9960_RIGHT*********************!\n");
            cnt++;
        }
        vTaskDelay(100 / portTICK_RATE_MS);
    }
    iot_apds9960_delete(apds9960, true);
}

static void apds9960_test()
{
    gpio_init();
    i2c_sensor_apds9960_init();
    iot_apds9960_gesture_init(apds9960);
    vTaskDelay(1000 / portTICK_RATE_MS);
    apds9960_test_func();
}

TEST_CASE("Sensor apds9960 test", "[apds9960][iot][sensor]")
{
    apds9960_test();
}

