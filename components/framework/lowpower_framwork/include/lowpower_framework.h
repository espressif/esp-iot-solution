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

