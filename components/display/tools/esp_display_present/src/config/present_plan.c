/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "pixel/present_pixel_ops.h"
#include "backend/present_backend_config.h"
#include "backend/present_backend_validate.h"
#include "config/present_plan.h"
#include "core/present_engine.h"
#include "config/present_mode.h"
#include "platform/present_limits.h"
#include "resources/present_memory.h"
#include <limits.h>
#include <string.h>

static size_t gcd(size_t a, size_t backend_config)
{
    while (backend_config) {
        size_t t = a % backend_config;
        a = backend_config;
        backend_config = t;
    }
    return a;
}
static bool mul(size_t a, size_t backend_config, size_t *out)
{
    if (backend_config && a > SIZE_MAX / backend_config) {
        return false;
    }
    *out = a * backend_config;
    return true;
}
static bool add(size_t a, size_t backend_config, size_t *out)
{
    if (a > SIZE_MAX - backend_config) {
        return false;
    }
    *out = a + backend_config;
    return true;
}
static bool lcm(size_t a, size_t backend_config, size_t *out)
{
    a = a ? a : 1;
    backend_config = backend_config ? backend_config : 1;
    return mul(a / gcd(a, backend_config), backend_config, out);
}
static bool align_up(size_t value, size_t alignment, size_t *out)
{
    alignment = alignment ? alignment : 1;
    size_t rem = value % alignment;
    return add(value, rem ? alignment - rem : 0, out);
}
static bool power_two(size_t x)
{
    return x && !(x & (x - 1));
}
static bool layout(uint32_t w, uint32_t h, size_t bpp, size_t stride_align,
                   esp_display_present_engine_pixel_format_t format, esp_display_present_byte_order_t order, esp_display_present_layout_t *out)
{
    size_t row, alignment;
    if (!mul(w, bpp, &row) || !lcm(bpp, stride_align, &alignment) || !align_up(row, alignment, &row)) {
        return false;
    }
    *out = (esp_display_present_layout_t) {
        w, h, row, format, order
    };
    return true;
}
static bool pool(esp_display_present_plan_pool_t *pool_config, size_t n, esp_display_present_layout_t l, size_t a, esp_display_present_plan_memory_t memory, bool borrowed)
{
    *pool_config = (esp_display_present_plan_pool_t) {
        .count = n,
        .layout = l,
        .memory = memory,
        .borrowed = borrowed
    };
    if (!n) {
        return true;
    }
    size_t span;
    if (!mul(l.stride_bytes, l.height, &span)) {
        return false;
    }
    if (borrowed) {
        /* Base alignment does not grant ownership of tail cache padding. */
        pool_config->bytes_each = span;
    } else if (!align_up(span, a, &pool_config->bytes_each)) {
        return false;
    }
    return mul(n, pool_config->bytes_each, &pool_config->total_bytes);
}
static bool executor_supports(const esp_display_present_executor_caps_t *caps, const esp_display_present_backend_config_t *config)
{
    return (caps->format_mask & (1u << config->format)) && (caps->rotation_mask & (1u << config->rotation)) &&
           (config->draw_order == config->panel_order || caps->byte_swap);
}
static bool valid_memory(esp_display_present_plan_memory_t m)
{
    return m == ESP_DISPLAY_PRESENT_MEMORY_INTERNAL_DMA || m == ESP_DISPLAY_PRESENT_MEMORY_EXTERNAL;
}

static esp_err_t resolve(const esp_display_present_plan_config_t *config, esp_display_present_resolved_plan_t *plan)
{
    if (!config || config->defaults < ESP_DISPLAY_PRESENT_PLAN_CURRENT_DEFAULTS || config->defaults > ESP_DISPLAY_PRESENT_PLAN_LEGACY_DEFAULTS ||
            !valid_memory(config->resources.tile_memory) || !valid_memory(config->resources.image_memory) || !valid_memory(config->resources.logical_memory)) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t e = present_mode_resolve_engine(&config->presenter, config->defaults, config->te_enabled, &plan->normalized);
    if (e != ESP_OK) {
        return e;
    }
    esp_display_present_backend_config_t *backend_config = &plan->normalized.backend;
    esp_display_present_engine_backend_t kind = plan->normalized.backend_kind;
    /* The INT_MAX geometry bound is plan-layer policy; the shared helper
     * covers the constraints plan and backend create already agree on. */
    if (backend_config->width > INT_MAX || backend_config->height > INT_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t shared = present_backend_config_validate_shared(backend_config);
    if (shared != ESP_OK) {
        return shared;
    }
    bool partial = backend_config->render_contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION;
    if (kind != ESP_DISPLAY_PRESENT_ENGINE_FB && !partial) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (backend_config->in_place && (kind != ESP_DISPLAY_PRESENT_ENGINE_REGION || backend_config->async_copy || backend_config->rotation != ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (backend_config->copy_execute && !backend_config->async_copy) {
        return ESP_ERR_INVALID_ARG;
    }
    if ((backend_config->deferred_submit && (kind != ESP_DISPLAY_PRESENT_ENGINE_REGION || !plan->normalized.background_service)) ||
            (plan->normalized.background_service && kind == ESP_DISPLAY_PRESENT_ENGINE_REGION && !backend_config->async_copy && !backend_config->deferred_submit)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (kind != ESP_DISPLAY_PRESENT_ENGINE_FB && (backend_config->ext_bufs || backend_config->ext_bytes || backend_config->scan_buf)) {
        return ESP_ERR_INVALID_ARG;
    }
    if ((!backend_config->ext_bufs && (backend_config->ext_bytes || backend_config->scan_buf)) ||
            (config->output.ext_bufs_required && !backend_config->ext_bufs)) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t images = kind == ESP_DISPLAY_PRESENT_ENGINE_FB ? backend_config->buffer_count : kind == ESP_DISPLAY_PRESENT_ENGINE_TE ?
                    (config->defaults == ESP_DISPLAY_PRESENT_PLAN_LEGACY_DEFAULTS ? 1 : 2) : 0;
    if (config->resources.image_count) {
        if (kind == ESP_DISPLAY_PRESENT_ENGINE_REGION || (kind == ESP_DISPLAY_PRESENT_ENGINE_FB && plan->normalized.mode != ESP_DISPLAY_PRESENT_MODE_DEFAULT && config->resources.image_count != images)) {
            return ESP_ERR_INVALID_ARG;
        }
        images = config->resources.image_count;
    }
    if ((kind == ESP_DISPLAY_PRESENT_ENGINE_TE && (images < 1 || images > 2)) ||
            (kind == ESP_DISPLAY_PRESENT_ENGINE_FB && (images < 1 || images > 3))) {
        return ESP_ERR_INVALID_ARG;
    }
    if (kind == ESP_DISPLAY_PRESENT_ENGINE_FB && images == 1 && !backend_config->active_writes) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t tiles = partial ? (config->resources.tile_count ? config->resources.tile_count :
                              kind == ESP_DISPLAY_PRESENT_ENGINE_FB ? 2 : backend_config->buffer_count ? backend_config->buffer_count : 2) : 0;
    if ((!partial && config->resources.tile_count) || tiles > PRESENT_MAX_TILE_BUFFERS) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t bpp = backend_config->format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3;
    if (partial && !backend_config->tile_rows) {
        size_t row;
        if (!mul(backend_config->width, bpp, &row)) {
            return ESP_ERR_INVALID_SIZE;
        }
        size_t rows = 32768 / row;
        backend_config->tile_rows = rows ? (rows < backend_config->height ? rows : backend_config->height) : 1;
    }
    if (partial && backend_config->tile_rows > backend_config->height) {
        return ESP_ERR_INVALID_ARG;
    }
    plan->output = config->output;
    if (!plan->output.x_alignment) {
        plan->output.x_alignment = 1;
    }
    if (!plan->output.y_alignment) {
        plan->output.y_alignment = 1;
    }
    if (!plan->output.w_align) {
        plan->output.w_align = 1;
    }
    if (!plan->output.h_align) {
        plan->output.h_align = 1;
    }
    if (!plan->output.base_alignment) {
        plan->output.base_alignment = PRESENT_BUFFER_ALIGNMENT;
    }
    if (!plan->output.stride_align) {
        plan->output.stride_align = 1;
    }
    if (!power_two(plan->output.base_alignment)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!plan->output.internal_dma && !plan->output.ext_mem) {
        return ESP_ERR_INVALID_ARG;
    }
    if (plan->output.full_only && kind == ESP_DISPLAY_PRESENT_ENGINE_REGION) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (backend_config->tx_staging && (kind != ESP_DISPLAY_PRESENT_ENGINE_REGION || !backend_config->async_copy)) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_display_present_plan_memory_t transport_memory = images ? config->resources.image_memory :
                                                         backend_config->tx_staging ? ESP_DISPLAY_PRESENT_MEMORY_INTERNAL_DMA : config->resources.tile_memory;
    bool external_tx = transport_memory == ESP_DISPLAY_PRESENT_MEMORY_EXTERNAL;
    size_t length_alignment = plan->output.ext_len_align ? plan->output.ext_len_align : 1;
    if (external_tx && kind == ESP_DISPLAY_PRESENT_ENGINE_REGION) {
        /* Tight rows with this width make every rendered band DMA-legal,
         * including the last short band. Negotiate before rasterization. */
        size_t w_align;
        if (!lcm(plan->output.w_align, length_alignment / gcd(length_alignment, bpp), &w_align) || w_align > UINT32_MAX) {
            return ESP_ERR_INVALID_SIZE;
        }
        plan->output.w_align = w_align;
    }
    if (kind == ESP_DISPLAY_PRESENT_ENGINE_REGION) {
        size_t wa, ha;
        if (!lcm(plan->output.x_alignment, plan->output.w_align, &wa) ||
                !lcm(plan->output.y_alignment, plan->output.h_align, &ha) || wa > UINT32_MAX || ha > UINT32_MAX) {
            return ESP_ERR_INVALID_SIZE;
        }
        plan->output.w_align = wa;
        plan->output.h_align = ha;
    }
    esp_display_present_executor_caps_t caps = {
        .base_alignment = 1,
        .dst_cap_align = 1,
        .stride_align = 1,
    };
    plan->executor_reason = ESP_DISPLAY_PRESENT_EXEC_SUPPORTED;
    if (config->executor) {
        if (!backend_config->async_copy || backend_config->copy_execute) {
            return ESP_ERR_INVALID_ARG;
        }
        e = esp_display_present_executor_get_caps(config->executor->primary, &caps);
        if (e != ESP_OK) {
            return e;
        }
        plan->preferred_executor = config->executor->primary;
        if (!executor_supports(&caps, backend_config)) {
            plan->executor_reason = ESP_DISPLAY_PRESENT_EXEC_TRANSFORM;
            if (!config->executor->fallback) {
                return ESP_ERR_NOT_SUPPORTED;
            }
            e = esp_display_present_executor_get_caps(config->executor->fallback, &caps);
            if (e != ESP_OK) {
                return e;
            }
            if (!executor_supports(&caps, backend_config)) {
                return ESP_ERR_NOT_SUPPORTED;
            }
            plan->preferred_executor = config->executor->fallback;
        }
        plan->probe_addrs = true;
        backend_config->copy_execute = esp_display_present_executor_dispatch_execute;
        backend_config->copy_user = (void *)config->executor;
    }
    size_t alignment;
    if (!lcm(plan->output.base_alignment, PRESENT_BUFFER_ALIGNMENT, &alignment) ||
            !lcm(alignment, caps.base_alignment, &alignment) || !lcm(alignment, caps.dst_cap_align, &alignment)) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (!power_two(alignment)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    plan->base_alignment = alignment;
    size_t stride_align;
    if (!lcm(plan->output.stride_align, caps.stride_align, &stride_align)) {
        return ESP_ERR_INVALID_SIZE;
    }
    bool quarter = backend_config->rotation == ESP_DISPLAY_PRESENT_ENGINE_ROTATE_90 || backend_config->rotation == ESP_DISPLAY_PRESENT_ENGINE_ROTATE_270;
    uint32_t pw = quarter ? backend_config->height : backend_config->width, ph = quarter ? backend_config->width : backend_config->height;
    if (!layout(backend_config->width, backend_config->height, bpp, caps.stride_align, backend_config->format, backend_config->draw_order, &plan->logical_layout) ||
            !layout(pw, ph, bpp, stride_align, backend_config->format, backend_config->panel_order, &plan->physical_layout)) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (external_tx && images) {
        size_t transfer_bytes;
        if (!mul(plan->physical_layout.stride_bytes, ph, &transfer_bytes)) {
            return ESP_ERR_INVALID_SIZE;
        }
        if (transfer_bytes % length_alignment) {
            return ESP_ERR_NOT_SUPPORTED;
        }
    }
    if (plan->output.tight_stride && plan->physical_layout.stride_bytes != (size_t)pw * bpp) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (pw % plan->output.w_align || ph % plan->output.h_align) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if ((caps.max_stride_pixels && (plan->logical_layout.stride_bytes / bpp > caps.max_stride_pixels || plan->physical_layout.stride_bytes / bpp > caps.max_stride_pixels)) ||
            (caps.max_height && (backend_config->height > caps.max_height || ph > caps.max_height))) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    size_t logical = 0;
    if (kind == ESP_DISPLAY_PRESENT_ENGINE_REGION) {
        plan->topology = ESP_DISPLAY_PRESENT_TOPOLOGY_REGION;
    } else if (partial) {
        plan->topology = ESP_DISPLAY_PRESENT_TOPOLOGY_TILE_TO_PHYSICAL;
    } else if (backend_config->rotation != ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0 || backend_config->draw_order != backend_config->panel_order) {
        plan->topology = ESP_DISPLAY_PRESENT_TOPOLOGY_LOGICAL_TO_PHYSICAL;
        logical = config->resources.logical_count ? config->resources.logical_count : 1;
    } else {
        plan->topology = ESP_DISPLAY_PRESENT_TOPOLOGY_DIRECT_SURFACE;
    }
    if ((!logical && config->resources.logical_count) || logical > backend_config->max_frames) {
        return ESP_ERR_INVALID_ARG;
    }
    if (logical > 1) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (kind == ESP_DISPLAY_PRESENT_ENGINE_REGION && backend_config->tile_rows < (quarter ? plan->output.w_align : plan->output.h_align)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    esp_display_present_layout_t tile = plan->logical_layout;
    tile.height = partial ? backend_config->tile_rows : 0;
    if (!pool(&plan->tiles, tiles, tile, alignment, config->resources.tile_memory, false) ||
            !pool(&plan->images, images, plan->physical_layout, alignment, config->resources.image_memory, backend_config->ext_bufs != NULL) ||
            !pool(&plan->logical, logical, plan->logical_layout, alignment, config->resources.logical_memory, false)) {
        return ESP_ERR_INVALID_SIZE;
    }
    bool convert = kind == ESP_DISPLAY_PRESENT_ENGINE_REGION && (backend_config->async_copy || backend_config->rotation != ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0 || (backend_config->draw_order != backend_config->panel_order && !backend_config->in_place));
    esp_display_present_layout_t converted = tile;
    converted.byte_order = backend_config->panel_order;
    if (quarter) {
        if (!layout(tile.height, tile.width, bpp, stride_align, backend_config->format, backend_config->panel_order, &converted)) {
            return ESP_ERR_INVALID_SIZE;
        }
    }
    if (!pool(&plan->conversion, convert ? tiles : 0, converted, alignment, backend_config->tx_staging ? ESP_DISPLAY_PRESENT_MEMORY_INTERNAL_DMA : config->resources.tile_memory, false)) {
        return ESP_ERR_INVALID_SIZE;
    }
    esp_display_present_plan_memory_t tx_memory = transport_memory;
    if ((tx_memory == ESP_DISPLAY_PRESENT_MEMORY_INTERNAL_DMA && !plan->output.internal_dma) || (tx_memory == ESP_DISPLAY_PRESENT_MEMORY_EXTERNAL && !plan->output.ext_mem)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    plan->tx_capacity = config->resources.tx_capacity ? config->resources.tx_capacity : kind == ESP_DISPLAY_PRESENT_ENGINE_REGION ? tiles : 1;
    if (!config->resources.tx_capacity && plan->output.max_inflight && plan->tx_capacity > plan->output.max_inflight) {
        plan->tx_capacity = plan->output.max_inflight;
    }
    if (!plan->tx_capacity || plan->tx_capacity > PRESENT_MAX_TILE_BUFFERS || (kind != ESP_DISPLAY_PRESENT_ENGINE_REGION && plan->tx_capacity != 1) ||
            (plan->output.max_inflight && plan->tx_capacity > plan->output.max_inflight)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    plan->record_capacity = backend_config->max_frames;
    esp_display_present_plan_pool_t *pools[] = {&plan->tiles, &plan->images, &plan->logical, &plan->conversion};
    for (size_t i = 0; i < 4; i++) {
        if (!add(plan->pixel_bytes, pools[i]->total_bytes, &plan->pixel_bytes)) {
            return ESP_ERR_INVALID_SIZE;
        }

    }
    size_t pixels;
    if (!mul(backend_config->width, backend_config->height, &pixels)) {
        return ESP_ERR_INVALID_SIZE;
    }
    plan->coverage_bytes = images ? pixels / 8 + (pixels % 8 != 0) : 0;
    if (config->pixel_budget_bytes && plan->pixel_bytes > config->pixel_budget_bytes) {
        return ESP_ERR_NO_MEM;
    }
    if (kind == ESP_DISPLAY_PRESENT_ENGINE_FB) {
        backend_config->tile_count = tiles;
        if (backend_config->ext_bufs && images != backend_config->buffer_count) {
            return ESP_ERR_INVALID_ARG;
        }
        backend_config->buffer_count = images;
    } else {
        backend_config->buffer_count = tiles;
        if (kind == ESP_DISPLAY_PRESENT_ENGINE_TE) {
            backend_config->compose_count = images;
        }
    }
    if (backend_config->ext_bufs) {
        bool found = backend_config->scan_buf == NULL;
        if (backend_config->ext_bytes < plan->images.bytes_each) {
            return ESP_ERR_INVALID_ARG;
        }
        for (size_t i = 0; i < images; i++) {
            uintptr_t base = (uintptr_t)backend_config->ext_bufs[i];
            if (!base || base % alignment || base > UINTPTR_MAX - backend_config->ext_bytes) {
                return ESP_ERR_INVALID_ARG;
            }
            if (backend_config->ext_bufs[i] == backend_config->scan_buf) {
                found = true;
            }
            for (size_t j = 0; j < i; j++) {
                uintptr_t other = (uintptr_t)backend_config->ext_bufs[j];
                if (base < other + backend_config->ext_bytes && other < base + backend_config->ext_bytes) {
                    return ESP_ERR_INVALID_ARG;
                }
            }
        }
        if (!found) {
            return ESP_ERR_INVALID_ARG;
        }
    }
    /* An injected gate and the native timing model would both constrain one
     * submission. Reject the ambiguous configuration instead of stacking them. */
    if (kind == ESP_DISPLAY_PRESENT_ENGINE_TE && backend_config->te_gate.admit && backend_config->te_timing.period_us) {
        return ESP_ERR_INVALID_ARG;
    }
    if (kind == ESP_DISPLAY_PRESENT_ENGINE_TE && !backend_config->te_gate.admit && backend_config->te_timing.period_us &&
            esp_display_present_te_window(&backend_config->te_timing, backend_config->te_budget_us, 0, 0, false).admission == ESP_DISPLAY_PRESENT_TE_UNSUPPORTED) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (kind == ESP_DISPLAY_PRESENT_ENGINE_TE && !backend_config->te_gate.admit && !backend_config->te_timing.period_us && (!backend_config->te_budget_us || backend_config->te_budget_us >= backend_config->te_window_us)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (plan->physical_layout.stride_bytes != (size_t)pw * bpp || plan->logical_layout.stride_bytes != (size_t)backend_config->width * bpp || alignment != PRESENT_BUFFER_ALIGNMENT) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    backend_config->tile_in_psram = config->resources.tile_memory == ESP_DISPLAY_PRESENT_MEMORY_EXTERNAL;
    backend_config->image_in_psram = config->resources.image_memory == ESP_DISPLAY_PRESENT_MEMORY_EXTERNAL;
    backend_config->logical_in_psram = config->resources.logical_memory == ESP_DISPLAY_PRESENT_MEMORY_EXTERNAL;
    backend_config->transfer_capacity = plan->tx_capacity;
    plan->normalized.output_caps = plan->output;
    plan->metadata_bytes = present_presenter_memory(&plan->normalized) +
                           (kind == ESP_DISPLAY_PRESENT_ENGINE_REGION ? present_region_memory(backend_config) : kind == ESP_DISPLAY_PRESENT_ENGINE_TE ? present_te_memory(backend_config) : present_fb_memory(backend_config));
    plan->stack_bytes = backend_config->async_copy ? 4096 : 0;
    if (plan->normalized.background_service && !add(plan->stack_bytes,
                                                    plan->normalized.worker_stack_bytes ? plan->normalized.worker_stack_bytes : 4096, &plan->stack_bytes)) {
        return ESP_ERR_INVALID_SIZE;
    }
    /* Caller reservation only (config->extra_bytes). Conversion-pool bytes
     * are already part of the peak pixel budget above; nothing is reserved
     * in the heap by planning. See present_plan.h for the three quantities. */
    plan->reserved_bytes = config->extra_bytes;
    if (!add(plan->pixel_bytes, plan->coverage_bytes, &plan->required_bytes) ||
            !add(plan->required_bytes, plan->metadata_bytes, &plan->required_bytes) ||
            !add(plan->required_bytes, plan->stack_bytes, &plan->required_bytes) ||
            !add(plan->required_bytes, plan->reserved_bytes, &plan->required_bytes)) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (config->total_budget_bytes && plan->required_bytes > config->total_budget_bytes) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
esp_err_t esp_display_present_plan_resolve(const esp_display_present_plan_config_t *config, esp_display_present_resolved_plan_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    esp_display_present_resolved_plan_t plan = {0};
    esp_err_t e = resolve(config, &plan);
    if (e == ESP_OK) {
        *out = plan;
    }
    return e;
}
esp_err_t esp_display_present_plan_render_area(const esp_display_present_resolved_plan_t *plan, const esp_display_present_rect_t *dirty, esp_display_present_rect_t *render, esp_display_present_rect_t *physical)
{
    if (!plan || !dirty || !render || !physical || !dirty->width || !dirty->height ||
            dirty->x > plan->logical_layout.width || dirty->y > plan->logical_layout.height ||
            dirty->width > plan->logical_layout.width - dirty->x || dirty->height > plan->logical_layout.height - dirty->y ||
            !plan->output.x_alignment || !plan->output.y_alignment || !plan->output.w_align || !plan->output.h_align) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_display_present_rect_t a;
    esp_err_t err = esp_display_present_rotate_rect(plan->logical_layout.width, plan->logical_layout.height, plan->normalized.backend.rotation, dirty, &a);
    if (err != ESP_OK) {
        return err;
    }
    if (plan->normalized.backend.render_contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL) {
        a = (esp_display_present_rect_t) {
            0, 0, plan->physical_layout.width, plan->physical_layout.height
        };
    } else if (plan->images.count) {
        /* Full-frame output alignment is already satisfied by the candidate;
         * do not force a PARTIAL producer to repaint that entire image. */
        *physical = a;
        *render = *dirty;
        return ESP_OK;
    } else {
        size_t x = a.x - a.x % plan->output.x_alignment, y = a.y - a.y % plan->output.y_alignment;
        size_t w, h, wa, ha;
        if (!lcm(plan->output.x_alignment, plan->output.w_align, &wa) ||
                !lcm(plan->output.y_alignment, plan->output.h_align, &ha) ||
                !align_up((size_t)a.x + a.width - x, wa, &w) || !align_up((size_t)a.y + a.height - y, ha, &h)) {
            return ESP_ERR_INVALID_SIZE;
        }
        /* A shifted aligned origin can run past the edge even when the whole
         * axis is legal. Conservatively expand to that full axis, never clip. */
        if (w > plan->physical_layout.width - x) {
            x = 0;
            w = plan->physical_layout.width;
        }
        if (h > plan->physical_layout.height - y) {
            y = 0;
            h = plan->physical_layout.height;
        }
        if (w % wa || h % ha) {
            return ESP_ERR_NOT_SUPPORTED;
        }
        a = (esp_display_present_rect_t) {
            x, y, w, h
        };
    }
    *physical = a;
    esp_display_present_engine_rotation_t inverse = (4 - plan->normalized.backend.rotation) % 4;
    return esp_display_present_rotate_rect(plan->physical_layout.width, plan->physical_layout.height, inverse, &a, render);
}
esp_err_t present_engine_create_with_plan(const esp_display_present_plan_config_t *config,
                                          esp_display_present_resolved_plan_t *out_plan,
                                          esp_display_present_engine_t **out)
{
    if (!out || !out_plan) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = NULL;
    esp_display_present_resolved_plan_t plan;
    esp_err_t e = esp_display_present_plan_resolve(config, &plan);
    if (e != ESP_OK) {
        return e;
    }
    *out_plan = plan;
    if (plan.pending_runtime) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    /* DEFAULT prevents applying the named preset a second time. */
    plan.normalized.mode = ESP_DISPLAY_PRESENT_MODE_DEFAULT;
    return esp_display_present_engine_create(&plan.normalized, out);
}

esp_err_t esp_display_present_engine_create_planned(const esp_display_present_plan_config_t *config, esp_display_present_engine_t **out)
{
    esp_display_present_resolved_plan_t plan;
    return present_engine_create_with_plan(config, &plan, out);
}
