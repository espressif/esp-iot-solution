// Copyright 2020-2021 Espressif Systems (Shanghai) PTE LTD
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

/*Definations of Board*/
#define BOARD_NAME "ESP32-LCDKit"
#define BOARD_VENDOR "Espressif"
#define BOARD_URL "null"

#define CONFIG_BOARD_SPI2_INIT 1

/**
 * Resource ID on Board,
 * You can use iot_board_get_handle(ID,handle) to refrence Resource's Handle
 * */
typedef enum {
    NULL_RESOURCE,
    BOARD_I2C0_ID,
    BOARD_SPI2_ID,
}board_res_id_t;

#define BOARD_IO_I2C0_SCL 3
#define BOARD_IO_I2C0_SDA 1

#define BOARD_IO_SPI2_SCK 22
#define BOARD_IO_SPI2_MOSI 21
#define BOARD_IO_SPI2_MISO 27

/*Definations of Peripheral*/
#define BOARD_I2C0_MODE I2C_MODE_MASTER
#define BOARD_I2C0_SPEED (100000)
#define BOARD_I2C0_SCL_PULLUP_EN _ENABLE
#define BOARD_I2C0_SDA_PULLUP_EN _ENABLE

/**< Screen inrerface pins */
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

#ifdef __cplusplus
extern "C"
{
#endif
/**
 * @brief Board level init.
 *        Peripherals can be chosen through menuconfig, which will be initialized with default configurations during iot_board_init.
 *        After board init, initialized peripherals can be referenced by handles directly.
 * 
 * @return esp_err_t 
 */
esp_err_t iot_board_init(void);

/**
 * @brief Board level deinit.
 *        After board deinit, initialized peripherals will be deinit and related handles will be set to NULL.
 * 
 * @return esp_err_t 
 */
esp_err_t iot_board_deinit(void);

/**
 * @brief Check if board is initialized 
 * 
 * @return true if board is initialized
 * @return false if board is not initialized
 */
bool iot_board_is_init(void);

/**
 * @brief Using resource's ID declared in board_res_id_t to get board level resource's handle
 * 
 * @param id Resource's ID declared in board_res_id_t
 * @return board_res_handle_t Resource's handle
 * if no related handle,NULL will be returned
 */
board_res_handle_t iot_board_get_handle(board_res_id_t id);

/**
 * @brief Get board information
 * 
 * @return String include BOARD_NAME etc. 
 */
char* iot_board_get_info();

#ifdef __cplusplus
}
#endif

#endif /* _IOT_BOARD_H_ */
