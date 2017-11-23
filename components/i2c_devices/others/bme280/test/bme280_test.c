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

#define BME280_TEST_CODE 1
#if BME280_TEST_CODE

#include <stdio.h>
#include "unity.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "iot_bme280.h"
#include "iot_i2c_bus.h"

#define I2C_MASTER_SCL_IO           21          /*!< gpio number for I2C master clock IO21*/
#define I2C_MASTER_SDA_IO           15          /*!< gpio number for I2C master data  IO15*/
#define I2C_MASTER_NUM              I2C_NUM_1   /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */

static i2c_bus_handle_t i2c_bus = NULL;
static bme280_handle_t dev = NULL;

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

void bme280_init()
{
    i2c_bus_init();
    dev = iot_bme280_create(i2c_bus, BME280_I2C_ADDRESS_DEFAULT);
    ESP_LOGI("BME280:", "iot_bme280_init:%d", iot_bme280_init(dev));
}

void bme280_test_task(void* pvParameters)
{
    int cnt = 100;
    while (cnt--) {
        ESP_LOGI("BME280:", "temperature:%f\n",
                iot_bme280_read_temperature(dev));
        vTaskDelay(300 / portTICK_RATE_MS);
        ESP_LOGI("BME280:", "humidity:%f\n", iot_bme280_read_humidity(dev));
        vTaskDelay(300 / portTICK_RATE_MS);
        ESP_LOGI("BME280:", "pressure:%f\n", iot_bme280_read_pressure(dev));
        vTaskDelay(300 / portTICK_RATE_MS);
    }
    vTaskDelete(NULL);
}

void bme280_test()
{
    bme280_init();
    xTaskCreate(bme280_test_task, "bme280_test_task", 1024 * 2, NULL, 10, NULL);
}

TEST_CASE("Device bme280 test", "[bme280][iot][device]")
{
    bme280_test();
}

#endif
