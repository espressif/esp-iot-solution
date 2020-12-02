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
#ifndef _IOT_EPAPER_IL0373_H_
#define _IOT_EPAPER_IL0373_H_

#ifdef __cplusplus
extern "C"
{
#endif

#define COLORED         0
#define UNCOLORED       1

/**
 * @brief   device initialization
 *
 * @param   lcd_conf configuration struct of il0373
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t epaper_il0373_init(const lcd_config_t *lcd_conf);

/**
 * @brief   Deinitial screen
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t epaper_il0373_deinit(void);

/**
 * @brief Set screen direction of rotation
 *
 * @param dir Pointer to a lcd_dir_t structure.
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t epaper_il0373_set_rotate(lcd_dir_t dir);

/**
 * @brief Set screen window
 *
 * @param x Starting point in X direction
 * @param y Starting point in Y direction
 * @param w width of window
 * @param h height of window
 * 
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL Failed
 */
esp_err_t epaper_il0373_set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

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
esp_err_t epaper_il0373_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);

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
esp_err_t epaper_il0373_draw_pixel(uint16_t x, uint16_t y, uint16_t chPoint);

/**
 * @brief Get screen information
 * 
 * @param info Pointer to a lcd_info_t structure.
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t epaper_il0373_get_info(lcd_info_t *info);

/**
 * @brief Get screen information
 * 
 * @param transmission transmission mode of il0303
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t epaper_il0373_select_transmission(uint8_t transmission);

/**
 * @brief   device  gary initialization
 *
 * @param   lcd_conf configuration struct of il0373
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t epaper_il0373_init_4Gray();

/**
 * @brief clean display buffer
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t epaper_il0703_display_Clean(void);

/**
 * @brief set il0373 sleep
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t epaper_il0373_sleep(void);

/**
 * @brief resfresh the epaper screen
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t epaper_il0373_refresh(void);

/**
 * @brief display picture on full epaper screen
 *
 * @param picData picture buffer
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
void epaper_il0373_pic_display_fullscreen(const unsigned char* picData);


/**
 * @brief display 4-gray picture on full epaper screen
 *
 * @param picData picture buffer
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
void epaper_il0373_pic_display_4bit (const unsigned char* picData);

/**
 * @brief display picture on full epaper screen
 *
 * @param x_start start x position
 * @param y_start start y position
 * @param old_data old picture buffer
 * @param new_data new picture buffer
 * @param PART_COLUMN picture width 
 * @param PART_LINE picture length
 * @param mode display mode
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
void epaper_il0373_partial_display(uint16_t x_start,uint16_t y_start,const unsigned char *old_data,const unsigned char *new_data,unsigned int PART_COLUMN,unsigned int PART_LINE,unsigned char mode);

/**
 * @brief lut download
 *
 */
void lut11(void);
void lut1(void);
void lut(void);

#ifdef __cplusplus
}
#endif

#endif
