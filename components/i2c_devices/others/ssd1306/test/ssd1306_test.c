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
#define SSD1306_TEST_CODE 1
#if SSD1306_TEST_CODE

#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "unity.h"
#include "esp_sleep.h"

#include "iot_touchpad.h"
#include "driver/touch_pad.h"
#include "driver/gpio.h"
#include "iot_i2c_bus.h"
#include "iot_ssd1306.h"
#include "ssd1306_fonts.h"

static const char* TAG = "ssd1306 test";

#define	OLED_SHOW_LEFT_TOUCH		9
#define OLED_SHOW_RIGHT_TOUCH		8
#define TOUCHPAD_THRES_PERCENT  	0.20

#define OLED_IIC_SCL_NUM            (gpio_num_t)4       /*!< gpio number for I2C master clock IO4*/
#define OLED_IIC_SDA_NUM            (gpio_num_t)17      /*!< gpio number for I2C master data IO17*/
#define OLED_IIC_NUM                I2C_NUM_0           /*!< I2C number >*/
#define OLED_IIC_FREQ_HZ            100000              /*!< I2C colock frequency >*/

#define POWER_CNTL_IO   			19

static i2c_bus_handle_t i2c_bus = NULL;
static ssd1306_handle_t dev = NULL;

tp_handle_t touchpad_dev0, touchpad_dev1;

time_t g_now;

/******** Test Function ****************/
esp_err_t ssd1306_show_time(ssd1306_handle_t dev)
{
    struct tm timeinfo;
    char strftime_buf[64];
    time(&g_now);
    g_now++;
    setenv("TZ", "GMT-8", 1);
    tzset();
    localtime_r(&g_now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

    iot_ssd1306_draw_3216char(dev, 0, 16, strftime_buf[11]);
    iot_ssd1306_draw_3216char(dev, 16, 16, strftime_buf[12]);
    iot_ssd1306_draw_3216char(dev, 32, 16, strftime_buf[13]);
    iot_ssd1306_draw_3216char(dev, 48, 16, strftime_buf[14]);
    iot_ssd1306_draw_3216char(dev, 64, 16, strftime_buf[15]);
    iot_ssd1306_draw_1616char(dev, 80, 32, strftime_buf[16]);
    iot_ssd1306_draw_1616char(dev, 96, 32, strftime_buf[17]);
    iot_ssd1306_draw_1616char(dev, 112, 32, strftime_buf[18]);

    char *day = strftime_buf;
    day[3] = '\0';
    iot_ssd1306_draw_string(dev, 87, 16, (const uint8_t *) day, 14, 1);
    iot_ssd1306_draw_string(dev, 0, 52, (const uint8_t *) "MUSIC", 12, 0);
    iot_ssd1306_draw_string(dev, 52, 52, (const uint8_t *) "MENU", 12, 0);
    iot_ssd1306_draw_string(dev, 98, 52, (const uint8_t *) "PHONE", 12, 0);

    return iot_ssd1306_refresh_gram(dev);
}

esp_err_t ssd1306_show_signs(ssd1306_handle_t dev)
{
    esp_err_t ret;
    ret = iot_ssd1306_clear_screen(dev, 0x00);
    if (ret == ESP_FAIL) {
        return ret;
    }

    iot_ssd1306_draw_bitmap(dev, 0, 2, &c_chSingal816[0], 16, 8);
    iot_ssd1306_draw_bitmap(dev, 24, 2, &c_chBluetooth88[0], 8, 8);
    iot_ssd1306_draw_bitmap(dev, 40, 2, &c_chMsg816[0], 16, 8);
    iot_ssd1306_draw_bitmap(dev, 64, 2, &c_chGPRS88[0], 8, 8);
    iot_ssd1306_draw_bitmap(dev, 90, 2, &c_chAlarm88[0], 8, 8);
    iot_ssd1306_draw_bitmap(dev, 112, 2, &c_chBat816[0], 16, 8);

    return iot_ssd1306_refresh_gram(dev);
}
/**
 * @brief i2c master initialization
 */
static void i2c_bus_init(void)
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = OLED_IIC_SDA_NUM;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = OLED_IIC_SCL_NUM;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = OLED_IIC_FREQ_HZ;
    i2c_bus = iot_i2c_bus_create(OLED_IIC_NUM, &conf);
}

static void dev_ssd1306_initialization(void)
{
    ESP_LOGI(TAG, "oled task start!");
    i2c_bus_init();
    dev = iot_ssd1306_create(i2c_bus, 0x3C);
    iot_ssd1306_refresh_gram(dev);
    ssd1306_show_signs(dev);
    ESP_LOGI(TAG, "oled finish!");
}

static void ssd1306_test_task(void* pvParameters)
{
    dev_ssd1306_initialization();
    while (1) {
        ssd1306_show_time(dev);
        vTaskDelay(100 / portTICK_RATE_MS);
    }
    iot_ssd1306_delete(dev, true);
    vTaskDelete(NULL);
}

static void power_cntl_init()
{
    gpio_config_t conf;
    conf.pin_bit_mask = (1 << POWER_CNTL_IO);
    conf.mode = GPIO_MODE_OUTPUT;
    conf.pull_up_en = 0;
    conf.pull_down_en = 0;
    conf.intr_type = 0;
    gpio_config(&conf);
}

static void power_cntl_on()
{
    gpio_set_level(POWER_CNTL_IO, 0);
}

static void power_cntl_off()
{
    gpio_set_level(POWER_CNTL_IO, 1);
}

static void ssd1306_test()
{
    power_cntl_init();
    power_cntl_on();
    xTaskCreate(&ssd1306_test_task, "ssd1306_test_task", 2048 * 2, NULL, 5,
            NULL);
}

TEST_CASE("Device ssd1306 test", "[ssd1306][iot][device]")
{
    ssd1306_test();
}

#endif
