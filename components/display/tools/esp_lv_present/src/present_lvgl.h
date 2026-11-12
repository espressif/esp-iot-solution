/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "core/present_engine.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_lv_present_native esp_lv_present_native_t;

/**
 * @brief LVGL 9 PARTIAL/FULL/DIRECT binding, selected from backend caps.
 * draw_rows is used only by PARTIAL; FULL/DIRECT allocate an entire LVGL frame. Caller initializes LVGL, provides tick and pumps
 * LVGL on the presenter's owner task. Caller serializes LVGL access. Exclusive
 * presenter use until binding is destroyed. No global singleton or tick changes.
 * Uses a separately allocated LVGL buffer; DMA never reads this buffer.
 * Binding reserves display user_data, flush callback and draw buffers: callers
 * may create UI objects but must not replace those fields or delete the display
 * directly. RGB888 is normalized from LVGL BGR storage into PV2 R,G,B bytes.
 *
 * @param presenter Presenter owned by the calling task.
 * @param draw_rows LVGL source buffer height for PARTIAL mode.
 * @param out out.
 * @return ESP_OK on success, or an error as specified by the lifecycle contract above.
 */
esp_err_t esp_lv_present_native_create(esp_display_present_engine_t *presenter, uint32_t draw_rows, esp_lv_present_native_t **out);

/**
 * @brief Opt-in zero-copy: FULL or two-physical-buffer DIRECT, RGB565/native
 * with LVGL-compatible stride only. DIRECT acquires before LVGL's historical
 * sync; the final flush submits the complete synchronized image once.
 * The next synchronization waits for actual scanout retirement.
 * Renderer leases a backend framebuffer before drawing, and detaches it before
 * refresh-ready. Keeps a private fallback buffer for safe failed admission.
 * Other layouts, including rotated DIRECT conversion canvases, use the copy
 * binding above. LVGL versions before 9.5 return ESP_ERR_NOT_SUPPORTED.
 *
 * @param presenter Presenter owned by the calling task.
 * @param out out.
 * @return ESP_OK on success, or an error as specified by the lifecycle contract above.
 */
esp_err_t esp_lv_present_native_create_zero_copy(esp_display_present_engine_t *presenter, esp_lv_present_native_t **out);
/* PARTIAL RGB565: acquire each presenter tile before LVGL renders it.
 * An independent fallback buffer is reserved for failed admission: LVGL 9.5
 * does not abort rendering when its RENDER_START event reports failure.
 * LVGL versions before 9.5 return ESP_ERR_NOT_SUPPORTED. */
esp_err_t esp_lv_present_native_create_pooled(esp_display_present_engine_t *presenter, uint32_t rows, esp_lv_present_native_t **out);
lv_display_t *esp_lv_present_native_get_display(esp_lv_present_native_t *binding);

/**
 * @brief Sticky first binding/presenter failure. A failure pauses the refresh timer;
 * no automatic retry of partly transmitted frames. Recreate after recovery.
 *
 * @param binding LVGL binding owned by the calling task.
 * @return ESP_OK on success, or an error as specified by the lifecycle contract above.
 */
esp_err_t esp_lv_present_native_get_error(esp_lv_present_native_t *binding);

/**
 * @brief Pause refresh and cancel any open frame; timeout retains the binding for
 * retry. Does not stop/destroy presenter or drain committed frames (they have
 * backend-owned pixel storage). Call on owner task, outside any LVGL callback.
 *
 * @param binding LVGL binding owned by the calling task.
 * @return ESP_OK on success, or an error as specified by the lifecycle contract above.
 */
esp_err_t esp_lv_present_native_destroy(esp_lv_present_native_t *binding);

/* Owner task, outside LVGL callbacks. Allocates replacement draw storage before
 * draining/changing the presenter. Timeout leaves old binding usable. Keeps the
 * LVGL display and object tree, changes logical resolution, invalidates all.
 * No panel MADCTL change and no recovery from an existing sticky fault. */
esp_err_t esp_lv_present_native_set_rotation(esp_lv_present_native_t *, esp_display_present_engine_rotation_t, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
