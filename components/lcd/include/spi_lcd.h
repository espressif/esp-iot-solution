/*

 ILI9341 SPI Transmit Data Functions

 Based on ESP8266 library https://github.com/Sermus/ESP8266_Adafruit_ILI9341

 Copyright (c) 2015-2016 Andrey Filimonov.  All rights reserved.

 Additions for ESP32 Copyright (c) 2016-2017 Espressif Systems (Shanghai) PTE LTD


 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 - Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
 - Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE

*/

#ifndef _SPI_LCD_H_
#define _SPI_LCD_H_

#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ILI9341 = 0,
    ST7789 = 1,
} lcd_model_t;

/**
 * @brief struct to map GPIO to LCD pins
 */
typedef struct {
    lcd_model_t lcd_model;
    uint8_t pin_num_miso; /*!<MasterIn, SlaveOut pin*/
    uint8_t pin_num_mosi; /*!<MasterOut, SlaveIn pin*/
    uint8_t pin_num_clk;  /*!<SPI Clock pin*/
    uint8_t pin_num_cs;   /*!<SPI Chip Select Pin*/
    uint8_t pin_num_dc;   /*!<Pin to select Data or Command for LCD*/
    uint8_t pin_num_rst;  /*!<Pin to hardreset LCD*/
    uint8_t pin_num_bckl; /*!<Pin for adjusting Backlight- can use PWM/DAC too*/
    int clk_freq;
} lcd_conf_t;

/**
 * @brief struct holding LCD IDs
 */
typedef struct {
    uint8_t mfg_id;         /*!<Manufacturer's ID*/
    uint8_t lcd_driver_id;  /*!<LCD driver Version ID*/
    uint8_t lcd_id;         /*!<LCD Unique ID*/
} lcd_id_t;

/** @brief Initialize the LCD by putting some data in the graphics registers
 *
 * @param pin_conf Pointer to the struct with mandatory pins required for the LCD
 * @param spi_dev Pointer to the SPI handler for sending the data
 */
void lcd_init(lcd_conf_t *pin_conf, spi_device_handle_t *spi_dev);

/*Used by adafruit functions to send data*/
void lcd_send_uint16_r(spi_device_handle_t spi, const uint16_t data, int32_t repeats);

/*Send a command to the ILI9341. Uses spi_device_transmit,
 which waits until the transfer is complete */
void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd);

/*Send data to the ILI9341. Uses spi_device_transmit,
 which waits until the transfer is complete */
void lcd_data(spi_device_handle_t spi, const uint8_t *data, int len);

/** @brief Read LCD IDs using SPI, not working yet
 * The 1st parameter is dummy data.
 * The 2nd parameter (ID1 [7:0]): LCD module's manufacturer ID.
 * The 3rd parameter (ID2 [7:0]): LCD module/driver version ID.
 * The 4th parameter (ID3 [7:0]): LCD module/driver ID.
 * @param spi spi handler
 * @param lcd_id pointer to struct for reading IDs
 */
void lcd_read_id(spi_device_handle_t spi, lcd_id_t *lcd_id);


#ifdef __cplusplus
}
#endif

#endif
