/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Display Manager - Internal API for managing LVGL displays
 */

#pragma once

/*********************
 *      INCLUDES
 *********************/
#include "esp_err.h"
#include "esp_lv_adapter_display.h"

#ifdef __cplusplus
extern "C" {
#endif

/**********************
 *   PUBLIC API
 **********************/

/**
 * @brief Register a new display with the adapter
 *
 * @param cfg Display configuration
 * @return lv_display_t* Created LVGL display handle, or NULL on failure
 */
lv_display_t *display_manager_register(const esp_lv_adapter_display_config_t *cfg);

/**
 * @brief Unregister and destroy a single display
 *
 * @param disp Display handle to unregister
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid display handle
 *      - ESP_ERR_INVALID_STATE: Adapter not initialized
 *      - ESP_ERR_NOT_FOUND: Display not found in registered list
 */
esp_err_t display_manager_unregister(lv_display_t *disp);

/**
 * @brief Set area rounding callback for display
 *
 * @param disp Display handle
 * @param rounder_cb Callback function (NULL to disable)
 * @param user_data User data passed to callback
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid display handle
 *      - ESP_ERR_NOT_FOUND: Display not found
 */
esp_err_t display_manager_set_area_rounder_cb(lv_display_t *disp,
                                              void (*rounder_cb)(lv_area_t *, void *),
                                              void *user_data);

/**
 * @brief Clear and destroy all registered displays
 */
void display_manager_clear(void);

/**
 * @brief Enable or disable dummy draw mode for a display
 *
 * @param disp Display handle
 * @param enable true to enable dummy draw, false to disable
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if display not found
 */
esp_err_t display_manager_set_dummy_draw(lv_display_t *disp, bool enable);

/**
 * @brief Register dummy draw callbacks for a display
 */
esp_err_t display_manager_set_dummy_draw_callbacks(lv_display_t *disp,
                                                   const esp_lv_adapter_dummy_draw_callbacks_t *cbs,
                                                   void *user_ctx);

esp_err_t display_manager_dummy_draw_blit(lv_display_t *disp,
                                          int x_start,
                                          int y_start,
                                          int x_end,
                                          int y_end,
                                          const void *frame_buffer,
                                          bool wait);

/**
 * @brief Request a full refresh on the specified display
 *
 * Marks the entire active screen as dirty so that LVGL will redraw on the next cycle.
 */
void display_manager_request_full_refresh(lv_display_t *disp);

/**
 * @brief Get the current dummy draw state for a display
 *
 * @param disp Display handle
 * @param enabled Output flag indicating whether dummy draw is active
 * @return ESP_OK on success, error if display not found or arguments invalid
 */
esp_err_t display_manager_get_dummy_draw_state(lv_display_t *disp, bool *enabled);

/**
 * @brief Notify LVGL that a flush has completed
 *
 * This wrapper ensures internal bookkeeping (e.g., FPS statistics) is updated
 * before signalling LVGL. Call it instead of lv_disp(ay)_flush_ready().
 */
#if LVGL_VERSION_MAJOR >= 9
void display_manager_flush_ready(lv_display_t *disp);
#else
void display_manager_flush_ready(lv_disp_drv_t *drv);
#endif

/**
 * @brief Calculate number of panel frame buffers required
 *
 * @param tear_avoid_mode Tearing effect avoidance mode
 * @param rotation Display rotation angle
 * @return uint8_t Required frame buffer count (1, 2, or 3)
 */
uint8_t display_manager_required_frame_buffer_count(esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode,
                                                    esp_lv_adapter_rotation_t rotation);

/**
 * @brief Get display node by LVGL display handle
 *
 * @param disp Display handle (NULL for first registered display)
 * @return esp_lv_adapter_display_node_t* Display node, or NULL if not found
 */
struct esp_lv_adapter_display_node *display_manager_get_node(lv_display_t *disp);

/**
 * @brief Wait for any pending flush operations to complete
 *
 * This function blocks until all ongoing flush operations for the specified
 * display have finished. It is used to ensure safe LCD panel detachment.
 *
 * @param disp Display handle
 * @param timeout_ms Timeout in milliseconds (-1 for infinite)
 * @return
 *      - ESP_OK: All flushes completed
 *      - ESP_ERR_TIMEOUT: Timeout waiting for flush completion
 *      - ESP_ERR_INVALID_ARG: Invalid display handle
 */
esp_err_t display_manager_wait_flush_done(lv_display_t *disp, int32_t timeout_ms);

/**
 * @brief Refetch framebuffers from LCD panel and update all references
 *
 * This function re-obtains framebuffer pointers from the LCD panel hardware
 * (for RGB/MIPI DSI interfaces) and updates all internal references in the
 * display node and bridge.
 *
 * @param disp Display handle
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid display handle
 *      - ESP_ERR_NOT_FOUND: Display not found
 *      - ESP_FAIL: Failed to refetch framebuffers
 */
esp_err_t display_manager_refetch_framebuffers(lv_display_t *disp);

/**
 * @brief Rebind LVGL draw buffers after framebuffers are refreshed
 *
 * Updates LVGL draw buffer references to point to the latest panel or
 * software-allocated buffers. This is required when displays are recovered
 * from sleep and new panel framebuffers are obtained.
 *
 * @param disp Display handle
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid display handle
 *      - ESP_ERR_NOT_FOUND: Display node not found
 *      - ESP_ERR_INVALID_STATE: Draw buffer state invalid
 */
esp_err_t display_manager_rebind_draw_buffers(lv_display_t *disp);

#ifdef __cplusplus
}
#endif
