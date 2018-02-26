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
#ifndef _IOT_XPT_PRIV_H
#define _IOT_XPT_PRIV_H

#include "string.h"
#include "stdio.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_partition.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "iot_xpt2046.h"
#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief   xpt2046 device initlize
 *
 * @param   xpt_conf pointer of xpt_conf_t
 * @param   spi pointer of spi_device_handle_t
 *
 * @return
 *     - void
 */
void iot_xpt2046_init(xpt_conf_t * xpt_conf, spi_device_handle_t * spi);

/**
 * @brief   read xpt2046 data
 *
 * @param   spi spi_handle_t
 * @param   command command of read data
 * @param   len command len
 *
 * @return
 *     - int x position
 */
uint16_t iot_xpt2046_readdata(spi_device_handle_t spi, const uint8_t command,
                              int len);

#ifdef __cplusplus
}
#endif

#endif
