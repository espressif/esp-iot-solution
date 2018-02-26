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

void CSsd1306::draw_num(uint8_t chXpos, uint8_t chYpos,
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
