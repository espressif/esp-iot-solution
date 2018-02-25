/*
 * ESPRSSIF MIT License
 *
 * Copyright (c) 2015 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef _SPI_LCD_H_
#define _SPI_LCD_H_

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
