/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file dummy_panel.c
 * @brief Dummy LCD panel and panel_io implementation for headless benchmarking
 */

#include <stdlib.h>
#include <string.h>
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io_interface.h"
#include "dummy_panel.h"

static const char *TAG = "dummy_panel";

/******************************************************************************
 * Dummy Panel IO Implementation
 ******************************************************************************/

typedef struct {
    esp_lcd_panel_io_t base;   // Base panel IO structure
    void *user_ctx;            // User context for callbacks
} dummy_panel_io_t;

static esp_err_t dummy_panel_io_del(esp_lcd_panel_io_t *io)
{
    ESP_LOGD(TAG, "Deleting dummy panel_io");
    dummy_panel_io_t *dummy_io = __containerof(io, dummy_panel_io_t, base);
    free(dummy_io);
    return ESP_OK;
}

static esp_err_t dummy_panel_io_tx_param(esp_lcd_panel_io_t *io, int lcd_cmd,
                                         const void *param, size_t param_size)
{
    // No-op: just return success
    return ESP_OK;
}

static esp_err_t dummy_panel_io_tx_color(esp_lcd_panel_io_t *io, int lcd_cmd,
                                         const void *color, size_t color_size)
{
    // No-op: just return success
    // In a real scenario, this would trigger on_color_trans_done callback
    return ESP_OK;
}

static esp_err_t dummy_panel_io_register_event_callbacks(esp_lcd_panel_io_handle_t io,
                                                         const esp_lcd_panel_io_callbacks_t *cbs,
                                                         void *user_ctx)
{
    ESP_LOGD(TAG, "Registering dummy panel_io callbacks");
    dummy_panel_io_t *dummy_io = __containerof(io, dummy_panel_io_t, base);

    if (cbs) {
        // Store callbacks if needed (for now we just acknowledge them)
        dummy_io->user_ctx = user_ctx;
    }

    return ESP_OK;
}

esp_err_t dummy_panel_io_create(esp_lcd_panel_io_handle_t *ret_io)
{
    ESP_RETURN_ON_FALSE(ret_io, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");

    dummy_panel_io_t *dummy_io = calloc(1, sizeof(dummy_panel_io_t));
    ESP_RETURN_ON_FALSE(dummy_io, ESP_ERR_NO_MEM, TAG, "No memory for dummy panel_io");

    // Initialize function pointers
    dummy_io->base.del = dummy_panel_io_del;
    dummy_io->base.tx_param = dummy_panel_io_tx_param;
    dummy_io->base.tx_color = dummy_panel_io_tx_color;
    dummy_io->base.register_event_callbacks = dummy_panel_io_register_event_callbacks;

    *ret_io = &dummy_io->base;
    ESP_LOGI(TAG, "Created dummy panel_io @%p", dummy_io);

    return ESP_OK;
}

/******************************************************************************
 * Dummy Panel Implementation
 ******************************************************************************/

typedef struct {
    esp_lcd_panel_t base;      // Base panel structure
    esp_lcd_panel_io_handle_t io;  // Panel IO handle
} dummy_panel_t;

static esp_err_t dummy_panel_del(esp_lcd_panel_t *panel)
{
    ESP_LOGD(TAG, "Deleting dummy panel");
    dummy_panel_t *dummy = __containerof(panel, dummy_panel_t, base);
    free(dummy);
    return ESP_OK;
}

static esp_err_t dummy_panel_reset(esp_lcd_panel_t *panel)
{
    // No-op: just return success
    ESP_LOGD(TAG, "Dummy panel reset (no-op)");
    return ESP_OK;
}

static esp_err_t dummy_panel_init(esp_lcd_panel_t *panel)
{
    // No-op: just return success
    ESP_LOGD(TAG, "Dummy panel init (no-op)");
    return ESP_OK;
}

static esp_err_t dummy_panel_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start,
                                         int x_end, int y_end, const void *color_data)
{
    // No-op: pretend we drew the bitmap successfully
    // This is the key function for headless operation - it does nothing with the data
    return ESP_OK;
}

static esp_err_t dummy_panel_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    // No-op: just return success
    return ESP_OK;
}

static esp_err_t dummy_panel_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    // No-op: just return success
    return ESP_OK;
}

static esp_err_t dummy_panel_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    // No-op: just return success
    return ESP_OK;
}

static esp_err_t dummy_panel_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    // No-op: just return success
    return ESP_OK;
}

static esp_err_t dummy_panel_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    // No-op: just return success
    return ESP_OK;
}

static esp_err_t dummy_panel_disp_sleep(esp_lcd_panel_t *panel, bool sleep)
{
    // No-op: just return success
    return ESP_OK;
}

esp_err_t dummy_panel_create(esp_lcd_panel_io_handle_t io, esp_lcd_panel_handle_t *ret_panel)
{
    ESP_RETURN_ON_FALSE(ret_panel, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");

    dummy_panel_t *dummy = calloc(1, sizeof(dummy_panel_t));
    ESP_RETURN_ON_FALSE(dummy, ESP_ERR_NO_MEM, TAG, "No memory for dummy panel");

    dummy->io = io;

    // Initialize all function pointers
    dummy->base.del = dummy_panel_del;
    dummy->base.reset = dummy_panel_reset;
    dummy->base.init = dummy_panel_init;
    dummy->base.draw_bitmap = dummy_panel_draw_bitmap;
    dummy->base.invert_color = dummy_panel_invert_color;
    dummy->base.mirror = dummy_panel_mirror;
    dummy->base.swap_xy = dummy_panel_swap_xy;
    dummy->base.set_gap = dummy_panel_set_gap;
    dummy->base.disp_on_off = dummy_panel_disp_on_off;
    dummy->base.disp_sleep = dummy_panel_disp_sleep;

    *ret_panel = &dummy->base;
    ESP_LOGI(TAG, "Created dummy panel @%p", dummy);

    return ESP_OK;
}
