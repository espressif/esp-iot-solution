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
#define BOARD_NAME "ESP32-MESHKIT-SENSE_V1_1"
#define BOARD_VENDOR "Espressif"
#define BOARD_URL "https://github.com/espressif/esp-iot-solution/blob/master/documents/evaluation_boards/ESP32-MeshKit-Sense_guide_cn.md"

/*Definations of MCU*/
#define BOARD_MCU_NAME "ESP32"
#define BOARD_MODULE_NAME "ESP32-WROOM-32D"
#define BOARD_MCU_FLASH_SIZE "4MB"
#define BOARD_MCU_RAM_SIZE "520KB"
#define BOARD_MCU_RAM_USER_SIZE "320KB"

/*Definations of IO*/
#define BOARD_IO_BUTTON_WARKUP 34
#define BOARD_IO_BUTTON_A BOARD_IO_BUTTON_WARKUP

#define BOARD_IO_LED_NETWORK_P 4
#define BOARD_IO_LED_WIFI_N 15
#define BOARD_IO_LED_BAT_GREEN_P 12
#define BOARD_IO_LED_BAT_RED_P 2

#define BOARD_IO_POWER_ON_SENSOR_N 27
#define BOARD_IO_POWER_ON_SCREEN_N 14

#define BOARD_IO_SENSOR_HTS221_DRDY 25
#define BOARD_IO_SENSOR_APDS9960_INT 26

#define BOARD_IO_I2C0_SCL 32
#define BOARD_IO_I2C0_SDA 33

#define BOARD_LED_NUM 4
#define BOARD_IO_LED_1 BOARD_IO_LED_NETWORK_P
#define BOARD_IO_LED_2 BOARD_IO_LED_WIFI_N
#define BOARD_IO_LED_3 BOARD_IO_LED_BAT_GREEN_P
#define BOARD_IO_LED_4 BOARD_IO_LED_BAT_RED_P

#define BOARD_LED_POLARITY_1 _POSITIVE
#define BOARD_LED_POLARITY_2 _NEGATIVE
#define BOARD_LED_POLARITY_3 _POSITIVE
#define BOARD_LED_POLARITY_4 _POSITIVE

//TODO:check it when release
//#define BOARD_IO_SCREEN_DOUT SENSOR_VIN
//#define BOARD_IO_SCREEN_CONTROL 13

#define BOARD_IO_SCREEN_BUSY 5
#define BOARD_IO_SCREEN_RST 18
#define BOARD_IO_SCREEN_DC 19
#define BOARD_IO_SCREEN_CS 21
#define BOARD_IO_SCREEN_DIN 23

#define BOARD_IO_PIN_SEL_OUTPUT ((1ULL<<BOARD_IO_LED_NETWORK_P) | (1ULL<<BOARD_IO_LED_WIFI_N) | (1ULL<<BOARD_IO_LED_BAT_GREEN_P) | (1ULL<<BOARD_IO_LED_BAT_RED_P) | (1ULL<<BOARD_IO_POWER_ON_SENSOR_N) | (1ULL<<BOARD_IO_POWER_ON_SCREEN_N))
#define BOARD_IO_PIN_SEL_INPUT ((1ULL<<BOARD_IO_SENSOR_HTS221_DRDY) | (1ULL<<BOARD_IO_SENSOR_APDS9960_INT))

/*Definations of Peripheral*/
#define BOARD_I2C0_MODE I2C_MODE_MASTER
#define BOARD_I2C0_SPEED (100000)
#define BOARD_I2C0_SCL_PULLUP_EN _ENABLE
#define BOARD_I2C0_SDA_PULLUP_EN _ENABLE

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************** Extended board level API ***********************/
/**
 * @brief set board sensor power
 * 
 * @param on_off set true if power on the sensors onboard, set false if power off them.
 * @return esp_err_t 
 */
esp_err_t iot_board_sensor_set_power(bool on_off);

/**
 * @brief get board sensor power
 * 
 * @return true sensor is power on
 * @return false sensor is not power on
 */
bool iot_board_sensor_get_power(void);

/**
 * @brief set board screen power 
 * 
 * @param on_off set true if power on the screen onboard, set false if power off it.
 * @return esp_err_t 
 */
esp_err_t iot_board_screen_set_power(bool on_off);

/**
 * @brief get board screen power 
 * 
 * @return true screen is power on 
 * @return false screen is not power on 
 */
bool iot_board_screen_get_power(void);

/**
 * @brief set led state
 * 
 * @param gpio_num led gpio num defined in iot_board.h 
 * @param turn_on true if turn on the LED, false is turn off the LED
 * @return esp_err_t 
 */
esp_err_t iot_board_led_set_state(int gpio_num, bool turn_on);

/**
 * @brief set all led state
 * 
 * @param turn_on true if turn on the LED, false is turn off the LED
 * @return esp_err_t 
 */
esp_err_t iot_board_led_all_set_state(bool turn_on);

/**
 * @brief toggle led state
 * 
 * @param gpio_num led gpio num defined in iot_board.h 
 * @return esp_err_t 
 */
esp_err_t iot_board_led_toggle_state(int gpio_num);

#ifdef __cplusplus
}
#endif

#endif /* _IOT_BOARD_H_ */
