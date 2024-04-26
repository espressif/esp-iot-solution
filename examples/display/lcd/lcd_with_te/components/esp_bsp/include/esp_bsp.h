
/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ESP BSP: ESP-BOX-3
 */

#pragma once

#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "lvgl.h"
#include "lv_port.h"

/**************************************************************************************************
 *  pinout
 **************************************************************************************************/
#define BSP_I2C_NUM                     (I2C_NUM_0)
#define BSP_I2C_CLK_SPEED_HZ            400000

// Bit number used to represent command and parameter
#define EXAMPLE_LCD_I80_CLK             (20 * 1000 * 1000)
#define EXAMPLE_LCD_CMD_BITS            8
#define EXAMPLE_LCD_PARAM_BITS          8

#define EXAMPLE_LCD_QSPI_HOST           (SPI2_HOST)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// LCD spec of I80 /////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define EXAMPLE_PIN_NUM_I80_DATA0       (GPIO_NUM_7)
#define EXAMPLE_PIN_NUM_I80_DATA1       (GPIO_NUM_8)
#define EXAMPLE_PIN_NUM_I80_DATA2       (GPIO_NUM_9)
#define EXAMPLE_PIN_NUM_I80_DATA3       (GPIO_NUM_10)
#define EXAMPLE_PIN_NUM_I80_DATA4       (GPIO_NUM_11)
#define EXAMPLE_PIN_NUM_I80_DATA5       (GPIO_NUM_12)
#define EXAMPLE_PIN_NUM_I80_DATA6       (GPIO_NUM_13)
#define EXAMPLE_PIN_NUM_I80_DATA7       (GPIO_NUM_14)
#define EXAMPLE_PIN_NUM_I80_PCLK        (GPIO_NUM_3)
#define EXAMPLE_PIN_NUM_I80_CS          (GPIO_NUM_44)
#define EXAMPLE_PIN_NUM_I80_DC          (GPIO_NUM_4)
#define EXAMPLE_PIN_NUM_I80_RD          (GPIO_NUM_2)
#define EXAMPLE_PIN_NUM_I80_RST         (GPIO_NUM_5)
#define EXAMPLE_PIN_NUM_I80_BL          (GPIO_NUM_38)
#define EXAMPLE_PIN_NUM_I80_TE          (GPIO_NUM_16)

#define EXAMPLE_PIN_NUM_I80_TOUCH_SCL   (GPIO_NUM_18)
#define EXAMPLE_PIN_NUM_I80_TOUCH_SDA   (GPIO_NUM_17)
#define EXAMPLE_PIN_NUM_I80_TOUCH_RST   (-1)
#define EXAMPLE_PIN_NUM_I80_TOUCH_INT   (GPIO_NUM_21)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// LCD spec of QSPI /////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define EXAMPLE_PIN_NUM_QSPI_CS         (GPIO_NUM_45)
#define EXAMPLE_PIN_NUM_QSPI_PCLK       (GPIO_NUM_47)
#define EXAMPLE_PIN_NUM_QSPI_DATA0      (GPIO_NUM_21)
#define EXAMPLE_PIN_NUM_QSPI_DATA1      (GPIO_NUM_48)
#define EXAMPLE_PIN_NUM_QSPI_DATA2      (GPIO_NUM_40)
#define EXAMPLE_PIN_NUM_QSPI_DATA3      (GPIO_NUM_39)
#define EXAMPLE_PIN_NUM_QSPI_RST        (GPIO_NUM_NC)
#define EXAMPLE_PIN_NUM_QSPI_DC         (GPIO_NUM_8)
#define EXAMPLE_PIN_NUM_QSPI_TE         (GPIO_NUM_38)
#define EXAMPLE_PIN_NUM_QSPI_BL         (GPIO_NUM_1)

#define EXAMPLE_PIN_NUM_QSPI_TOUCH_SCL  (GPIO_NUM_8)
#define EXAMPLE_PIN_NUM_QSPI_TOUCH_SDA  (GPIO_NUM_4)
#define EXAMPLE_PIN_NUM_QSPI_TOUCH_RST  (-1)
#define EXAMPLE_PIN_NUM_QSPI_TOUCH_INT  (-1)

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief BSP display configuration structure
 *
 */
typedef struct {
    lvgl_port_cfg_t lvgl_port_cfg;  /*!< Configuration for the LVGL port */
    uint32_t buffer_size;           /*!< Size of the buffer for the screen in pixels */
    lv_disp_rot_t rotate;           /*!< Rotation configuration for the display */
} bsp_display_cfg_t;

/**
 * @brief Init I2C driver
 *
 * @return
 *      - ESP_OK                On success
 *      - ESP_ERR_INVALID_ARG   I2C parameter error
 *      - ESP_FAIL              I2C driver installation error
 *
 */
esp_err_t bsp_i2c_init(void);

/**
 * @brief Deinit I2C driver and free its resources
 *
 * @return
 *      - ESP_OK                On success
 *      - ESP_ERR_INVALID_ARG   I2C parameter error
 *
 */
esp_err_t bsp_i2c_deinit(void);

/**
 * @brief Initialize display
 *
 * This function initializes SPI, display controller and starts LVGL handling task.
 * LCD backlight must be enabled separately by calling bsp_display_brightness_set()
 *
 * @param cfg display configuration
 *
 * @return Pointer to LVGL display or NULL when error occurred
 */
lv_disp_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg);

/**
 * @brief Get pointer to input device (touch, buttons, ...)
 *
 * @note The LVGL input device is initialized in bsp_display_start() function.
 *
 * @return Pointer to LVGL input device or NULL when not initialized
 */
lv_indev_t *bsp_display_get_input_dev(void);

/**
 * @brief Take LVGL mutex
 *
 * @param timeout_ms Timeout in [ms]. 0 will block indefinitely.
 * @return true  Mutex was taken
 * @return false Mutex was NOT taken
 */
bool bsp_display_lock(uint32_t timeout_ms);

/**
 * @brief Give LVGL mutex
 *
 */
void bsp_display_unlock(void);

#ifdef __cplusplus
}
#endif
