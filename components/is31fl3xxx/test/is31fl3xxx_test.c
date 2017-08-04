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
  
#define IS31FL3XXX_TEST_CODE 1
#if IS31FL3XXX_TEST_CODE

#define IS31FL3218_TEST 0
#define IS31FL3736_TEST 1

#include <stdio.h>
#include "driver/i2c.h"
#include "is31fl3218.h"
#include "is31fl3736.h"

#define I2C_MASTER_SCL_IO    21        /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO    22        /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM     I2C_NUM_1   /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0  /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0  /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ    400000   /*!< I2C master clock frequency */

i2c_bus_handle_t i2c_bus = NULL;
is31fl3736_handle_t led3736 = NULL;
/**
 * @brief i2c master initialization
 */
void led_dev_init()
{
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_bus = i2c_bus_create(i2c_master_port, &conf);
    led3736 = sensor_is31fl3736_create(i2c_bus, GPIO_NUM_32, 0, 0, 0XCF);
}

//------TOUCH func debug------//
#include "touchpad.h"
int vol = 5;
void touch_task(void* arg)
{
    touch_pad_t pads[] = {TOUCH_PAD_NUM6, TOUCH_PAD_NUM7, TOUCH_PAD_NUM8, TOUCH_PAD_NUM9};
    touchpad_slide_handle_t slide = touchpad_slide_create(4, pads);
    int vol_prev = 0;
    uint8_t reg, reg_val;
    while(1) {
        int pos = touchpad_slide_position(slide);
        if (pos == 255) {
            printf("pos invalid\n");
        } else {
            printf("position: %d\n", pos);
            vol = pos * 12 / 30;
            if (vol == vol_prev) {
            } else {
                vol_prev = vol;
                for (int i = 6; i < IS31FL3736_CSX_MAX; i++) {
                    for (int j = 0; j < IS31FL3736_SWY_MAX; j++) {
                        reg = i * 2 + j * 0x10;
                        if (j < vol) {
                            reg_val = 50;
                        } else {
                            reg_val = 0;
                        }
                        is31fl3736_write_reg(I2C_MASTER_NUM, reg, &reg_val, 1);
                    }
                }
            }
        }
        vTaskDelay(50/portTICK_PERIOD_MS);
    }
}

void is32f13xxx_bar_task(void* arg)
{
    int i, j;
    uint8_t reg, reg_val;
    int vol_prev = 0;
    while(1) {
        uint8_t buf[12] = {0};
        if(vol == vol_prev) {
        } else {
            vol_prev = vol;
            for (i = 6; i < IS31FL3736_CSX_MAX; i++) {
                for (j = 0; j < IS31FL3736_SWY_MAX; j++) {
                    reg = i * 2 + j * 0x10;
                    if (j < vol) {
                        reg_val = 50;
                    } else {
                        reg_val = 0;
                    }
                    is31fl3736_write_reg(I2C_MASTER_NUM, reg, &reg_val, 1);
                }
            }
        }
        vTaskDelay(10/portTICK_PERIOD_MS);
    }
}

esp_err_t is31fl3736_send_buf(i2c_port_t i2c_port, uint8_t x, uint8_t y, char *c, uint8_t duty, uint8_t* buf)
{
    is31fl3736_fill_buf(led3736, duty, buf);
    return ESP_OK;
}

#include "led_12_8_image.h"
/**
 * @brief display the char in matrix
 */
esp_err_t is31fl3736_display_buf(i2c_port_t i2c_port, uint8_t x, uint8_t y, char *c, uint8_t duty, uint8_t* buf)
{
    uint8_t i;
    if (x > 8 || y > 12) {
        return ESP_FAIL;
    }
    switch (*c) {
        case 'e':
            for (i = 0; i < 12; i++) {
                if (image_e[i]) {
                    if (i + y < 12) {
                        buf[i + y] = image_e[i];
                        buf[i + y] = (buf[i + y] << x) & 0xff;
                    }
                }
            }
            break;
        case 's':
            for (i = 0; i < 12; i++) {
                if (image_s[i]) {
                    if (i + y < 12) {
                        buf[i + y] = image_s[i];
                        buf[i + y] = (buf[i + y] << x) & 0xff;
                    }
                }
            }
            break;
        case 'p':
            for (i = 0; i < 12; i++) {
                if (image_p[i]) {
                    if (i + y < 12) {
                        buf[i + y] = image_p[i];
                        buf[i + y] = (buf[i + y] << x) & 0xff;
                    }
                }
            }
            break;
        default:
            return ESP_FAIL;
            break;
    }
    return ESP_OK;
}


void is31f13xxx_test_task(void* pvParameters)
{
    int i=11;
#if IS31FL3218_TEST
    float bh1750_data;
    is31fl3218_info_t info;
    info.i2c_port = I2C_MASTER_NUM;
    IS31FL3218_CH_MUM(info.ch_bit, IS31FL3218_CH_NUM_MAX);
    printf("enable ch bit: 0x%08x\r\n", info.ch_bit);
    ESP_ERROR_CHECK( is31fl3218_init(&info) );
    ESP_ERROR_CHECK( is31fl3218_channel_set(info.i2c_port, info.ch_bit, 0) );
    while(1){
        vTaskDelay(100 / portTICK_RATE_MS);
        ESP_ERROR_CHECK( is31fl3218_channel_set(info.i2c_port, IS31FL3218_CH_BIT(i), j) );
        if(++i > IS31FL3218_CH_NUM_MAX - 1) {
            i = 0;
            if (j == 0) 
                j = 255;
            else
                j = 0;
        }
        
    }
#elif IS31FL3736_TEST
    char c = 'e';
    static uint8_t dir = 0;
    is31fl3736_write_page(led3736, IS31FL3736_PAGE(1));
    extern void initGame();
    initGame();
    //xTaskCreate(is32f13xxx_bar_task, "is32f13xxx_bar_task", 2048, NULL, 10, NULL);
    //xTaskCreate(touch_task, "touch_task", 1024*2, NULL, 11, NULL);
    while(1) {
        if (dir == 0) {
            vTaskDelay(150 / portTICK_RATE_MS);
            uint8_t buf[12] = {0};
            c = 'e';
            is31fl3736_display_buf(I2C_MASTER_NUM, 0, i, &c, 50,buf);
            c = 's';
            is31fl3736_display_buf(I2C_MASTER_NUM, 0, i+4, &c, 50,buf);
            c = 'p';
            is31fl3736_display_buf(I2C_MASTER_NUM, 0, i+8, &c, 50,buf);
            is31fl3736_send_buf(I2C_MASTER_NUM, 0, i+8, &c, 50,buf);
            if (--i < 0 - 4) {
                dir = 1;
            }
        } else {
            vTaskDelay(50 / portTICK_RATE_MS);
            uint8_t buf[12] = {0};
            c = 'e';
            is31fl3736_display_buf(I2C_MASTER_NUM, 0, i, &c, 50,buf);
            c = 's';
            is31fl3736_display_buf(I2C_MASTER_NUM, 0, i+4, &c, 50,buf);
            c = 'p';
            is31fl3736_display_buf(I2C_MASTER_NUM, 0, i+8, &c, 50,buf);
            is31fl3736_send_buf(I2C_MASTER_NUM, 0, i+8, &c, 50,buf);
            if (++i > 12) {
                dir = 0;
            }
        }
    }
    vTaskDelete(NULL);
#endif
}

void fill_pixel(uint8_t x, uint8_t y, uint8_t duty)
{
    if (x == -1 || y == -1) {
        return;
    }
    uint8_t reg = x * 2 + y * 0x10;
    is31fl3736_write_reg(led3736, reg, &duty, 1);
}

void is31f13xxx_test()
{
    led_dev_init();
    xTaskCreate(is31f13xxx_test_task, "is31f13xxx_test_task", 1024 * 2, NULL, 10, NULL);
}
#endif
