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
#ifndef _IOT_ULP_MONITOR_H_
#define _IOT_ULP_MONITOR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/adc.h"

/**
  * @brief  initialize deep sleep ulp monitor
  *
  * @param  program_addr the RTC slow memory address of ulp program
  * @param  data_addr the RTC slow memory address to store data
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_ulp_monitor_init(uint16_t program_addr, uint16_t data_addr);

/**
  * @brief  read adc sample value in ulp
  *
  * @param  adc_chn the adc channel to read
  * @param  low_threshold low threshold of adc value, the ulp would wake up the chip if adc value is lower than this threshold
  * @param  high_threshold high threshold of adc value
  * @param  data_offset the adc value would be stored at rtc slow memory from data_addr+data_offset to data_addr+data_offset+data_num
  * @param  data_num the max number of adc value can be stored in rtc slow memory
  * @param  num_max_wake decide whether to wake up the chip if max numbers of adc value are stored 
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_ulp_add_adc_monitor(adc1_channel_t adc_chn, int16_t low_threshold, int16_t high_threshold, uint8_t data_offset, uint8_t data_num, bool num_max_wake);

/**
  * @brief  read temprature sensor in ulp
  *
  * @param  low_threshold low threshold of temprature value, the ulp would wake up the chip if temprature value is lower than this threshold
  * @param  high_threshold high threshold of temprature value
  * @param  data_offset the temprature value would be stored at rtc slow memory from data_addr+data_offset to data_addr+data_offset+data_num
  * @param  data_num the max number of temprature value can be stored in rtc slow memory
  * @param  num_max_wake decide whether to wake up the chip if max numbers of temprature value are stored 
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_ulp_add_temprature_monitor(int16_t low_threshold, int16_t high_threshold, uint8_t data_offset, uint8_t data_num, bool num_max_wake);

/**
  * @brief  start running the ulp program
  *
  * @param  meas_per_hour the number of ulp measurement per seconds
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_ulp_monitor_start(uint32_t meas_per_hour);

/**
  * @brief  read value from RTC slow memory
  *
  * @param  addr the address you want to read from
  *
  * @return
  */
uint16_t iot_ulp_data_read(size_t addr);

/**
  * @brief  write value to RTC slow memory
  *
  * @param  addr the address you want to write at
  * @param  value the value to write
  *
  * @return
  */
void iot_ulp_data_write(size_t addr, uint16_t value);

#ifdef __cplusplus
}
#endif
#endif
