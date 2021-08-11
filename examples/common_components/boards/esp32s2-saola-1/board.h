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
#define BOARD_NAME "ESP32-S2-Saola-1"
#define BOARD_VENDOR "Espressif"
#define BOARD_URL "https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-saola-1-v1.2.html"

/*Definations of MCU*/
#define BOARD_MCU_NAME "ESP32S2"
#define BOARD_MCU_FLASH_SIZE "4MB"
#define BOARD_MCU_RAM_SIZE "320KB"
#define BOARD_MCU_RAM_USER_SIZE "320KB"

/*Definations of IO*/
#define BOARD_IO_BUTTON_A _UNDEFINE
#define BOARD_IO_BUTTON_B _UNDEFINE
#define BOARD_IO_BUTTON_C _UNDEFINE

#define BOARD_IO_I2C0_SCL 10
#define BOARD_IO_I2C0_SDA 11

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

/*Definations of Peripheral*/
#define BOARD_I2C0_MODE I2C_MODE_MASTER
#define BOARD_I2C0_SPEED (100000)
#define BOARD_I2C0_SCL_PULLUP_EN _ENABLE
#define BOARD_I2C0_SDA_PULLUP_EN _ENABLE

#endif /* _IOT_BOARD_H_ */
