/* Lowpower EVB Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#ifndef _IOT_LOWPOWER_EVB_SENSOR_H_
#define _IOT_LOWPOWER_EVB_SENSOR_H_

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief power on sensor.
 */
void sensor_power_on();

/**
 * @brief initialize sensor.
 */
void sensor_init();

/**
 * @brief get humidity value of hts221.
 *
 * @return
 *     - humidity value.
 */
float sensor_hts221_get_hum();

/**
 * @brief get temperature value of hts221.
 *
 * @return
 *     - temperature value.
 */
float sensor_hts221_get_temp();

/**
 * @brief get luminance value of bh1750.
 *
 * @return
 *     - luminance value.
 */
float sensor_bh1750_get_lum();

/**
 * @brief power on sensor by RTC IO.
 */
void rtc_sensor_power_on(void);

/**
 * @brief initialize RTC IO that sensor used.
 */
void rtc_sensor_io_init(void);

#ifdef __cplusplus
}
#endif

#endif

