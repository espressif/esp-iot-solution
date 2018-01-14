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

#define APDS9960_TEST_CODE 1
#if APDS9960_TEST_CODE

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
#endif

