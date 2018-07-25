/* Lowpower EVB Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#ifndef _IOT_LOWPOWER_EVB_ULP_OPT_H_
#define _IOT_LOWPOWER_EVB_ULP_OPT_H_

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief load ULP program into RTC memory, should be called once after power-on reset.
 */
void init_ulp_program();

/**
 * @brief It starts the ULP program and resets measurement counter, should be 
 *        called every time before going into deep sleep.
 */
void start_ulp_program();

/**
 * @brief set sensor value number that ulp need to read in once deepsleep.
 *
 * @param value_number value number of sensor.
 */
void set_ulp_read_value_number(uint16_t value_number);

/**
 * @brief set interval of ulp read sensor.
 *
 * @time_ms time interval(ms).
 */
void set_ulp_read_interval(uint32_t time_ms);

/**
 * @brief get humidity value from RTC memory.
 *
 * @param hum humidity value pointer.
 * @param value_num humidity value number.
 */
void get_ulp_hts221_humidity(float hum[], uint16_t value_num);

/**
 * @brief get temperature value from RTC memory.
 *
 * @param temp temperature value pointer.
 * @param value_num temperature value number.
 */
void get_ulp_hts221_temperature(float temp[], uint16_t value_num);

/**
 * @brief get luminance value from RTC memory.
 *
 * @param lum luminance value pointer.
 * @param value_num luminance value number.
 */
void get_ulp_bh1750_luminance(float lum[], uint16_t value_num);

#ifdef __cplusplus
}
#endif

#endif

