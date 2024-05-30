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
#define BOARD_NAME "ESP32-S2-Saola-1"
#define BOARD_VENDOR "Espressif"
#define BOARD_URL "https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-saola-1-v1.2.html"

/*Definitions of MCU Information*/
#define BOARD_MCU_NAME "ESP32S2"
#define BOARD_MCU_FLASH_SIZE "4MB"
#define BOARD_MCU_RAM_SIZE "320KB"
#define BOARD_MCU_RAM_USER_SIZE "320KB"

/*Definitions of Button*/
#define BOARD_BUTTON_NUM 1
#define BOARD_IO_BUTTON_1 0

/*Definitions of LED*/
#define BOARD_LED_NUM 1
#define BOARD_LED_TYPE_1 LED_TYPE_RGB
#define BOARD_IO_LED_1 17

/* IO I2C0 */
#define BOARD_IO_I2C0_SCL 10
#define BOARD_IO_I2C0_SDA 11

/* IO SPI2 */
#define BOARD_IO_SPI2_SCK 36
#define BOARD_IO_SPI2_MOSI 35
#define BOARD_IO_SPI2_MISO 37

/* Free pins */
#define BOARD_IO_FREE_1 1
#define BOARD_IO_FREE_2 2
#define BOARD_IO_FREE_3 3
#define BOARD_IO_FREE_4 4
#define BOARD_IO_FREE_5 5
#define BOARD_IO_FREE_6 6
#define BOARD_IO_FREE_7 7
#define BOARD_IO_FREE_8 8
#define BOARD_IO_FREE_9 9
#define BOARD_IO_FREE_12 12
#define BOARD_IO_FREE_13 13
#define BOARD_IO_FREE_14 14
#define BOARD_IO_FREE_15 15
#define BOARD_IO_FREE_16 16
#define BOARD_IO_FREE_17 17

#define BOARD_IO_FREE_18 18
#define BOARD_IO_FREE_19 19
#define BOARD_IO_FREE_20 20
#define BOARD_IO_FREE_21 21
#define BOARD_IO_FREE_26 26
#define BOARD_IO_FREE_33 33
#define BOARD_IO_FREE_34 34
#define BOARD_IO_FREE_38 38
#define BOARD_IO_FREE_39 39
#define BOARD_IO_FREE_40 40
#define BOARD_IO_FREE_41 41
#define BOARD_IO_FREE_42 42
#define BOARD_IO_FREE_45 45
#define BOARD_IO_FREE_46 46

#endif /* _IOT_BOARD_H_ */
