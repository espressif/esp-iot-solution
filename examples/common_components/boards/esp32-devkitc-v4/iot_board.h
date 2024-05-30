/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IOT_BOARD_H_
#define _IOT_BOARD_H_

#include "esp_err.h"
#include "board_common.h"

/**
 * Resource ID on Board,
 * You can use iot_board_get_handle(ID,handle) to reference Resource's Handle
 * */
typedef enum {
    NULL_RESOURCE,
    BOARD_I2C0_ID,
    BOARD_SPI2_ID,
} board_res_id_t;

/*Definitions of Board Information*/
#define BOARD_NAME "ESP32-Devkitc"
#define BOARD_VENDOR "Espressif"
#define BOARD_URL "https://www.espressif.com/en/products/devkits/esp32-devkitc"

/*Definitions of MCU Information*/
#define BOARD_MCU_NAME "ESP32"
#define BOARD_MCU_FLASH_SIZE "4MB"
#define BOARD_MCU_RAM_SIZE "520KB"
#define BOARD_MCU_RAM_USER_SIZE "320KB"

/* Free pins */
#define BOARD_IO_FREE_2 2
#define BOARD_IO_FREE_4 4
#define BOARD_IO_FREE_5 5
#define BOARD_IO_FREE_12 12
#define BOARD_IO_FREE_13 13
#define BOARD_IO_FREE_14 14
#define BOARD_IO_FREE_15 15
#define BOARD_IO_FREE_16 16
#define BOARD_IO_FREE_17 17
#define BOARD_IO_FREE_25 25
#define BOARD_IO_FREE_26 26
#define BOARD_IO_FREE_27 27
#define BOARD_IO_FREE_32 32
#define BOARD_IO_FREE_33 33
#define BOARD_IO_FREE_34 34
#define BOARD_IO_FREE_35 35

/*Definitions of I2C0*/
#define BOARD_IO_I2C0_SCL 22
#define BOARD_IO_I2C0_SDA 21

/*Definitions of SPI2*/
#define BOARD_IO_SPI2_SCK 18
#define BOARD_IO_SPI2_MOSI 23
#define BOARD_IO_SPI2_MISO 19

/* IO Button */
#define BOARD_BUTTON_NUM 1
#define BOARD_IO_BUTTON_1 0

#endif /* _IOT_BOARD_H_ */
