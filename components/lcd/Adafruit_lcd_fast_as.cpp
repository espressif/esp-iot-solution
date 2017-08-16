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
#include "Font7s.h"

/*Rotation Defines*/
#define MADCTL_MY  0x80
#define MADCTL_MX  0x40
#define MADCTL_MV  0x20
#define MADCTL_ML  0x10
#define MADCTL_RGB 0x00
#define MADCTL_BGR 0x08
#define MADCTL_MH  0x04


#define SWAPBYTES(i) ((i>>8) | (i<<8))

Adafruit_lcd::Adafruit_lcd(spi_device_handle_t spi_t, bool dma_en, int dma_word_size) : Adafruit_GFX_AS(LCD_TFTWIDTH, LCD_TFTHEIGHT)
{
    tabcolor = 0;
    spi = spi_t;
    dma_mode = dma_en;
    dma_buf_size = dma_word_size;
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

void Adafruit_lcd::_fastSendBuf(const uint16_t* buf, int point_num, bool swap)
{
    if ((point_num * sizeof(uint16_t)) <= (16 * sizeof(uint32_t))) {
        transmitData((uint8_t*) buf, sizeof(uint16_t) * point_num);
    } else {
        int gap_point = dma_buf_size;
        uint16_t* data_buf = (uint16_t*) malloc(gap_point * sizeof(uint16_t));
        int offset = 0;
        while (point_num > 0) {
            int trans_points = point_num > gap_point ? gap_point : point_num;

            if (swap) {
                for (int i = 0; i < trans_points; i++) {
                    data_buf[i] = SWAPBYTES(buf[i + offset]);
                }
            } else {
                memcpy((uint8_t*) data_buf, (uint8_t*) (buf + offset), trans_points * sizeof(uint16_t));
            }
            transmitData((uint8_t*) (data_buf), trans_points * sizeof(uint16_t));
            offset += trans_points;
            point_num -= trans_points;
        }
        free(data_buf);
        data_buf = NULL;
    }
}

void Adafruit_lcd::_fastSendRep(uint16_t val, int rep_num)
{
    int point_num = rep_num;
    int gap_point = dma_buf_size;
    gap_point = (gap_point > point_num ? point_num : gap_point);
    uint16_t* data_buf = (uint16_t*) malloc(gap_point * sizeof(uint16_t));
    int offset = 0;
    while (point_num > 0) {
        for (int i = 0; i < gap_point; i++) {
            data_buf[i] = val;
        }
        int trans_points = point_num > gap_point ? gap_point : point_num;
        transmitData((uint8_t*) (data_buf), sizeof(uint16_t) * trans_points);
        offset += trans_points;
        point_num -= trans_points;
    }
    free(data_buf);
    data_buf = NULL;
}

void Adafruit_lcd::drawBitmap(int16_t x, int16_t y, const uint16_t *bitmap, int16_t w, int16_t h)
{
    setAddrWindow(x, y, x + w - 1, y + h - 1);
    if (dma_mode) {
        _fastSendBuf(bitmap, w * h);
    } else {
        for (int i = 0; i < w * h; i++) {
            transmitData(SWAPBYTES(bitmap[i]), 1);
        }
    }
}

void Adafruit_lcd::drawBitmapFont(int16_t x, int16_t y, uint8_t w, uint8_t h, const uint16_t *bitmap)
{
    //Saves some memory and SWAPBYTES as compared to above API
    setAddrWindow(x, y, x + w - 1, y + h - 1);
    if (dma_mode) {
        _fastSendBuf(bitmap, w * h, false);
    } else {
        transmitData((uint8_t*) bitmap, sizeof(uint16_t) * w * h);
    }
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
    if (dma_mode) {
        _fastSendRep(SWAPBYTES(color), h);
    } else {
        transmitData(SWAPBYTES(color), h);
    }
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
    if (dma_mode) {
        _fastSendRep(SWAPBYTES(color), w);
    } else {
        transmitData(SWAPBYTES(color), w);
    }
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
    if (dma_mode) {
        _fastSendRep(SWAPBYTES(color), h * w);
    } else {
        transmitData(SWAPBYTES(color), h * w);
    }

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


int Adafruit_lcd::drawFloatSevSeg(float floatNumber, uint8_t decimal, uint16_t poX, uint16_t poY, uint8_t size)
{
    unsigned int temp = 0;
    float decy = 0.0;
    float rounding = 0.5;
    float eep = 0.000001;
    int sumX = 0;
    uint16_t xPlus = 0;

    if (floatNumber - 0.0 < eep) {
        xPlus = drawUnicodeSevSeg('-', poX, poY, size);
        floatNumber = -floatNumber;
        poX  += xPlus;
        sumX += xPlus;
    }

    for (unsigned char i = 0; i < decimal; ++i) {
        rounding /= 10.0;
    }
    floatNumber += rounding;
    temp = (long)floatNumber;
    xPlus = drawNumberSevSeg(temp, poX, poY, size);
    poX  += xPlus;
    sumX += xPlus;

    if (decimal > 0) {
        xPlus = drawUnicodeSevSeg('.', poX, poY, size);
        poX += xPlus;                                       /* Move cursor right   */
        sumX += xPlus;
    } else {
        return sumX;
    }

    decy = floatNumber - temp;
    for (unsigned char i = 0; i < decimal; i++) {
        decy *= 10;                                         /* For the next decimal*/
        temp = decy;                                        /* Get the decimal     */
        xPlus = drawNumberSevSeg(temp, poX, poY, size);
        poX += xPlus;                                       /* Move cursor right   */
        sumX += xPlus;
        decy -= temp;
    }
    return sumX;
}

int Adafruit_lcd::drawUnicodeSevSeg(uint16_t uniCode, uint16_t x, uint16_t y, uint8_t size)
{
    if (size) {
        uniCode -= 32;
    }
    uint16_t width = 0;
    uint16_t height = 0;
    const uint8_t *flash_address = 0;
    int8_t gap = 0;

    if (size == 7) {
        flash_address = chrtbl_f7s[uniCode];
        width = *(widtbl_f7s + uniCode);
        height = CHR_HGT_F7S;
        gap = 2;
    }

    uint16_t w = (width + 7) / 8;
    uint8_t line = 0;

    setAddrWindow(x, y, x + w * 8 - 1, y + height - 1);
    uint16_t* data_buf = (uint16_t*) malloc(dma_buf_size * sizeof(uint16_t));
    int point_num = w * height * 8;
    int idx = 0;
    int trans_points = point_num > dma_buf_size ? dma_buf_size : point_num;
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < w; j++) {
            line = *(flash_address + w*i+j);
            for(int m = 0; m < 8; m++) {
                if ((line >> (7-m)) & 0x1) {
                    data_buf[idx++] = SWAPBYTES(textcolor);
                } else {
                    data_buf[idx++] = SWAPBYTES(textbgcolor);
                }

                if(idx >= trans_points) {
                    transmitData((uint8_t*) (data_buf), trans_points * sizeof(uint16_t));
                    point_num -= trans_points;
                    idx = 0;
                    trans_points = point_num > dma_buf_size ? dma_buf_size : point_num;
                }

            }
        }
    }
    free(data_buf);
    data_buf = NULL;
    return width + gap;
}

int Adafruit_lcd::drawStringSevSeg(const char *string, uint16_t poX, uint16_t poY, uint8_t size)
{
    uint16_t sumX = 0;
    while (*string) {
        uint16_t xPlus = drawUnicodeSevSeg(*string, poX, poY, size);
        sumX += xPlus;
        string++;
        poX += xPlus;                                     /* Move cursor right*/
    }
    return sumX;
}

int Adafruit_lcd::drawNumberSevSeg(int long_num, uint16_t poX, uint16_t poY, uint8_t size)
{
    char tmp[10];
    if (long_num < 0) {
        snprintf(tmp, sizeof(tmp), "%d", long_num);
    } else {
        snprintf(tmp, sizeof(tmp), "%u", long_num);
    }
    return drawStringSevSeg(tmp, poX, poY, size);
}

