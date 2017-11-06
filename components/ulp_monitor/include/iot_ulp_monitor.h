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