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

#ifndef _IOT_SOCKET_CONFIG_H_
#define _IOT_SOCKET_CONFIG_H_
#include "driver/gpio.h"

#define SOCKET_NAME_SPACE   "socket"
#define SOCKET_PARAM_KEY    "socket_param"

#define PM_CF_IO_NUM    25
#define PM_CFI_IO_NUM   26
#define PM_POWER_PARAM  1293699
#define PM_VOLTAGE_PARAM    102961
#define PM_CURRENT_PARAM    13670

#define BUTTON_IO_NUM_MAIN     4
#define BUTTON_ACTIVE_LEVEL   0

#define RELAY_CLOSE_LEVEL   RELAY_CLOSE_HIGH
#define RELAY_CONTROL_MODE  RELAY_GPIO_CONTROL
#define RELAY_IO_MODE       RELAY_IO_NORMAL

#define LED_DARK_LEVEL  LED_DARK_LOW
#define NET_LED_NUM     GPIO_NUM_19

#define SOCKET_UNIT_NUM     2
static const gpio_num_t button_io[SOCKET_UNIT_NUM] = {GPIO_NUM_5, GPIO_NUM_2};
static const gpio_num_t led_io[SOCKET_UNIT_NUM] = {GPIO_NUM_17, GPIO_NUM_18};
static const gpio_num_t relay_ctl_io[SOCKET_UNIT_NUM] = {GPIO_NUM_12, GPIO_NUM_13};
static const gpio_num_t relay_clk_io[SOCKET_UNIT_NUM] = {};
#endif