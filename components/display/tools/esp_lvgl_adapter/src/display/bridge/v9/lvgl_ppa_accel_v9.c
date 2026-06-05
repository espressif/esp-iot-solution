/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * LVGL v9 PPA Hardware Acceleration
 */

#include "lvgl_port_ppa.h"
#include "soc/soc_caps.h"

#if CONFIG_SOC_PPA_SUPPORTED

#include "lvgl_port_alignment.h"
#include "esp_log.h"
#include "lvgl.h"
#include "lvgl_private.h"
#include "src/draw/sw/blend/lv_draw_sw_blend.h"
#include "src/draw/sw/blend/lv_draw_sw_blend_private.h"
#include "src/draw/sw/blend/lv_draw_sw_blend_to_rgb565.h"
#include "src/draw/sw/blend/lv_draw_sw_blend_to_rgb888.h"
#include "src/draw/lv_draw.h"
#include "src/draw/lv_draw_buf.h"
#include "driver/ppa.h"
#include "esp_cache.h"
#include "esp_private/esp_cache_private.h"
#include "esp_memory_utils.h"
#include "common/display_bridge_common.h"
#include "stdlib/lv_mem.h"
#include "misc/lv_color.h"

static ppa_client_handle_t s_blend_handle = NULL;
static ppa_client_handle_t s_fill_handle = NULL;
static bool s_handler_registered = false;

static void lv_draw_ppa_v9_handler(lv_draw_task_t *t, const lv_draw_sw_blend_dsc_t *dsc);
static void lv_draw_ppa_v9_handler_rgb888(lv_draw_task_t *t, const lv_draw_sw_blend_dsc_t *dsc);

static lv_draw_sw_custom_blend_handler_t s_custom_handler = {
    .dest_cf = LV_COLOR_FORMAT_RGB565,
    .handler = lv_draw_ppa_v9_handler,
};
static lv_draw_sw_custom_blend_handler_t s_custom_handler_rgb888 = {
    .dest_cf = LV_COLOR_FORMAT_RGB888,
    .handler = lv_draw_ppa_v9_handler_rgb888,
};

static void lvgl_port_ppa_v9_register_handler(lv_color_format_t cf);
static size_t ppa_get_cache_line_size(const void *addr);
static size_t ppa_get_aligned_buffer_size(const void *addr, size_t size);
static bool ppa_buffer_cache_aligned(const void *addr);
static void ppa_cache_msync_addr(const void *addr, size_t size, int flag);
static void ppa_cache_sync_region(const lv_area_t *area, const lv_area_t *buf_area, void *buf, uint32_t px_size, int flag);
static void ppa_cache_invalidate(const lv_area_t *area, const lv_area_t *buf_area, void *buf, uint32_t px_size);
static void ppa_blend(lv_color_t *bg_buf, const lv_area_t *bg_area, const lv_color_t *fg_buf,
                      const lv_area_t *fg_area, uint16_t fg_stride_px, const lv_area_t *block_area, lv_opa_t opa);
static void ppa_fill(lv_color_t *bg_buf, const lv_area_t *bg_area, const lv_area_t *block_area, lv_color_t color);
static void lv_draw_ppa_v9_sw_fallback(lv_draw_task_t *t, const lv_draw_sw_blend_dsc_t *dsc);

void lvgl_port_ppa_v9_init(lv_display_t *display)
{
    if (!display) {
        return;
    }

    lv_color_format_t cf = lv_display_get_color_format(display);
    if (cf != LV_COLOR_FORMAT_RGB565 && cf != LV_COLOR_FORMAT_RGB888) {
        return;
    }

    if (s_blend_handle == NULL && s_fill_handle == NULL) {
        ppa_client_config_t blend_cfg = {.oper_type = PPA_OPERATION_BLEND};
        ppa_client_config_t fill_cfg = {.oper_type = PPA_OPERATION_FILL};
        ESP_ERROR_CHECK(ppa_register_client(&blend_cfg, &s_blend_handle));
        ESP_ERROR_CHECK(ppa_register_client(&fill_cfg, &s_fill_handle));
    }

    lvgl_port_ppa_v9_register_handler(cf);
}

static void lvgl_port_ppa_v9_register_handler(lv_color_format_t cf)
{
    if (s_handler_registered) {
        return;
    }
    if (cf == LV_COLOR_FORMAT_RGB888) {
        lv_draw_sw_register_blend_handler(&s_custom_handler_rgb888);
    } else {
        lv_draw_sw_register_blend_handler(&s_custom_handler);
    }
    s_handler_registered = true;
}

static size_t ppa_get_cache_line_size(const void *addr)
{
    if (!addr) {
        return 0;
    }

    return display_bridge_get_cache_line_size_by_addr(addr);
}

static size_t ppa_get_aligned_buffer_size(const void *addr, size_t size)
{
    size_t align = ppa_get_cache_line_size(addr);

    return (align > 0) ? LVGL_PORT_PPA_ALIGN_UP(size, align) : size;
}

static bool ppa_buffer_cache_aligned(const void *addr)
{
    size_t align = ppa_get_cache_line_size(addr);

    return (align == 0) || (((uintptr_t)addr & (align - 1)) == 0);
}

static void ppa_cache_msync_addr(const void *addr, size_t size, int flag)
{
    size_t align = ppa_get_cache_line_size(addr);
    if (!addr || size == 0 || align == 0) {
        return;
    }

    uintptr_t start = (uintptr_t)addr;
    uintptr_t aligned_start = start & ~((uintptr_t)align - 1);
    size_t aligned_size = LVGL_PORT_PPA_ALIGN_UP(size + (start - aligned_start), align);

    esp_cache_msync((void *)aligned_start, aligned_size, flag);
}

/* Sync the framebuffer rows covering @area. @px_size is the destination bytes
 * per pixel (e.g. 2 for RGB565, 3 for RGB888); it must match the buffer layout,
 * not sizeof(lv_color_t), which is always 3 in LVGL v9. */
static void ppa_cache_sync_region(const lv_area_t *area, const lv_area_t *buf_area, void *buf, uint32_t px_size, int flag)
{
    if (!area || !buf_area || !buf || px_size == 0) {
        return;
    }

    lv_coord_t width = lv_area_get_width(area);
    lv_coord_t height = lv_area_get_height(area);
    lv_coord_t buf_w = lv_area_get_width(buf_area);
    lv_coord_t buf_h = lv_area_get_height(buf_area);

    if (width <= 0 || height <= 0 || buf_w <= 0 || buf_h <= 0) {
        return;
    }

    lv_coord_t off_y = area->y1 - buf_area->y1;

    if (area->x1 < buf_area->x1 || off_y < 0 || area->x2 > buf_area->x2 || (off_y + height) > buf_h) {
        return;
    }

    uint8_t *start = (uint8_t *)buf + (size_t)off_y * buf_w * px_size;
    size_t bytes = (size_t)buf_w * height * px_size;
    ppa_cache_msync_addr(start, bytes, flag);
}

static void ppa_cache_invalidate(const lv_area_t *area, const lv_area_t *buf_area, void *buf, uint32_t px_size)
{
    ppa_cache_sync_region(area, buf_area, buf, px_size, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
}

static void ppa_blend(lv_color_t *bg_buf, const lv_area_t *bg_area, const lv_color_t *fg_buf,
                      const lv_area_t *fg_area, uint16_t fg_stride_px, const lv_area_t *block_area, lv_opa_t opa)
{
    uint16_t bg_w = lv_area_get_width(bg_area);
    uint16_t bg_h = lv_area_get_height(bg_area);
    uint16_t bg_off_x = block_area->x1 - bg_area->x1;
    uint16_t bg_off_y = block_area->y1 - bg_area->y1;

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
    size_t out_buffer_size = ppa_get_aligned_buffer_size(bg_buf, sizeof(lv_color_t) * bg_w * bg_h);

    ppa_blend_oper_config_t cfg = {
        .in_bg = {
            .buffer = bg_buf,
            .pic_w = bg_w,
            .pic_h = bg_h,
            .block_w = block_w,
            .block_h = block_h,
            .block_offset_x = bg_off_x,
            .block_offset_y = bg_off_y,
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
            .buffer_size = out_buffer_size,
            .pic_w = bg_w,
            .pic_h = bg_h,
            .block_offset_x = bg_off_x,
            .block_offset_y = bg_off_y,
            .blend_cm = PPA_BLEND_COLOR_MODE_RGB565,
        },
        .bg_rgb_swap = 0,
        .bg_byte_swap = 0,
        .bg_alpha_update_mode = PPA_ALPHA_FIX_VALUE,
        .bg_alpha_fix_val = 255 - opa,
        .fg_rgb_swap = 0,
        .fg_byte_swap = 0,
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
    size_t out_buffer_size = ppa_get_aligned_buffer_size(bg_buf, sizeof(lv_color_t) * bg_w * bg_h);

    lv_color32_t c32 = lv_color_to_32(color, LV_OPA_COVER);
    uint32_t argb = ((uint32_t)c32.alpha << 24) | ((uint32_t)c32.red << 16) |
                    ((uint32_t)c32.green << 8) | ((uint32_t)c32.blue);

    ppa_fill_oper_config_t cfg = {
        .out = {
            .buffer = bg_buf,
            .buffer_size = out_buffer_size,
            .pic_w = bg_w,
            .pic_h = bg_h,
            .block_offset_x = block_area->x1 - bg_area->x1,
            .block_offset_y = block_area->y1 - bg_area->y1,
            .fill_cm = PPA_FILL_COLOR_MODE_RGB565,
        },
        .fill_block_w = lv_area_get_width(block_area),
        .fill_block_h = lv_area_get_height(block_area),
        .fill_argb_color.val = argb,
        .mode = PPA_TRANS_MODE_BLOCKING,
    };

    ESP_ERROR_CHECK(ppa_do_fill(s_fill_handle, &cfg));
}

static void lv_draw_ppa_v9_sw_fallback(lv_draw_task_t *t, const lv_draw_sw_blend_dsc_t *dsc)
{
    lv_layer_t *layer = t->target_layer;
    if (!layer || !layer->draw_buf) {
        return;
    }

    lv_area_t blend_area;
    if (!lv_area_intersect(&blend_area, dsc->blend_area, &t->clip_area)) {
        return;
    }

    uint32_t layer_stride = layer->draw_buf->header.stride;

    /* Handle fill operations */
    if (dsc->src_buf == NULL) {
        lv_draw_sw_blend_fill_dsc_t fill_dsc;
        lv_memzero(&fill_dsc, sizeof(fill_dsc));
        fill_dsc.dest_w = lv_area_get_width(&blend_area);
        fill_dsc.dest_h = lv_area_get_height(&blend_area);
        fill_dsc.dest_stride = layer_stride;
        fill_dsc.opa = dsc->opa;
        fill_dsc.color = dsc->color;
        if (dsc->mask_buf == NULL || dsc->mask_res == LV_DRAW_SW_MASK_RES_FULL_COVER) {
            fill_dsc.mask_buf = NULL;
        } else {
            fill_dsc.mask_buf = dsc->mask_buf;
            fill_dsc.mask_stride = dsc->mask_stride ? dsc->mask_stride : lv_area_get_width(dsc->mask_area);
            fill_dsc.mask_buf += fill_dsc.mask_stride * (blend_area.y1 - dsc->mask_area->y1) +
                                 (blend_area.x1 - dsc->mask_area->x1);
        }

        fill_dsc.relative_area = blend_area;
        lv_area_move(&fill_dsc.relative_area, -layer->buf_area.x1, -layer->buf_area.y1);
        fill_dsc.dest_buf = lv_draw_layer_go_to_xy(layer,
                                                   blend_area.x1 - layer->buf_area.x1,
                                                   blend_area.y1 - layer->buf_area.y1);
        lv_draw_sw_blend_color_to_rgb565(&fill_dsc);
        return;
    }

    /* Handle image blending */
    lv_draw_sw_blend_image_dsc_t image_dsc;
    lv_memzero(&image_dsc, sizeof(image_dsc));
    image_dsc.dest_w = lv_area_get_width(&blend_area);
    image_dsc.dest_h = lv_area_get_height(&blend_area);
    image_dsc.dest_stride = layer_stride;
    image_dsc.opa = dsc->opa;
    image_dsc.blend_mode = dsc->blend_mode;
    const lv_area_t *src_area = dsc->src_area ? dsc->src_area : dsc->blend_area;
    uint32_t src_px_size = lv_color_format_get_size(dsc->src_color_format);
    image_dsc.src_stride = dsc->src_stride ? dsc->src_stride : (lv_area_get_width(src_area) * src_px_size);
    image_dsc.src_color_format = dsc->src_color_format;

    const uint8_t *src_buf = dsc->src_buf;
    src_buf += (size_t)(blend_area.y1 - src_area->y1) * image_dsc.src_stride;
    src_buf += (size_t)(blend_area.x1 - src_area->x1) * src_px_size;
    image_dsc.src_buf = src_buf;

    if (dsc->mask_buf == NULL || dsc->mask_res == LV_DRAW_SW_MASK_RES_FULL_COVER) {
        image_dsc.mask_buf = NULL;
    } else {
        image_dsc.mask_buf = dsc->mask_buf;
        image_dsc.mask_stride = dsc->mask_stride ? dsc->mask_stride : lv_area_get_width(dsc->mask_area);
        image_dsc.mask_buf += image_dsc.mask_stride * (blend_area.y1 - dsc->mask_area->y1) +
                              (blend_area.x1 - dsc->mask_area->x1);
    }

    image_dsc.relative_area = blend_area;
    lv_area_move(&image_dsc.relative_area, -layer->buf_area.x1, -layer->buf_area.y1);
    if (src_area) {
        image_dsc.src_area = *src_area;
        lv_area_move(&image_dsc.src_area, -layer->buf_area.x1, -layer->buf_area.y1);
    } else {
        lv_memset(&image_dsc.src_area, 0, sizeof(image_dsc.src_area));
    }
    image_dsc.dest_buf = lv_draw_layer_go_to_xy(layer,
                                                blend_area.x1 - layer->buf_area.x1,
                                                blend_area.y1 - layer->buf_area.y1);

    lv_draw_sw_blend_image_to_rgb565(&image_dsc);
}

/**
 * @brief PPA hardware acceleration handler for LVGL v9 blending operations
 *
 * Accelerates RGB565 image blending and opaque fills. Falls back to CPU for:
 * - Semi-transparent fills (PPA has ~28% overhead: redundant DMA + cache sync)
 * - Complex masking, small areas (<100px), unsupported formats
 */
static void lv_draw_ppa_v9_handler(lv_draw_task_t *t, const lv_draw_sw_blend_dsc_t *dsc)
{
    lv_layer_t *layer = t->target_layer;
    if (!layer || !layer->draw_buf || layer->color_format != LV_COLOR_FORMAT_RGB565) {
        lv_draw_ppa_v9_sw_fallback(t, dsc);
        return;
    }

    lv_area_t block_area;
    if (!_lv_area_intersect(&block_area, dsc->blend_area, &t->clip_area)) {
        return;
    }

    /* PPA doesn't support complex masking */
    if (dsc->mask_buf && dsc->mask_res != LV_DRAW_SW_MASK_RES_FULL_COVER &&
            dsc->mask_res != LV_DRAW_SW_MASK_RES_UNKNOWN) {
        lv_draw_ppa_v9_sw_fallback(t, dsc);
        return;
    }

    /* Small areas: DMA setup overhead exceeds benefit */
    if (lv_area_get_size(&block_area) <= 100) {
        lv_draw_ppa_v9_sw_fallback(t, dsc);
        return;
    }

    lv_color_t *bg_buf = (lv_color_t *)layer->draw_buf->data;
    if (!bg_buf || !ppa_buffer_cache_aligned(bg_buf)) {
        lv_draw_ppa_v9_sw_fallback(t, dsc);
        return;
    }

    if (block_area.x1 < layer->buf_area.x1 || block_area.y1 < layer->buf_area.y1 ||
            block_area.x2 > layer->buf_area.x2 || block_area.y2 > layer->buf_area.y2) {
        lv_draw_ppa_v9_sw_fallback(t, dsc);
        return;
    }

    /* Writeback cache before PPA reads (RGB565 destination = 2 bytes/pixel) */
    ppa_cache_sync_region(&block_area, &layer->buf_area, bg_buf, 2, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

    /* Image blending: RGB565 format only */
    if (dsc->src_buf) {
        if (dsc->src_color_format != LV_COLOR_FORMAT_RGB565) {
            lv_draw_ppa_v9_sw_fallback(t, dsc);
            return;
        }

        const lv_area_t *src_area = dsc->src_area ? dsc->src_area : dsc->blend_area;
        uint32_t src_px_size = lv_color_format_get_size(dsc->src_color_format);
        size_t src_stride = dsc->src_stride ? dsc->src_stride : (lv_area_get_width(src_area) * src_px_size);

        if (src_px_size == 0 || (src_stride % src_px_size) != 0) {
            lv_draw_ppa_v9_sw_fallback(t, dsc);
            return;
        }

        lv_coord_t src_off_x = block_area.x1 - src_area->x1;
        lv_coord_t src_off_y = block_area.y1 - src_area->y1;
        if (src_off_x < 0 || src_off_y < 0) {
            lv_draw_ppa_v9_sw_fallback(t, dsc);
            return;
        }

        /* Writeback source buffer cache */
        const uint8_t *src_ptr8 = (const uint8_t *)dsc->src_buf + (size_t)src_off_y * src_stride;
        size_t src_bytes = src_stride * lv_area_get_height(&block_area);
        ppa_cache_msync_addr(src_ptr8, src_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

        uint16_t src_stride_px = src_stride / src_px_size;
        ppa_blend(bg_buf, &layer->buf_area, (const lv_color_t *)dsc->src_buf,
                  src_area, src_stride_px, &block_area, dsc->opa);

        /* Invalidate cache after PPA writes */
        ppa_cache_invalidate(&block_area, &layer->buf_area, bg_buf, 2);
        return;
    }

    if (dsc->opa >= LV_OPA_MAX) {
        ppa_fill(bg_buf, &layer->buf_area, &block_area, dsc->color);
        ppa_cache_invalidate(&block_area, &layer->buf_area, bg_buf, 2);
        return;
    }

    /* Semi-transparent fills: use CPU (more efficient than PPA) */
    lv_draw_ppa_v9_sw_fallback(t, dsc);
}

/* ──────────────────────────────────────────────────────────────────────────
 * RGB888 display acceleration
 *
 * PPA-accelerated paths (fall back to CPU otherwise):
 *   - ARGB8888 source : per-pixel alpha blend onto the RGB888 framebuffer
 *                       (single PPA blend op; pairs with the decoder option
 *                        CONFIG_ESP_LV_DECODER_PJPG_HW_ALPHA_BLEND).
 *   - Opaque RGB565 / RGB888 source : CPU fallback. The SW blend descriptor
 *                                     does not expose image draw flags such as
 *                                     tile mode, and tiled images require
 *                                     LVGL's normal source clipping path.
 *   - Opaque fill : PPA fill.
 *
 * RGB565A8 sources fall back to CPU on purpose: the PPA route for planar
 * RGB565A8 (SRM + per-pixel CPU alpha insert + blend) measured slower than the
 * CPU blend and needs a full-frame ARGB scratch buffer.
 * ────────────────────────────────────────────────────────────────────────── */

/* PPA fill: solid color into RGB888 buffer. */
static void ppa_fill_rgb888(uint8_t *bg_buf, const lv_area_t *bg_area,
                            const lv_area_t *block_area, lv_color_t color)
{
    uint32_t bg_w = lv_area_get_width(bg_area);
    uint32_t bg_h = lv_area_get_height(bg_area);
    size_t out_buf_size = ppa_get_aligned_buffer_size(bg_buf, (size_t)bg_w * bg_h * 3);

    lv_color32_t c32 = lv_color_to_32(color, LV_OPA_COVER);
    uint32_t argb = ((uint32_t)c32.alpha << 24) | ((uint32_t)c32.red << 16) |
                    ((uint32_t)c32.green << 8) | ((uint32_t)c32.blue);

    ppa_fill_oper_config_t cfg = {
        .out = {
            .buffer = bg_buf,
            .buffer_size = out_buf_size,
            .pic_w = bg_w,
            .pic_h = bg_h,
            .block_offset_x = block_area->x1 - bg_area->x1,
            .block_offset_y = block_area->y1 - bg_area->y1,
            .fill_cm = PPA_FILL_COLOR_MODE_RGB888,
        },
        .fill_block_w = lv_area_get_width(block_area),
        .fill_block_h = lv_area_get_height(block_area),
        .fill_argb_color.val = argb,
        .mode = PPA_TRANS_MODE_BLOCKING,
    };

    ESP_ERROR_CHECK(ppa_do_fill(s_fill_handle, &cfg));
}

/* Software fallback for RGB888 destination. */
static void lv_draw_ppa_v9_sw_fallback_rgb888(lv_draw_task_t *t, const lv_draw_sw_blend_dsc_t *dsc)
{
    lv_layer_t *layer = t->target_layer;
    if (!layer || !layer->draw_buf) {
        return;
    }

    lv_area_t blend_area;
    if (!lv_area_intersect(&blend_area, dsc->blend_area, &t->clip_area)) {
        return;
    }

    uint32_t layer_stride = layer->draw_buf->header.stride;

    if (dsc->src_buf == NULL) {
        lv_draw_sw_blend_fill_dsc_t fill_dsc;
        lv_memzero(&fill_dsc, sizeof(fill_dsc));
        fill_dsc.dest_w = lv_area_get_width(&blend_area);
        fill_dsc.dest_h = lv_area_get_height(&blend_area);
        fill_dsc.dest_stride = layer_stride;
        fill_dsc.opa = dsc->opa;
        fill_dsc.color = dsc->color;
        if (dsc->mask_buf && dsc->mask_res != LV_DRAW_SW_MASK_RES_FULL_COVER) {
            fill_dsc.mask_buf = dsc->mask_buf;
            fill_dsc.mask_stride = dsc->mask_stride ? dsc->mask_stride
                                   : lv_area_get_width(dsc->mask_area);
            fill_dsc.mask_buf += fill_dsc.mask_stride * (blend_area.y1 - dsc->mask_area->y1)
                                 + (blend_area.x1 - dsc->mask_area->x1);
        }
        fill_dsc.relative_area = blend_area;
        lv_area_move(&fill_dsc.relative_area, -layer->buf_area.x1, -layer->buf_area.y1);
        fill_dsc.dest_buf = lv_draw_layer_go_to_xy(layer,
                                                   blend_area.x1 - layer->buf_area.x1,
                                                   blend_area.y1 - layer->buf_area.y1);
        lv_draw_sw_blend_color_to_rgb888(&fill_dsc, 3);
        return;
    }

    lv_draw_sw_blend_image_dsc_t image_dsc;
    lv_memzero(&image_dsc, sizeof(image_dsc));
    image_dsc.dest_w = lv_area_get_width(&blend_area);
    image_dsc.dest_h = lv_area_get_height(&blend_area);
    image_dsc.dest_stride = layer_stride;
    image_dsc.opa = dsc->opa;
    image_dsc.blend_mode = dsc->blend_mode;
    image_dsc.src_color_format = dsc->src_color_format;
    const lv_area_t *src_area = dsc->src_area ? dsc->src_area : dsc->blend_area;
    uint32_t src_px_size = lv_color_format_get_size(dsc->src_color_format);
    image_dsc.src_stride = dsc->src_stride ? dsc->src_stride
                           : (lv_area_get_width(src_area) * src_px_size);

    const uint8_t *src_buf = dsc->src_buf;
    src_buf += (size_t)(blend_area.y1 - src_area->y1) * image_dsc.src_stride;
    src_buf += (size_t)(blend_area.x1 - src_area->x1) * src_px_size;
    image_dsc.src_buf = src_buf;

    if (dsc->mask_buf && dsc->mask_res != LV_DRAW_SW_MASK_RES_FULL_COVER) {
        image_dsc.mask_buf = dsc->mask_buf;
        image_dsc.mask_stride = dsc->mask_stride ? dsc->mask_stride
                                : lv_area_get_width(dsc->mask_area);
        image_dsc.mask_buf += image_dsc.mask_stride * (blend_area.y1 - dsc->mask_area->y1)
                              + (blend_area.x1 - dsc->mask_area->x1);
    }
    image_dsc.relative_area = blend_area;
    lv_area_move(&image_dsc.relative_area, -layer->buf_area.x1, -layer->buf_area.y1);
    if (src_area) {
        image_dsc.src_area = *src_area;
        lv_area_move(&image_dsc.src_area, -layer->buf_area.x1, -layer->buf_area.y1);
    }
    image_dsc.dest_buf = lv_draw_layer_go_to_xy(layer,
                                                blend_area.x1 - layer->buf_area.x1,
                                                blend_area.y1 - layer->buf_area.y1);
    lv_draw_sw_blend_image_to_rgb888(&image_dsc, 3);
}

/*
 * @brief PPA hardware acceleration handler for RGB888 display.
 *
 * Falls back to software for: masks, small areas (<100px), unsupported formats
 * (including planar RGB565A8) and semi-transparent fills.
 */
static void lv_draw_ppa_v9_handler_rgb888(lv_draw_task_t *t, const lv_draw_sw_blend_dsc_t *dsc)
{
    lv_layer_t *layer = t->target_layer;
    if (!layer || !layer->draw_buf || layer->color_format != LV_COLOR_FORMAT_RGB888) {
        lv_draw_ppa_v9_sw_fallback_rgb888(t, dsc);
        return;
    }

    lv_area_t block_area;
    if (!_lv_area_intersect(&block_area, dsc->blend_area, &t->clip_area)) {
        return;
    }

    if (dsc->mask_buf && dsc->mask_res != LV_DRAW_SW_MASK_RES_FULL_COVER &&
            dsc->mask_res != LV_DRAW_SW_MASK_RES_UNKNOWN) {
        lv_draw_ppa_v9_sw_fallback_rgb888(t, dsc);
        return;
    }

    if (lv_area_get_size(&block_area) <= 100) {
        lv_draw_ppa_v9_sw_fallback_rgb888(t, dsc);
        return;
    }

    uint8_t *bg_buf = (uint8_t *)layer->draw_buf->data;
    if (!bg_buf || !ppa_buffer_cache_aligned(bg_buf)) {
        lv_draw_ppa_v9_sw_fallback_rgb888(t, dsc);
        return;
    }

    if (block_area.x1 < layer->buf_area.x1 || block_area.y1 < layer->buf_area.y1 ||
            block_area.x2 > layer->buf_area.x2 || block_area.y2 > layer->buf_area.y2) {
        lv_draw_ppa_v9_sw_fallback_rgb888(t, dsc);
        return;
    }

    uint32_t block_w = lv_area_get_width(&block_area);
    uint32_t block_h = lv_area_get_height(&block_area);
    uint32_t bg_off_x = block_area.x1 - layer->buf_area.x1;
    uint32_t bg_off_y = block_area.y1 - layer->buf_area.y1;
    uint32_t bg_w = lv_area_get_width(&layer->buf_area);
    uint32_t bg_h = lv_area_get_height(&layer->buf_area);

    /* Writeback bg cache before PPA reads (RGB888 destination = 3 bytes/pixel) */
    ppa_cache_sync_region(&block_area, &layer->buf_area, bg_buf, 3, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

    /* ── Image blend path ─────────────────────────────────────────────── */
    if (dsc->src_buf) {
        const lv_area_t *src_area = dsc->src_area ? dsc->src_area : dsc->blend_area;
        uint32_t img_w = lv_area_get_width(src_area);
        uint32_t img_h = lv_area_get_height(src_area);
        size_t out_buf_size = ppa_get_aligned_buffer_size(bg_buf, (size_t)bg_w * bg_h * 3);

        /* LVGL does not pre-clip blend_area to src_area on the custom-handler path
         * (unlike the default path in lv_draw_sw_blend). Guard the source bounds so
         * PPA never reads outside the image (also avoids unsigned offset underflow). */
        if (block_area.x1 < src_area->x1 || block_area.y1 < src_area->y1 ||
                block_area.x2 > src_area->x2 || block_area.y2 > src_area->y2) {
            lv_draw_ppa_v9_sw_fallback_rgb888(t, dsc);
            return;
        }
        uint32_t off_x = block_area.x1 - src_area->x1;
        uint32_t off_y = block_area.y1 - src_area->y1;

        /* ── ARGB8888 source: direct PPA blend (per-pixel alpha, no CPU work) ── */
        if (dsc->src_color_format == LV_COLOR_FORMAT_ARGB8888) {
            uint32_t src_stride_bytes = dsc->src_stride ? dsc->src_stride : (img_w * 4);
            uint32_t src_stride_px = src_stride_bytes / 4;

            /* Write back only the block rows PPA will read (not the whole image). */
            const uint8_t *src_start = (const uint8_t *)dsc->src_buf + (size_t)off_y * src_stride_bytes;
            ppa_cache_msync_addr(src_start, (size_t)src_stride_bytes * block_h,
                                 ESP_CACHE_MSYNC_FLAG_DIR_C2M);

            ppa_blend_oper_config_t cfg = {
                .in_bg = {
                    .buffer = bg_buf, .pic_w = bg_w, .pic_h = bg_h,
                    .block_w = block_w, .block_h = block_h,
                    .block_offset_x = bg_off_x, .block_offset_y = bg_off_y,
                    .blend_cm = PPA_BLEND_COLOR_MODE_RGB888,
                },
                .in_fg = {
                    .buffer = (void *)dsc->src_buf,
                    .pic_w = src_stride_px, .pic_h = img_h,
                    .block_w = block_w, .block_h = block_h,
                    .block_offset_x = off_x, .block_offset_y = off_y,
                    .blend_cm = PPA_BLEND_COLOR_MODE_ARGB8888,
                },
                .out = {
                    .buffer = bg_buf, .buffer_size = out_buf_size,
                    .pic_w = bg_w, .pic_h = bg_h,
                    .block_offset_x = bg_off_x, .block_offset_y = bg_off_y,
                    .blend_cm = PPA_BLEND_COLOR_MODE_RGB888,
                },
                .bg_alpha_update_mode = PPA_ALPHA_FIX_VALUE, .bg_alpha_fix_val = LV_OPA_COVER,
                .fg_alpha_update_mode = (dsc->opa >= LV_OPA_MAX) ? PPA_ALPHA_NO_CHANGE : PPA_ALPHA_SCALE,
                .fg_alpha_scale_ratio = (float)dsc->opa / 255.0f,
                .mode = PPA_TRANS_MODE_BLOCKING,
            };
            ESP_ERROR_CHECK(ppa_do_blend(s_blend_handle, &cfg));
            ppa_cache_sync_region(&block_area, &layer->buf_area, bg_buf, 3, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
            return;
        }

        /* Opaque RGB565/RGB888 images can include tiled image draws. The SW
         * custom blend descriptor does not expose the original image draw flags,
         * so keep this path on CPU instead of using PPA SRM with incomplete
         * source-coordinate context. */
        if (dsc->opa >= LV_OPA_MAX &&
                (dsc->src_color_format == LV_COLOR_FORMAT_RGB565 ||
                 dsc->src_color_format == LV_COLOR_FORMAT_RGB888)) {
            lv_draw_ppa_v9_sw_fallback_rgb888(t, dsc);
            return;
        }

        /* Other source formats (e.g. planar RGB565A8): CPU blend. */
        lv_draw_ppa_v9_sw_fallback_rgb888(t, dsc);
        return;
    }

    /* ── Opaque fill path ─────────────────────────────────────────────── */
    if (dsc->opa >= LV_OPA_MAX) {
        ppa_fill_rgb888(bg_buf, &layer->buf_area, &block_area, dsc->color);
        ppa_cache_sync_region(&block_area, &layer->buf_area, bg_buf, 3, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
        return;
    }

    /* Semi-transparent fill: use CPU */
    lv_draw_ppa_v9_sw_fallback_rgb888(t, dsc);
}

#else /* !CONFIG_SOC_PPA_SUPPORTED */

void lvgl_port_ppa_v9_init(lv_display_t *display)
{
    (void)display;
}

#endif /* CONFIG_SOC_PPA_SUPPORTED */
