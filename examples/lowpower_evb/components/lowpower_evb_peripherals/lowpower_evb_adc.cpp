/* Lowpower EVB Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include "driver/adc.h"
#include "lowpower_evb_adc.h"

/* ADC channel to get voltage */
#define ADC_CHANNEL_GET_VOLTAGE     ADC1_CHANNEL_0

void adc_init()
{
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
    adc1_config_width(ADC_WIDTH_BIT_12);
}

float adc_get_supply_voltage()
{
    int adc_value = adc1_get_raw(ADC_CHANNEL_GET_VOLTAGE);
    float supply_voltage = 3 * 3.6 * (float)adc_value / 4096;
    return supply_voltage;
}
