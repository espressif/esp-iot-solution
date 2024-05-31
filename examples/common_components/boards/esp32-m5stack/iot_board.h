/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IOT_BOARD_H_
#define _IOT_BOARD_H_

#include "esp_err.h"
#include "i2c_bus.h"
#include "spi_bus.h"

/*ENABLE Initialization Process in _board_init(void)*/
#define _ENABLE 1
#define _DISABLE 0
#define _UNDEFINE
typedef void* board_res_handle_t;

/*Definitions of Board*/
#define BOARD_NAME "ESP32-M5Stack"
#define BOARD_VENDOR "M5Stack"
#define BOARD_URL "null"

#define CONFIG_BOARD_SPI3_INIT 1

/**
 * Resource ID on Board,
 * You can use iot_board_get_handle(ID,handle) to reference Resource's Handle
 * */
typedef enum {
    NULL_RESOURCE,
    BOARD_I2C0_ID,
    BOARD_SPI3_ID,
} board_res_id_t;

#define BOARD_IO_I2C0_SCL 3
#define BOARD_IO_I2C0_SDA 1

#define BOARD_IO_SPI2_SCK 22
#define BOARD_IO_SPI2_MOSI 21
#define BOARD_IO_SPI2_MISO 27

#define BOARD_IO_SPI3_SCK 18
#define BOARD_IO_SPI3_MOSI 23
#define BOARD_IO_SPI3_MISO 19

/*Definitions of Peripheral*/
#define BOARD_I2C0_MODE I2C_MODE_MASTER
#define BOARD_I2C0_SPEED (100000)
#define BOARD_I2C0_SCL_PULLUP_EN _ENABLE
#define BOARD_I2C0_SDA_PULLUP_EN _ENABLE

/**< Screen interface pins */
#define BOARD_LCD_SPI_HOST 2
#define BOARD_LCD_SPI_CLOCK_FREQ 20000000
#define BOARD_LCD_SPI_MISO_PIN BOARD_IO_SPI3_MISO
#define BOARD_LCD_SPI_MOSI_PIN BOARD_IO_SPI3_MOSI
#define BOARD_LCD_SPI_CLK_PIN BOARD_IO_SPI3_SCK
#define BOARD_LCD_SPI_CS_PIN 14
#define BOARD_LCD_SPI_DC_PIN 27
#define BOARD_LCD_SPI_RESET_PIN 33
#define BOARD_LCD_SPI_BL_PIN 32

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
#define BOARD_LCD_I2C_CLOCK_FREQ BOARD_I2C0_SPEED
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
