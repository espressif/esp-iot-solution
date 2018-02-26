/* Lowpower EVB Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#ifndef _IOT_LOWPOWER_EVB_EPAPER_H_
#define _IOT_LOWPOWER_EVB_EPAPER_H_

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief initialize epaper.
 */
void epaper_display_init();

/**
 * @brief display the sensor data and supply voltage.
 *
 * @param hum humidity value.
 * @param temp temperature value.
 * @param lum luminance value.
 * @param supply_voltage supply voltage value.
 */
void epaper_show_page(float hum, float temp, float lum, float supply_voltage);

/**
 * @brief disable epaper.
 */
void epaper_disable();

#ifdef __cplusplus
}
#endif

#endif

