/* Lowpower EVB Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#ifndef _IOT_LOWPOWER_EVB_ADC_H_
#define _IOT_BATTERY_EB_ADC_H_

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief initialize ADC.
  */
void adc_init();

/**
  * @brief get supply voltage.
  *
  * @return 
  *     - the value of sulpply_voltage. uint:mV
  */
uint32_t adc_get_supply_voltage();

#ifdef __cplusplus
}
#endif

#endif

