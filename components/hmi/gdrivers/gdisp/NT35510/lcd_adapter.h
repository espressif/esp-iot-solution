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
void board_lcd_flush(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h);

/**
 * @brief Fill an area using a bitmap
 * @pre GDISP_HARDWARE_BITFILLS is GFXON
 *
 * @param x,y The area position
 * @param w,h The area size
 * @param bitmap The pointer to the bitmap
 *
 * @note The parameter variables must not be altered by the driver.
 */
void board_lcd_blit_area(int16_t x, int16_t y, uint16_t* bitmap, int16_t w, int16_t h);

/**
 * @brief Write cmd to lcd.
 *
 * @param cmd will write command
 */
void board_lcd_write_cmd(uint16_t cmd);

/**
 *  @brief Write data to lcd.
 *
 * @param data will write data
 */
void board_lcd_write_data(uint16_t data);

/**
 * @brief Write data to lcd register.
 *
 * @param reg lcd register
 * @param data will write data
 */
void board_lcd_write_reg(uint16_t reg, uint16_t data);

/**
 * @brief Repeatedly write byte data to lcd point_num times.
 *
 * @param data data will write data
 * @param x repeat times is x * y
 * @param y repeat times is x * y
 */
void board_lcd_write_datas(uint16_t data, uint16_t x, uint16_t y);

/**
 * @brief Write byte data to lcd.
 *
 * @param data will write a byte data
 */
void board_lcd_write_data_byte(uint8_t data);

/**
 * @brief Set backlight of lcd
 *
 * @param data backlight value
 */
void board_lcd_set_backlight(uint16_t data);

/**
 * @brief Set orientation of lcd
 *
 * @param orientation orientation value
 */
void board_lcd_set_orientation(uint16_t orientation);

#ifdef __cplusplus
}
#endif

#endif /* __IOT_UGFX_LCD_ADAPTER_H__ */
