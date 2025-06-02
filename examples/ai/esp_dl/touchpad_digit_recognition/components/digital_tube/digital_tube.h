/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "ch455g.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
* @brief   Digital tube controller (ch455g) initialization
*
* @param i2c_num   I2C number
* @param sda_pin   I2C SDA pin number
* @param scl_pin   I2C SCL pin number
*
* @return
*      - ESP_OK: Successfully initialized digital tube controller driver
*      - ESP_ERR_NO_MEM: Insufficient memory
*      - Others: Unknown esp-idf I2C driver errors
*/
#define digital_tube_driver_install(i2c_num, sda_pin, scl_pin)  ch455g_driver_install(i2c_num, sda_pin, scl_pin)

/**
 * @brief   Release resources allocated using digital_tube_driver_install()
 *
 * @return
 *      - ESP_OK: Successfully released resources
 *      - ESP_ERR_INVALID_STATE: Driver is not initialized
 */
#define digital_tube_driver_uninstall()  ch455_driver_uninstall()

/**
 * @brief   Enable digital tube
 *
 * @return
 */
esp_err_t digital_tube_enable(void);

//TODO: add digital_tube_disable

/**
 * @brief   Clear specified digital tube's current pattern
 *
 * @param index     Digital tube index (0~3)
 *
 * @return
 *      - ESP_OK: Successfully cleared pattern
 *      - ESP_ERR_INVALID_ARG: Invalid digital tube index (address)
 */
esp_err_t digital_tube_clear(uint8_t index);

/**
* @brief   ch455g write number and index
*
* This function sends a command to ch455g and gets it control the
* specified index digital tube display the specified number.
*
* @param index   Digital tube index  (0~3)
* @param num     Digital tube number (0~9)
*
* @return
*      - ESP_OK: Successfully write digital number and index
*      - ESP_ERR_NO_MEM: Insufficient memory
*      - Others: Unknown esp-idf i2c driver errors
*/
esp_err_t digital_tube_write_num(uint8_t index, uint8_t num);

/**
 * @brief   Clear digital tube's current pattern
 *
 * @return
 *      - ESP_OK: Successfully cleared pattern
 *      - ESP_ERR_INVALID_ARG: Invalid digital tube index (address)
 */
esp_err_t digital_tube_clear_all(void);

/**
 * TODO: Put it into BSP
 * @brief   X-line digital tube display number
 *
 * This function controls the x-line digital tube (which located in the first line of
 * the display area) display the specified number.
 *
 * @param num   Digital tube number (0~99)
 *
 * @return
 *      - ESP_OK: Successfully display number in x-line digital tube
 *      - Others: Unknown esp-idf i2c driver errors
 */
esp_err_t digital_tube_show_x(uint8_t num);

/**
 * TODO: Put it into BSP
 * @brief   Y-line digital tube display number
 *
 * This function controls the y-line digital tube (which located in the second line of
 * the display area) display the specified number.
 *
 * @param num   Digital tube number (0~99)
 *
 * @return
 *      - ESP_OK: Successfully display number in y-line digital tube
 *      - Others: Unknown esp-idf i2c driver errors
 */
esp_err_t digital_tube_show_y(uint8_t num);

/**
 * TODO: Put it into BSP
 * TODO: refactor into non-blocking
 * @brief   Digital tube displays reloading
 */
void digital_tube_show_reload(uint32_t ms);

#ifdef __cplusplus
}
#endif
