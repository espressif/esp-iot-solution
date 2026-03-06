/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lv_adapter.h"
#include "hw_init.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Context populated by example_lvgl_init()
 *
 * All handles the caller may need after initialisation are stored here.
 * Fields that do not apply to the current hardware configuration are set
 * to NULL (e.g. touch on an encoder-only board).
 */
typedef struct {
    lv_display_t *disp;                          /*!< LVGL display handle */
    lv_indev_t *touch;                           /*!< Touch input device (NULL if not available) */
    lv_indev_t *encoder;                         /*!< Encoder input device (NULL if not available) */
    esp_lcd_panel_handle_t panel;                /*!< LCD panel handle */
    esp_lcd_panel_io_handle_t panel_io;          /*!< LCD panel IO handle */
    esp_lv_adapter_rotation_t rotation;          /*!< Applied display rotation */
    esp_lv_adapter_tear_avoid_mode_t tear_mode;  /*!< Applied tear-avoidance mode */
} example_lvgl_ctx_t;

/**
 * @brief One-call initialisation of LCD + LVGL adapter + input device
 *
 * This helper performs the complete initialisation sequence that most
 * GUI examples share.  Reading the implementation in example_lvgl_init.c
 * is recommended — it serves as a step-by-step reference:
 *
 *   1. Read display rotation from Kconfig (CONFIG_EXAMPLE_DISPLAY_ROTATION_*)
 *   2. Select tear-avoidance mode for the LCD interface; for SPI / QSPI
 *      interfaces, TE-sync is enabled automatically when the panel provides
 *      a TE GPIO (detected via hw_lcd_get_te_gpio())
 *   3. Initialise the LCD panel          — hw_lcd_init()
 *   4. Initialise the LVGL adapter       — esp_lv_adapter_init()
 *   5. Register the display              — esp_lv_adapter_register_display()
 *   6. Initialise touch or encoder       — hw_touch_init() / hw_knob_get_*()
 *   7. Start the LVGL worker task        — esp_lv_adapter_start()
 *   8. (Optional) Start FPS monitor task — controlled by Kconfig
 *      CONFIG_ESP_LVGL_ADAPTER_ENABLE_FPS_STATS
 *
 * @param[out] ctx  Pointer to context structure; filled on success
 * @return
 *      - ESP_OK on success
 *      - Relevant error code on failure (logged internally)
 */
esp_err_t example_lvgl_init(example_lvgl_ctx_t *ctx);

#ifdef __cplusplus
}
#endif
