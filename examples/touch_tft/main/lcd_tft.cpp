/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
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
#include "lwip/api.h"
#include "bitmap.h"
#include "include/app_touch.h"
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
        .lcd_model = LCD_MOD_ST7789, //LCD_MOD_ILI9341,//LCD_MOD_ST7789,
        .pin_num_miso = CONFIG_HW_LCD_MISO_GPIO,
        .pin_num_mosi = CONFIG_HW_LCD_MOSI_GPIO,
        .pin_num_clk = CONFIG_HW_LCD_CLK_GPIO,
        .pin_num_cs = CONFIG_HW_LCD_CS_GPIO,
        .pin_num_dc = CONFIG_HW_LCD_DC_GPIO,
        .pin_num_rst = CONFIG_HW_LCD_RESET_GPIO,
        .pin_num_bckl = CONFIG_HW_LCD_BL_GPIO,
        .clk_freq = 10 * 1000 * 1000,
        .rst_active_level = 0,
        .bckl_active_level = 1,
        .spi_host = HSPI_HOST,
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

