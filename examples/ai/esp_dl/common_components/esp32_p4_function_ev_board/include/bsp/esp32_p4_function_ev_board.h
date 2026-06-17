/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "bsp/display.h"
#include "lvgl.h"
#include "esp_lv_adapter.h"

/**************************************************************************************************
 *  BSP Board Name
 **************************************************************************************************/
#define BSP_BOARD_ESP32_P4_FUNCTION_EV_BOARD

/**************************************************************************************************
 *  BSP Capabilities
 **************************************************************************************************/
#define BSP_CAPS_DISPLAY        1
#define BSP_CAPS_TOUCH          1
#define BSP_CAPS_CAMERA         1

/**************************************************************************************************
 *  Pinout
 **************************************************************************************************/

/* I2C (shared by the LCD touch controller and the camera SCCB) */
#define BSP_I2C_SCL           (GPIO_NUM_8)
#define BSP_I2C_SDA           (GPIO_NUM_7)

/* Display and Touch */
#if CONFIG_BSP_LCD_TYPE_1024_600
#define BSP_LCD_BACKLIGHT     (GPIO_NUM_26)
#define BSP_LCD_RST           (GPIO_NUM_27)
#define BSP_LCD_TOUCH_RST     (GPIO_NUM_NC)
#define BSP_LCD_TOUCH_INT     (GPIO_NUM_NC)
#else
#define BSP_LCD_BACKLIGHT     (GPIO_NUM_23)
#define BSP_LCD_RST           (GPIO_NUM_NC)
#define BSP_LCD_TOUCH_RST     (GPIO_NUM_NC)
#define BSP_LCD_TOUCH_INT     (GPIO_NUM_NC)
#endif

/* Camera (MIPI-CSI) */
#define BSP_CAMERA_GPIO_XCLK (GPIO_NUM_NC)
#define BSP_CAMERA_RST       (GPIO_NUM_NC)

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
 * I2C interface
 **************************************************************************************************/
#define BSP_I2C_NUM     CONFIG_BSP_I2C_NUM

/**
 * @brief Init I2C driver
 */
esp_err_t bsp_i2c_init(void);

/**
 * @brief Deinit I2C driver and free its resources
 */
esp_err_t bsp_i2c_deinit(void);

/**
 * @brief Get I2C driver handle
 */
i2c_master_bus_handle_t bsp_i2c_get_handle(void);

/**************************************************************************************************
 * LCD interface (MIPI-DSI + LVGL)
 *
 * LVGL is not thread safe: take bsp_display_lock() before any lv_* call, release with bsp_display_unlock().
 **************************************************************************************************/
#define BSP_LCD_PIXEL_CLOCK_MHZ     (80)

/**
 * @brief BSP display configuration structure
 */
typedef struct {
    bsp_display_config_t hw_cfg;          /*!< Display HW (MIPI-DSI) configuration */
    esp_lv_adapter_rotation_t rotation;   /*!< LVGL display rotation */
    uint32_t task_stack_size;             /*!< LVGL task stack size in bytes, 0 uses adapter default */
} bsp_display_cfg_t;

/**
 * @brief Initialize display (MIPI-DSI + display controller + LVGL task)
 *
 * @return Pointer to LVGL display or NULL when error occurred
 */
lv_display_t *bsp_display_start(void);

/**
 * @brief Initialize display with a custom configuration
 *
 * @return Pointer to LVGL display or NULL when error occurred
 */
lv_display_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg);

/**
 * @brief Deinitialize display
 */
void bsp_display_stop(lv_display_t *display);

/**
 * @brief Get pointer to input device (touch)
 */
lv_indev_t *bsp_display_get_input_dev(void);

/**
 * @brief Take LVGL mutex
 *
 * @param timeout_ms Timeout in [ms]. 0 will block indefinitely.
 */
bool bsp_display_lock(uint32_t timeout_ms);

/**
 * @brief Give LVGL mutex
 */
void bsp_display_unlock(void);

/**
 * @brief Rotate screen
 */
void bsp_display_rotate(lv_display_t *disp, lv_disp_rotation_t rotation);

/**************************************************************************************************
 * Camera interface (MIPI-CSI)
 **************************************************************************************************/
#include "esp_video_device.h"
#define BSP_CAMERA_DEVICE       ESP_VIDEO_MIPI_CSI_DEVICE_NAME
#define BSP_CAMERA_ROTATION     (0)

/**
 * @brief BSP camera configuration structure (for future use)
 */
typedef struct {
    uint8_t dummy;
} bsp_camera_cfg_t;

/**
 * @brief Initialize camera sensor through esp_video.
 */
esp_err_t bsp_camera_start(const bsp_camera_cfg_t *cfg);

#ifdef __cplusplus
}
#endif
