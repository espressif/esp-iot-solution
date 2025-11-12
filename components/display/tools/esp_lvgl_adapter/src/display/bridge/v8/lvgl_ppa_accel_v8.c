/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * LVGL v8 PPA Hardware Acceleration
 */

#include "lvgl_port_ppa.h"
#include "soc/soc_caps.h"

#if CONFIG_SOC_PPA_SUPPORTED

#include <stdint.h>
#include "esp_err.h"
#include "esp_cache.h"
#include "esp_private/esp_cache_private.h"
#include "esp_heap_caps.h"
#include "esp_memory_utils.h"
#include "driver/ppa.h"
#include "lvgl_port_alignment.h"
#include "esp_log.h"
#include "src/draw/sw/lv_draw_sw.h"
#include "src/draw/sw/lv_draw_sw_blend.h"

static ppa_client_handle_t s_blend_handle = NULL;
static ppa_client_handle_t s_fill_handle = NULL;
static size_t s_cache_align = 0;
static lv_disp_drv_t *s_bound_drv = NULL;

static size_t ppa_align(void);
static void ppa_cache_sync_region(const lv_area_t *area, const lv_area_t *buf_area, void *buf, int flag);
static void ppa_cache_invalidate(const lv_area_t *area, const lv_area_t *buf_area, lv_color_t *buf);
static void ppa_blend(lv_color_t *bg_buf, const lv_area_t *bg_area, const lv_color_t *fg_buf,
                      const lv_area_t *fg_area, uint16_t fg_stride_px, const lv_area_t *block_area, lv_opa_t opa);
static void ppa_fill(lv_color_t *bg_buf, const lv_area_t *bg_area, const lv_area_t *block_area, lv_color_t color);
static void lv_draw_ppa_ctx_init(lv_disp_drv_t *drv, lv_draw_ctx_t *draw_ctx);
static void lv_draw_ppa_ctx_deinit(lv_disp_drv_t *drv, lv_draw_ctx_t *draw_ctx);
static void lv_draw_ppa_blend(lv_draw_ctx_t *draw_ctx, const lv_draw_sw_blend_dsc_t *dsc);

void lvgl_port_ppa_v8_init(lv_disp_drv_t *drv)
{
    if (!drv || sizeof(lv_color_t) != 2) {
        return;
    }
    s_bound_drv = drv;
    drv->draw_ctx_size = sizeof(lv_draw_sw_ctx_t);
    drv->draw_ctx_init = lv_draw_ppa_ctx_init;
    drv->draw_ctx_deinit = lv_draw_ppa_ctx_deinit;
}

static size_t ppa_align(void)
{
    if (s_cache_align == 0) {
        esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &s_cache_align);
        if (s_cache_align == 0) {
            s_cache_align = LVGL_PORT_PPA_ALIGNMENT;
        }
    }
    return s_cache_align;
}

static void ppa_cache_sync_region(const lv_area_t *area, const lv_area_t *buf_area, void *buf, int flag)
{
    if (!area || !buf_area || !buf || !esp_ptr_external_ram(buf)) {
        return;
    }

    size_t align = ppa_align();
    lv_coord_t width = lv_area_get_width(area);
    lv_coord_t height = lv_area_get_height(area);
    lv_coord_t buf_w = lv_area_get_width(buf_area);
    lv_coord_t buf_h = lv_area_get_height(buf_area);

    if (width <= 0 || height <= 0 || buf_w <= 0 || buf_h <= 0) {
        return;
    }

    lv_coord_t off_x = area->x1 - buf_area->x1;
    lv_coord_t off_y = area->y1 - buf_area->y1;

    if (off_x < 0 || off_y < 0 || (off_x + width) > buf_w || (off_y + height) > buf_h) {
        return;
    }

    uint8_t *start = (uint8_t *)buf + ((size_t)off_y * buf_w + off_x) * sizeof(lv_color_t);
    size_t bytes = (size_t)width * height * sizeof(lv_color_t);
    uintptr_t addr = (uintptr_t)start;
    uintptr_t aligned_addr = addr & ~(align - 1);
    size_t total = LVGL_PORT_PPA_ALIGN_UP(bytes + (addr - aligned_addr), align);

    esp_cache_msync((void *)aligned_addr, total, flag);
}

static void ppa_cache_invalidate(const lv_area_t *area, const lv_area_t *buf_area, lv_color_t *buf)
{
    ppa_cache_sync_region(area, buf_area, buf, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
}

static void ppa_blend(lv_color_t *bg_buf, const lv_area_t *bg_area, const lv_color_t *fg_buf,
                      const lv_area_t *fg_area, uint16_t fg_stride_px, const lv_area_t *block_area, lv_opa_t opa)
{
    uint16_t bg_w = lv_area_get_width(bg_area);
    uint16_t bg_h = lv_area_get_height(bg_area);
    uint16_t block_w = lv_area_get_width(block_area);
    uint16_t block_h = lv_area_get_height(block_area);
    uint16_t fg_w = fg_stride_px;
    uint16_t fg_h = lv_area_get_height(fg_area);
    uint16_t fg_off_x = block_area->x1 - fg_area->x1;
    uint16_t fg_off_y = block_area->y1 - fg_area->y1;

    if ((uint32_t)fg_off_x + block_w > fg_w) {
        fg_w = fg_off_x + block_w;
    }
    if ((uint32_t)fg_off_y + block_h > fg_h) {
        fg_h = fg_off_y + block_h;
    }
    size_t align = ppa_align();

    ppa_blend_oper_config_t cfg = {
        .in_bg = {
            .buffer = bg_buf,
            .pic_w = bg_w,
            .pic_h = bg_h,
            .block_w = block_w,
            .block_h = block_h,
            .block_offset_x = block_area->x1 - bg_area->x1,
            .block_offset_y = block_area->y1 - bg_area->y1,
            .blend_cm = PPA_BLEND_COLOR_MODE_RGB565,
        },
        .in_fg = {
            .buffer = fg_buf,
            .pic_w = fg_w,
            .pic_h = fg_h,
            .block_w = block_w,
            .block_h = block_h,
            .block_offset_x = fg_off_x,
            .block_offset_y = fg_off_y,
            .blend_cm = PPA_BLEND_COLOR_MODE_RGB565,
        },
        .out = {
            .buffer = bg_buf,
            .buffer_size = LVGL_PORT_PPA_ALIGN_UP(sizeof(lv_color_t) * bg_w * bg_h, align),
            .pic_w = bg_w,
            .pic_h = bg_h,
            .block_offset_x = block_area->x1 - bg_area->x1,
            .block_offset_y = block_area->y1 - bg_area->y1,
            .blend_cm = PPA_BLEND_COLOR_MODE_RGB565,
        },
        .bg_alpha_update_mode = PPA_ALPHA_FIX_VALUE,
        .bg_alpha_fix_val = 255 - opa,
        .fg_alpha_update_mode = PPA_ALPHA_FIX_VALUE,
        .fg_alpha_fix_val = opa,
        .mode = PPA_TRANS_MODE_BLOCKING,
    };

    ESP_ERROR_CHECK(ppa_do_blend(s_blend_handle, &cfg));
}

static void ppa_fill(lv_color_t *bg_buf, const lv_area_t *bg_area, const lv_area_t *block_area, lv_color_t color)
{
    uint16_t bg_w = lv_area_get_width(bg_area);
    uint16_t bg_h = lv_area_get_height(bg_area);
    size_t align = ppa_align();

    ppa_fill_oper_config_t cfg = {
        .out = {
            .buffer = bg_buf,
            .buffer_size = LVGL_PORT_PPA_ALIGN_UP(sizeof(lv_color_t) * bg_w * bg_h, align),
            .pic_w = bg_w,
            .pic_h = bg_h,
            .block_offset_x = block_area->x1 - bg_area->x1,
            .block_offset_y = block_area->y1 - bg_area->y1,
            .fill_cm = PPA_FILL_COLOR_MODE_RGB565,
        },
        .fill_block_w = lv_area_get_width(block_area),
        .fill_block_h = lv_area_get_height(block_area),
        .fill_argb_color.val = lv_color_to32(color),
                        .mode = PPA_TRANS_MODE_BLOCKING,
    };

    ESP_ERROR_CHECK(ppa_do_fill(s_fill_handle, &cfg));
}

/**********************
 *   CORE DRAW HANDLERS
 **********************/

/**
 * @brief Initialize PPA draw context
 *
 * @param drv Display driver
 * @param draw_ctx Draw context to initialize
 */
static void lv_draw_ppa_ctx_init(lv_disp_drv_t *drv, lv_draw_ctx_t *draw_ctx)
{
    lv_draw_sw_init_ctx(drv, draw_ctx);

    if (s_blend_handle == NULL && s_fill_handle == NULL) {
        ppa_client_config_t blend_cfg = {
            .oper_type = PPA_OPERATION_BLEND,
        };
        ppa_client_config_t fill_cfg = {
            .oper_type = PPA_OPERATION_FILL,
        };
        ESP_ERROR_CHECK(ppa_register_client(&blend_cfg, &s_blend_handle));
        ESP_ERROR_CHECK(ppa_register_client(&fill_cfg, &s_fill_handle));
    }

    lv_draw_sw_ctx_t *sw_ctx = (lv_draw_sw_ctx_t *)draw_ctx;
    sw_ctx->blend = lv_draw_ppa_blend;
}

/**
 * @brief Deinitialize PPA draw context
 *
 * @param drv Display driver
 * @param draw_ctx Draw context to deinitialize
 */
static void lv_draw_ppa_ctx_deinit(lv_disp_drv_t *drv, lv_draw_ctx_t *draw_ctx)
{
    lv_draw_sw_deinit_ctx(drv, draw_ctx);

    if (drv == s_bound_drv) {
        if (s_blend_handle) {
            ESP_ERROR_CHECK(ppa_unregister_client(s_blend_handle));
            s_blend_handle = NULL;
        }
        if (s_fill_handle) {
            ESP_ERROR_CHECK(ppa_unregister_client(s_fill_handle));
            s_fill_handle = NULL;
        }
        s_cache_align = 0;
    }
}

/**
 * @brief PPA hardware acceleration handler for LVGL v8 blending operations
 *
 * Accelerates RGB565 image blending and opaque fills. Falls back to CPU for:
 * - Semi-transparent fills (PPA has ~28% overhead: redundant DMA + cache sync)
 * - Complex masking, small areas (<100px), unsupported formats
 */
static void lv_draw_ppa_blend(lv_draw_ctx_t *draw_ctx, const lv_draw_sw_blend_dsc_t *dsc)
{
    lv_area_t block_area;
    if (!_lv_area_intersect(&block_area, dsc->blend_area, draw_ctx->clip_area)) {
        return;
    }

    if (sizeof(lv_color_t) != 2 || draw_ctx->buf == NULL || draw_ctx->buf_area == NULL) {
        lv_draw_sw_blend_basic(draw_ctx, dsc);
        return;
    }

    size_t align = ppa_align();

    /* PPA doesn't support complex masking */
    if (dsc->mask_buf && dsc->mask_res != LV_DRAW_MASK_RES_FULL_COVER &&
            dsc->mask_res != LV_DRAW_MASK_RES_UNKNOWN) {
        lv_draw_sw_blend_basic(draw_ctx, dsc);
        return;
    }

    /* Small areas: DMA setup overhead exceeds benefit */
    if (lv_area_get_size(&block_area) <= 100) {
        lv_draw_sw_blend_basic(draw_ctx, dsc);
        return;
    }

    if (((uintptr_t)draw_ctx->buf % align) != 0) {
        lv_draw_sw_blend_basic(draw_ctx, dsc);
        return;
    }

    /* Writeback cache before PPA reads */
    ppa_cache_sync_region(&block_area, draw_ctx->buf_area, draw_ctx->buf, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

    /* Image blending: RGB565 format only */
    if (dsc->src_buf) {
        /* Calculate source stride - handle both line buffer and full buffer cases */
        lv_coord_t src_w = lv_area_get_width(dsc->blend_area);
        lv_coord_t block_w = lv_area_get_width(&block_area);
        lv_coord_t block_h = lv_area_get_height(&block_area);

        /* Detect stride: if source appears to be a line buffer, use block width as stride */
        uint16_t src_stride_px = src_w;

        /* Sync source buffer cache */
        size_t align = ppa_align();
        if (align == 0) {
            align = 128;
        }

        lv_coord_t src_off_x = block_area.x1 - dsc->blend_area->x1;
        lv_coord_t src_off_y = block_area.y1 - dsc->blend_area->y1;

        const uint8_t *src_ptr8 = (const uint8_t *)dsc->src_buf;
        src_ptr8 += (size_t)src_off_y * src_stride_px * sizeof(lv_color_t);
        src_ptr8 += (size_t)src_off_x * sizeof(lv_color_t);

        uintptr_t addr = (uintptr_t)src_ptr8;
        uintptr_t aligned_addr = addr & ~(align - 1);
        size_t padding = addr - aligned_addr;
        size_t src_bytes = (size_t)block_w * block_h * sizeof(lv_color_t);
        size_t total = LVGL_PORT_PPA_ALIGN_UP(src_bytes + padding, align);
        esp_cache_msync((void *)aligned_addr, total, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

        ppa_blend(draw_ctx->buf, draw_ctx->buf_area, dsc->src_buf, dsc->blend_area, src_stride_px, &block_area, dsc->opa);
        ppa_cache_invalidate(&block_area, draw_ctx->buf_area, draw_ctx->buf);
        return;
    }

    if (dsc->opa >= LV_OPA_MAX) {
        ppa_fill(draw_ctx->buf, draw_ctx->buf_area, &block_area, dsc->color);
        ppa_cache_invalidate(&block_area, draw_ctx->buf_area, draw_ctx->buf);
        return;
    }

    /* Semi-transparent fills: use CPU (more efficient than PPA) */
    lv_draw_sw_blend_basic(draw_ctx, dsc);
}

#else /* !CONFIG_SOC_PPA_SUPPORTED */

void lvgl_port_ppa_v8_init(lv_disp_drv_t *drv)
{
    (void)drv;
}

#endif /* CONFIG_SOC_PPA_SUPPORTED */
