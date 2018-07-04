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
#include "lwip/api.h"
#include "bitmap.h"
#include "app_touch.h"
#include "iot_lcd.h"

typedef struct
{
    uint8_t frame_num;
} camera_evt_t;

CEspLcd* tft = NULL;

void lcd_display_top_left()
{
    tft->drawBitmap(0, 0, arrow_left_top, 48, 48);
    tft->drawString("please input top-left button...", 5, 64);
}

void lcd_display_top_right()
{
    tft->drawBitmap(320 - 48, 0, arrow_right_top, 48, 48);
    tft->drawString("please input top-right button...", 5, 78);
}

void lcd_display_bottom_right()
{
    tft->drawBitmap(320 - 48, 240 - 48, arrow_right_bottom, 48, 48);
    tft->drawString("please input bottom-right button...", 5, 92);
}

void lcd_display_bottom_left()
{
    tft->drawBitmap(0, 240 - 48, arrow_left_bottom, 48, 48);
    tft->drawString("please input bottom-left button...", 5, 106);
}

void lcd_change_text_size(int size)
{
    tft->setTextSize(size);
}

void lcd_display_coordinate(int x, int y)
{
    tft->fillRect(15, 10, 17, 17, COLOR_WHITE);
    tft->drawNumber(x, 15, 10);
    tft->drawNumber(y, 15, 19);
}

void lcd_display_point(int x, int y)
{
    tft->fillRect(x, 240 - y, 3, 3, COLOR_BLACK);
}

void clear_screen()
{
    tft->fillScreen(COLOR_WHITE);
}

void lcd_tft_init()
{
    lcd_conf_t lcd_pins = {
        .lcd_model = LCD_MOD_ILI9341,
        .pin_num_miso = CONFIG_UT_LCD_MISO_GPIO,
        .pin_num_mosi = CONFIG_UT_LCD_MOSI_GPIO,
        .pin_num_clk = CONFIG_UT_LCD_CLK_GPIO,
        .pin_num_cs = CONFIG_UT_LCD_CS_GPIO,
        .pin_num_dc = CONFIG_UT_LCD_DC_GPIO,
        .pin_num_rst = CONFIG_UT_LCD_RESET_GPIO,
        .pin_num_bckl = CONFIG_UT_LCD_BL_GPIO,
        .clk_freq = 10 * 1000 * 1000,
        .rst_active_level = 0,
        .bckl_active_level = 1,
        .spi_host = HSPI_HOST,
        .init_spi_bus = true,
    };

    /*Initialize SPI Handler*/
    if (tft == NULL) {
        tft = new CEspLcd(&lcd_pins);
    }

    /*screen initialize*/
    tft->invertDisplay(false);
    tft->setRotation(3);
    tft->fillScreen(COLOR_WHITE);
    tft->setTextSize(1);
    tft->setTextColor(COLOR_BLACK, COLOR_WHITE);
    tft->drawString("Status: Initialize touch ...", 5, 50);
}

