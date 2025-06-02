/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CH455_MAX_TUBE_NUM      4
#define CH455_I2C_ADDR          0x40
#define CH455_I2C_MASK          0x3E

#define CH455_BIT_ENABLE        0x01
#define CH455_BIT_SLEEP         0x04
#define CH455_BIT_7SEG          0x08
#define CH455_BIT_INTENS1       0x10
#define CH455_BIT_INTENS2       0x20
#define CH455_BIT_INTENS3       0x30
#define CH455_BIT_INTENS4       0x40
#define CH455_BIT_INTENS5       0x50
#define CH455_BIT_INTENS6       0x60
#define CH455_BIT_INTENS7       0x70
#define CH455_BIT_INTENS8       0x00

#define CH455_DIG0              0x1400
#define CH455_DIG1              0x1500
#define CH455_DIG2              0x1600
#define CH455_DIG3              0x1700

typedef enum {
    CH455_SYS_OFF = 0x0400,
    CH455_SYS_ON  = (CH455_SYS_OFF | CH455_BIT_ENABLE),
    CH455_SYS_SLEEP_OFF = CH455_SYS_OFF,
    CH455_SYS_SLEEP_ON  = (CH455_SYS_OFF | CH455_BIT_SLEEP),
    CH455_SYS_SEG_7_ON = (CH455_SYS_ON | CH455_BIT_7SEG),
    CH455_SYS_SEG_8_ON = (CH455_SYS_ON | 0x00)
} ch455_system_cmd_t;

/**
 * @brief   ch455 driver initialization
 *
 * @param i2c_num   I2C port number
 * @param sda_pin   I2C SDA pin
 * @param scl_pin   I2C SCL pin
 *
 * @return
 *      - ESP_OK: Successfully initialized ch455 driver
 *      - ESP_ERR_NO_MEM: Insufficient memory
 */
esp_err_t ch455g_driver_install(i2c_port_t i2c_num, gpio_num_t sda_pin, gpio_num_t scl_pin);

/**
 * @brief   Release resources allocated using ch455g_driver_install()
 *
 * @return
 *      - ESP_OK: Successfully released resources
 *      - ESP_ERR_INVALID_STATE: Driver is not initialized
 */
esp_err_t ch455_driver_uninstall(void);

/**
 * @brief   Send one command to ch455
 *
 * @param ch455_cmd     ch455 command
 * @return
 *      - ESP_OK: Successfully send command to ch455
 *      - Others: Unknown I2C driver exceptions
 */
esp_err_t ch455g_write_cmd(uint16_t ch455_cmd);

#ifdef __cplusplus
}
#endif
