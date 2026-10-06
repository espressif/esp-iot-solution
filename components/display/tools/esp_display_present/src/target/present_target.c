/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_display_present_target.h"
#include "esp_heap_caps.h"
#ifdef ESP_PLATFORM
#include "esp_idf_version.h"
#endif
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"

#include "esp_display_present_geometry.h"
#include "present_target_internal.h"

#if SOC_MIPI_DSI_SUPPORTED || __has_include("esp_lcd_mipi_dsi.h")
#include "esp_lcd_mipi_dsi.h"
#define ESP_DISPLAY_PRESENT_HAS_DPI 1
#endif

#if SOC_LCDCAM_RGB_LCD_SUPPORTED || __has_include("esp_lcd_panel_rgb.h")
#include "esp_lcd_panel_rgb.h"
#define ESP_DISPLAY_PRESENT_HAS_RGB 1
#endif

static const char *TAG = "present_target";

struct esp_display_present_target {
    esp_display_present_target_info_t info;
    esp_display_present_target_callbacks_t callbacks;
    void *callback_user_ctx;
    portMUX_TYPE callback_lock;
    volatile uint32_t active_isr_count;
    bool accepting_callbacks;
    bool callbacks_registered;
};

static const char *present_mode_preset_name(esp_display_present_mode_t mode)
{
    switch (mode) {
    case ESP_DISPLAY_PRESENT_MODE_AUTO:
        return "AUTO";
    case ESP_DISPLAY_PRESENT_MODE_DOUBLE_DIRECT:
        return "DOUBLE_DIRECT";
    case ESP_DISPLAY_PRESENT_MODE_DOUBLE_FULL:
        return "DOUBLE_FULL";
    case ESP_DISPLAY_PRESENT_MODE_DOUBLE_PARTIAL:
        return "DOUBLE_PARTIAL";
    case ESP_DISPLAY_PRESENT_MODE_TRIPLE_FULL:
        return "TRIPLE_FULL";
    case ESP_DISPLAY_PRESENT_MODE_TRIPLE_PARTIAL:
        return "TRIPLE_PARTIAL";
    case ESP_DISPLAY_PRESENT_MODE_TE_SYNC:
        return "TE_SYNC";
    case ESP_DISPLAY_PRESENT_MODE_NONE:
        return "NONE";
    default:
        return "UNKNOWN";
    }
}

static const char *present_panel_interface_name(
    esp_display_present_panel_interface_t panel_interface)
{
    switch (panel_interface) {
    case ESP_DISPLAY_PRESENT_PANEL_IF_RGB:
        return "RGB";
    case ESP_DISPLAY_PRESENT_PANEL_IF_MIPI_DSI:
        return "MIPI_DSI";
    case ESP_DISPLAY_PRESENT_PANEL_IF_OTHER:
        return "IO/GRAM";
    default:
        return "UNKNOWN";
    }
}

static const char *
pipeline_commit_name(esp_display_present_frame_commit_policy_t commit)
{
    switch (commit) {
    case ESP_DISPLAY_PRESENT_FRAME_COMMIT_WAIT_INFLIGHT:
        return "wait_inflight";
    case ESP_DISPLAY_PRESENT_FRAME_COMMIT_TE_PUSH:
        return "te_push";
    case ESP_DISPLAY_PRESENT_FRAME_COMMIT_SWITCH:
    default:
        return "switch";
    }
}

static bool IRAM_ATTR target_isr_enter(esp_display_present_target_t *target)
{
    if (target == NULL) {
        return false;
    }
    portENTER_CRITICAL_ISR(&target->callback_lock);
    bool accepted = target->accepting_callbacks;
    if (accepted) {
        ++target->active_isr_count;
    }
    portEXIT_CRITICAL_ISR(&target->callback_lock);
    return accepted;
}

static void IRAM_ATTR target_isr_leave(esp_display_present_target_t *target)
{
    portENTER_CRITICAL_ISR(&target->callback_lock);
    if (target->active_isr_count != 0) {
        --target->active_isr_count;
    }
    portEXIT_CRITICAL_ISR(&target->callback_lock);
}

bool esp_display_present_target_notify_transfer_done_from_isr(
    esp_display_present_target_t *target)
{
    if (!target_isr_enter(target)) {
        return false;
    }
    bool need_yield =
        target->callbacks.on_transfer_done != NULL &&
        target->callbacks.on_transfer_done(target->callback_user_ctx);
    target_isr_leave(target);
    return need_yield;
}

bool esp_display_present_target_notify_frame_done_from_isr(
    esp_display_present_target_t *target)
{
    if (!target_isr_enter(target)) {
        return false;
    }
    bool need_yield = target->callbacks.on_frame_done != NULL &&
                      target->callbacks.on_frame_done(target->callback_user_ctx);
    target_isr_leave(target);
    return need_yield;
}

#ifdef ESP_DISPLAY_PRESENT_HAS_DPI
static bool IRAM_ATTR
target_dpi_color_done(esp_lcd_panel_handle_t panel,
                      esp_lcd_dpi_panel_event_data_t *event, void *user_ctx)
{
    (void)panel;
    (void)event;
    return esp_display_present_target_notify_transfer_done_from_isr(user_ctx);
}

static bool IRAM_ATTR
target_dpi_frame_done(esp_lcd_panel_handle_t panel,
                      esp_lcd_dpi_panel_event_data_t *event, void *user_ctx)
{
    (void)panel;
    (void)event;
    return esp_display_present_target_notify_frame_done_from_isr(user_ctx);
}
#endif

#ifdef ESP_DISPLAY_PRESENT_HAS_RGB
static bool IRAM_ATTR target_rgb_color_done(
    esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *event,
    void *user_ctx)
{
    (void)panel;
    (void)event;
    return esp_display_present_target_notify_transfer_done_from_isr(user_ctx);
}

static bool IRAM_ATTR target_rgb_frame_done(
    esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *event,
    void *user_ctx)
{
    (void)panel;
    (void)event;
    return esp_display_present_target_notify_frame_done_from_isr(user_ctx);
}
#endif

static bool IRAM_ATTR target_io_color_done(esp_lcd_panel_io_handle_t io,
                                           esp_lcd_panel_io_event_data_t *event,
                                           void *user_ctx)
{
    (void)io;
    (void)event;
    return esp_display_present_target_notify_transfer_done_from_isr(user_ctx);
}

static esp_err_t
target_register_hardware_callbacks(esp_display_present_target_t *target)
{
    switch (target->info.hw.panel_interface) {
    case ESP_DISPLAY_PRESENT_PANEL_IF_MIPI_DSI:
#ifdef ESP_DISPLAY_PRESENT_HAS_DPI
    {
        esp_lcd_dpi_panel_event_callbacks_t callbacks = {
            .on_color_trans_done = target_dpi_color_done,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 1, 0)
            .on_frame_buf_complete = target_dpi_frame_done,
#else
            .on_refresh_done = target_dpi_frame_done,
#endif
        };
        return esp_lcd_dpi_panel_register_event_callbacks(target->info.hw.panel,
                                                          &callbacks, target);
    }
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
    case ESP_DISPLAY_PRESENT_PANEL_IF_RGB:
#ifdef ESP_DISPLAY_PRESENT_HAS_RGB
    {
        esp_lcd_rgb_panel_event_callbacks_t callbacks = {
            .on_color_trans_done = target_rgb_color_done,
            .on_frame_buf_complete = target_rgb_frame_done,
        };
        return esp_lcd_rgb_panel_register_event_callbacks(target->info.hw.panel,
                                                          &callbacks, target);
    }
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
    case ESP_DISPLAY_PRESENT_PANEL_IF_OTHER:
    default: {
        esp_lcd_panel_io_callbacks_t callbacks = {
            .on_color_trans_done = target_io_color_done,
        };
        return esp_lcd_panel_io_register_event_callbacks(target->info.hw.io,
                                                         &callbacks, target);
    }
    }
}

static esp_err_t
target_unregister_hardware_callbacks(esp_display_present_target_t *target)
{
    switch (target->info.hw.panel_interface) {
    case ESP_DISPLAY_PRESENT_PANEL_IF_MIPI_DSI:
#ifdef ESP_DISPLAY_PRESENT_HAS_DPI
    {
        const esp_lcd_dpi_panel_event_callbacks_t callbacks = {0};
        return esp_lcd_dpi_panel_register_event_callbacks(target->info.hw.panel,
                                                          &callbacks, NULL);
    }
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
    case ESP_DISPLAY_PRESENT_PANEL_IF_RGB:
#ifdef ESP_DISPLAY_PRESENT_HAS_RGB
    {
        const esp_lcd_rgb_panel_event_callbacks_t callbacks = {0};
        return esp_lcd_rgb_panel_register_event_callbacks(target->info.hw.panel,
                                                          &callbacks, NULL);
    }
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
    case ESP_DISPLAY_PRESENT_PANEL_IF_OTHER:
    default: {
        const esp_lcd_panel_io_callbacks_t callbacks = {0};
        return esp_lcd_panel_io_register_event_callbacks(target->info.hw.io,
                                                         &callbacks, NULL);
    }
    }
}

esp_err_t esp_display_present_target_create(
    const esp_display_present_target_config_t *config,
    esp_display_present_pixel_format_t pixel_format, uint16_t width,
    uint16_t height, esp_display_present_target_t **out_target)
{
    if (out_target != NULL) {
        *out_target = NULL;
    }
    ESP_RETURN_ON_FALSE(out_target, ESP_ERR_INVALID_ARG, TAG,
                        "invalid target output");

    esp_display_present_target_info_t info;
    esp_err_t ret =
        present_target_resolve_info(config, pixel_format, width, height, &info);
    ESP_RETURN_ON_ERROR(ret, TAG, "invalid target config");

    esp_display_present_target_t *target = heap_caps_calloc(
                                               1, sizeof(*target), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(target, ESP_ERR_NO_MEM, TAG, "no target memory");
    target->callback_lock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
    target->info = info;
    ESP_LOGI(TAG,
             "target: mode=%s panel=%s gram=%u rotation=%d"
             " te=%u fb=%u drawbuf=%ux%u",
             present_mode_preset_name(info.fb.profile.mode),
             present_panel_interface_name(info.hw.panel_interface),
             (unsigned)info.hw.panel_gram, (int)info.hw.rotation,
             (unsigned)esp_display_present_te_sync_is_enabled(&info.hw.te_sync),
             (unsigned)info.fb.frame_buffer_count,
             (unsigned)info.drawbuf.lines,
             (unsigned)info.drawbuf.buffers);
    ESP_LOGI(TAG, "pipeline: commit=%s slot=%ux%u",
             pipeline_commit_name(info.pipeline.commit),
             (unsigned)info.pipeline.slot_lines,
             (unsigned)info.pipeline.slot_count);
    *out_target = target;
    return ESP_OK;
}

esp_err_t esp_display_present_target_set_callbacks(
    esp_display_present_target_t *target,
    const esp_display_present_target_callbacks_t *callbacks, void *user_ctx)
{
    if (target == NULL || callbacks == NULL ||
            callbacks->on_transfer_done == NULL ||
            (target->info.hw.panel_interface != ESP_DISPLAY_PRESENT_PANEL_IF_OTHER &&
             callbacks->on_frame_done == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (target->callbacks_registered) {
        return ESP_ERR_INVALID_STATE;
    }
    target->callbacks = *callbacks;
    target->callback_user_ctx = user_ctx;
    portENTER_CRITICAL(&target->callback_lock);
    target->accepting_callbacks = true;
    portEXIT_CRITICAL(&target->callback_lock);
    esp_err_t ret = target_register_hardware_callbacks(target);
    if (ret != ESP_OK) {
        portENTER_CRITICAL(&target->callback_lock);
        target->accepting_callbacks = false;
        portEXIT_CRITICAL(&target->callback_lock);
        memset(&target->callbacks, 0, sizeof(target->callbacks));
        target->callback_user_ctx = NULL;
        return ret;
    }
    target->callbacks_registered = true;
    return ESP_OK;
}

esp_err_t esp_display_present_target_clear_callbacks(
    esp_display_present_target_t *target)
{
    if (target == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!target->callbacks_registered) {
        return ESP_OK;
    }
    portENTER_CRITICAL(&target->callback_lock);
    target->accepting_callbacks = false;
    portEXIT_CRITICAL(&target->callback_lock);
    esp_err_t ret = target_unregister_hardware_callbacks(target);
    if (ret != ESP_OK) {
        return ret;
    }
    while (target->active_isr_count != 0) {
        taskYIELD();
    }
    target->callbacks_registered = false;
    memset(&target->callbacks, 0, sizeof(target->callbacks));
    target->callback_user_ctx = NULL;
    return ESP_OK;
}

const esp_display_present_target_info_t *esp_display_present_target_get_info(
    const esp_display_present_target_t *target)
{
    return target != NULL ? &target->info : NULL;
}

esp_err_t
esp_display_present_target_map_physical_point_to_logical(const esp_display_present_target_t *target,
                                                         int32_t *x, int32_t *y)
{
    if (target == NULL || x == NULL || y == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_display_present_point_t physical = {.x = *x, .y = *y};
    esp_display_present_point_t logical;
    esp_err_t ret = esp_display_present_geometry_map_physical_point_to_logical(
                        target->info.hw.rotation,
    (esp_display_present_size_t) {
        target->info.hw.width,
               target->info.hw.height
    },
    &physical, &logical);
    if (ret == ESP_OK) {
        *x = logical.x;
        *y = logical.y;
    }
    return ret;
}

esp_err_t
esp_display_present_target_map_logical_area_to_physical(const esp_display_present_target_t *target,
                                                        const esp_display_present_area_t *logical,
                                                        esp_display_present_area_t *out_physical)
{
    return target != NULL ? esp_display_present_geometry_map_logical_area_to_physical(
               target->info.hw.rotation,
    (esp_display_present_size_t) {
        .width = target->info.hw.width,
        .height = target->info.hw.height,
    },
    logical, out_physical)
        : ESP_ERR_INVALID_ARG;
}

esp_err_t
esp_display_present_target_delete(esp_display_present_target_t *target)
{
    if (target == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (target->callbacks_registered || target->active_isr_count != 0) {
        return ESP_ERR_INVALID_STATE;
    }
    free(target);
    return ESP_OK;
}
