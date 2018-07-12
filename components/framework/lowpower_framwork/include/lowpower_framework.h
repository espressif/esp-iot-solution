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

#ifndef _IOT_LOWPOWER_FRAMEWORK_H_
#define _IOT_LOWPOWER_FRAMEWORK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"

/**
  * @brief start lowpower application framework.
  */
void lowpower_framework_start();

/**
  * @brief lowpower application callback type.
  */
typedef enum {
    LPFCB_DEVICE_INIT = 0,           /*!<lowpower device initialize, called when power on*/
    LPFCB_WIFI_CONNECT,              /*!<config wifi and connect to AP, called after lowpower device intialize*/
    LPFCB_GET_DATA_BY_CPU,           /*!<use cpu to read sensor data, called when wakeup except for ulp wakeup*/
    LPFCB_ULP_PROGRAM_INIT,          /*!<initialize ULP program, we need to load program into ULP, called before start deepsleep*/
    LPFCB_GET_DATA_FROM_ULP,         /*!<get data from ULP, called when ULP wakeup*/
    LPFCB_SEND_DATA_TO_SERVER,       /*!<connect to AP and send data to server, called after acquired data*/
    LPFCB_SEND_DATA_DONE,            /*!<called when send data success*/
    LPFCB_START_DEEP_SLEEP,          /*!<set wakeup cause and start deepsleep*/
} lowpower_framework_cb_t;

/**
  * @brief register callback
  *
  * @param cb_type callback type.
  * @param cb_func callback function pointer.
  *
  * @return 
  *     - ESP_OK success
  *     - ESP_FAIL fail
  */
esp_err_t lowpower_framework_register_callback(lowpower_framework_cb_t cb_type, void *cb_func);

#ifdef __cplusplus
}
#endif

#endif

