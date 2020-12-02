// Copyright 2015-2020 Espressif Systems (Shanghai) PTE LTD
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
#ifndef _IOT_EPAPER_IL91874_H_
#define _IOT_EPAPER_IL91874_H_

#ifdef __cplusplus
extern "C"
{
#endif

#define COLORED         0
#define UNCOLORED       1

/**
 * @brief   device initialization
 *
 * @param   lcd_conf configuration struct of ssd1306
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t epaper_il91874_init(const lcd_config_t *lcd_conf);

/**
 * @brief   Deinitial screen
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t epaper_il91874_deinit(void);

/**
 * @brief Get screen information
 * 
 * @param info Pointer to a lcd_info_t structure.
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t epaper_il91874_get_info(lcd_info_t *info);

/**
 * @brief Set screen direction of rotation
 *
 * @param dir Pointer to a lcd_dir_t structure.
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t epaper_il91874_set_rotate(lcd_dir_t dir);


esp_err_t epaper_il91874_set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

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
esp_err_t epaper_il91874_draw_pixel(uint16_t x, uint16_t y, uint16_t chPoint);

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
esp_err_t epaper_il91874_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);

esp_err_t epaper_il91874_select_transmission(uint8_t transmission);

void epaper_il91874_refresh(void);

void epaper_il91874_refresh_partial(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

void epaper_il91874_sleep(void);
void EPD_Display(uint16_t *Imageblack, uint16_t *Imagered);

#ifdef __cplusplus
}
#endif

#endif

