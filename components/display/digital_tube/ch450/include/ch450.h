/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _CH450_H_
#define _CH450_H_

#include "esp_log.h"
#include "ch450.h"
#include "i2c_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Commands
 *
 */
typedef enum {
    CH450_SEG_2 = 0x64,
    CH450_SEG_3 = 0x66,
    CH450_SEG_4 = 0x68,
    CH450_SEG_5 = 0x6A,
    CH450_SEG_6 = 0x6C,
    CH450_SEG_7 = 0x6E,
    CH450_SYS = 0x48,
    CH450_READ = 0x4F,
} ch450_cmd_t;

typedef void *ch450_handle_t;

/**
 * @brief Create and init sensor object and return a sensor handle
 *
 * @param bus I2C bus object handle
 *
 * @return
 *     - NULL Fail
 *     - Others Success
 */
ch450_handle_t ch450_create(i2c_bus_handle_t bus);

/**
 * @brief Delete and release a device object
 *
 * @param dev object handle of ch450
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t ch450_delete(ch450_handle_t dev);

/**
 * @brief write register of ch450
 *
 * @param dev object handle of ch450
 * @param ch450_cmd command of setting register of ch450
 * @param val register value
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t ch450_write(ch450_handle_t dev, ch450_cmd_t ch450_cmd, uint8_t val);

/**
 * @brief Write number to 7-segment device
 *
 * @param dev object handle of ch450
 * @param seg_idx led index, 0 - 5
 * @param val Digit value
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t ch450_write_num(ch450_handle_t dev, uint8_t seg_idx, uint8_t val);

#ifdef __cplusplus
}
#endif
#endif
