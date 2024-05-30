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
#define BOARD_NAME "ESP32-MESHKIT-SENSE_V1_1"
#define BOARD_VENDOR "Espressif"
#define BOARD_URL "https://github.com/espressif/esp-iot-solution/blob/master/documents/evaluation_boards/ESP32-MeshKit-Sense_guide_cn.md"

/*Definitions of MCU Information*/
#define BOARD_MCU_NAME "ESP32"
#define BOARD_MODULE_NAME "ESP32-WROOM-32D"
#define BOARD_MCU_FLASH_SIZE "4MB"
#define BOARD_MCU_RAM_SIZE "520KB"
#define BOARD_MCU_RAM_USER_SIZE "320KB"

/*Definitions of Button*/
#define BOARD_BUTTON_NUM 1
#define BOARD_IO_BUTTON_WARKUP 34
#define BOARD_IO_BUTTON_1 BOARD_IO_BUTTON_WARKUP
#define BOARD_BUTTON_POLARITY_1 _NEGATIVE

/*Definitions of LED*/
#define BOARD_IO_LED_NETWORK_P 4
#define BOARD_IO_LED_WIFI_N 15
#define BOARD_IO_LED_BAT_GREEN_P 12
#define BOARD_IO_LED_BAT_RED_P 2

#define BOARD_LED_NUM 4
#define BOARD_IO_LED_1 BOARD_IO_LED_NETWORK_P
#define BOARD_IO_LED_2 BOARD_IO_LED_WIFI_N
#define BOARD_IO_LED_3 BOARD_IO_LED_BAT_GREEN_P
#define BOARD_IO_LED_4 BOARD_IO_LED_BAT_RED_P
#define BOARD_LED_POLARITY_1 _POSITIVE
#define BOARD_LED_POLARITY_2 _NEGATIVE
#define BOARD_LED_POLARITY_3 _POSITIVE
#define BOARD_LED_POLARITY_4 _POSITIVE

/*Definitions of GPIO*/
#define BOARD_IO_POWER_ON_SENSOR_N 27
#define BOARD_IO_POWER_ON_SCREEN_N 14
#define BOARD_IO_SENSOR_HTS221_DRDY 25
#define BOARD_IO_SENSOR_APDS9960_INT 26

#define BOARD_IO_PIN_SEL_OUTPUT ((1ULL<<BOARD_IO_POWER_ON_SENSOR_N) | (1ULL<<BOARD_IO_POWER_ON_SCREEN_N))
#define BOARD_IO_PIN_SEL_INPUT ((1ULL<<BOARD_IO_SENSOR_HTS221_DRDY) | (1ULL<<BOARD_IO_SENSOR_APDS9960_INT))

/*Definitions of I2C0*/
#define BOARD_IO_I2C0_SCL 32
#define BOARD_IO_I2C0_SDA 33

/*********************************** esp32-meshkit-sense board specified definition ***********************/
#define BOARD_IO_SCREEN_CONTROL 13
#define BOARD_IO_SCREEN_BUSY 5
#define BOARD_IO_SCREEN_RST 18
#define BOARD_IO_SCREEN_DC 19
#define BOARD_IO_SCREEN_CS 21
#define BOARD_IO_SCREEN_DIN 23

extern const board_specific_callbacks_t board_esp32_meshkit_callbacks;
#define BOARD_CALLBACKS board_esp32_meshkit_callbacks

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

#ifdef __cplusplus
}
#endif

#endif /* _IOT_BOARD_H_ */
