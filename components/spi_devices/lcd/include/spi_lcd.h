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
#ifndef _IOT_SPI_LCD_H_
#define _IOT_SPI_LCD_H_

#include "driver/spi_master.h"
#include "iot_lcd.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Initialize the LCD by putting some data in the graphics registers
 *
 * @param pin_conf Pointer to the struct with mandatory pins required for the LCD
 * @param spi_wr_dev Pointer to the SPI handler for sending the data
 * @return lcd id
 */
uint32_t lcd_init(lcd_conf_t* lcd_conf, spi_device_handle_t *spi_wr_dev, lcd_dc_t *dc, int dma_chan);

/*Used by adafruit functions to send data*/
void lcd_send_uint16_r(spi_device_handle_t spi, const uint16_t data, int32_t repeats, lcd_dc_t *dc);

/*Send a command to the ILI9341. Uses spi_device_transmit,
 which waits until the transfer is complete */
void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd, lcd_dc_t *dc);

/*Send data to the ILI9341. Uses spi_device_transmit,
 which waits until the transfer is complete */
void lcd_data(spi_device_handle_t spi, const uint8_t *data, int len, lcd_dc_t *dc);

/** @brief Read LCD IDs using SPI, not working yet
 * The 1st parameter is dummy data.
 * The 2nd parameter (ID1 [7:0]): LCD module's manufacturer ID.
 * The 3rd parameter (ID2 [7:0]): LCD module/driver version ID.
 * The 4th parameter (ID3 [7:0]): LCD module/driver ID.
 * @param spi spi handler
 * @param lcd_id pointer to struct for reading IDs
 */
void lcd_read_id(spi_device_handle_t spi, lcd_id_t *lcd_id, lcd_dc_t *dc);

/**
 * @brief get LCD ID
 */
uint32_t lcd_get_id(spi_device_handle_t spi, lcd_dc_t *dc);

#ifdef __cplusplus
}
#endif

#endif
