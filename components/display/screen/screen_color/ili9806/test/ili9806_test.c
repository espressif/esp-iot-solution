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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "unity.h"
#include "string.h"
#include "driver/i2c.h"
#include "iot_ft5x06.h"
#include "iot_i2c_bus.h"
#include "i2s_lcd_com.h"
#include "gussblur.h"
#include "iot_ili9806.h"
#include "pic3.h"

#define I2S_PORT_NUM      (0)

#define LCD_D0_PIN        (19)
#define LCD_D1_PIN        (21)
#define LCD_D2_PIN        (0)
#define LCD_D3_PIN        (22)
#define LCD_D4_PIN        (23)
#define LCD_D5_PIN        (33)
#define LCD_D6_PIN        (32)
#define LCD_D7_PIN        (27)
#define LCD_D8_PIN        (25)
#define LCD_D9_PIN        (26)
#define LCD_D10_PIN       (12)
#define LCD_D11_PIN       (13)
#define LCD_D12_PIN       (14)
#define LCD_D13_PIN       (15)
#define LCD_D14_PIN       (2)
#define LCD_D15_PIN       (4)
#define LCD_WR_PIN        (18)
#define LCD_RS_PIN        (5)

#define RGB(r, g, b) (((r & 0x1f) << 11) | ((g & 0x3f) << 5) | (b & 0x1f))
#define R(rgb) ((rgb >> 11) & 0x1f)
#define G(rgb) ((rgb >> 5) & 0x3f)
#define B(rgb) (rgb & 0x1f)

#define OFFSET_X (2)
#define OFFSET_Y (2)
#define Y_SIZE (480 - 4)
#define X_SIZE (854 - 4)

#define I2C_NUM (0)
#define I2C_SCL (3)
#define I2C_SDA (1)

static ili9806_handle_t ili9806_handle;

touch_info_t touch_info;

//Touch panel interface init.
static void touch_task(void *param)
{
    i2c_bus_handle_t i2c_bus = NULL;
    ft5x06_handle_t dev = NULL;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_SCL,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 200000,
    };
    i2c_bus = iot_i2c_bus_create(I2C_NUM, &conf);
    dev = iot_ft5x06_create(i2c_bus, FT5X06_ADDR_DEF);
    while (1) {
        memset(&touch_info, 0, sizeof(touch_info_t));
        if (iot_ft5x06_touch_report(dev, &touch_info) == ESP_OK) {
            if (touch_info.touch_point) {
                vTaskDelay(50 / portTICK_PERIOD_MS);
            } else {
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }
        }
    }
}

static void gauss_blur(int idx, uint16_t *src, uint16_t *dst)
{
    uint16_t *now = (uint16_t *)src + OFFSET_X + OFFSET_Y * 854;
    uint16_t *pre1 = now - 854;
    uint16_t *pre2 = pre1 - 854;
    uint16_t *next1 = now + 854;
    uint16_t *next2 = next1 + 854;
    uint32_t r, g, b;
    uint16_t *buf = dst + OFFSET_X + OFFSET_Y * 854;
    uint32_t *tab = blur_tab_5x5[idx];
    for (int i = 0; i < Y_SIZE; i++) {
        for (int j = 0; j < X_SIZE; j++) {
            r = (R(*(pre2 - 2)) + R(*(pre2 + 2)) + R(*(next2 - 2)) + R(*(next2 + 2))) * tab[0] +
                (R(*(pre2 - 1)) + R(*(pre2 + 1)) + R(*(next2 - 1)) + R(*(next2 + 1)) + R(*(pre1 - 2)) + R(*(pre1 + 2)) + R(*(next1 - 2)) + R(*(next1 + 2))) * tab[1] +
                (R(*pre2) + R(*next2) + R(*(now - 2)) + R(*(now + 2))) * tab[2] +
                (R(*(pre1 - 1)) + R(*(pre1 + 1)) + R(*(next1 - 1)) + R(*(next1 + 1))) * tab[3] +
                (R(*pre1) + R(*(now - 1)) + R(*(now + 1)) + R(*(next1))) * tab[4] +
                (R(*now)) * tab[5];

            g = (G(*(pre2 - 2)) + G(*(pre2 + 2)) + G(*(next2 - 2)) + G(*(next2 + 2))) * tab[0] +
                (G(*(pre2 - 1)) + G(*(pre2 + 1)) + G(*(next2 - 1)) + G(*(next2 + 1)) + G(*(pre1 - 2)) + G(*(pre1 + 2)) + G(*(next1 - 2)) + G(*(next1 + 2))) * tab[1] +
                (G(*pre2) + G(*next2) + G(*(now - 2)) + G(*(now + 2))) * tab[2] +
                (G(*(pre1 - 1)) + G(*(pre1 + 1)) + R(*(next1 - 1)) + G(*(next1 + 1))) * tab[3] +
                (G(*pre1) + G(*(now - 1)) + G(*(now + 1)) + G(*(next1))) * tab[4] +
                (G(*now)) * tab[5];

            b = (B(*(pre2 - 2)) + B(*(pre2 + 2)) + B(*(next2 - 2)) + B(*(next2 + 2))) * tab[0] +
                (B(*(pre2 - 1)) + B(*(pre2 + 1)) + B(*(next2 - 1)) + B(*(next2 + 1)) + B(*(pre1 - 2)) + B(*(pre1 + 2)) + B(*(next1 - 2)) + B(*(next1 + 2))) * tab[1] +
                (B(*pre2) + B(*next2) + B(*(now - 2)) + R(*(now + 2))) * tab[2] +
                (B(*(pre1 - 1)) + B(*(pre1 + 1)) + B(*(next1 - 1)) + B(*(next1 + 1))) * tab[3] +
                (B(*pre1) + B(*(now - 1)) + B(*(now + 1)) + B(*(next1))) * tab[4] +
                (B(*now)) * tab[5];

            r = r / 1000000;
            g = g / 1000000;
            b = b / 1000000;
            *buf++ = RGB(r, g, b);
            pre2++;
            pre1++;
            now++;
            next1++;
            next2++;
        }
        buf = buf + 854 - X_SIZE;
        pre2 = pre1 - X_SIZE;
        pre1 = now - X_SIZE;
        now = next1 - X_SIZE;
        next1 = next2 - X_SIZE;
        next2 = next2 + 854 - X_SIZE;
    }
}

void gauss_blur_test(void)
{
    i2s_lcd_config_t i2s_lcd_pin_conf = {
#ifdef CONFIG_BIT_MODE_8BIT
        .data_width = 8,
        .data_io_num = {
            LCD_D0_PIN,  LCD_D1_PIN,  LCD_D2_PIN,  LCD_D3_PIN,
            LCD_D4_PIN,  LCD_D5_PIN,  LCD_D6_PIN,  LCD_D7_PIN,
        },
#else // CONFIG_BIT_MODE_16BIT
        .data_width = 16,
        .data_io_num = {
            LCD_D0_PIN,  LCD_D1_PIN,  LCD_D2_PIN,  LCD_D3_PIN,
            LCD_D4_PIN,  LCD_D5_PIN,  LCD_D6_PIN,  LCD_D7_PIN,
            LCD_D8_PIN,  LCD_D9_PIN,  LCD_D10_PIN, LCD_D11_PIN,
            LCD_D12_PIN, LCD_D13_PIN, LCD_D14_PIN, LCD_D15_PIN,
        },
#endif // CONFIG_BIT_MODE_16BIT
        .ws_io_num = LCD_WR_PIN,
        .rs_io_num = LCD_RS_PIN,
    };
    ili9806_handle = iot_ili9806_create(854, 480, I2S_PORT_NUM, &i2s_lcd_pin_conf);
    ili9806_dev_t *ili9806 = (ili9806_dev_t *)ili9806_handle;
    if (ili9806 == NULL) {
        LCD_LOG("ili9806 create fail!\n");
        return;
    }
    iot_ili9806_set_orientation(ili9806_handle, LCD_DISP_ROTATE_270);
    xTaskCreate(touch_task, "touch_task", 2048, NULL, 10, NULL);
    uint16_t *buff = ili9806->lcd_buf;
    uint16_t *pic = (uint16_t*)gImage_pic3;
    iot_ili9806_draw_bmp(ili9806_handle, pic, 0, 0, 480, 854);
    gauss_blur(8, pic, buff);
    while (1) {
        if (touch_info.touch_point) {
            iot_ili9806_draw_bmp(ili9806_handle, buff, 0, 0, 480, 854);
        } else {
            iot_ili9806_draw_bmp(ili9806_handle, pic, 0, 0, 480, 854);
        }
    }
}

TEST_CASE("Cap gauss blur test", "[Cap TFT test][iot][device]")
{
    gauss_blur_test();
}