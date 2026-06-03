/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "hal/lcd_types.h"
#include "esp_err.h"
#include "esp_lcd_panel_vendor.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LCD panel initialization commands.
 */
typedef struct {
    int cmd;                /*!< The specific LCD command */
    const void *data;       /*!< Buffer that holds the command specific data */
    size_t data_bytes;      /*!< Size of `data` in memory, in bytes */
    unsigned int delay_ms;  /*!< Delay in milliseconds after this command */
} st77926_lcd_init_cmd_t;

/**
 * @brief LCD panel vendor configuration.
 *
 * @note This structure can be used to override default initialization commands.
 * @note This structure needs to be passed to the `vendor_config` field in `esp_lcd_panel_dev_config_t`.
 */
typedef struct {
    const st77926_lcd_init_cmd_t *init_cmds; /*!< Pointer to initialization commands array.
                                              *   Set to NULL if using default commands.
                                              *   The array should be declared as `static const` and positioned outside the function.
                                              */
    uint16_t init_cmds_size;                 /*!< Number of commands in above array */
} st77926_vendor_config_t;

/**
 * @brief Create LCD panel for model ST77926 with QSPI interface
 *
 * @param[in]  io LCD panel IO handle
 * @param[in]  panel_dev_config General panel device configuration
 * @param[out] ret_panel Returned LCD panel handle
 * @return
 *      - ESP_OK: Success
 *      - Otherwise: Fail
 */
esp_err_t esp_lcd_new_panel_st77926(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                    esp_lcd_panel_handle_t *ret_panel);

/**
 * @brief LCD panel QSPI bus configuration structure
 */
#define ST77926_PANEL_BUS_QSPI_CONFIG(sclk, d0, d1, d2, d3, max_trans_sz) \
    {                                                                     \
        .sclk_io_num = sclk,                                              \
        .data0_io_num = d0,                                               \
        .data1_io_num = d1,                                               \
        .data2_io_num = d2,                                               \
        .data3_io_num = d3,                                               \
        .max_transfer_sz = max_trans_sz,                                  \
    }

/**
 * @brief LCD panel QSPI IO configuration structure
 */
#define ST77926_PANEL_IO_QSPI_CONFIG(cs, cb, cb_ctx) \
    {                                                \
        .cs_gpio_num = cs,                           \
        .dc_gpio_num = -1,                           \
        .spi_mode = 0,                               \
        .pclk_hz = 40 * 1000 * 1000,                 \
        .trans_queue_depth = 10,                     \
        .on_color_trans_done = cb,                   \
        .user_ctx = cb_ctx,                          \
        .lcd_cmd_bits = 32,                          \
        .lcd_param_bits = 8,                         \
        .flags = {                                   \
            .quad_mode = true,                       \
        },                                           \
    }

#ifdef __cplusplus
}
#endif
