/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IOT_BOARD_H_
#define _IOT_BOARD_H_

#include "esp_err.h"
#include "board_common.h"

/*Definitions of Board Information*/
#define BOARD_NAME "ESP32-LCDKit"
#define BOARD_VENDOR "Espressif"
#define BOARD_URL "https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/esp32-lcdkit/user_guide.html"

/**
 * Resource ID on Board,
 * You can use iot_board_get_handle(ID,handle) to reference Resource's Handle
 * */
typedef enum {
    NULL_RESOURCE,
    BOARD_I2C0_ID,
    BOARD_SPI2_ID,
} board_res_id_t;

/*Definitions of I2C0*/
#define BOARD_IO_I2C0_SCL 3
#define BOARD_IO_I2C0_SDA 1

/*Definitions of SPI2*/
#define BOARD_IO_SPI2_SCK 22
#define BOARD_IO_SPI2_MOSI 21
#define BOARD_IO_SPI2_MISO 27

/*********************************** esp32-lcdkit board specified definition ***********************/
/**< Screen interface pins */
#define BOARD_LCD_SPI_HOST 1
#define BOARD_LCD_SPI_CLOCK_FREQ 20000000
#define BOARD_LCD_SPI_MISO_PIN BOARD_IO_SPI2_MISO
#define BOARD_LCD_SPI_MOSI_PIN BOARD_IO_SPI2_MOSI
#define BOARD_LCD_SPI_CLK_PIN BOARD_IO_SPI2_SCK
#define BOARD_LCD_SPI_CS_PIN 5
#define BOARD_LCD_SPI_DC_PIN 19
#define BOARD_LCD_SPI_RESET_PIN 18
#define BOARD_LCD_SPI_BL_PIN 23

#define BOARD_LCD_I2S_BITWIDTH 16
#define BOARD_LCD_I2S_PORT_NUM 0
#define BOARD_LCD_I2S_CS_PIN -1
#define BOARD_LCD_I2S_WR_PIN 18
#define BOARD_LCD_I2S_RS_PIN 5
#define BOARD_LCD_I2S_D0_PIN 19
#define BOARD_LCD_I2S_D1_PIN 21
#define BOARD_LCD_I2S_D2_PIN 0
#define BOARD_LCD_I2S_D3_PIN 22
#define BOARD_LCD_I2S_D4_PIN 23
#define BOARD_LCD_I2S_D5_PIN 33
#define BOARD_LCD_I2S_D6_PIN 32
#define BOARD_LCD_I2S_D7_PIN 27
#define BOARD_LCD_I2S_D8_PIN 25
#define BOARD_LCD_I2S_D9_PIN 26
#define BOARD_LCD_I2S_D10_PIN 12
#define BOARD_LCD_I2S_D11_PIN 13
#define BOARD_LCD_I2S_D12_PIN 14
#define BOARD_LCD_I2S_D13_PIN 15
#define BOARD_LCD_I2S_D14_PIN 2
#define BOARD_LCD_I2S_D15_PIN 4
#define BOARD_LCD_I2S_RESET_PIN -1
#define BOARD_LCD_I2S_BL_PIN -1

#define BOARD_LCD_I2C_PORT_NUM 0
#define BOARD_LCD_I2C_CLOCK_FREQ 100000
#define BOARD_LCD_I2C_SCL_PIN 4
#define BOARD_LCD_I2C_SDA_PIN 5

/**< Touch panel interface pins */
/**
 * When both the screen and the touch panel are SPI interfaces,
 * they can choose to share a SPI host. The board ESP32-LCDKit is this.
 */
#define BOARD_TOUCH_SPI_CS_PIN 32
#define BOARD_TOUCH_I2C_PORT_NUM 0
#define BOARD_TOUCH_I2C_SCL_PIN BOARD_IO_I2C0_SCL
#define BOARD_TOUCH_I2C_SDA_PIN BOARD_IO_I2C0_SDA

/**< SDCard interface pins */
#define BOARD_SDCARD_D0_PIN 2
#define BOARD_SDCARD_D1_PIN 4
#define BOARD_SDCARD_D2_PIN 12
#define BOARD_SDCARD_D3_PIN 13
#define BOARD_SDCARD_CLK_PIN 14
#define BOARD_SDCARD_CMD_PIN 15
#define BOARD_SDCARD_DET_PIN 34

/**< Audio output interface pins */
#define BOARD_AUDIO_CH_LEFT_PIN 25
#define BOARD_AUDIO_CH_RIGHT_PIN 26

#endif /* _IOT_BOARD_H_ */
