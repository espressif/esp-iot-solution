/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#include "hal/lcd_types.h"
#include "esp_lcd_panel_vendor.h"

#if SOC_LCD_RGB_SUPPORTED
#include "esp_lcd_panel_rgb.h"
#endif

#if SOC_MIPI_DSI_SUPPORTED
#include "esp_lcd_mipi_dsi.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LCD panel initialization commands.
 *
 */
typedef struct {
    int cmd;                /*<! The specific LCD command */
    const void *data;       /*<! Buffer that holds the command specific data */
    size_t data_bytes;      /*<! Size of `data` in memory, in bytes */
    unsigned int delay_ms;  /*<! Delay in milliseconds after this command */
} st7701_lcd_init_cmd_t;

/**
 * @brief LCD panel vendor configuration.
 *
 * @note  This structure needs to be passed to the `vendor_config` field in `esp_lcd_panel_dev_config_t`.
 *
 */
typedef struct {
    const st7701_lcd_init_cmd_t *init_cmds;         /*!< Pointer to initialization commands array. Set to NULL if using default commands.
                                                     *   The array should be declared as `static const` and positioned outside the function.
                                                     *   Please refer to `vendor_specific_init_default` in source file.
                                                     */
    uint16_t init_cmds_size;                        /*<! Number of commands in above array */
    union {
#if SOC_LCD_RGB_SUPPORTED
        const esp_lcd_rgb_panel_config_t *rgb_config;   /*!< RGB panel configuration */
#endif
#if SOC_MIPI_DSI_SUPPORTED
        struct {
            esp_lcd_dsi_bus_handle_t dsi_bus;               /*!< MIPI-DSI bus configuration */
            const esp_lcd_dpi_panel_config_t *dpi_config;   /*!< MIPI-DPI panel configuration */
        } mipi_config;
#endif
    };
    struct {
        unsigned int use_mipi_interface: 1;         /*<! Set to 1 if using MIPI interface, default is RGB interface */
        unsigned int mirror_by_cmd: 1;              /*<! The `mirror()` function will be implemented by LCD command if set to 1.
                                                     *   Otherwise, the function will be implemented by software.
                                                     *   This flag is only valid for the RGB interface.
                                                     */

        union {
            unsigned int auto_del_panel_io: 1;
            unsigned int enable_io_multiplex: 1;
        };  /*<! Delete the panel IO instance automatically if set to 1. All `*_by_cmd` flags will be invalid.
             *   If the panel IO pins are sharing other pins of the RGB interface to save GPIOs,
             *   Please set it to 1 to release the panel IO and its pins (except CS signal).
             *   This flag is only valid for the RGB interface.
             */
    } flags;
} st7701_vendor_config_t;

/**
 * @brief Create LCD panel for model ST7701
 *
 * @note  When `enable_io_multiplex` is set to 1, this function will first initialize the ST7701 with vendor specific initialization and then calls `esp_lcd_new_rgb_panel()` to create an RGB LCD panel. And the `esp_lcd_panel_init()` function will only initialize RGB.
 * @note  When `enable_io_multiplex` is set to 0, this function will only call `esp_lcd_new_rgb_panel()` to create an RGB LCD panel. And the `esp_lcd_panel_init()` function will initialize both the ST7701 and RGB.
 * @note  Vendor specific initialization can be different between manufacturers, should consult the LCD supplier for initialization sequence code.
 *
 * @param[in]  io LCD panel IO handle
 * @param[in]  panel_dev_config General panel device configuration (`vendor_config` and `rgb_config` are necessary)
 * @param[out] ret_panel Returned LCD panel handle
 * @return
 *      - ESP_ERR_INVALID_ARG   if parameter is invalid
 *      - ESP_OK                on success
 *      - Otherwise             on fail
 */
esp_err_t esp_lcd_new_panel_st7701(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel);

/**
 * @brief 3-wire SPI panel IO configuration structure
 *
 * @param[in] line_cfg SPI line configuration
 * @param[in] scl_active_edge SCL signal active edge, 0: rising edge, 1: falling edge
 *
 */
#define ST7701_PANEL_IO_3WIRE_SPI_CONFIG(line_cfg, scl_active_edge) \
    {                                                               \
        .line_config = line_cfg,                                    \
        .expect_clk_speed = PANEL_IO_3WIRE_SPI_CLK_MAX,             \
        .spi_mode = scl_active_edge ? 1 : 0,                        \
        .lcd_cmd_bytes = 1,                                         \
        .lcd_param_bytes = 1,                                       \
        .flags = {                                                  \
            .use_dc_bit = 1,                                        \
            .dc_zero_on_data = 0,                                   \
            .lsb_first = 0,                                         \
            .cs_high_active = 0,                                    \
            .del_keep_cs_inactive = 1,                              \
        },                                                          \
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Default Configuration Macros for RGB Interface /////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief RGB timing structure
 *
 * @note  refresh_rate = (pclk_hz * data_width) / (h_res + hsync_pulse_width + hsync_back_porch + hsync_front_porch)
 *                                              / (v_res + vsync_pulse_width + vsync_back_porch + vsync_front_porch)
 *                                              / bits_per_pixel
 *
 */
#define ST7701_480_480_PANEL_60HZ_RGB_TIMING()      \
    {                                               \
        .pclk_hz = 16 * 1000 * 1000,                \
        .h_res = 480,                               \
        .v_res = 480,                               \
        .hsync_pulse_width = 10,                    \
        .hsync_back_porch = 10,                     \
        .hsync_front_porch = 20,                    \
        .vsync_pulse_width = 10,                    \
        .vsync_back_porch = 10,                     \
        .vsync_front_porch = 10,                    \
        .flags.pclk_active_neg = false,             \
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////// Default Configuration Macros for MIPI-DSI Interface //////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief MIPI-DSI bus configuration structure
 */
#define ST7701_PANEL_BUS_DSI_2CH_CONFIG()                \
    {                                                    \
        .bus_id = 0,                                     \
        .num_data_lanes = 2,                             \
        .phy_clk_src = 0,                                \
        .lane_bit_rate_mbps = 500,                       \
    }

/**
 * @brief MIPI-DBI panel IO configuration structure
 *
 */
#define ST7701_PANEL_IO_DBI_CONFIG()  \
    {                                 \
        .virtual_channel = 0,         \
        .lcd_cmd_bits = 8,            \
        .lcd_param_bits = 8,          \
    }

/**
 * @brief MIPI DPI configuration structure
 *
 * @note  refresh_rate = (dpi_clock_freq_mhz * 1000000) / (h_res + hsync_pulse_width + hsync_back_porch + hsync_front_porch)
 *                                                      / (v_res + vsync_pulse_width + vsync_back_porch + vsync_front_porch)
 *
 * @param[in] px_format Pixel format of the panel
 *
 */
#define ST7701_480_360_PANEL_60HZ_DPI_CONFIG(px_format)  \
    {                                                    \
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,     \
        .dpi_clock_freq_mhz = 15,                        \
        .virtual_channel = 0,                            \
        .pixel_format = px_format,                       \
        .num_fbs = 1,                                    \
        .video_timing = {                                \
            .h_size = 480,                               \
            .v_size = 360,                               \
            .hsync_back_porch = 10,                      \
            .hsync_pulse_width = 10,                     \
            .hsync_front_porch = 20,                     \
            .vsync_back_porch = 10,                      \
            .vsync_pulse_width = 10,                     \
            .vsync_front_porch = 10,                     \
        },                                               \
        .flags.use_dma2d = true,                         \
    }

#ifdef __cplusplus
}
#endif
