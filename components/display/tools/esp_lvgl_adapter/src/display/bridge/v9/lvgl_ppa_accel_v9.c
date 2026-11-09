/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * LVGL v9 PPA Hardware Acceleration
 */

#include "lvgl_port_ppa.h"
#include "soc/soc_caps.h"

#if CONFIG_SOC_PPA_SUPPORTED

#include <stdlib.h>
#include "lvgl_port_alignment.h"
#include "esp_log.h"
#include "lvgl_port_draw_private.h"
#include "driver/ppa.h"
#include "esp_cache.h"
#include "esp_private/esp_cache_private.h"
#include "esp_memory_utils.h"
#include "common/display_bridge_common.h"

static ppa_client_handle_t s_blend_handle = NULL;
static ppa_client_handle_t s_fill_handle = NULL;
static const char *TAG = "esp_lvgl:ppa";

typedef struct ppa_display {
    lv_display_t *display;
    lv_color_format_t color_format;
    struct ppa_display *next;
} ppa_display_t;

typedef struct {
    unsigned display_count;
    lv_draw_sw_blend_handler_t previous_handler;
} ppa_format_t;

/* Registration/deletion run under the LVGL lock with drawing quiesced. */
static ppa_display_t *s_displays;
static ppa_format_t s_rgb565;
static ppa_format_t s_rgb888;

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

static void ppa_display_deleted(lv_event_t *event);
static void ppa_release_display(ppa_display_t *entry);
static void ppa_release_clients(void);
static bool ppa_task_enabled(const lv_draw_task_t *task);
static size_t ppa_get_cache_line_size(const void *addr);
static size_t ppa_get_aligned_buffer_size(const void *addr, size_t size);
static bool ppa_buffer_cache_aligned(const void *addr);
static void ppa_cache_msync_addr(const void *addr, size_t size, int flag);
static void ppa_cache_sync_region(const lv_area_t *area, const lv_area_t *buf_area, void *buf, uint32_t px_size, int flag);
static void ppa_cache_invalidate(const lv_area_t *area, const lv_area_t *buf_area, void *buf, uint32_t px_size);
static bool ppa_try_draw(lv_draw_task_t *task, const lv_draw_sw_blend_dsc_t *dsc, lv_color_format_t cf);
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

    for (ppa_display_t *entry = s_displays; entry; entry = entry->next) {
        if (entry->display == display) {
            return;
        }
    }
    ppa_display_t *entry = calloc(1, sizeof(*entry));
    if (!entry) {
        ESP_LOGW(TAG, "No memory for PPA display; using software drawing");
        return;
    }

    if (!s_displays) {
        ppa_client_config_t blend_cfg = {.oper_type = PPA_OPERATION_BLEND};
        ppa_client_config_t fill_cfg = {.oper_type = PPA_OPERATION_FILL};
        esp_err_t ret = ppa_register_client(&blend_cfg, &s_blend_handle);
        if (ret == ESP_OK) {
            ret = ppa_register_client(&fill_cfg, &s_fill_handle);
        }
        if (ret != ESP_OK) {
            ppa_release_clients();
            ESP_LOGW(TAG, "Cannot register PPA clients (%d); using software drawing", ret);
            free(entry);
            return;
        }
    }

    ppa_format_t *format = cf == LV_COLOR_FORMAT_RGB565 ? &s_rgb565 : &s_rgb888;
    lv_draw_sw_custom_blend_handler_t *handler = cf == LV_COLOR_FORMAT_RGB565 ?
                                                 &s_custom_handler : &s_custom_handler_rgb888;
    if (format->display_count == 0) {
        format->previous_handler = lv_draw_sw_get_blend_handler(cf);
        if (!lv_draw_sw_register_blend_handler(handler)) {
            if (!s_displays) {
                ppa_release_clients();
            }
            format->previous_handler = NULL;
            free(entry);
            ESP_LOGW(TAG, "Cannot install PPA blend handler; using software drawing");
            return;
        }
    }
    format->display_count++;
    entry->display = display;
    entry->color_format = cf;
    entry->next = s_displays;
    s_displays = entry;
    /* LVGL <=9.5 returns void here; the event count works across v9 versions. */
    uint32_t event_count = lv_display_get_event_count(display);
    lv_display_add_event_cb(display, ppa_display_deleted, LV_EVENT_DELETE, entry);
    if (lv_display_get_event_count(display) == event_count) {
        ppa_release_display(entry);
        ESP_LOGW(TAG, "Cannot install PPA cleanup callback; using software drawing");
    }
}

static void ppa_release_display(ppa_display_t *entry)
{
    ppa_display_t **cursor = &s_displays;
    while (*cursor && *cursor != entry) {
        cursor = &(*cursor)->next;
    }
    if (!*cursor) {
        return;
    }
    *cursor = entry->next;
    lv_color_format_t cf = entry->color_format;
    ppa_format_t *format = cf == LV_COLOR_FORMAT_RGB565 ? &s_rgb565 : &s_rgb888;
    lv_draw_sw_blend_handler_t handler = cf == LV_COLOR_FORMAT_RGB565 ?
                                         lv_draw_ppa_v9_handler : lv_draw_ppa_v9_handler_rgb888;
    if (--format->display_count == 0) {
        /* Do not overwrite a handler installed by the application meanwhile. */
        if (lv_draw_sw_get_blend_handler(cf) == handler) {
            if (format->previous_handler) {
                lv_draw_sw_custom_blend_handler_t previous = {
                    .dest_cf = cf,
                    .handler = format->previous_handler,
                };
                lv_draw_sw_register_blend_handler(&previous);
            } else {
                lv_draw_sw_unregister_blend_handler(cf);
            }
        }
        format->previous_handler = NULL;
    }
    free(entry);
    if (!s_displays) {
        ppa_release_clients();
    }
}

static void ppa_release_clients(void)
{
    if (s_fill_handle) {
        ESP_ERROR_CHECK(ppa_unregister_client(s_fill_handle));
        s_fill_handle = NULL;
    }
    if (s_blend_handle) {
        ESP_ERROR_CHECK(ppa_unregister_client(s_blend_handle));
        s_blend_handle = NULL;
    }
}

static void ppa_display_deleted(lv_event_t *event)
{
    ppa_release_display(lv_event_get_user_data(event));
}

static bool ppa_task_enabled(const lv_draw_task_t *task)
{
    const lv_draw_dsc_base_t *base = task->draw_dsc;
    if (base && base->obj) {
        lv_display_t *display = lv_obj_get_display(base->obj);
        for (ppa_display_t *entry = s_displays; entry; entry = entry->next) {
            if (entry->display == display) {
                return true;
            }
        }
        return false;
    }

    const lv_layer_t *root = task->target_layer;
    while (root && root->parent) {
        root = root->parent;
    }
    for (ppa_display_t *entry = s_displays; entry; entry = entry->next) {
        if (root == entry->display->layer_head) {
            return true;
        }
    }
    /* A canvas/snapshot without an owner must not inherit another display's flag. */
    return false;
}

/* The custom handler runs before LVGL clips image draws to source/mask bounds. */
static bool ppa_get_blend_area(lv_area_t *area, const lv_draw_task_t *task, const lv_draw_sw_blend_dsc_t *dsc)
{
    if (!lv_area_intersect(area, dsc->blend_area, &task->clip_area)) {
        return false;
    }
    if (dsc->src_buf) {
        const lv_area_t *source = dsc->src_area ? dsc->src_area : dsc->blend_area;
        if (!lv_area_intersect(area, area, source)) {
            return false;
        }
    }
    if ((dsc->src_buf || (dsc->mask_buf && dsc->mask_res != LV_DRAW_SW_MASK_RES_FULL_COVER)) &&
            dsc->mask_area &&
            !lv_area_intersect(area, area, dsc->mask_area)) {
        return false;
    }
    return true;
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
    size_t align = lvgl_port_ppa_output_alignment(addr);

    return (align > 0) ? LVGL_PORT_PPA_ALIGN_UP(size, align) : size;
}

static bool ppa_buffer_cache_aligned(const void *addr)
{
    size_t align = lvgl_port_ppa_output_alignment(addr);

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

/* Both opaque destination formats share the same eligibility and cache rules.
 * A mask can be a PPA A8 foreground only for a single fill color; images with
 * an independent mask (including planar RGB565A8) still use software. */
static bool ppa_try_draw(lv_draw_task_t *task, const lv_draw_sw_blend_dsc_t *dsc, lv_color_format_t cf)
{
    lv_layer_t *layer = task->target_layer;
    if (!layer || !layer->draw_buf || layer->color_format != cf ||
            !ppa_task_enabled(task) || dsc->blend_mode != LV_BLEND_MODE_NORMAL) {
        return false;
    }

    bool masked = dsc->mask_buf && dsc->mask_res != LV_DRAW_SW_MASK_RES_FULL_COVER;
    if (dsc->src_buf) {
        if (masked || (dsc->src_color_format != LV_COLOR_FORMAT_ARGB8888 &&
                       !(cf == LV_COLOR_FORMAT_RGB565 && dsc->src_color_format == LV_COLOR_FORMAT_RGB565))) {
            return false;
        }
    } else if ((!masked && dsc->opa < LV_OPA_MAX) ||
               (masked && (!dsc->mask_area || dsc->mask_stride < 0))) {
        return false;
    }

    lv_area_t block;
    if (!ppa_get_blend_area(&block, task, dsc)) {
        return true;
    }
    uint8_t *buffer = layer->draw_buf->data;
    bool copy = !masked && dsc->opa >= LV_OPA_MAX &&
                (!dsc->src_buf || dsc->src_color_format == LV_COLOR_FORMAT_RGB565);
    if (!buffer || !lvgl_port_ppa_use_hw(lv_area_get_size(&block), copy, buffer)) {
        return false;
    }
#if CONFIG_IDF_TARGET_ESP32P4
    /* Sparse RGB888 PSRAM masks stay faster in software at medium sizes. */
    if (masked && cf == LV_COLOR_FORMAT_RGB888 &&
            lv_area_get_size(&block) < 256 * 256 && esp_ptr_external_ram(buffer)) {
        return false;
    }
#endif

    uint32_t width = lv_area_get_width(&layer->buf_area);
    uint32_t height = lv_area_get_height(&layer->buf_area);
    uint32_t bytes = cf == LV_COLOR_FORMAT_RGB565 ? 2 : 3;
    /* PPA's DMA2D picture dimensions are 14-bit on both supported targets. */
    const uint32_t max_dimension = 0x3FFF;
    if (!width || !height || width > max_dimension || height > max_dimension ||
            block.x1 < layer->buf_area.x1 || block.y1 < layer->buf_area.y1 ||
            block.x2 > layer->buf_area.x2 || block.y2 > layer->buf_area.y2 ||
            layer->draw_buf->header.stride != width * bytes ||
            !ppa_buffer_cache_aligned(buffer) ||
            !display_bridge_dma2d_window_is_compatible(buffer, width,
                                                       block.x1 - layer->buf_area.x1,
                                                       lv_area_get_width(&block), bytes)) {
        return false;
    }
    size_t buffer_size = ppa_get_aligned_buffer_size(buffer, (size_t)width * height * bytes);
    if (buffer_size > layer->draw_buf->data_size) {
        return false;
    }

    const void *source = dsc->src_buf ? dsc->src_buf : masked ? dsc->mask_buf : NULL;
    uint32_t source_bytes = 0;
    uint32_t source_stride = 0;
    uint32_t source_height = 0;
    uint32_t source_x = 0;
    uint32_t source_y = 0;
    if (source) {
        const lv_area_t *source_area = masked ? dsc->mask_area : dsc->src_area ? dsc->src_area : dsc->blend_area;
        source_bytes = masked ? 1 : dsc->src_color_format == LV_COLOR_FORMAT_RGB565 ? 2 : 4;
        source_height = lv_area_get_height(source_area);
        source_stride = masked ? (uint32_t)dsc->mask_stride : dsc->src_stride;
        if (!source_stride) {
            source_stride = lv_area_get_width(source_area) * source_bytes;
        }
        source_x = block.x1 - source_area->x1;
        source_y = block.y1 - source_area->y1;
        if (!source_height || source_height > max_dimension || source_stride % source_bytes ||
                source_stride / source_bytes > max_dimension ||
                source_x + lv_area_get_width(&block) > source_stride / source_bytes ||
                source_y + lv_area_get_height(&block) > source_height ||
                !display_bridge_dma2d_window_is_compatible(source, source_stride / source_bytes,
                                                           source_x, lv_area_get_width(&block), source_bytes)) {
            return false;
        }
        /* DMA cannot preserve general overlapping image/mask reads. */
        uintptr_t src = (uintptr_t)source;
        uintptr_t dst = (uintptr_t)buffer;
        size_t source_size = (size_t)source_stride * source_height;
        if (src < dst + buffer_size && dst < src + source_size) {
            return false;
        }
    }

    ppa_blend_color_mode_t dest = cf == LV_COLOR_FORMAT_RGB565 ?
                                  PPA_BLEND_COLOR_MODE_RGB565 : PPA_BLEND_COLOR_MODE_RGB888;
    ppa_cache_sync_region(&block, &layer->buf_area, buffer, bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    if (source) {
        const uint8_t *row = (const uint8_t *)source + (size_t)source_y * source_stride;
        ppa_cache_msync_addr(row, (size_t)source_stride * lv_area_get_height(&block), ESP_CACHE_MSYNC_FLAG_DIR_C2M);
        lv_color32_t color = {0};
        if (masked) {
            color = lv_color_to_32(dsc->color, LV_OPA_COVER);
        }
        bool rgb565 = !masked && dsc->src_color_format == LV_COLOR_FORMAT_RGB565;
        ppa_blend_color_mode_t foreground = masked ? PPA_BLEND_COLOR_MODE_A8 :
                                            rgb565 ? PPA_BLEND_COLOR_MODE_RGB565 : PPA_BLEND_COLOR_MODE_ARGB8888;
        ppa_alpha_update_mode_t alpha = PPA_ALPHA_NO_CHANGE;
        if (rgb565) {
            alpha = PPA_ALPHA_FIX_VALUE;
        } else if (dsc->opa < LV_OPA_MAX) {
            alpha = PPA_ALPHA_SCALE;
        }
        ppa_blend_oper_config_t cfg = {
            .in_bg = {
                .buffer = buffer, .pic_w = width, .pic_h = height,
                .block_w = lv_area_get_width(&block), .block_h = lv_area_get_height(&block),
                .block_offset_x = block.x1 - layer->buf_area.x1, .block_offset_y = block.y1 - layer->buf_area.y1,
                .blend_cm = dest,
            },
            .in_fg = {
                .buffer = source, .pic_w = source_stride / source_bytes, .pic_h = source_height,
                .block_w = lv_area_get_width(&block), .block_h = lv_area_get_height(&block),
                .block_offset_x = source_x, .block_offset_y = source_y,
                .blend_cm = foreground,
            },
            .out = {
                .buffer = buffer, .buffer_size = buffer_size, .pic_w = width, .pic_h = height,
                .block_offset_x = block.x1 - layer->buf_area.x1, .block_offset_y = block.y1 - layer->buf_area.y1,
                .blend_cm = dest,
            },
            .bg_alpha_update_mode = PPA_ALPHA_FIX_VALUE, .bg_alpha_fix_val = LV_OPA_COVER,
            .fg_alpha_update_mode = alpha,
            .fg_fix_rgb_val = {.r = color.red, .g = color.green, .b = color.blue},
            .mode = PPA_TRANS_MODE_BLOCKING,
        };
        if (rgb565) {
            cfg.fg_alpha_fix_val = dsc->opa >= LV_OPA_MAX ? LV_OPA_COVER : dsc->opa;
        } else if (dsc->opa < LV_OPA_MAX) {
            cfg.fg_alpha_scale_ratio = (float)dsc->opa / 255.0f;
        }
        ESP_ERROR_CHECK(ppa_do_blend(s_blend_handle, &cfg));
    } else {
        lv_color32_t color = lv_color_to_32(dsc->color, LV_OPA_COVER);
        uint32_t argb = ((uint32_t)color.alpha << 24) | ((uint32_t)color.red << 16) |
                        ((uint32_t)color.green << 8) | color.blue;
        ppa_fill_oper_config_t cfg = {
            .out = {
                .buffer = buffer, .buffer_size = buffer_size, .pic_w = width, .pic_h = height,
                .block_offset_x = block.x1 - layer->buf_area.x1, .block_offset_y = block.y1 - layer->buf_area.y1,
                .fill_cm = cf == LV_COLOR_FORMAT_RGB565 ? PPA_FILL_COLOR_MODE_RGB565 : PPA_FILL_COLOR_MODE_RGB888,
            },
            .fill_block_w = lv_area_get_width(&block), .fill_block_h = lv_area_get_height(&block),
            .fill_argb_color.val = argb,
            .mode = PPA_TRANS_MODE_BLOCKING,
        };
        ESP_ERROR_CHECK(ppa_do_fill(s_fill_handle, &cfg));
    }
    ppa_cache_invalidate(&block, &layer->buf_area, buffer, bytes);
    return true;
}

static void lv_draw_ppa_v9_sw_fallback(lv_draw_task_t *t, const lv_draw_sw_blend_dsc_t *dsc)
{
    if (s_rgb565.previous_handler) {
        s_rgb565.previous_handler(t, dsc);
        return;
    }
    lv_layer_t *layer = t->target_layer;
    if (!layer || !layer->draw_buf) {
        return;
    }

    lv_area_t blend_area;
    if (!ppa_get_blend_area(&blend_area, t, dsc)) {
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
    uint32_t src_bpp = lv_color_format_get_bpp(dsc->src_color_format);
    image_dsc.src_stride = dsc->src_stride ? dsc->src_stride : ((lv_area_get_width(src_area) * src_bpp + 7) >> 3);
    image_dsc.src_color_format = dsc->src_color_format;

    const uint8_t *src_buf = dsc->src_buf;
    src_buf += (size_t)(blend_area.y1 - src_area->y1) * image_dsc.src_stride;
    src_buf += ((size_t)(blend_area.x1 - src_area->x1) * src_bpp) >> 3;
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
    image_dsc.src_area = *src_area;
    lv_area_move(&image_dsc.src_area, -layer->buf_area.x1, -layer->buf_area.y1);
    image_dsc.dest_buf = lv_draw_layer_go_to_xy(layer,
                                                blend_area.x1 - layer->buf_area.x1,
                                                blend_area.y1 - layer->buf_area.y1);

    lv_draw_sw_blend_image_to_rgb565(&image_dsc);
}

static void lv_draw_ppa_v9_handler(lv_draw_task_t *t, const lv_draw_sw_blend_dsc_t *dsc)
{
    if (!ppa_try_draw(t, dsc, LV_COLOR_FORMAT_RGB565)) {
        lv_draw_ppa_v9_sw_fallback(t, dsc);
    }
}

/* Software fallback for RGB888 destination. */
static void lv_draw_ppa_v9_sw_fallback_rgb888(lv_draw_task_t *t, const lv_draw_sw_blend_dsc_t *dsc)
{
    if (s_rgb888.previous_handler) {
        s_rgb888.previous_handler(t, dsc);
        return;
    }
    lv_layer_t *layer = t->target_layer;
    if (!layer || !layer->draw_buf) {
        return;
    }

    lv_area_t blend_area;
    if (!ppa_get_blend_area(&blend_area, t, dsc)) {
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
    uint32_t src_bpp = lv_color_format_get_bpp(dsc->src_color_format);
    image_dsc.src_stride = dsc->src_stride ? dsc->src_stride
                           : ((lv_area_get_width(src_area) * src_bpp + 7) >> 3);

    const uint8_t *src_buf = dsc->src_buf;
    src_buf += (size_t)(blend_area.y1 - src_area->y1) * image_dsc.src_stride;
    src_buf += ((size_t)(blend_area.x1 - src_area->x1) * src_bpp) >> 3;
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
    image_dsc.src_area = *src_area;
    lv_area_move(&image_dsc.src_area, -layer->buf_area.x1, -layer->buf_area.y1);
    image_dsc.dest_buf = lv_draw_layer_go_to_xy(layer,
                                                blend_area.x1 - layer->buf_area.x1,
                                                blend_area.y1 - layer->buf_area.y1);
    lv_draw_sw_blend_image_to_rgb888(&image_dsc, 3);
}

static void lv_draw_ppa_v9_handler_rgb888(lv_draw_task_t *t, const lv_draw_sw_blend_dsc_t *dsc)
{
    if (!ppa_try_draw(t, dsc, LV_COLOR_FORMAT_RGB888)) {
        lv_draw_ppa_v9_sw_fallback_rgb888(t, dsc);
    }
}

#else /* !CONFIG_SOC_PPA_SUPPORTED */

void lvgl_port_ppa_v9_init(lv_display_t *display)
{
    (void)display;
}

#endif /* CONFIG_SOC_PPA_SUPPORTED */
