/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @file
 * @brief ESP LCD & Touch: AXS15231B
 */

#pragma once

#include "hal/spi_ll.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_panel_vendor.h"

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
} axs15231b_lcd_init_cmd_t;

/**
 * @brief LCD panel vendor configuration.
 *
 * @note  This structure needs to be passed to the `vendor_config` field in `esp_lcd_panel_dev_config_t`.
 *
 */
typedef struct {
    const axs15231b_lcd_init_cmd_t *init_cmds;  /*!< Pointer to initialization commands array. Set to NULL if using default commands.
                                                 *   The array should be declared as `static const` and positioned outside the function.
                                                 *   Please refer to `vendor_specific_init_default` in source file.
                                                 */
    uint16_t init_cmds_size;                    /*<! Number of commands in above array */
    struct {
        unsigned int use_qspi_interface: 1;     /*<! Set to 1 if use QSPI interface, default is SPI interface */
    } flags;
} axs15231b_vendor_config_t;

/**
 * @brief Create LCD panel for model AXS15231B
 *
 * @note  Vendor specific initialization can be different between manufacturers, should consult the LCD supplier for initialization sequence code.
 *
 * @param[in] io LCD panel IO handle
 * @param[in] panel_dev_config general panel device configuration
 * @param[out] ret_panel Returned LCD panel handle
 * @return
 *          - ESP_ERR_INVALID_ARG   if parameter is invalid
 *          - ESP_ERR_NO_MEM        if out of memory
 *          - ESP_OK                on success
 */
esp_err_t esp_lcd_new_panel_axs15231b(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel);

/**
 * @brief LCD panel bus configuration structure
 *
 */
#define AXS15231B_PANEL_BUS_I80_CONFIG(dc, wr, clk, d0, d1, d2, d3, d4, d5, d6, d7, b_w, max_trans_sz) \
    {                                                          \
        .dc_gpio_num = dc,                                     \
        .wr_gpio_num = wr,                                     \
        .clk_src = clk,                                        \
        .data_gpio_nums = {                                    \
            d0,                                                \
            d1,                                                \
            d2,                                                \
            d3,                                                \
            d4,                                                \
            d5,                                                \
            d6,                                                \
            d7,                                                \
        },                                                     \
        .bus_width = b_w,                                      \
        .max_transfer_bytes = max_trans_sz,                    \
    }

#define AXS15231B_PANEL_BUS_SPI_CONFIG(sclk, mosi, max_trans_sz)  \
    {                                                           \
        .sclk_io_num = sclk,                                    \
        .mosi_io_num = mosi,                                    \
        .miso_io_num = -1,                                      \
        .quadhd_io_num = -1,                                    \
        .quadwp_io_num = -1,                                    \
        .max_transfer_sz = max_trans_sz,                        \
    }
#define AXS15231B_PANEL_BUS_QSPI_CONFIG(sclk, d0, d1, d2, d3, max_trans_sz) \
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
#define AXS15231B_PANEL_IO_I80_CONFIG(cs, dc, cb, cb_ctx)       \
    {                                                           \
        .cs_gpio_num = cs,                                      \
        .pclk_hz = 20 * 1000 * 1000,                            \
        .on_color_trans_done = cb,                              \
        .trans_queue_depth = 10,                                \
        .user_ctx = cb_ctx,                                     \
        .dc_levels = {                                          \
            .dc_idle_level = 0,                                 \
            .dc_cmd_level = 0,                                  \
            .dc_dummy_level = 0,                                \
            .dc_data_level = 1,                                 \
        },                                                      \
        .lcd_cmd_bits = 8,                                      \
        .lcd_param_bits = 8,                                    \
    }

#define AXS15231B_PANEL_IO_SPI_CONFIG(cs, dc, cb, cb_ctx)       \
    {                                                           \
        .cs_gpio_num = cs,                                      \
        .dc_gpio_num = dc,                                      \
        .spi_mode = 3,                                          \
        .pclk_hz = 40 * 1000 * 1000,                            \
        .trans_queue_depth = 10,                                \
        .on_color_trans_done = cb,                              \
        .user_ctx = cb_ctx,                                     \
        .lcd_cmd_bits = 8,                                      \
        .lcd_param_bits = 8,                                    \
    }

#define AXS15231B_PANEL_IO_QSPI_CONFIG(cs, cb, cb_ctx)          \
    {                                                           \
        .cs_gpio_num = cs,                                      \
        .dc_gpio_num = -1,                                      \
        .spi_mode = 3,                                          \
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

/**
 * @brief Create a new AXS15231B1B touch driver
 *
 * @note  The I2C communication should be initialized before use this function.
 *
 * @param io LCD panel IO handle, it should be created by `esp_lcd_new_panel_io_i2c()`
 * @param config Touch panel configuration
 * @param tp Touch panel handle
 * @return
 *      - ESP_OK: on success
 */
esp_err_t esp_lcd_touch_new_i2c_axs15231b(const esp_lcd_panel_io_handle_t io, const esp_lcd_touch_config_t *config, esp_lcd_touch_handle_t *tp);

/**
 * @brief I2C address of the AXS15231B controller
 *
 */
#define ESP_LCD_TOUCH_IO_I2C_AXS15231B_ADDRESS    (0x3B)

/**
 * @brief Touch IO configuration structure
 *
 */

#define ESP_LCD_TOUCH_IO_I2C_AXS15231B_CONFIG()             \
    {                                                       \
        .dev_addr = ESP_LCD_TOUCH_IO_I2C_AXS15231B_ADDRESS, \
        .control_phase_bytes = 1,                           \
        .dc_bit_offset = 0,                                 \
        .lcd_cmd_bits = 8,                                  \
        .flags =                                            \
        {                                                   \
            .disable_control_phase = 1,                     \
        }                                                   \
    }

#define ESP_LCD_TOUCH_IO_I2C_AXS15231B_CONFIG_EX(scl_speed_hz)     \
    {                                                           \
        .dev_addr = ESP_LCD_TOUCH_IO_I2C_AXS15231B_ADDRESS,     \
        .control_phase_bytes = 1,                               \
        .dc_bit_offset = 0,                                     \
        .lcd_cmd_bits = 8,                                      \
        .flags =                                                \
        {                                                       \
            .disable_control_phase = 1,                         \
        },                                                      \
        .scl_speed_hz = scl_speed_hz,                         \
    }

#ifdef __cplusplus
}
#endif
