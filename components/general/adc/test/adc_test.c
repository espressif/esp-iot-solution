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
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iot_adc.h"
#include "unity.h"

#define EVB_ADC_CHANNEL ADC_CHANNEL_0     /* GPIO36 */
#define EVB_ADC_ATTEN   ADC_ATTEN_DB_11
#define EVB_ADC_UNIT    ADC_UNIT_1
#define EVB_ADC_BIT_WIDTH  ADC_WIDTH_BIT_12

static adc_handle_t adc_dev = NULL;

void adc_test()
{
    adc_dev = iot_adc_create(EVB_ADC_UNIT, EVB_ADC_CHANNEL, EVB_ADC_ATTEN, EVB_ADC_BIT_WIDTH);

    for (size_t i = 0; i < 16; i++) {
        uint32_t voltage_mv = iot_adc_get_voltage(adc_dev);
        printf("Voltage obtained by ADC: %d mV\n", voltage_mv);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    iot_adc_delete(adc_dev);
}

TEST_CASE("ADC test", "[adc][iot]")
{
    adc_test();
}

