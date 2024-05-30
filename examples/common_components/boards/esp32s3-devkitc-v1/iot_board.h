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
#define BOARD_NAME "ESP32-S3-DevKitC-1"
#define BOARD_VENDOR "Espressif"
#define BOARD_URL "https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/hw-reference/esp32s3/user-guide-devkitc-1.html"

/*Definitions of MCU Information*/
#define BOARD_MCU_NAME "ESP32S3"
#define BOARD_MODULE_NAME "ESP32-S3-WROOM"
#define BOARD_MCU_FLASH_SIZE "4MB"
#define BOARD_MCU_RAM_SIZE "512KB"
#define BOARD_MCU_RAM_USER_SIZE "384KB"

/*Definitions of Button*/
#define BOARD_BUTTON_NUM 1
#define BOARD_IO_BUTTON_1 0

/*Definitions of LED*/
#define BOARD_LED_NUM 1
#define BOARD_LED_TYPE_1 LED_TYPE_RGB
#define BOARD_IO_LED_1 48

/* IO I2C0 */
#define BOARD_IO_I2C0_SCL 10
#define BOARD_IO_I2C0_SDA 11

/* IO SPI2 */
#define BOARD_IO_SPI2_SCK 36
#define BOARD_IO_SPI2_MOSI 35
#define BOARD_IO_SPI2_MISO 37

/* Free pins - left */
#define BOARD_IO_FREE_4 4
#define BOARD_IO_FREE_5 5
#define BOARD_IO_FREE_6 6
#define BOARD_IO_FREE_7 7
#define BOARD_IO_FREE_15 15
#define BOARD_IO_FREE_16 16
#define BOARD_IO_FREE_17 17
#define BOARD_IO_FREE_18 18
#define BOARD_IO_FREE_8 8
#define BOARD_IO_FREE_3 3
#define BOARD_IO_FREE_46 46
#define BOARD_IO_FREE_9 9
// #define BOARD_IO_FREE_10 10
// #define BOARD_IO_FREE_11 11
#define BOARD_IO_FREE_12 12
#define BOARD_IO_FREE_13 13
#define BOARD_IO_FREE_14 14

/* Free pins - right */
#define BOARD_IO_FREE_43 43
#define BOARD_IO_FREE_44 44
#define BOARD_IO_FREE_1 1
#define BOARD_IO_FREE_2 2
#define BOARD_IO_FREE_42 42
#define BOARD_IO_FREE_41 41
#define BOARD_IO_FREE_40 40
#define BOARD_IO_FREE_39 39
#define BOARD_IO_FREE_38 38
// #define BOARD_IO_FREE_37 37
// #define BOARD_IO_FREE_36 36
// #define BOARD_IO_FREE_35 35
// #define BOARD_IO_FREE_0 0
#define BOARD_IO_FREE_45 45
#define BOARD_IO_FREE_48 48
#define BOARD_IO_FREE_47 47
#define BOARD_IO_FREE_21 21
// #define BOARD_IO_FREE_20 20
// #define BOARD_IO_FREE_19 19

//USB Configuration (fixed)
#define BOARD_IO_USB_DP 20
#define BOARD_IO_USB_DN 19
#endif /* _IOT_BOARD_H_ */
