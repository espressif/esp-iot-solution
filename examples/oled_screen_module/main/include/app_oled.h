/* OLED screen Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */
#ifndef _APP_OLED_H_
#define _APP_OLED_H_

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
