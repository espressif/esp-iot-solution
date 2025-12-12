/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#include "hal/lcd_types.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_idf_version.h"

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
} co5300_lcd_init_cmd_t;

/**
 * @brief LCD panel vendor configuration.
 *
 * @note  This structure needs to be passed to the `vendor_config` field in `esp_lcd_panel_dev_config_t`.
 *
 */
typedef struct {
    const co5300_lcd_init_cmd_t *init_cmds;         /*!< Pointer to initialization commands array. Set to NULL if using default commands.
                                                     *   The array should be declared as `static const` and positioned outside the function.
                                                     *   Please refer to `vendor_specific_init_default` in source file.
                                                     */
    uint16_t init_cmds_size;                        /*<! Number of commands in above array */
    union {
#if SOC_MIPI_DSI_SUPPORTED
        struct {
            esp_lcd_dsi_bus_handle_t dsi_bus;               /*!< MIPI-DSI bus configuration */
            const esp_lcd_dpi_panel_config_t *dpi_config;   /*!< MIPI-DPI panel configuration */
        } mipi_config;
#endif
    };
    struct {
        unsigned int use_mipi_interface: 1;         /*<! Set to 1 if using MIPI interface, default is RGB interface */
        unsigned int use_qspi_interface: 1;         /*<! Set to 1 if use QSPI interface, default is SPI interface */
    } flags;
} co5300_vendor_config_t;

/**
 * @brief Create LCD panel for model CO5300
 *
 * @note  When `enable_io_multiplex` is set to 1, this function will first initialize the CO5300 with vendor specific initialization and then calls `esp_lcd_new_rgb_panel()` to create an RGB LCD panel. And the `esp_lcd_panel_init()` function will only initialize RGB.
 * @note  When `enable_io_multiplex` is set to 0, this function will only call `esp_lcd_new_rgb_panel()` to create an RGB LCD panel. And the `esp_lcd_panel_init()` function will initialize both the CO5300 and RGB.
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
esp_err_t esp_lcd_new_panel_co5300(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel);

/**
 * @brief Set brightness for CO5300 LCD panel
 *
 * @param[in] panel LCD panel handle
 * @param[in] brightness_percent Brightness percentage (0-100)
 * @return
 *      - ESP_ERR_INVALID_ARG   if parameter is invalid
 *      - ESP_ERR_INVALID_STATE if panel IO not initialized
 *      - ESP_OK                on success
 *      - Otherwise             on fail
 */
esp_err_t esp_lcd_panel_co5300_set_brightness(esp_lcd_panel_handle_t panel, uint8_t brightness_percent);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////// Default Configuration Macros for SPI/QSPI Interface //////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief SPI/QSPI bus configuration structure
 */
#define CO5300_PANEL_BUS_SPI_CONFIG(sclk, mosi, max_trans_sz)   \
    {                                                           \
        .sclk_io_num = sclk,                                    \
        .mosi_io_num = mosi,                                    \
        .miso_io_num = -1,                                      \
        .quadhd_io_num = -1,                                    \
        .quadwp_io_num = -1,                                    \
        .max_transfer_sz = max_trans_sz,                        \
    }
#define CO5300_PANEL_BUS_QSPI_CONFIG(sclk, d0, d1, d2, d3, max_trans_sz) \
    {                                                           \
        .sclk_io_num = sclk,                                    \
        .data0_io_num = d0,                                     \
        .data1_io_num = d1,                                     \
        .data2_io_num = d2,                                     \
        .data3_io_num = d3,                                     \
        .max_transfer_sz = max_trans_sz,                        \
    }

/**
 * @brief LCD panel IO configuration structure
 *
 */
#define CO5300_PANEL_IO_SPI_CONFIG(cs, dc, cb, cb_ctx)          \
    {                                                           \
        .cs_gpio_num = cs,                                      \
        .dc_gpio_num = dc,                                      \
        .spi_mode = 0,                                          \
        .pclk_hz = 40 * 1000 * 1000,                            \
        .trans_queue_depth = 10,                                \
        .on_color_trans_done = cb,                              \
        .user_ctx = cb_ctx,                                     \
        .lcd_cmd_bits = 8,                                      \
        .lcd_param_bits = 8,                                    \
    }
#define CO5300_PANEL_IO_QSPI_CONFIG(cs, cb, cb_ctx)             \
    {                                                           \
        .cs_gpio_num = cs,                                      \
        .dc_gpio_num = -1,                                      \
        .spi_mode = 0,                                          \
        .pclk_hz = 40 * 1000 * 1000,                            \
        .trans_queue_depth = 10,                                \
        .on_color_trans_done = cb,                              \
        .user_ctx = cb_ctx,                                     \
        .lcd_cmd_bits = 32,                                     \
        .lcd_param_bits = 8,                                    \
        .flags = {                                              \
            .quad_mode = true,                                  \
        },                                                      \
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////// Default Configuration Macros for MIPI-DSI Interface //////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief MIPI-DSI bus configuration structure
 */
#define CO5300_PANEL_BUS_DSI_1CH_CONFIG()                 \
    {                                                     \
        .bus_id = 0,                                      \
        .num_data_lanes = 1,                              \
        .phy_clk_src = 0,                                 \
        .lane_bit_rate_mbps = 480,                        \
    }

/**
 * @brief MIPI DBI panel IO configuration structure
 *
 */
#define CO5300_PANEL_IO_DBI_CONFIG() \
    {                                 \
        .virtual_channel = 0,         \
        .lcd_cmd_bits = 8,            \
        .lcd_param_bits = 8,          \
    }

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
/**
 * @brief MIPI DPI configuration structure
 *
 * @note  refresh_rate = (dpi_clock_freq_mhz * 1000000) / (h_res + hsync_pulse_width + hsync_back_porch + hsync_front_porch)
 *                                                      / (v_res + vsync_pulse_width + vsync_back_porch + vsync_front_porch)
 *
 */
#define CO5300_466_466_PANEL_60HZ_DPI_CONFIG(px_format)          \
    {                                                            \
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,             \
        .dpi_clock_freq_mhz = 16,                                \
        .virtual_channel = 0,                                    \
        .pixel_format = px_format,                               \
        .num_fbs = 1,                                            \
        .video_timing = {                                        \
            .h_size = 466,                                       \
            .v_size = 466,                                       \
            .hsync_back_porch = 32,                              \
            .hsync_pulse_width = 4,                              \
            .hsync_front_porch = 32,                             \
            .vsync_back_porch = 12,                              \
            .vsync_pulse_width = 4,                              \
            .vsync_front_porch = 18,                             \
        },                                                       \
        .flags.use_dma2d = true,                                 \
    }
#endif

/**
 * @brief MIPI DPI configuration structure
 *
 * @note  refresh_rate = (dpi_clock_freq_mhz * 1000000) / (h_res + hsync_pulse_width + hsync_back_porch + hsync_front_porch)
 *                                                      / (v_res + vsync_pulse_width + vsync_back_porch + vsync_front_porch)
 *
 */
#define CO5300_466_466_PANEL_60HZ_DPI_CONFIG_CF(color_format)    \
    {                                                            \
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,             \
        .dpi_clock_freq_mhz = 16,                                \
        .virtual_channel = 0,                                    \
        .in_color_format = color_format,                         \
        .num_fbs = 1,                                            \
        .video_timing = {                                        \
            .h_size = 466,                                       \
            .v_size = 466,                                       \
            .hsync_back_porch = 32,                              \
            .hsync_pulse_width = 4,                              \
            .hsync_front_porch = 32,                             \
            .vsync_back_porch = 12,                              \
            .vsync_pulse_width = 4,                              \
            .vsync_front_porch = 18,                             \
        },                                                       \
    }

#ifdef __cplusplus
}
#endif
