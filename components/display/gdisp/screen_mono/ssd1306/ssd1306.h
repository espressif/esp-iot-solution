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
#ifndef __IOT_LCD_SSD1306_H__
#define __IOT_LCD_SSD1306_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "lcd_types.h"


/**
 * @brief   device initialization
 *
 * @param   lcd_conf configuration struct of ssd1306
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lcd_ssd1306_init(const lcd_config_t *lcd_conf);

/**
 * @brief   Deinitial screen
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lcd_ssd1306_deinit(void);

/**
 * @brief Get screen information
 * 
 * @param info Pointer to a lcd_info_t structure.
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lcd_ssd1306_get_info(lcd_info_t *info);

/**
 * @brief Set screen direction of rotation
 *
 * @param dir Pointer to a lcd_dir_t structure.
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lcd_ssd1306_set_rotate(lcd_dir_t dir);

/**
 * @brief Draw one pixel in screen with color
 * 
 * @param x X co-ordinate of set orientation
 * @param y Y co-ordinate of set orientation
 * @param chPoint New color of the pixel
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lcd_ssd1306_draw_pixel(uint16_t x, uint16_t y, uint16_t chPoint);

/**
 * @brief Fill the pixels on LCD screen with bitmap
 * 
 * @param x Starting point in X direction
 * @param y Starting point in Y direction
 * @param w width of image in bitmap array
 * @param h height of image in bitmap array
 * @param bitmap pointer to bitmap array
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lcd_ssd1306_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);

/**
 * @brief Write a buffer to the screen to update the display
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lcd_ssd1306_refresh_gram(void);

/**
 * @brief Clear screen and buffer
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lcd_ssd1306_clear_screen(void);

/**
 * @brief Clear buffer
 * 
 * @return esp_err_t 
 */
esp_err_t lcd_ssd1306_clear_buffer(void);

/**
 * @brief Set the contrast of screen
 * 
 * @param contrast Contrast to set
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lcd_ssd1306_set_contrast(uint8_t contrast);

/**
 * @brief Turn on the screen
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lcd_ssd1306_display_on(void);

/**
 * @brief Turn off the screen
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lcd_ssd1306_display_off(void);

/**
 * @brief Start horizontal scroll
 * 
 * @param dir Direction of horizontal scroll
 * @param start start page
 * @param stop end page
 * @param interval time interval of scroll
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lcd_ssd1306_start_horizontal_scroll(uint8_t dir, uint8_t start, uint8_t stop, uint8_t interval);

/**
 * @brief 
 * 
 * @param start 
 * @param stop 
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lcd_ssd1306_start_vertical_scroll(uint8_t start, uint8_t stop);

/**
 * @brief 
 * 
 * @param start 
 * @param stop 
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lcd_ssd1306_start_scroll_diagRight(uint8_t start, uint8_t stop);

/**
 * @brief Stop screen scroll
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t lcd_ssd1306_stop_scroll(void);

#ifdef __cplusplus
}
#endif

#endif
