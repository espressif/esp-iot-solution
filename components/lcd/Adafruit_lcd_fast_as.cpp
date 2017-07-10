/*
This is the subclass graphics library for all our displays, providing a common
set of graphics primitives (points, lines, circles, etc.)

Adafruit invests time and resources providing this open source code, please
support Adafruit & open-source hardware by purchasing products from Adafruit!

Based on ESP8266 library https://github.com/Sermus/ESP8266_Adafruit_lcd

Copyright (c) 2015-2016 Andrey Filimonov.  All rights reserved.

Additions for ESP32 Copyright (c) 2016-2017 Espressif Systems (Shanghai) PTE LTD

Copyright (c) 2013 Adafruit Industries.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
 */


#include "Adafruit_lcd_fast_as.h"
#include "Adafruit_GFX_AS.h"
#include "spi_lcd.h"

/*Rotation Defines*/
#define MADCTL_MY  0x80
#define MADCTL_MX  0x40
#define MADCTL_MV  0x20
#define MADCTL_ML  0x10
#define MADCTL_RGB 0x00
#define MADCTL_BGR 0x08
#define MADCTL_MH  0x04


#define SWAPBYTES(i) ((i>>8) | (i<<8))

Adafruit_lcd::Adafruit_lcd(spi_device_handle_t spi_t) : Adafruit_GFX_AS(LCD_TFTWIDTH, LCD_TFTHEIGHT)
{
    tabcolor = 0;
    spi = spi_t;
}

void Adafruit_lcd::setSpiBus(spi_device_handle_t spi_dev)
{
    spi = spi_dev;
}

void Adafruit_lcd::transmitCmdData(uint8_t cmd, const uint8_t data, uint8_t numDataByte)
{
    lcd_cmd(spi, (const uint8_t) cmd);
    lcd_data(spi, &data, 1);
}

void Adafruit_lcd::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    if ((x < 0) || (x >= _width) || (y < 0) || (y >= _height)) {
        return;
    }
    setAddrWindow(x, y, x + 1, y + 1);
    transmitData(SWAPBYTES(color));
}

void Adafruit_lcd::drawBitmap(int16_t x, int16_t y, const uint16_t *bitmap, int16_t w, int16_t h)
{
    setAddrWindow(x, y, x + w - 1, y + h - 1);
    int point_num = w * h;
    uint16_t* data_buf = (uint16_t*) malloc(point_num * sizeof(uint16_t));
    for (int i = 0; i < point_num; i++) {
        data_buf[i] = SWAPBYTES(bitmap[i]);
    }

    int gap_point = 1024;
    uint16_t* cur_ptr = data_buf;
    while (point_num > 0) {
        int trans_points = point_num > gap_point ? gap_point : point_num;
        transmitData((uint8_t*) (cur_ptr), sizeof(uint16_t) * trans_points);
        cur_ptr += trans_points;
        point_num -= trans_points;
    }
    free(data_buf);
    data_buf = NULL;
}

void Adafruit_lcd::drawBitmapFont(int16_t x, int16_t y, uint8_t w, uint8_t h, const uint16_t *bitmap)
{
    //Saves some memory and SWAPBYTES as compared to above API
    setAddrWindow(x, y, x + w - 1, y + h - 1);
    transmitData((uint8_t*) bitmap, sizeof(uint16_t) * w * h);
}

void Adafruit_lcd::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{
    // Rudimentary clipping
    if ((x >= _width) || (y >= _height)) {
        return;
    }
    if ((y + h - 1) >= _height) {
        h = _height - y;
    }
    setAddrWindow(x, y, x, y + h - 1);
    transmitData(SWAPBYTES(color), h);
}

void Adafruit_lcd::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
{
    // Rudimentary clipping
    if ((x >= _width) || (y >= _height)) {
        return;
    }
    if ((x + w - 1) >= _width) {
        w = _width - x;
    }
    setAddrWindow(x, y, x + w - 1, y);
    transmitData(SWAPBYTES(color), w);
}

void Adafruit_lcd::fillScreen(uint16_t color)
{
    fillRect(0, 0, _width, _height, color);
}

void Adafruit_lcd::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    // rudimentary clipping (drawChar w/big text requires this)
    if ((x >= _width) || (y >= _height)) {
        return;
    }
    if ((x + w - 1) >= _width) {
        w = _width - x;
    }
    if ((y + h - 1) >= _height) {
        h = _height - y;
    }
    setAddrWindow(x, y, x + w - 1, y + h - 1);
    transmitData(SWAPBYTES(color), h * w);
}

uint16_t Adafruit_lcd::color565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void Adafruit_lcd::scrollTo(uint16_t y)
{
    transmitCmd(0x37);
    transmitData(y);
}

void Adafruit_lcd::setRotation(uint8_t m)
{
    uint8_t data = 0;
    rotation = m % 4;  //Can't be more than 3
    switch (rotation) {
        case 0:
            data = MADCTL_MX | MADCTL_BGR;
            _width = LCD_TFTWIDTH;
            _height = LCD_TFTHEIGHT;
            break;
        case 1:
            data = MADCTL_MV | MADCTL_BGR;
            _width = LCD_TFTHEIGHT;
            _height = LCD_TFTWIDTH;
            break;
        case 2:
            data = MADCTL_MY | MADCTL_BGR;
            _width = LCD_TFTWIDTH;
            _height = LCD_TFTHEIGHT;
            break;
        case 3:
            data = MADCTL_MX | MADCTL_MY | MADCTL_MV | MADCTL_BGR;
            _width = LCD_TFTHEIGHT;
            _height = LCD_TFTWIDTH;
            break;
    }
    transmitCmdData(LCD_MADCTL, data, 1);
}

void Adafruit_lcd::invertDisplay(bool i)
{
    transmitCmd(i ? LCD_INVON : LCD_INVOFF);
}
