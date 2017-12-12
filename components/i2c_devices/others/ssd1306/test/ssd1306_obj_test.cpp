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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "unity.h"
#include "driver/gpio.h"
#include "iot_i2c_bus.h"
#include "iot_ssd1306.h"

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
