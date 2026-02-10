/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Display Bridge Interface - Abstract interface for LVGL version-specific implementations
 */

#pragma once

/*********************
 *      INCLUDES
 *********************/
#include "esp_err.h"
#include "lvgl.h"
#include "adapter_internal.h"

/*********************
 *      TYPEDEFS
 *********************/

/**
 * @brief Display bridge abstract interface
 *
 * This structure defines the interface that both v8 and v9 bridge implementations must provide.
 */
typedef struct esp_lv_adapter_display_bridge esp_lv_adapter_display_bridge_t;

struct esp_lv_adapter_display_bridge {
    /**
     * @brief Flush callback - transfer rendered buffer to display
     *
     * @param bridge Bridge instance
     * @param disp_ref LVGL display reference (lv_disp_drv_t* for v8, lv_display_t* for v9)
     * @param area Area being flushed
     * @param color_map Rendered pixel data
     */
    void (*flush)(esp_lv_adapter_display_bridge_t *bridge,
                  void *disp_ref,
                  const lv_area_t *area,
                  uint8_t *color_map);

    /**
     * @brief Destroy bridge and free resources
     *
     * @param bridge Bridge instance to destroy
     */
    void (*destroy)(esp_lv_adapter_display_bridge_t *bridge);

    /**
     * @brief Enable or disable dummy draw mode
     *
     * @param bridge Bridge instance
     * @param enable true to enable dummy draw, false to disable
     */
    void (*set_dummy_draw)(esp_lv_adapter_display_bridge_t *bridge, bool enable);

    /**
     * @brief Update dummy draw callbacks
     *
     * @param bridge Bridge instance
     * @param cbs    Callback collection (NULL to clear)
     * @param ctx    User context
     */
    void (*set_dummy_draw_callbacks)(esp_lv_adapter_display_bridge_t *bridge,
                                     const esp_lv_adapter_dummy_draw_callbacks_t *cbs,
                                     void *ctx);

    esp_err_t (*dummy_draw_blit)(esp_lv_adapter_display_bridge_t *bridge,
                                 int x_start,
                                 int y_start,
                                 int x_end,
                                 int y_end,
                                 const void *frame_buffer,
                                 bool wait);

    /**
     * @brief Update panel handle and configuration for rebind
     *
     * @param bridge Bridge instance
     * @param cfg Updated runtime configuration with new panel handle
     * @return
     *      - ESP_OK: Success
     *      - ESP_FAIL: Update failed
     */
    esp_err_t (*update_panel)(esp_lv_adapter_display_bridge_t *bridge,
                              const esp_lv_adapter_display_runtime_config_t *cfg);

    /**
     * @brief Set area rounding callback
     *
     * @param bridge Bridge instance
     * @param rounder_cb Callback function (NULL to disable)
     * @param user_data User data passed to callback
     */
    void (*set_area_rounder)(esp_lv_adapter_display_bridge_t *bridge,
                             void (*rounder_cb)(lv_area_t *, void *),
                             void *user_data);
};

/**********************
 *   FACTORY FUNCTIONS
 **********************/

/**
 * @brief Create LVGL v8 display bridge
 *
 * @param cfg Runtime configuration
 * @return esp_lv_adapter_display_bridge_t* Created bridge instance, or NULL on failure
 */
esp_lv_adapter_display_bridge_t *esp_lv_adapter_display_bridge_v8_create(const esp_lv_adapter_display_runtime_config_t *cfg);

/**
 * @brief Create LVGL v9 display bridge
 *
 * @param cfg Runtime configuration
 * @return esp_lv_adapter_display_bridge_t* Created bridge instance, or NULL on failure
 */
esp_lv_adapter_display_bridge_t *esp_lv_adapter_display_bridge_v9_create(const esp_lv_adapter_display_runtime_config_t *cfg);
