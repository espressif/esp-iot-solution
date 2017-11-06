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

#define SSD1306_TEST_CODE 1
#if SSD1306_TEST_CODE

#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "unity.h"
#include "esp_deep_sleep.h"

#include "touchpad.h"
#include "driver/touch_pad.h"
#include "driver/gpio.h"
#include "i2c_bus.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"

#define	OLED_SHOW_LEFT_TOUCH		9
#define OLED_SHOW_RIGHT_TOUCH		8
#define TOUCHPAD_THRES_PERCENT  	950
#define TOUCHPAD_FILTER_INTERVAL  	10
static const char* TAG = "ssd1306 test";

#define POWER_CNTL_IO   			19

static i2c_bus_handle_t i2c_bus = NULL;
static ssd1306_handle_t dev = NULL;

touchpad_handle_t touchpad_dev0, touchpad_dev1;

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
//ets_printf("The current date/time in Shanghai is: %s\n", strftime_buf);

    ssd1306_draw_3216char(dev, 0, 16, strftime_buf[11]);
    ssd1306_draw_3216char(dev, 16, 16, strftime_buf[12]);
    ssd1306_draw_3216char(dev, 32, 16, strftime_buf[13]);
    ssd1306_draw_3216char(dev, 48, 16, strftime_buf[14]);
    ssd1306_draw_3216char(dev, 64, 16, strftime_buf[15]);
    ssd1306_draw_1616char(dev, 80, 32, strftime_buf[16]);
    ssd1306_draw_1616char(dev, 96, 32, strftime_buf[17]);
    ssd1306_draw_1616char(dev, 112, 32, strftime_buf[18]);
//ssd1306_draw_bitmap(87, 16, &c_chBmp4016[0], 40, 16);

    char *day = strftime_buf;
    day[3] = '\0';
    ssd1306_display_string(dev, 87, 16, (const uint8_t *) day, 14, 1);
    ssd1306_display_string(dev, 0, 52, (const uint8_t *) "MUSIC", 12, 0);
    ssd1306_display_string(dev, 52, 52, (const uint8_t *) "MENU", 12, 0);
    ssd1306_display_string(dev, 98, 52, (const uint8_t *) "PHONE", 12, 0);

    return ssd1306_refresh_gram(dev);
}

esp_err_t ssd1306_show_signs(ssd1306_handle_t dev)
{
    esp_err_t ret;
    ret = ssd1306_clear_screen(dev, 0x00);
    if (ret == ESP_FAIL) {
        return ret;
    }

    ssd1306_draw_bitmap(dev, 0, 2, &c_chSingal816[0], 16, 8);
    ssd1306_draw_bitmap(dev, 24, 2, &c_chBluetooth88[0], 8, 8);
    ssd1306_draw_bitmap(dev, 40, 2, &c_chMsg816[0], 16, 8);
    ssd1306_draw_bitmap(dev, 64, 2, &c_chGPRS88[0], 8, 8);
    ssd1306_draw_bitmap(dev, 90, 2, &c_chAlarm88[0], 8, 8);
    ssd1306_draw_bitmap(dev, 112, 2, &c_chBat816[0], 16, 8);

    return ssd1306_refresh_gram(dev);
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
    i2c_bus = i2c_bus_create(OLED_IIC_NUM, &conf);
}

static void dev_ssd1306_initialization(void)
{
    printf("oled task start!\n");
    i2c_bus_init();
    dev = dev_ssd1306_create(i2c_bus, 0x3C);
    ssd1306_refresh_gram(dev);
    ssd1306_show_signs(dev);
    printf("oled finish!\n");
}

static void ssd1306_test_task(void* pvParameters)
{
    int cnt = 2;
    dev_ssd1306_initialization();
    while (1) {
        ssd1306_show_time(dev);
        vTaskDelay(100 / portTICK_RATE_MS);
    }
    dev_ssd1306_delete(dev, true);
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

static void tap_cb(void *arg)
{
    touchpad_handle_t touchpad_dev = (touchpad_handle_t) arg;
    touch_pad_t tp_num = touchpad_num_get(touchpad_dev);
    printf("tap callback of touch pad num %d\n", tp_num);
    printf("deep sleep start\n");
//    power_cntl_off();
//    esp_deep_sleep_enable_touchpad_wakeup();
//    esp_deep_sleep_start();
}

static void ssd1306_test()
{
    power_cntl_init();
    power_cntl_on();
    xTaskCreate(&ssd1306_test_task, "ssd1306_test_task", 2048 * 2, NULL, 5,
            NULL);

    touchpad_dev0 = touchpad_create(OLED_SHOW_LEFT_TOUCH,
    TOUCHPAD_THRES_PERCENT, TOUCHPAD_FILTER_INTERVAL);
    touchpad_dev1 = touchpad_create(OLED_SHOW_RIGHT_TOUCH,
    TOUCHPAD_THRES_PERCENT, TOUCHPAD_FILTER_INTERVAL);

    touchpad_add_cb(touchpad_dev0, TOUCHPAD_CB_RELEASE, tap_cb, touchpad_dev0);
    touchpad_add_cb(touchpad_dev1, TOUCHPAD_CB_RELEASE, tap_cb, touchpad_dev1);


//    vTaskDelay((30 * 1000) / portTICK_RATE_MS);
//    ESP_LOGI(TAG, "touchpad 0 deleted");
//    touchpad_delete(touchpad_dev0);
//    vTaskDelay((30 * 1000) / portTICK_RATE_MS);
//    ESP_LOGI(TAG, "touchpad 1 deleted");
//    touchpad_delete(touchpad_dev1);
}

TEST_CASE("Device ssd1306 test", "[ssd1306][iot][device]")
{
    ssd1306_test();
}

#endif
