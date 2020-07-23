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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "unity.h"
#include "driver/gpio.h"
#include "iot_i2c_bus.h"
#include "iot_ssd1306.h"

#define OLED_IIC_SCL_NUM            (gpio_num_t)4       /*!< gpio number for I2C master clock IO4*/
#define OLED_IIC_SDA_NUM            (gpio_num_t)17      /*!< gpio number for I2C master data IO17*/
#define OLED_IIC_NUM                I2C_NUM_0           /*!< I2C number >*/
#define OLED_IIC_FREQ_HZ            100000              /*!< I2C colock frequency >*/

#define POWER_CNTL_IO   			19

extern  "C" {
    extern time_t g_now;  ////***************
    extern esp_err_t ssd1306_show_time(ssd1306_handle_t device);
    extern esp_err_t ssd1306_show_signs(ssd1306_handle_t device);
}

void power_cntl_init()
{
    gpio_config_t conf;
    conf.pin_bit_mask = (1 << POWER_CNTL_IO);
    conf.mode = GPIO_MODE_OUTPUT;
    conf.pull_up_en = (gpio_pullup_t) 0;
    conf.pull_down_en = (gpio_pulldown_t) 0;
    conf.intr_type = (gpio_int_type_t) 0;
    gpio_config(&conf);
}

void power_cntl_on()
{
    gpio_set_level((gpio_num_t) POWER_CNTL_IO, 0);
}

void power_cntl_off()
{
    gpio_set_level((gpio_num_t) POWER_CNTL_IO, 1);
}

extern "C" void ssd1306_obj_test()
{
    power_cntl_init();
    power_cntl_on();
    CI2CBus i2c_bus(OLED_IIC_NUM, OLED_IIC_SCL_NUM, OLED_IIC_SDA_NUM);
    CSsd1306 ssd1306(&i2c_bus);
    ssd1306_show_signs(ssd1306.get_dev_handle());
    while (1) {
        ssd1306_show_time(ssd1306.get_dev_handle());
        vTaskDelay(100 / portTICK_RATE_MS);
    }
    printf("heap: %d\n", esp_get_free_heap_size());
}

TEST_CASE("Device ssd1306 obj test", "[ssd1306_cpp][iot][device]")
{
    ssd1306_obj_test();
}
