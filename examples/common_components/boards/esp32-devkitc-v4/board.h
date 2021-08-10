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
#include "board_common.h"

/**
 * Resource ID on Board,
 * You can use iot_board_get_handle(ID,handle) to refrence Resource's Handle
 * */
typedef enum {
    NULL_RESOURCE,
    BOARD_I2C0_ID,
    BOARD_SPI2_ID,
}board_res_id_t;

/*Definations of Board*/
#define BOARD_NAME "ESP32-Devkitc"
#define BOARD_VENDOR "Espressif"
#define BOARD_URL "https://docs.espressif.com/projects/esp-idf/en/latest/esp32/hw-reference/modules-and-boards.html"

/*Definations of MCU*/
#define BOARD_MCU_NAME "ESP32"
#define BOARD_MCU_FLASH_SIZE "4MB"
#define BOARD_MCU_RAM_SIZE "520KB"
#define BOARD_MCU_RAM_USER_SIZE "320KB"

/*Definations of IO*/
#define BOARD_IO_BUTTON_A _UNDEFINE
#define BOARD_IO_BUTTON_B _UNDEFINE
#define BOARD_IO_BUTTON_C _UNDEFINE

#define BOARD_IO_I2C0_SCL 22
#define BOARD_IO_I2C0_SDA 21

#define BOARD_IO_SPI2_SCK 18
#define BOARD_IO_SPI2_MOSI 23
#define BOARD_IO_SPI2_MISO 19

/*Definations of Peripheral*/
#define BOARD_I2C0_MODE I2C_MODE_MASTER
#define BOARD_I2C0_SPEED (100000)
#define BOARD_I2C0_SCL_PULLUP_EN _ENABLE
#define BOARD_I2C0_SDA_PULLUP_EN _ENABLE

#endif /* _IOT_BOARD_H_ */
