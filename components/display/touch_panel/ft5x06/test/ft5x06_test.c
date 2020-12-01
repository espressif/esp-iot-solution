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
#include "iot_ft5x06.h"
#include "iot_i2c_bus.h"

#define I2C_SCL                     (3)
#define I2C_SDA                     (1)
#define TOUCH_INT_IO                (0)
#define TOUCH_INT_IO_MUX            (PERIPHS_IO_MUX_GPIO35_U)
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0
#define I2C_FREQ_HZ                 (100000)

static i2c_bus_handle_t i2c_bus = NULL;
static ft5x06_handle_t dev = NULL;

/**
 * @brief i2c master initialization
 */
static void i2c_bus_init()
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_SCL,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };
    i2c_bus = iot_i2c_bus_create(I2C_MASTER_NUM, &conf);
}

void ft5x06_init()
{
    i2c_bus_init();
    gpio_set_direction(TOUCH_INT_IO, GPIO_MODE_INPUT);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO35_U, 2);
    dev = iot_ft5x06_create(i2c_bus, FT5X06_ADDR_DEF);
    if(dev == NULL) {
        ESP_LOGE("FT5X06", "ft5x06 device create fail\n");
        ESP_LOGE("FT5X06", "%s:%d (%s)- assert failed!\n", __FILE__, __LINE__, __FUNCTION__);
        abort();
    }
    ESP_LOGI("FT5X06", "ft5x06 device create successed\n");
}

void ft5x06_test_task(void *param)
{
    int i = 0;
    touch_info_t touch_info;
    int flag = 0;
    while (1) {
        if(gpio_get_level(TOUCH_INT_IO) == 0 && iot_ft5x06_touch_report(dev, &touch_info) == ESP_OK) {
            ESP_LOGI("FT5X06","point %d\n", touch_info.touch_point);
            for(i = 0; i < touch_info.touch_point; i++) {
               ESP_LOGI("FT5X06","touch point %d  x:%d  y:%d\n", i, touch_info.curx[i], touch_info.cury[i]);
            }
            vTaskDelay(20/portTICK_PERIOD_MS);
            flag = 0;
        } else {
            if(flag == 0) {
                ESP_LOGI("FT5X06","release\n");
            }
            flag = 1;
            vTaskDelay(10/portTICK_PERIOD_MS);
        }
    }
    vTaskDelete(NULL);
}

void ft5x06_test()
{
    ft5x06_init();
    xTaskCreate(ft5x06_test_task, "ft5x06_test_task", 1024 * 2, NULL, 10, NULL);
    while (1) {
        vTaskDelay(50/portTICK_PERIOD_MS);
    }
}

TEST_CASE("Device ft5x06 test", "[ft5x06][iot][device]")
{
    ft5x06_test();
}
