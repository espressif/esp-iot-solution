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
#ifndef _IOT_ILI9806_H_
#define _IOT_ILI9806_H_

#include "i2s_lcd_com.h"
#include "iot_i2s_lcd.h"

#define ILI9806_CASET   0x2A
#define ILI9806_RASET   0x2B
#define ILI9806_RAMWR   0x2C
#define ILI9806_MADCTL  0x36

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ili9806_dev {
    i2s_lcd_handle_t i2s_lcd_handle;
    uint16_t x_size;
    uint16_t y_size;
    uint16_t xset_cmd;
    uint16_t yset_cmd;
    uint16_t *lcd_buf;
    uint16_t pix;
} ili9806_dev_t;

typedef void* ili9806_handle_t;

/**
 * @brief set ili9806 orientation
 * 
 * @param ili9806_handle the handle of ili9806
 * @param orientation ili9806 orientation
 */
void iot_ili9806_set_orientation(ili9806_handle_t ili9806_handle, lcd_orientation_t orientation);

/**
 * @brief set ili9806 box of write 
 * 
 * @param ili9806_handle the handle of ili9806
 * @param x ili9806 start address
 * @param y ili9806 start address
 * @param x_size ili9806 area width
 * @param y_size ili9806 area heigth
 */
void iot_ili9806_set_box(ili9806_handle_t ili9806_handle, uint16_t x, uint16_t y, uint16_t x_size, uint16_t y_size);

/**
 * @brief ili9806 refresh
 * 
 * @param ili9806_handle the handle of ili9806
 */
void iot_ili9806_refresh(ili9806_handle_t ili9806_handle);

/**
 * @brief fill screen background with color
 * 
 * @param ili9806_handle the handle of ili9806
 * @param color Color to be filled
 */
void iot_ili9806_fill_screen(ili9806_handle_t ili9806_handle, uint16_t color);

/**
 * @brief Print an array of pixels: Used to display pictures usually
 * 
 * @param ili9806_handle the handle of ili9806
 * @param bmp the pointer of image
 * @param x ili9806 start address
 * @param y ili9806 start address
 * @param x_size ili9806 area width
 * @param y_size ili9806 area heigth
 */
void iot_ili9806_draw_bmp(ili9806_handle_t ili9806_handle, uint16_t *bmp, uint16_t x, uint16_t y, uint16_t x_size, uint16_t y_size);

/**
 * @brief print string to ili9806
 * 
 * @param ili9806_handle the handle of ili9806
 * @param str string to print
 * @param x ili9806 start address
 * @param y ili9806 start address
 * @param x_size ili9806 area width
 * @param y_size ili9806 area heigth
 * @param wcolor string color
 * @param bcolor background color
 */
void iot_ili9806_put_char(ili9806_handle_t ili9806_handle, uint8_t *str, uint16_t x, uint16_t y, uint16_t x_size, uint16_t y_size, uint16_t wcolor, uint16_t bcolor);

/**
 * @brief print ascll to memory
 * 
 * @param ili9806_handle the handle of ili9806
 * @param str ascll to print
 * @param x ili9806 start address
 * @param y ili9806 start address
 * @param wcolor ascll color
 * @param bcolor background color
 */
void inline iot_ili9806_asc8x16_to_men(ili9806_handle_t ili9806_handle, char str, uint16_t x, uint16_t y, uint16_t wcolor, uint16_t bcolor);

/**
 * @brief print ascll to ili9806
 * 
 * @param ili9806_handle the handle of ili9806
 * @param str ascll to print
 * @param x ili9806 start address
 * @param y ili9806 start address
 * @param wcolor ascll color
 * @param bcolor background color
 */
void iot_ili9806_put_asc8x16(ili9806_handle_t ili9806_handle, char str, uint16_t x, uint16_t y, uint16_t wcolor, uint16_t bcolor);

/**
 * @brief print string to ili9806
 * 
 * @param ili9806_handle the handle of ili9806
 * @param str string to print
 * @param x ili9806 start address
 * @param y ili9806 start address
 * @param wcolor string color
 * @param bcolor background color
 */
void iot_ili9806_put_string8x16(ili9806_handle_t ili9806_handle, char *str, uint16_t x, uint16_t y, uint16_t wcolor, uint16_t bcolor);

/**
 * @brief ili9806 initlizetion
 * 
 * @param ili9806_handle the handle of ili9806
 */
void iot_ili9806_init(ili9806_handle_t ili9806_handle);

/**
 * @brief create ili9806 handle
 * 
 * @param x_size ili9806 screen width
 * @param x_size ili9806 screen height
 * 
 * @return
 *      - the handle of ili9806
 */
ili9806_handle_t iot_ili9806_create(uint16_t x_size, uint16_t y_size, i2s_port_t i2s_port, i2s_lcd_config_t *pin_conf);

#ifdef __cplusplus
}
#endif

#endif