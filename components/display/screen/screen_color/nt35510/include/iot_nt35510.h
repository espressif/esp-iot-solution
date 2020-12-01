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
#ifndef _IOT_NT35510_H_
#define _IOT_NT35510_H_

#include "i2s_lcd_com.h"
#include "iot_i2s_lcd.h"

#define NT35510_CASET   0x2A00
#define NT35510_RASET   0x2B00
#define NT35510_RAMWR   0x2C00
#define NT35510_MADCTL  0x3600

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nt35510_dev {
    i2s_lcd_handle_t i2s_lcd_handle;
    uint16_t x_size;
    uint16_t y_size;
    uint16_t xset_cmd;
    uint16_t yset_cmd;
    uint16_t *lcd_buf;
    uint16_t pix;
} nt35510_dev_t;

typedef void* nt35510_handle_t;

/**
 * @brief set nt35510 orientation
 * 
 * @param nt35510_handle the handle of nt35510
 * @param orientation nt35510 orientation
 */
void iot_nt35510_set_orientation(nt35510_handle_t nt35510_handle, lcd_orientation_t orientation);

/**
 * @brief set nt35510 box of write 
 * 
 * @param nt35510_handle the handle of nt35510
 * @param x nt35510 start address
 * @param y nt35510 start address
 * @param x_size nt35510 area width
 * @param y_size nt35510 area heigth
 */
void iot_nt35510_set_box(nt35510_handle_t nt35510_handle, uint16_t x, uint16_t y, uint16_t x_size, uint16_t y_size);

/**
 * @brief nt35510 refresh
 * 
 * @param nt35510_handle the handle of nt35510
 */
void iot_nt35510_refresh(nt35510_handle_t nt35510_handle);

/**
 * @brief fill screen background with color
 * 
 * @param nt35510_handle the handle of nt35510
 * @param color Color to be filled
 */
void iot_nt35510_fill_screen(nt35510_handle_t nt35510_handle, uint16_t color);

/**
 * @brief fill the area of screen with color
 * 
 * @param nt35510_handle the handle of nt35510
 * @param color Color to be filled
 * @param x area width
 * @param y area height
 */
void iot_nt35510_fill_area(nt35510_handle_t nt35510_handle, uint16_t color, uint16_t x, uint16_t y);

/**
 * @brief fill the area of screen with color
 * 
 * @param nt35510_handle the handle of nt35510
 * @param color Color to be filled
 * @param x start coordinate
 * @param y start coordinate
 * @param x_size area width
 * @param y_size area height
 */
void iot_nt35510_fill_rect(nt35510_handle_t nt35510_handle, uint16_t color, uint16_t x, uint16_t y, uint16_t x_size, uint16_t y_size);
/**
 * @brief Print an array of pixels: Used to display pictures usually
 * 
 * @param nt35510_handle the handle of nt35510
 * @param bmp the pointer of image
 * @param x nt35510 start address
 * @param y nt35510 start address
 * @param x_size nt35510 area width
 * @param y_size nt35510 area heigth
 */
void iot_nt35510_draw_bmp(nt35510_handle_t nt35510_handle, uint16_t *bmp, uint16_t x, uint16_t y, uint16_t x_size, uint16_t y_size);

/**
 * @brief print string to nt35510
 * 
 * @param nt35510_handle the handle of nt35510
 * @param str string to print
 * @param x nt35510 start address
 * @param y nt35510 start address
 * @param x_size nt35510 area width
 * @param y_size nt35510 area heigth
 * @param wcolor string color
 * @param bcolor background color
 */
void iot_nt35510_put_char(nt35510_handle_t nt35510_handle, uint8_t *str, uint16_t x, uint16_t y, uint16_t x_size, uint16_t y_size, uint16_t wcolor, uint16_t bcolor);

/**
 * @brief print ascll to memory
 * 
 * @param nt35510_handle the handle of nt35510
 * @param str ascll to print
 * @param x nt35510 start address
 * @param y nt35510 start address
 * @param wcolor ascll color
 * @param bcolor background color
 */
void iot_nt35510_asc8x16_to_men(nt35510_handle_t nt35510_handle, char str, uint16_t x, uint16_t y, uint16_t wcolor, uint16_t bcolor);

/**
 * @brief print ascll to nt35510
 * 
 * @param nt35510_handle the handle of nt35510
 * @param str ascll to print
 * @param x nt35510 start address
 * @param y nt35510 start address
 * @param wcolor ascll color
 * @param bcolor background color
 */
void iot_nt35510_put_asc8x16(nt35510_handle_t nt35510_handle, char str, uint16_t x, uint16_t y, uint16_t wcolor, uint16_t bcolor);

/**
 * @brief print string to nt35510
 * 
 * @param nt35510_handle the handle of nt35510
 * @param str string to print
 * @param x nt35510 start address
 * @param y nt35510 start address
 * @param wcolor string color
 * @param bcolor background color
 */
void iot_nt35510_put_string8x16(nt35510_handle_t nt35510_handle, char *str, uint16_t x, uint16_t y, uint16_t wcolor, uint16_t bcolor);

/**
 * @brief nt35510 initlizetion
 * 
 * @param nt35510_handle the handle of nt35510
 */
void iot_nt35510_init(nt35510_handle_t nt35510_handle);

/**
 * @brief create nt35510 handle
 * 
 * @param x_size nt35510 screen width
 * @param x_size nt35510 screen height
 * @param i2s_port i2s num
 * @param pin_conf i2s lcd config
 * 
 * @return
 *      - the handle of nt35510
 */
nt35510_handle_t iot_nt35510_create(uint16_t x_size, uint16_t y_size, i2s_port_t i2s_port, i2s_lcd_config_t *pin_conf);

#ifdef __cplusplus
}
#endif

#endif /* _IOT_NT35510_H_ */
