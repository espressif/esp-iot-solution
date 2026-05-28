/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_codec_dev.h"
#include "esp_err.h"
#include "bsp/display.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Shared I2C */
#define BSP_I2C_SDA                    (GPIO_NUM_0)
#define BSP_I2C_SCL                    (GPIO_NUM_1)

/* RGB LCD */
#define BSP_LCD_H_RES                  (800)
#define BSP_LCD_V_RES                  (480)
#define BSP_LCD_PIXEL_CLOCK_HZ         (20 * 1000 * 1000)
#define BSP_LCD_DATA_WIDTH             (16)
#define BSP_LCD_BITS_PER_PIXEL         (16)
#define BSP_LCD_HSYNC_PULSE_WIDTH      (1)
#define BSP_LCD_HSYNC_BACK_PORCH       (40)
#define BSP_LCD_HSYNC_FRONT_PORCH      (20)
#define BSP_LCD_VSYNC_PULSE_WIDTH      (1)
#define BSP_LCD_VSYNC_BACK_PORCH       (10)
#define BSP_LCD_VSYNC_FRONT_PORCH      (5)

#define BSP_LCD_DATA0                  (GPIO_NUM_8)
#define BSP_LCD_DATA1                  (GPIO_NUM_9)
#define BSP_LCD_DATA2                  (GPIO_NUM_10)
#define BSP_LCD_DATA3                  (GPIO_NUM_11)
#define BSP_LCD_DATA4                  (GPIO_NUM_12)
#define BSP_LCD_DATA5                  (GPIO_NUM_13)
#define BSP_LCD_DATA6                  (GPIO_NUM_14)
#define BSP_LCD_DATA7                  (GPIO_NUM_15)
#define BSP_LCD_DATA8                  (GPIO_NUM_16)
#define BSP_LCD_DATA9                  (GPIO_NUM_17)
#define BSP_LCD_DATA10                 (GPIO_NUM_18)
#define BSP_LCD_DATA11                 (GPIO_NUM_19)
#define BSP_LCD_DATA12                 (GPIO_NUM_33)
#define BSP_LCD_DATA13                 (GPIO_NUM_34)
#define BSP_LCD_DATA14                 (GPIO_NUM_35)
#define BSP_LCD_DATA15                 (GPIO_NUM_36)
#define BSP_LCD_PCLK                   (GPIO_NUM_40)
#define BSP_LCD_DE                     (GPIO_NUM_43)
#define BSP_LCD_HSYNC                  (GPIO_NUM_44)
#define BSP_LCD_VSYNC                  (GPIO_NUM_45)
#define BSP_LCD_BACKLIGHT              (GPIO_NUM_NC)
#define BSP_LCD_DISP_EN                (GPIO_NUM_NC)

/* LCD Touch */
#define BSP_LCD_TOUCH_I2C_SDA          BSP_I2C_SDA
#define BSP_LCD_TOUCH_I2C_SCL          BSP_I2C_SCL
#define BSP_LCD_TOUCH_RST              (GPIO_NUM_NC)
#define BSP_LCD_TOUCH_INT              (GPIO_NUM_NC)

/* Audio */
#define BSP_AUDIO_I2S_MCLK             (GPIO_NUM_2)
#define BSP_AUDIO_I2S_SCLK             (GPIO_NUM_3)
#define BSP_AUDIO_I2S_LRCLK            (GPIO_NUM_4)
#define BSP_AUDIO_I2S_SDOUT            (GPIO_NUM_5)
#define BSP_AUDIO_I2S_DSIN             (GPIO_NUM_6)
#define BSP_AUDIO_PA_CTRL              (GPIO_NUM_7)

#define BSP_AUDIO_DEFAULT_SAMPLE_RATE      (48000)
#define BSP_AUDIO_DEFAULT_BITS_PER_SAMPLE  (16)
#define BSP_AUDIO_DEFAULT_CHANNELS         (2)

esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config);
esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void);
esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void);

esp_err_t bsp_display_backlight_on(void);
esp_err_t bsp_display_backlight_off(void);
esp_err_t bsp_display_brightness_init(void);
esp_err_t bsp_display_brightness_set(int brightness_percent);
int bsp_display_brightness_get(void);

#ifdef __cplusplus
}
#endif
