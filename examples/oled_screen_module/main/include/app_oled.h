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
#ifndef _IOT_EXAMPLE1_H_
#define _IOT_EXAMPLE1_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/gpio.h"
#include "iot_ssd1306.h"
#include "ssd1306_fonts.h"

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

class COled: public CSsd1306
{
public:
    COled(CI2CBus *p_i2c_bus, uint8_t addr = SSD1306_I2C_ADDRESS): CSsd1306(p_i2c_bus, addr)
    {
    }
    ~COled();
    esp_err_t show_time();
    esp_err_t show_humidity(float humidity);
    esp_err_t show_temp(float temprature);
    void show_signs();
    void clean();
    void init();
};
#endif

#endif  
