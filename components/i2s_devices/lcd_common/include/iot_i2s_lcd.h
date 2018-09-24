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
#ifndef __IOT_I2S_H__
#define __IOT_I2S_H__

#include "driver/i2s.h"
#include "esp_err.h"
#include <esp_types.h>

typedef struct {
    int     data_width;          /*!< Parallel data width, 16bit or 8bit available */
    uint8_t data_io_num[16];     /*!< Parallel data output IO*/
    int     ws_io_num;           /*!< write clk io*/
    int     rs_io_num;           /*!< rs io num */
} i2s_lcd_config_t;

typedef struct {
    i2s_lcd_config_t i2s_lcd_conf;/*!<I2S lcd parameters */
    i2s_port_t i2s_port;          /*!<I2S port number */
} i2s_lcd_t;

typedef void* i2s_lcd_handle_t;

typedef enum lcd_orientation{
    LCD_DISP_ROTATE_0 = 0,
    LCD_DISP_ROTATE_90 = 1,
    LCD_DISP_ROTATE_180 = 2,
    LCD_DISP_ROTATE_270 = 3,
} lcd_orientation_t;

/**
 * @brief I2S lcd bus create
 *
 * @param i2s_num i2s_port_t
 * @param pin_conf i2s pin configuration
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Failed
 */
i2s_lcd_handle_t i2s_lcd_create(i2s_port_t i2s_num, i2s_lcd_config_t *pin_conf);

/**
 * @brief Write byte data.
 *
 * @param i2s_lcd i2s_lcd_handle_t
 * @param src will write data
 * @param size data length
 * @param ticks_to_wait The maximum amount of time the task should block
 * waiting for an item to receive should the queue be empty at the time
 * of the call.	 xQueueReceive() will return immediately if xTicksToWait
 * is zero and the queue is empty.  The time is defined in tick periods so the
 * constant portTICK_PERIOD_MS should be used to convert to real time if this is
 * required.
 * @param swap swap high/low byte
 * 
 * @return
 *      - write data length
 */
int i2s_lcd_write_data(i2s_lcd_handle_t i2s_lcd, const char *src, size_t size, TickType_t ticks_to_wait, bool swap);

#endif //__IOT_I2S_H__
