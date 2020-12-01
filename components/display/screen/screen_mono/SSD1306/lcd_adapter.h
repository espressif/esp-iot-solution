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
#ifndef __IOT_UGFX_LCD_ADAPTER_H__
#define __IOT_UGFX_LCD_ADAPTER_H__

/* C Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* RTOS Includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Lcd initlize.
 */
void board_lcd_init();

/**
 * @brief Flush the specified area at (x,y),
 *        the width of the specified area is w, the height of the specified area is h.
 *
 * @param x The ordinate of the starting point of the specified area.
 * @param y The abscissa of the starting point of the specified region.
 * @param bitmap Flush data for the specified area.
 * @param w the width of the specified area.
 * @param h the height of the specified area.
 */
void board_lcd_flush(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h);

/**
 * @brief Write cmd to lcd.
 *
 * @param cmd will write command
 */
void board_lcd_write_cmd(uint8_t cmd);

/**
 * @brief Write data to lcd.
 *
 * @param data will write data
 */
void board_lcd_write_data(uint8_t data);

/**
 * @brief Write some data to lcd
 *
 * @param data Pointer to write data
 * @param length The size of the data to be written
 */
void board_lcd_write_datas(uint8_t *data, uint16_t length);

/**
 * @brief Set backlight of lcd
 *
 * @param data backlight value
 */
void board_lcd_set_backlight(uint16_t data);

#ifdef __cplusplus
}
#endif

#endif /* __IOT_UGFX_LCD_ADAPTER_H__ */
