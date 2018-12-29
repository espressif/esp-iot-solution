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
#ifndef __IOT_UGFX_TOUCH_SCREEN_ADAPTER_H__
#define __IOT_UGFX_TOUCH_SCREEN_ADAPTER_H__

/* C Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* RTOS Includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

/* SPI Includes */
#include "iot_xpt2046.h"

#define XPT2046_CLK_FREQ (2 * 1000 * 1000)

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief Touch initlize.
  */
void board_touch_init();

/**
  * @brief Get touch whether to press.
  *
  * @return
  *     - true Pressed
  *     - false Not pressed
  */
bool board_touch_is_pressed();

/**
  * @brief Get position of pressed
  *
  * @param port the specified direction of position with port
  *
  * @return
  *     - int the pressed position of specified direction
  */
int board_touch_get_position(int port);

#ifdef __cplusplus
}
#endif

#endif
