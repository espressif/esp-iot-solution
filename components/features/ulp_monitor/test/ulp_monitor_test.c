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
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "iot_ulp_monitor.h"
#include <time.h>
#include <sys/time.h>
#include "esp32/ulp.h"
#include "driver/adc.h"
#include "driver/dac.h"
#include "unity.h"

#define ULP_PROGRAM_ADDR    0
#define ULP_DATA_ADDR       80
#define ADC_DATA_OFFSET     0
#define ADC_DATA_NUM        5
#define TEMP_DATA_OFFSET    10
#define TEMP_DATA_NUM       5
static RTC_DATA_ATTR struct timeval sleep_enter_time;

void ulp_monitor_test()
{
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_ULP) {
         struct timeval now;
        gettimeofday(&now, NULL);
        int sleep_time_ms = (now.tv_sec - sleep_enter_time.tv_sec) * 1000 + (now.tv_usec - sleep_enter_time.tv_usec) / 1000;
        ESP_LOGI("ulp_monitor_test", "Time speet in deep sleep: %dms", sleep_time_ms);
        for (int i = 0; i < ADC_DATA_NUM+2; i++) {
            printf("the data %d of adc is:%d\n", i, iot_ulp_data_read(ULP_DATA_ADDR + ADC_DATA_OFFSET + i));
        }
        for (int i = 0; i < TEMP_DATA_NUM+2; i++) {
            printf("the data %d of temprature is:%d\n", i, iot_ulp_data_read(ULP_DATA_ADDR + TEMP_DATA_OFFSET + i));
        }
    }
    dac_output_enable(DAC_CHANNEL_1);
    dac_output_voltage(DAC_CHANNEL_1, 127);
    ESP_LOGI("ulp_monitor_test", "ulp deep sleep wakeup test");
    memset(RTC_SLOW_MEM, 0, CONFIG_ULP_COPROC_RESERVE_MEM);
    iot_ulp_monitor_init(ULP_PROGRAM_ADDR, ULP_DATA_ADDR);
    //todo: to use the new-style definition of ADC_WIDTH_12Bit.
    adc1_config_width(ADC_WIDTH_12Bit);
    //todo: to use the new-style definition of ADC_ATTEN_11db
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_11db);
    iot_ulp_add_adc_monitor(ADC1_CHANNEL_6, 10, 4000, ADC_DATA_OFFSET, ADC_DATA_NUM, true);
    //iot_ulp_add_temprature_monitor(0, 300, TEMP_DATA_OFFSET, TEMP_DATA_NUM, true);
    iot_ulp_monitor_start(3600 / 10);
    gettimeofday(&sleep_enter_time, NULL);
    esp_deep_sleep_start();
}

TEST_CASE("ULP monitor test", "[ulp_monitor][iot][ulp]")
{
    ulp_monitor_test();
}
