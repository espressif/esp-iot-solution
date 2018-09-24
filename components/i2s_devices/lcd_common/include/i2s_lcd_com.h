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
#ifndef   __LCD_COM_H__
#define   __LCD_COM_H__

#include "stdio.h"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iot_i2s_lcd.h"

#define LCD_LOG(s)    ESP_LOGI("LCD", "[%s   %d] : %s\n", __FUNCTION__, __LINE__, s);

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Write data to lcd.
 *
 * @param i2s_lcd_handle i2s_lcd_handle_t
 * @param data will write data
 */
void iot_i2s_lcd_write_data(i2s_lcd_handle_t i2s_lcd_handle, uint16_t data);

/**
 * @brief Write cmd to lcd.
 *
 * @param i2s_lcd_handle i2s_lcd_handle_t
 * @param cmd will write command
 */
void iot_i2s_lcd_write_cmd(i2s_lcd_handle_t i2s_lcd_handle, uint16_t cmd);

/**
 * @brief Write data to lcd register.
 *
 * @param i2s_lcd_handle i2s_lcd_handle_t
 * @param reg register address
 * @param data will write data
 */
void iot_i2s_lcd_write_reg(i2s_lcd_handle_t i2s_lcd_handle, uint16_t reg, uint16_t data);

/**
 * @brief Write data to lcd.
 *
 * @param i2s_lcd_handle i2s_lcd_handle_t
 * @param data will write data 
 * @param len data length
 */
void iot_i2s_lcd_write(i2s_lcd_handle_t i2s_lcd_handle, uint16_t *data, uint32_t len);

/**
 * @brief Lcd pin configuration.
 * 
 * @param i2s_port i2s num
 * @param pin_conf i2s lcd config
 * 
 * @return i2s_lcd_handle_t
 */
i2s_lcd_handle_t iot_i2s_lcd_pin_cfg(i2s_port_t i2s_port, i2s_lcd_config_t *pin_conf);

#ifdef __cplusplus
}
#endif

#endif