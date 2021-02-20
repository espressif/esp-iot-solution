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
#include "driver/i2c.h"
#include "is31fl3218.h"
#include "is31fl3736.h"
// #include "led_12_8_image.h"

#define I2C_MASTER_SCL_IO    21        /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO    15        /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM     I2C_NUM_1   /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0  /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0  /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ    400000   /*!< I2C master clock frequency */

static const uint8_t image_e[12] = {
    0x1f, 0x15, 0x15,0x0
};

static const uint8_t image_s[12] = {
    0x17, 0x15, 0x1d,0x0
};

static const uint8_t image_p[12] = {
    0x1f, 0x05, 0x07,0x0
};

static i2c_bus_handle_t i2c_bus = NULL;
static is31fl3736_handle_t led3736 = NULL;
void fill_pixel(int x, int y, uint8_t duty);

/**
 * @brief i2c master initialization
 */
void led_dev_init()
{
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_bus = i2c_bus_create(i2c_master_port, &conf);
    TEST_ASSERT_NOT_NULL(i2c_bus);
    led3736 = is31fl3736_create(i2c_bus, GPIO_NUM_32, 0, 0, 0XCF);
    TEST_ASSERT_NOT_NULL(led3736);
}

esp_err_t is31fl3736_send_buf(i2c_port_t i2c_port, uint8_t x, uint8_t y, char *c, uint8_t duty, uint8_t* buf)
{
    is31fl3736_fill_buf(led3736, duty, buf);
    return ESP_OK;
}

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

void is31f13736_test_task(void* pvParameters)
{
    int i = 11;
    char c = 'e';
    static uint8_t dir = 0;
    is31fl3736_write_page(led3736, IS31FL3736_PAGE(1));
    extern void initGame(void*);
    initGame(&fill_pixel);
    //xTaskCreate(is32f13xxx_bar_task, "is32f13xxx_bar_task", 2048, NULL, 10, NULL);
    //xTaskCreate(touch_task, "touch_task", 1024*2, NULL, 11, NULL);
    int cnt = 1;
    while (cnt) {
        if (dir == 0) {
            vTaskDelay(150 / portTICK_RATE_MS);
            uint8_t buf[12] = { 0 };
            c = 'e';
            is31fl3736_display_buf(I2C_MASTER_NUM, 0, i, &c, 50, buf);
            c = 's';
            is31fl3736_display_buf(I2C_MASTER_NUM, 0, i + 4, &c, 50, buf);
            c = 'p';
            is31fl3736_display_buf(I2C_MASTER_NUM, 0, i + 8, &c, 50, buf);
            is31fl3736_send_buf(I2C_MASTER_NUM, 0, i + 8, &c, 50, buf);
            if (--i < 0 - 4) {
                dir = 1;
            }
        } else {
            vTaskDelay(50 / portTICK_RATE_MS);
            uint8_t buf[12] = { 0 };
            c = 'e';
            is31fl3736_display_buf(I2C_MASTER_NUM, 0, i, &c, 50, buf);
            c = 's';
            is31fl3736_display_buf(I2C_MASTER_NUM, 0, i + 4, &c, 50, buf);
            c = 'p';
            is31fl3736_display_buf(I2C_MASTER_NUM, 0, i + 8, &c, 50, buf);
            is31fl3736_send_buf(I2C_MASTER_NUM, 0, i + 8, &c, 50, buf);
            if (++i > 12) {
                dir = 0;
                cnt--;
            }
        }
    }
    vTaskDelete(NULL);
}

void fill_pixel(int x, int y, uint8_t duty)
{
    if (x == -1 || y == -1) {
        return;
    }
    uint8_t reg = x * 2 + y * 0x10;
    is31fl3736_write(led3736, reg, &duty, 1);
}

void is31f13736_test()
{
    led_dev_init();
    xTaskCreate(is31f13736_test_task, "is31f13xxx_test_task", 1024 * 2, NULL, 10, NULL);
}

TEST_CASE("I2C led is31f13736 test", "[is31f13736][iot][fxled]")
{
    is31f13736_test();
}

