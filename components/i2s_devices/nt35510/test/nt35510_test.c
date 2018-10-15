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
#include <string.h>
#include "unity.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "iot_ft5x06.h"
#include "iot_i2c_bus.h"
#include "iot_nt35510.h"
#include "i2s_lcd_com.h"
#include "ppc.h"

// FT5X06 Config
#define I2C_NUM (0)
#define I2C_SCL (3)
#define I2C_SDA (1)

// NT35510 Config
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

static nt35510_handle_t nt35510_handle;

//Store the touch information
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
        if (iot_ft5x06_touch_report(dev, &touch_info) == ESP_OK) {
            if (touch_info.touch_point) {
                vTaskDelay(50 / portTICK_PERIOD_MS);
            } else {
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }
        }
    }
}

static void pic_slide_tset(void)
{
    uint16_t *raw = NULL;
    uint16_t *buf = NULL;
    uint16_t pox = 0;
    float stepx = 1.0;
    float stepy = 1.0;
    float k = 1.0;
    float m = 1.0;
    uint16_t lenx = 0;
    uint16_t leny = 0;
    uint16_t len = 0;
    uint16_t offsetx = 0;
    uint16_t i, j;
    uint16_t idx = 0;
    uint16_t idy = 0;
    int cnt = 0;
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
    nt35510_handle = iot_nt35510_create(480, 800, I2S_PORT_NUM, &i2s_lcd_pin_conf);
    nt35510_dev_t *device = (nt35510_dev_t *)nt35510_handle;
    if (nt35510_handle == NULL) {
        LCD_LOG("nt35510 create fail!\n");
        return;
    }
    iot_nt35510_set_orientation(nt35510_handle, LCD_DISP_ROTATE_270);
    iot_nt35510_fill_screen(nt35510_handle, 0xaefc);
    xTaskCreate(touch_task, "touch_task", 2048 * 2, NULL, 10, NULL);
    iot_nt35510_draw_bmp(nt35510_handle, (uint16_t *)gImage_pic, 0, 0, 480, 800);
    int refresh = 1;
    while (1) {
        if (touch_info.touch_point == 1) {
            pox = 479 - touch_info.curx[0];
            stepy = 1.0 - 1.0 * pox / 480;
            refresh = 1;
            if (stepy < 0.2) {
                stepy = 0.2;
            }
        } else {
            if (refresh == 1) {
                iot_nt35510_draw_bmp(nt35510_handle, (uint16_t *)gImage_pic, 0, 0, 480, 800);
                refresh = 0;
                continue;
            }
            stepy += 0.4;
            if (stepy >= 1.0) {
                refresh = 1;
                stepy = 1.0;
            }
        }

        leny = stepy * 480;
        len = stepy * 800;
        k = 1.0 / stepy;
        stepx = 1.0 * (800 - len) / leny;
        i = 0;
        while (1) {
            j = 0;
            idy = k * i;
            if (idy > 480) {
                break;
            }
            lenx = stepx * i + len;
            m = 1.0 * 800 / lenx;
            offsetx = (800 - lenx) / 2;
            raw = (uint16_t *)&(gImage_pic[idy * 800 * 2]);
            buf = &(device->lcd_buf[offsetx]);
            for (int i = 0; i < 500; i++) {
                device->lcd_buf[i] = 0xaefb;
            }
            for (int i = 500; i < 800; i++) {
                device->lcd_buf[i] = 0xaefe;
            }
            while (1) {
                idx = m * j;
                if (j > lenx || idx > 800) {
                    break;
                }
                buf[j] = raw[idx];
                j++;
            }
            iot_i2s_lcd_write(device->i2s_lcd_handle, device->lcd_buf, 800 * 2);
            cnt++;
            i++;
        }
        for (int i = 0; i < 500; i++) {
            device->lcd_buf[i] = 0xaefb;
        }
        for (int i = 500; i < 800; i++) {
            device->lcd_buf[i] = 0xaefe;
        }
        for (int c = 0; c < 480 - cnt; c++) {
            iot_i2s_lcd_write(device->i2s_lcd_handle, device->lcd_buf, 800 * 2);
        }
        cnt = 0;
    }
}

TEST_CASE("Cap picture slide test", "[Cap TFT test][iot][device]")
{
    pic_slide_tset();
}
