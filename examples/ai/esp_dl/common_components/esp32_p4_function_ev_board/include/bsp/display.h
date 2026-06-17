/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include "esp_lcd_types.h"
#include "esp_lcd_mipi_dsi.h"
#include "sdkconfig.h"

/* LCD color formats */
#define ESP_LCD_COLOR_FORMAT_RGB565    (1)
#define ESP_LCD_COLOR_FORMAT_RGB888    (2)

/* LCD display color format */
#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
#define BSP_LCD_COLOR_FORMAT        (ESP_LCD_COLOR_FORMAT_RGB888)
#else
#define BSP_LCD_COLOR_FORMAT        (ESP_LCD_COLOR_FORMAT_RGB565)
#endif
/* LCD display color bytes endianness */
#define BSP_LCD_BIGENDIAN           (0)
/* LCD display color bits */
#define BSP_LCD_BITS_PER_PIXEL      (16)
/* LCD display color space */
#define BSP_LCD_COLOR_SPACE         (LCD_RGB_ELEMENT_ORDER_RGB)

#if CONFIG_BSP_LCD_TYPE_1024_600
/* LCD display definition 1024x600 */
#define BSP_LCD_H_RES              (1024)
#define BSP_LCD_V_RES              (600)
#else
/* LCD display definition 1280x800 */
#define BSP_LCD_H_RES              (800)
#define BSP_LCD_V_RES              (1280)
#endif

#define BSP_LCD_MIPI_DSI_LANE_NUM          (2)    // 2 data lanes
#define BSP_LCD_MIPI_DSI_LANE_BITRATE_MBPS (1000) // 1Gbps

#define BSP_MIPI_DSI_PHY_PWR_LDO_CHAN       (3)  // LDO_VO3 is connected to VDD_MIPI_DPHY
#define BSP_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV (2500)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BSP display configuration structure
 */
typedef struct {
    struct {
        mipi_dsi_phy_clock_source_t phy_clk_src;  /*!< DSI bus config - clock source */
        uint32_t lane_bit_rate_mbps;              /*!< DSI bus config - lane bit rate */
    } dsi_bus;
} bsp_display_config_t;

/**
 * @brief BSP display return handles
 */
typedef struct {
    esp_lcd_dsi_bus_handle_t    mipi_dsi_bus;  /*!< MIPI DSI bus handle */
    esp_lcd_panel_io_handle_t   io;            /*!< ESP LCD IO handle */
    esp_lcd_panel_handle_t      panel;         /*!< ESP LCD panel (color) handle */
    esp_lcd_panel_handle_t      control;       /*!< ESP LCD panel (control) handle */
} bsp_lcd_handles_t;

/**
 * @brief Create and initialize the MIPI-DSI LCD panel.
 *
 * @param[in]  config    display configuration
 * @param[out] ret_panel esp_lcd panel handle
 * @param[out] ret_io    esp_lcd IO handle
 * @return ESP_OK on success, otherwise an esp_lcd error code.
 */
esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel,
                          esp_lcd_panel_io_handle_t *ret_io);

/**
 * @brief Create and initialize the MIPI-DSI LCD panel, returning all handles.
 *
 * @param[in]  config      display configuration
 * @param[out] ret_handles all esp_lcd handles in one structure
 * @return ESP_OK on success, otherwise an esp_lcd error code.
 */
esp_err_t bsp_display_new_with_handles(const bsp_display_config_t *config, bsp_lcd_handles_t *ret_handles);

/**
 * @brief Delete the display panel and free its resources.
 */
void bsp_display_delete(void);

/**
 * @brief Initialize the display backlight (LEDC PWM).
 */
esp_err_t bsp_display_brightness_init(void);

/**
 * @brief Deinitialize the display backlight.
 */
esp_err_t bsp_display_brightness_deinit(void);

/**
 * @brief Set display brightness in percent (backlight must be initialized first).
 */
esp_err_t bsp_display_brightness_set(int brightness_percent);

/**
 * @brief Turn the display backlight on.
 */
esp_err_t bsp_display_backlight_on(void);

/**
 * @brief Turn the display backlight off.
 */
esp_err_t bsp_display_backlight_off(void);

#ifdef __cplusplus
}
#endif
