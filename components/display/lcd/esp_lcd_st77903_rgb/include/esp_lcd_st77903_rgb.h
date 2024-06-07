/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_types.h"

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
} st77903_lcd_init_cmd_t;

/**
 * @brief LCD panel vendor configuration.
 *
 * @note  This structure can be used to enable QSPI mode and override default initialization commands.
 * @note  This structure needs to be passed to the `vendor_config` field in `esp_lcd_panel_dev_config_t`.
 *
 */
typedef struct {
    const esp_lcd_rgb_panel_config_t *rgb_config;
    const st77903_lcd_init_cmd_t *init_cmds;    /*!< Pointer to initialization commands array. Set to NULL if using default commands.
                                                 *   The array should be declared as `static const` and positioned outside the function.
                                                 *   Please refer to `vendor_specific_init_default` in source file
                                                 */
    uint16_t init_cmds_size;                    /*<! Number of commands in above array */

    struct {
        unsigned int mirror_by_cmd: 1;          /*<! The `esp_lcd_panel_mirror()` function will be implemented by LCD command if set to 1.
                                                 *   Otherwise, the function will be implemented by software.
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
} st77903_vendor_config_t;

/**
 * @brief 3-wire SPI panel IO configuration structure
 *
 * @param[in] line_cfg SPI line configuration
 * @param[in] scl_active_edge SCL signal active edge, 0: rising edge, 1: falling edge
 *
 */
#define ST77903_RGB_PANEL_IO_3WIRE_SPI_CONFIG(line_cfg, scl_active_edge)    \
    {                                                                       \
        .line_config = line_cfg,                                            \
        .expect_clk_speed = PANEL_IO_3WIRE_SPI_CLK_MAX,                     \
        .spi_mode = scl_active_edge ? 1 : 0,                                \
        .lcd_cmd_bytes = 1,                                                 \
        .lcd_param_bytes = 1,                                               \
        .flags = {                                                          \
            .use_dc_bit = 1,                                                \
            .dc_zero_on_data = 0,                                           \
            .lsb_first = 0,                                                 \
            .cs_high_active = 0,                                            \
            .del_keep_cs_inactive = 1,                                      \
        },                                                                  \
    }

/**
 * @brief  Initialize ST77903 LCD panel with RGB interface
 *
 * @param[in]  io LCD panel IO handle
 * @param[in]  panel_dev_config LCD panel device configuration
 * @param[out] ret_panel LCD panel handle
 * @return
 *      - ESP_OK:    Success
 *      - Otherwise: Fail
 */
esp_err_t esp_lcd_new_panel_st77903_rgb(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                        esp_lcd_panel_handle_t *ret_panel);

/**
 * @brief RGB timing structure
 *
 * @note  refresh_rate = (pclk_hz * data_width) / (h_res + hsync_pulse_width + hsync_back_porch + hsync_front_porch)
 *                                              / (v_res + vsync_pulse_width + vsync_back_porch + vsync_front_porch)
 *                                              / bits_per_pixel
 *
 */
#define ST77903_RGB_320_480_PANEL_48HZ_RGB_TIMING() \
    {                                               \
        .pclk_hz = 24 * 1000 * 1000,                \
        .h_res = 320,                               \
        .v_res = 480,                               \
        .hsync_pulse_width = 3,                     \
        .hsync_back_porch = 3,                      \
        .hsync_front_porch = 6,                     \
        .vsync_pulse_width = 1,                     \
        .vsync_back_porch = 6,                      \
        .vsync_front_porch = 6,                     \
        .flags.pclk_active_neg = false,             \
    }

#ifdef __cplusplus
}
#endif
