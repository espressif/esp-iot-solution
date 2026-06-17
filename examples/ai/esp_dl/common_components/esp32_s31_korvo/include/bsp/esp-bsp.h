/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "bsp/display.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_BOARD_ESP32_S31_KORVO

/* Shared I2C (also used as the camera SCCB bus). */
#define BSP_I2C_SDA               (GPIO_NUM_0)
#define BSP_I2C_SCL               (GPIO_NUM_1)

/* RGB LCD data/control pins. */
#define BSP_LCD_DATA0             (GPIO_NUM_8)   /* B3 */
#define BSP_LCD_DATA1             (GPIO_NUM_9)   /* B4 */
#define BSP_LCD_DATA2             (GPIO_NUM_10)  /* B5 */
#define BSP_LCD_DATA3             (GPIO_NUM_11)  /* B6 */
#define BSP_LCD_DATA4             (GPIO_NUM_12)  /* B7 */
#define BSP_LCD_DATA5             (GPIO_NUM_13)  /* G2 */
#define BSP_LCD_DATA6             (GPIO_NUM_14)  /* G3 */
#define BSP_LCD_DATA7             (GPIO_NUM_15)  /* G4 */
#define BSP_LCD_DATA8             (GPIO_NUM_16)  /* G5 */
#define BSP_LCD_DATA9             (GPIO_NUM_17)  /* G6 */
#define BSP_LCD_DATA10            (GPIO_NUM_18)  /* G7 */
#define BSP_LCD_DATA11            (GPIO_NUM_19)  /* R3 */
#define BSP_LCD_DATA12            (GPIO_NUM_33)  /* R4 */
#define BSP_LCD_DATA13            (GPIO_NUM_34)  /* R5 */
#define BSP_LCD_DATA14            (GPIO_NUM_35)  /* R6 */
#define BSP_LCD_DATA15            (GPIO_NUM_36)  /* R7 */
#define BSP_LCD_PCLK              (GPIO_NUM_40)
#define BSP_LCD_DE                (GPIO_NUM_43)
#define BSP_LCD_HSYNC             (GPIO_NUM_44)
#define BSP_LCD_VSYNC             (GPIO_NUM_45)
#define BSP_LCD_BACKLIGHT         (GPIO_NUM_NC)
#define BSP_LCD_DISP_EN           (GPIO_NUM_NC)

/* LCD touch shares the board I2C bus. */
#define BSP_LCD_TOUCH_RST         (GPIO_NUM_NC)
#define BSP_LCD_TOUCH_INT         (GPIO_NUM_NC)

/* DVP camera, SCCB shares the board I2C bus. */
#include "esp_video_device.h"
#define BSP_CAMERA_DEVICE         ESP_VIDEO_DVP_DEVICE_NAME
/* PPA camera post-processing: BE->LE byte swap + mirror/rotation. Tune to the mounting. */
#define BSP_CAMERA_RGB565_BYTE_SWAP   (1)
#define BSP_CAMERA_ROTATION_DEGREES   (0)   /* 0 / 90 / 180 / 270 */
#define BSP_CAMERA_MIRROR_X           (1)
#define BSP_CAMERA_MIRROR_Y           (1)
#define BSP_CAMERA_SCCB_SDA       BSP_I2C_SDA
#define BSP_CAMERA_SCCB_SCL       BSP_I2C_SCL
#define BSP_CAMERA_XCLK           (GPIO_NUM_55)
#define BSP_CAMERA_XCLK_FREQ      (20 * 1000 * 1000)
#define BSP_CAMERA_PCLK           (GPIO_NUM_54)
#define BSP_CAMERA_VSYNC          (GPIO_NUM_56)
#define BSP_CAMERA_HSYNC          (GPIO_NUM_57)
#define BSP_CAMERA_D0             (GPIO_NUM_46)
#define BSP_CAMERA_D1             (GPIO_NUM_47)
#define BSP_CAMERA_D2             (GPIO_NUM_48)
#define BSP_CAMERA_D3             (GPIO_NUM_49)
#define BSP_CAMERA_D4             (GPIO_NUM_50)
#define BSP_CAMERA_D5             (GPIO_NUM_51)
#define BSP_CAMERA_D6             (GPIO_NUM_52)
#define BSP_CAMERA_D7             (GPIO_NUM_53)
#define BSP_CAMERA_RESET          (GPIO_NUM_NC)
#define BSP_CAMERA_PWDN           (GPIO_NUM_NC)

/**
 * @brief BSP camera configuration structure (kept API-compatible with the P4 board).
 */
typedef struct {
    uint8_t dummy;
} bsp_camera_cfg_t;

/**
 * @brief Initialize the DVP camera through esp_video (V4L2 /dev/video0).
 *
 * @param cfg Reserved for future use, pass NULL.
 */
esp_err_t bsp_camera_start(const bsp_camera_cfg_t *cfg);

/**
 * @brief Set LCD backlight brightness.
 *
 * The ESP32-S31-Korvo RGB sub-board has no dimmable backlight, so this is a
 * no-op kept for API compatibility with the P4 board.
 */
esp_err_t bsp_display_brightness_set(int brightness_percent);

/**
 * @brief Initialize the shared I2C master bus (idempotent).
 */
esp_err_t bsp_i2c_init(void);

/**
 * @brief Get the shared I2C master bus handle, initializing it if needed.
 *
 * @return The I2C bus handle, or NULL on failure.
 */
i2c_master_bus_handle_t bsp_i2c_get_handle(void);

#ifdef __cplusplus
}
#endif
