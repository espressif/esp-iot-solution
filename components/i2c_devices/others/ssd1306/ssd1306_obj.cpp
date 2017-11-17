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

#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "iot_i2c_bus.h"
#include "include/iot_ssd1306.h"

CSsd1306::CSsd1306(CI2CBus *p_i2c_bus, uint8_t addr)
{
    bus = p_i2c_bus;
    m_dev_handle = iot_ssd1306_create(bus->get_bus_handle(), addr);
}

CSsd1306::~CSsd1306()
{
    iot_ssd1306_delete(m_dev_handle, false);
    m_dev_handle = NULL;
}

void CSsd1306::draw_point(uint8_t chXpos, uint8_t chYpos,
        uint8_t chPoint)
{
    iot_ssd1306_fill_point(m_dev_handle, chXpos, chYpos, chPoint);
}

esp_err_t CSsd1306::fill_rectangle_screen(uint8_t chXpos1,
        uint8_t chYpos1, uint8_t chXpos2, uint8_t chYpos2, uint8_t chDot)
{
    return iot_ssd1306_fill_rectangle(m_dev_handle, chXpos1, chYpos1,
            chXpos2, chYpos2, chDot);
}

void CSsd1306::draw_char(uint8_t chXpos, uint8_t chYpos,
        uint8_t chChr, uint8_t chSize, uint8_t chMode)
{
    iot_ssd1306_draw_char(m_dev_handle, chXpos, chYpos, chChr, chSize,
            chMode);
}

void CSsd1306::display_num(uint8_t chXpos, uint8_t chYpos,
        uint32_t chNum, uint8_t chLen, uint8_t chSize)
{
    iot_ssd1306_draw_num(m_dev_handle, chXpos, chYpos, chNum, chLen, chSize);
}

void CSsd1306::draw_1616char(uint8_t chXpos, uint8_t chYpos,
        uint8_t chChar)
{
    iot_ssd1306_draw_1616char(m_dev_handle, chXpos, chYpos, chChar);
}

void CSsd1306::draw_3216char(uint8_t chXpos, uint8_t chYpos,
        uint8_t chChar)
{
    iot_ssd1306_draw_3216char(m_dev_handle, chXpos, chYpos, chChar);
}

void CSsd1306::draw_bitmap(uint8_t chXpos, uint8_t chYpos,
        const uint8_t *pchBmp, uint8_t chWidth, uint8_t chHeight)
{
    iot_ssd1306_draw_bitmap(m_dev_handle, chXpos, chYpos, pchBmp, chWidth,
            chHeight);
}

esp_err_t CSsd1306::refresh_gram(void)
{
    return iot_ssd1306_refresh_gram(m_dev_handle);
}

esp_err_t CSsd1306::clear_screen(uint8_t chFill)
{
    return iot_ssd1306_clear_screen(m_dev_handle, chFill);
}

esp_err_t CSsd1306::draw_string(uint8_t chXpos, uint8_t chYpos,
        const uint8_t *pchString, uint8_t chSize, uint8_t chMode)
{
    return iot_ssd1306_draw_string(m_dev_handle, chXpos, chYpos, pchString,
            chSize, chMode);
}

ssd1306_handle_t CSsd1306::get_dev_handle()
{
    return m_dev_handle;
}
