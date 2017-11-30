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
#ifndef _CH450_H_
#define _CH450_H_
#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>
#include <string.h>
#include "driver/i2c.h"
#include "esp_log.h"
#include "iot_ch450.h"
#include "iot_i2c_bus.h"
typedef void* ch450_handle_t;

typedef enum {
    CH450_SEG_2 = 0x64,
    CH450_SEG_3 = 0x66,
    CH450_SEG_4 = 0x68,
    CH450_SEG_5 = 0x6A,
    CH450_SEG_6 = 0x6C,
    CH450_SEG_7 = 0x6E,
    CH450_SYS = 0x48,
    CH450_READ = 0x4F,
} ch450_cmd_t;

/**
 * @brief Create and init sensor object and return a sensor handle
 *
 * @param bus I2C bus object handle
 *
 * @return
 *     - NULL Fail
 *     - Others Success
 */
ch450_handle_t iot_ch450_create(i2c_bus_handle_t bus);

/**
 * @brief Delete and release a device object
 *
 * @param sensor object handle of ch450
 * @param del_bus Whether to delete the I2C bus
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_ch450_delete(ch450_handle_t dev, bool del_bus);

/**
 * @brief write register of ch450
 *
 * @param dev object handle of ch450
 * @param ch450_cmd command of setting register of ch450
 * @param val register value
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_ch450_write(ch450_handle_t dev, ch450_cmd_t ch450_cmd, uint8_t val);

/**
 * @brief Write number to 7-segment device
 *
 * @param dev object handle of ch450
 * @param seg_idx led index, 0 - 5
 * @param val Digit value
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_ch450_write_num(ch450_handle_t dev, uint8_t seg_idx, uint8_t val);

#ifdef __cplusplus
}
#endif
#endif
