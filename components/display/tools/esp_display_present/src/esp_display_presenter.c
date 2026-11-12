/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0 */
#include "core/present_producer.h"
#include "core/present_control.h"
#include "core/present_session.h"
#include "esp_display_present.h"
#include "config/present_geometry.h"
#include "resources/present_resources.h"
#include "config/present_mode.h"
#include "core/present_engine.h"
#include "platform/present_os.h"
#include <limits.h>
#include <string.h>
#ifndef PRESENT_HOST
#include "esp_log.h"
#endif

/* Public ID mapping only. The engine remains the authority for the held
 * native lease's capacity, layout and ownership. */
typedef struct {
    esp_display_present_lease_id_t native_id;
    void *pixels;
    uint32_t public_id;
} present_lease_identity_t;

struct esp_display_presenter {
    present_resources_t *resources;
    present_mutex_t *gate;
    present_lock_t events;
    present_task_t producer;
    esp_display_presenter_config_t config;
    esp_display_present_backend_caps_t caps;
    esp_display_presenter_frame_status_t timeline;
    esp_display_presenter_state_t state;
    present_lease_identity_t lease;
    esp_display_present_render_alignment_t render_alignment;
    uint32_t next_lease;
    bool active, full_next, lvgl_bound, cancel_pending;
    uint64_t vsyncs, confirm_after, tracked_id;
    esp_display_present_engine_receipt_t tracked;
    esp_err_t fault_error;
};

static esp_display_present_engine_t *core(esp_display_presenter_t *presenter)
{
    return present_resources_presenter(presenter->resources);
}
static bool PRESENT_ISR_ATTR vsync(void *user, uint64_t timestamp, bool isr)
{
    (void)timestamp;
    esp_display_presenter_t *presenter = user;
    present_lock_enter(&presenter->events, isr);
    if (presenter->vsyncs != UINT64_MAX) {
        ++presenter->vsyncs;
    }
    present_lock_exit(&presenter->events, isr);
    return false;
}
static uint64_t edges(esp_display_presenter_t *presenter)
{
    present_lock_enter(&presenter->events, false);
    uint64_t n = presenter->vsyncs;
    present_lock_exit(&presenter->events, false);
    return n;
}
/* Classification contract: TRANSFER_TIMEOUT is reserved for an original
 * ESP_ERR_TIMEOUT. Any other accepted-operation failure is TRANSFER_FAILED
 * and keeps its original error in fault_error for diagnosis. Producer
 * protocol is only latched by the facade's own protocol checks. */
static esp_display_present_fault_reason_t fault_reason_for(esp_err_t error)
{
    return error == ESP_ERR_TIMEOUT ? ESP_DISPLAY_PRESENT_FAULT_TRANSFER_TIMEOUT
           : ESP_DISPLAY_PRESENT_FAULT_TRANSFER_FAILED;
}

static void fault(esp_display_presenter_t *presenter, esp_display_present_fault_reason_t reason, esp_err_t error)
{
    if (presenter->timeline.fault_reason == ESP_DISPLAY_PRESENT_FAULT_NONE) {
        presenter->timeline.fault_reason = reason;
        presenter->fault_error = error;
    }
    if (presenter->state == ESP_DISPLAY_PRESENTER_STATE_OPEN) {
        presenter->state = ESP_DISPLAY_PRESENTER_STATE_FAULTED;
    }
}
static uint32_t budget(esp_display_presenter_t *presenter)
{
    return presenter->config.pipeline_acquire_timeout_ms ? presenter->config.pipeline_acquire_timeout_ms : 1000;
}
static uint32_t remaining_budget(esp_display_presenter_t *presenter, uint64_t start)
{
    uint32_t timeout = budget(presenter);
    uint64_t elapsed = present_now_ms() - start;
    return timeout == UINT32_MAX ? timeout : elapsed >= timeout ? 0 : timeout - (uint32_t)elapsed;
}
static esp_err_t producer(esp_display_presenter_t *presenter)
{
    if (!present_same_task(presenter->producer, present_current_task()) || presenter->lvgl_bound) {
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}
/* Logical cancellation is immediate. Only true backend drain releases pixels.
 * Called under the facade gate; a pending cancel prevents all new producers. */
static esp_err_t finish_cancel(esp_display_presenter_t *presenter, uint32_t timeout)
{
    if (!presenter->cancel_pending) {
        return ESP_OK;
    }
    esp_err_t error = present_same_task(presenter->producer, present_current_task()) ?
                      present_presenter_cancel_budget(core(presenter), timeout) :
                      present_presenter_control_finish_cancel(core(presenter), timeout);
    if (error == ESP_OK) {
        presenter->cancel_pending = false;
    } else if (error != ESP_ERR_TIMEOUT) {
        fault(presenter, ESP_DISPLAY_PRESENT_FAULT_PRODUCER_PROTOCOL, error);
    }
    return error;
}
static esp_err_t refresh_caps(esp_display_presenter_t *presenter)
{
    return esp_display_present_engine_get_caps(core(presenter), &presenter->caps);
}
/* Observation never grants buffer ownership. A failed frame cannot advance a
 * successful completion watermark. Untracked LVGL commits share core numbering. */
static esp_err_t observe_core_timeline(esp_display_presenter_t *presenter)
{
    esp_display_present_engine_status_t status = {0};
    bool valid = false;
    esp_err_t error = present_presenter_control_get_status_snapshot(core(presenter), &status, &valid);
    if (valid) {
        presenter->timeline.last_submitted_frame = status.committed_frames;
    }
    if (error != ESP_OK || status.first_error != ESP_OK) {
        esp_err_t failure = status.first_error != ESP_OK ? status.first_error : error;
        fault(presenter, fault_reason_for(failure), failure);
    } else {
        presenter->timeline.completed_transfer_frame = status.completed_frames;
        /* region present follows transfer; only tracked confirmation uses VSYNC. */
        presenter->timeline.completed_present_frame = status.completed_frames;
    }
    return error;
}

static esp_err_t collect_tracking_receipt(esp_display_presenter_t *presenter)
{
    if (!presenter->tracked.value) {
        return ESP_OK;
    }
    esp_display_present_frame_status_t observed;
    esp_err_t error = present_presenter_control_query_frame(core(presenter), presenter->tracked, &observed);
    if (error != ESP_OK || !observed.terminal) {
        return error;
    }
    if (observed.result == ESP_OK && observed.present_confirmed && presenter->tracked_id && !presenter->confirm_after &&
            presenter->timeline.fault_reason == ESP_DISPLAY_PRESENT_FAULT_NONE) {
        uint64_t n = edges(presenter);
        presenter->confirm_after = n <= UINT64_MAX - 2 ? n + 2 : UINT64_MAX;
    } else if (observed.result != ESP_OK) {
        fault(presenter, fault_reason_for(observed.result), observed.result);
        presenter->tracked_id = presenter->confirm_after = 0;
    }
    error = present_presenter_control_release(core(presenter), presenter->tracked);
    if (error == ESP_OK) {
        presenter->tracked.value = 0;
    }
    return error;
}

static void publish_tracking_watermark(esp_display_presenter_t *presenter)
{
    if (presenter->timeline.fault_reason == ESP_DISPLAY_PRESENT_FAULT_NONE && !presenter->tracked.value &&
            presenter->tracked_id && presenter->confirm_after && edges(presenter) >= presenter->confirm_after) {
        presenter->timeline.confirmed_present_frame = presenter->tracked_id;
        presenter->tracked_id = presenter->confirm_after = 0;
    }
}

static esp_err_t poll(esp_display_presenter_t *presenter)
{
    if (!core(presenter)) {
        return ESP_OK;
    }
    esp_err_t error = observe_core_timeline(presenter);
    esp_err_t receipt_error = collect_tracking_receipt(presenter);
    if (receipt_error != ESP_OK) {
        return receipt_error;
    }
    publish_tracking_watermark(presenter);
    return error;
}

esp_err_t esp_display_present_get_required_frame_buffer_count(const esp_display_present_target_config_t *t, uint8_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = 0;
    present_mode_resolution_t mode;
    esp_err_t error = present_mode_resolve_target(t, &mode);
    if (error == ESP_OK) {
        *out = mode.frame_buffers;
    }
    return error;
}

esp_err_t esp_display_presenter_create(const esp_display_presenter_config_t *cfg, esp_display_presenter_t **out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = NULL;
    if (!cfg) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_display_presenter_t *presenter = present_alloc(sizeof(*presenter));
    if (!presenter) {
        return ESP_ERR_NO_MEM;
    }
    presenter->gate = present_mutex_create();
    if (!presenter->gate || !present_lock_init(&presenter->events)) {
        present_mutex_destroy(presenter->gate);
        present_free(presenter);
        return ESP_ERR_NO_MEM;
    }
    presenter->config = *cfg;
    presenter->producer = present_current_task();
    presenter->full_next = true;
    presenter->next_lease = 1;
    presenter->timeline.generation = 1;
    bool bounce = false;
#ifdef CONFIG_PV2_LEGACY_RGB_BOUNCE_CONFIRMED
    bounce = CONFIG_PV2_LEGACY_RGB_BOUNCE_CONFIRMED;
#endif
    esp_err_t error = present_resources_create(cfg, bounce, vsync, presenter, &presenter->resources);
    if (error == ESP_OK) {
        /* These read-only queries cannot fail for a successfully created core. */
        error = refresh_caps(presenter);
    }
    if (error != ESP_OK) {
#ifndef PRESENT_HOST
        ESP_LOGE("present_v2", "create failed=%s size=%ux%u format=%d input=%d damage=%u mode=%d rows=%u tiles=%u fb=%u",
                 esp_err_to_name(error), (unsigned)cfg->width, (unsigned)cfg->height,
                 cfg->pixel_format, cfg->target.hw.input_pixel_format, (unsigned)cfg->max_damage_areas,
                 cfg->target.fb.mode, (unsigned)cfg->target.drawbuf.lines,
                 (unsigned)cfg->target.drawbuf.buffers, (unsigned)cfg->target.fb.frame_buffer_count);
#endif
        if (presenter->resources) {
            (void)present_resources_destroy(presenter->resources, UINT32_MAX);
        }
        present_lock_destroy(&presenter->events);
        present_mutex_destroy(presenter->gate);
        present_free(presenter);
        return error;
    }
    presenter->timeline.completion = presenter->caps.supports_present_confirmation ? ESP_DISPLAY_PRESENT_COMPLETION_FRAMEBUFFER : ESP_DISPLAY_PRESENT_COMPLETION_TRANSFER;
    presenter->timeline.tracks_transfer = !presenter->caps.supports_present_confirmation;
    presenter->timeline.accepting_callbacks = true;
    *out = presenter;
    return ESP_OK;
}

static bool legacy_framebuffer(const esp_display_presenter_t *presenter)
{
    return presenter->config.target.hw.panel_type == ESP_DISPLAY_PRESENT_PANEL_RGB ||
           presenter->config.target.hw.panel_type == ESP_DISPLAY_PRESENT_PANEL_MIPI_DSI;
}
static bool previous_readable(const esp_display_presenter_t *presenter)
{
    return legacy_framebuffer(presenter) && presenter->config.target.fb.mode == ESP_DISPLAY_PRESENT_MODE_DOUBLE_DIRECT &&
           !presenter->config.target.hw.rotation && !presenter->config.target.hw.swap_bytes;
}
esp_err_t esp_display_presenter_get_caps(const esp_display_presenter_t *input, esp_display_presenter_caps_t *out)
{
    if (!input || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_display_presenter_t *presenter = (esp_display_presenter_t *)input;
    (void)present_mutex_take(presenter->gate, UINT32_MAX);
    const esp_display_present_backend_caps_t *caps = &presenter->caps;
    *out = (esp_display_presenter_caps_t) {
        .contract = caps->contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION ? ESP_DISPLAY_PRESENT_CONTRACT_PARTITION : caps->contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT ? ESP_DISPLAY_PRESENT_CONTRACT_DIRECT : ESP_DISPLAY_PRESENT_CONTRACT_FULL,
        .supports_coverage_areas = legacy_framebuffer(presenter) && caps->contract != ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL,
        .surface_retains_content = false, /* Scanout leases never retain a writable front surface. */
        .previous_surface_readable = previous_readable(presenter),
        .width = caps->draw_layout.width,
        .height = caps->draw_layout.height,
        .stride_bytes = caps->draw_layout.stride_bytes,
        .pixel_format = presenter->config.pixel_format,
        .drawbuf_bytes = caps->contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION ? caps->drawbuf_capacity_bytes : 0,
        .max_damage_areas = presenter->config.max_damage_areas ? presenter->config.max_damage_areas : caps->max_damage_areas
    };
    present_mutex_give(presenter->gate);
    return ESP_OK;
}
esp_err_t esp_display_presenter_get_drawbuf(const esp_display_presenter_t *input, esp_display_presenter_drawbuf_t *out)
{
    if (!input || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    esp_display_presenter_t *presenter = (esp_display_presenter_t *)input;
    (void)present_mutex_take(presenter->gate, UINT32_MAX);
    esp_err_t error = presenter->caps.contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION ? ESP_OK : ESP_ERR_INVALID_STATE;
    if (error == ESP_OK) {
        void *buffers[3] = {0};
        size_t count = 0, bytes = 0;
        error = esp_display_present_engine_get_draw_buffers(core(presenter), buffers, 3, &count, &bytes);
        if (error == ESP_OK) {
            *out = (esp_display_presenter_drawbuf_t) {
                .count = count > 2 ? 2 : count,
                .lines = presenter->caps.max_tile_rows,
                .bytes = bytes,
            };
            for (size_t i = 0; i < out->count; ++i) {
                out->buffers[i] = buffers[i];
            }
        }
    }
    present_mutex_give(presenter->gate);
    return error;
}
esp_err_t esp_display_presenter_get_buffer_alignment(const esp_display_presenter_t *input, size_t *out)
{
    if (!input || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_display_presenter_t *presenter = (esp_display_presenter_t *)input;
    (void)present_mutex_take(presenter->gate, UINT32_MAX);
    *out = presenter->caps.buffer_alignment;
    present_mutex_give(presenter->gate);
    return ESP_OK;
}
esp_err_t esp_display_presenter_get_frame_status(esp_display_presenter_t *presenter, esp_display_presenter_frame_status_t *out)
{
    if (!presenter || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)present_mutex_take(presenter->gate, UINT32_MAX);
    esp_err_t error = poll(presenter);
    *out = presenter->timeline;
    present_mutex_give(presenter->gate);
    return error;
}
esp_err_t esp_display_presenter_get_state(const esp_display_presenter_t *input, esp_display_presenter_state_t *out)
{
    esp_display_presenter_t *presenter = (esp_display_presenter_t *)input;
    if (!presenter || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)present_mutex_take(presenter->gate, UINT32_MAX);
    (void)poll(presenter);
    *out = presenter->state;
    present_mutex_give(presenter->gate);
    return ESP_OK;
}
esp_err_t esp_display_presenter_get_fault_reason(const esp_display_presenter_t *input, esp_display_present_fault_reason_t *out)
{
    esp_display_presenter_t *presenter = (esp_display_presenter_t *)input;
    if (!presenter || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)present_mutex_take(presenter->gate, UINT32_MAX);
    (void)poll(presenter);
    *out = presenter->timeline.fault_reason;
    present_mutex_give(presenter->gate);
    return ESP_OK;
}
esp_err_t esp_display_presenter_get_fault_error(const esp_display_presenter_t *input, esp_err_t *out)
{
    esp_display_presenter_t *presenter = (esp_display_presenter_t *)input;
    if (!presenter || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)present_mutex_take(presenter->gate, UINT32_MAX);
    (void)poll(presenter);
    *out = presenter->fault_error;
    present_mutex_give(presenter->gate);
    return ESP_OK;
}
esp_err_t esp_display_presenter_rebind_producer(esp_display_presenter_t *presenter)
{
    if (!presenter) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)present_mutex_take(presenter->gate, UINT32_MAX);
    esp_err_t error = presenter->active || presenter->lvgl_bound ? ESP_ERR_INVALID_STATE :
                      finish_cancel(presenter, budget(presenter));
    if (error == ESP_OK) {
        error = poll(presenter);
    }
    if (error == ESP_OK) {
        error = presenter->active || presenter->lvgl_bound || presenter->state != ESP_DISPLAY_PRESENTER_STATE_OPEN ? ESP_ERR_INVALID_STATE : esp_display_present_engine_rebind_producer(core(presenter), budget(presenter));
    }
    if (error == ESP_OK) {
        presenter->producer = present_current_task();
    }
    present_mutex_give(presenter->gate);
    return error;
}
static esp_err_t rectangle(const esp_display_present_area_t *a, uint32_t width, uint32_t height, esp_display_present_rect_t *r)
{
    if (!esp_display_present_geometry_area_is_valid(a, width, height)) {
        return ESP_ERR_INVALID_ARG;
    }
    *r = (esp_display_present_rect_t) {
        a->x1, a->y1, a->x2 - a->x1 + 1, a->y2 - a->y1 + 1
    };
    return ESP_OK;
}
/* V1 validates damage even when the negotiated raster is full. Full raster
 * is represented by full=true, count=0; the flag never supplies pixel coverage. */
static esp_err_t plan_frame(esp_display_presenter_t *presenter,
                            const esp_display_present_surface_request_t *input,
                            esp_display_present_rect_t *areas, bool has_output, size_t capacity,
                            size_t *count, bool *full)
{
    const esp_display_present_surface_request_t empty = {0};
    const esp_display_present_surface_request_t *request = input ? input : &empty;
    size_t n = request->dirty_area_count;
    size_t max_damage = presenter->config.max_damage_areas ? presenter->config.max_damage_areas : 32;
    uint32_t width = presenter->caps.draw_layout.width, height = presenter->caps.draw_layout.height;
    if ((!request->dirty_areas) != (n == 0) || n > max_damage || n > capacity || (n && !has_output)) {
        return ESP_ERR_INVALID_ARG;
    }
    for (size_t i = 0; i < n; ++i) {
        esp_display_present_rect_t validated;
        esp_err_t error = rectangle(&request->dirty_areas[i], width, height, &validated);
        if (error == ESP_OK && i < 32) {
            areas[i] = validated;
        }
        if (error != ESP_OK) {
            return error;
        }
    }
    if (request->has_scroll) {
        esp_display_present_rect_t viewport;
        if (rectangle(&request->scroll_viewport, width, height, &viewport) != ESP_OK ||
                (!request->scroll_dx && !request->scroll_dy)) {
            return ESP_ERR_INVALID_ARG;
        }
        if (presenter->caps.contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION) {
            return ESP_ERR_NOT_SUPPORTED;
        }
    }
    bool whole_dirty = n == 1 && areas[0].x == 0 && areas[0].y == 0 &&
                       areas[0].width == width && areas[0].height == height;
    /* DIRECT inherits a coherent previous logical image before acquire.
     * FULL, transforms, scroll and persistent single-FB keep V1's full raster. */
    /* More producer damage than the core's bounded hint list requests a
     * genuine full raster; coverage still comes from successful writes. */
    *full = presenter->full_next || !n || n > 32 || whole_dirty ||
            presenter->caps.contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL ||
            (presenter->caps.contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT && (!previous_readable(presenter) || request->has_scroll));
    *count = *full ? 0 : n;
    const esp_display_present_size_t logical_size = {width, height};
    for (size_t i = 0; i < *count; ++i) {
        esp_display_present_area_t aligned;
        esp_err_t error = esp_display_present_geometry_align_area(
                              logical_size, &request->dirty_areas[i], &request->render_alignment, &aligned);
        if (error != ESP_OK) {
            return error;
        }
        esp_display_present_rect_t r;
        error = rectangle(&aligned, width, height, &r);
        if (error == ESP_OK) {
            error = esp_display_present_engine_resolve_render_area(core(presenter), &r, &areas[i]);
        }
        if (error != ESP_OK) {
            return error;
        }
    }
    return ESP_OK;
}
esp_err_t esp_display_presenter_begin_next_frame(esp_display_presenter_t *presenter, const esp_display_present_surface_request_t *request,
                                                 esp_display_present_area_t *out, size_t capacity, size_t *count, bool *full)
{
    if (count) {
        *count = 0;
    }
    if (full) {
        *full = false;
    }
    if (!presenter || !count || !full) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)present_mutex_take(presenter->gate, UINT32_MAX);
    esp_err_t error = producer(presenter);
    if (error == ESP_OK && (presenter->state != ESP_DISPLAY_PRESENTER_STATE_OPEN || presenter->active)) {
        error = ESP_ERR_INVALID_STATE;
    }
    esp_display_present_rect_t areas[32];
    size_t n = 0;
    bool all = false;
    if (error == ESP_OK) {
        error = plan_frame(presenter, request, areas, out != NULL, capacity, &n, &all);
    }
    if (error == ESP_OK) {
        error = poll(presenter);
    }
    if (error == ESP_OK && presenter->state != ESP_DISPLAY_PRESENTER_STATE_OPEN) {
        error = ESP_ERR_INVALID_STATE;
    }
    uint64_t start = present_now_ms();
    if (error == ESP_OK) {
        error = finish_cancel(presenter, remaining_budget(presenter, start));
    }
    while (error == ESP_OK && presenter->tracked_id) {
        if (present_expired(start, budget(presenter))) {
            error = ESP_ERR_TIMEOUT;
            break;
        }
        /* Completion and cancellation are independent control ingress. The
         * caller must exclude deletion for the entire API call, not just while
         * the mutex is held (same rule as V1 status/control APIs). */
        present_mutex_give(presenter->gate);
        present_pause();
        (void)present_mutex_take(presenter->gate, UINT32_MAX);
        error = producer(presenter);
        if (error == ESP_OK && presenter->state != ESP_DISPLAY_PRESENTER_STATE_OPEN) {
            error = ESP_ERR_INVALID_STATE;
        }
        if (error == ESP_OK) {
            error = poll(presenter);
        }
    }
    if (error == ESP_OK) {
        error = present_resources_prepare(presenter->resources, remaining_budget(presenter, start));
    }
    esp_display_present_frame_request_t req = {
        .damage = areas,
        .damage_count = n,
        .full_redraw = all,
    };
    if (error == ESP_OK) {
        error = present_presenter_begin_budget(core(presenter), &req, remaining_budget(presenter, start));
    }
    if (error == ESP_OK) {
        presenter->active = true;
        presenter->render_alignment = request ? request->render_alignment : (esp_display_present_render_alignment_t) {
            0
        };
        for (size_t i = 0; i < n; ++i) {
            out[i] = (esp_display_present_area_t) {
                areas[i].x, areas[i].y, areas[i].x + areas[i].width - 1, areas[i].y + areas[i].height - 1
            };
        }
        *count = n;
        *full = all;
    }
    present_mutex_give(presenter->gate);
    return error;
}
static esp_err_t resolve_rows(void *user, uint32_t id, size_t stride, size_t requested, size_t *out)
{
    if (!user || !out || requested > UINT32_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = 0;
    esp_display_presenter_t *presenter = user;
    (void)present_mutex_take(presenter->gate, UINT32_MAX);
    esp_err_t error = producer(presenter);
    if (error == ESP_OK && presenter->state != ESP_DISPLAY_PRESENTER_STATE_OPEN) {
        error = ESP_ERR_INVALID_STATE;
    }
    uint32_t rows = 0;
    if (error == ESP_OK) {
        error = id && id == presenter->lease.public_id ? present_presenter_resolve_lease_rows(core(presenter), presenter->lease.native_id, stride, requested, &rows) : ESP_ERR_INVALID_STATE;
    }
    if (error == ESP_OK) {
        uint16_t alignment = presenter->render_alignment.height_pixels;
        if (presenter->caps.contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION && rows < requested && alignment > 1) {
            rows -= rows % alignment;
        }
        if (!rows || rows > UINT16_MAX) {
            error = ESP_ERR_INVALID_SIZE;
        }
    }
    if (error == ESP_OK) {
        *out = rows;
    }
    present_mutex_give(presenter->gate);
    return error;
}
esp_err_t esp_display_presenter_acquire_buffer(esp_display_presenter_t *presenter, esp_display_presenter_buffer_t *out)
{
    if (out) {
        memset(out, 0, sizeof(*out));
    }
    if (!presenter || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)present_mutex_take(presenter->gate, UINT32_MAX);
    esp_err_t error = producer(presenter);
    if (error == ESP_OK && (!presenter->active || presenter->lease.public_id || !presenter->next_lease || presenter->state != ESP_DISPLAY_PRESENTER_STATE_OPEN)) {
        error = ESP_ERR_INVALID_STATE;
    }
    esp_display_present_lease_t lease = {0};
    if (error == ESP_OK) {
        error = esp_display_present_engine_acquire_buffer(core(presenter), &lease);
    }
    if (error == ESP_OK) {
        presenter->lease = (present_lease_identity_t) {
            .native_id = lease.id,
            .pixels = lease.surface.pixels,
            .public_id = presenter->next_lease++
        };
        *out = (esp_display_presenter_buffer_t) {
            .surface = {lease.surface.pixels, lease.surface.layout.stride_bytes,
                        lease.surface.layout.width, lease.surface.layout.height, presenter->config.pixel_format
                       },
            .capacity_bytes = lease.surface.capacity_bytes,
            .lease_id = presenter->lease.public_id,
            .resolve_rows = resolve_rows,
            .resolve_rows_ctx = presenter
        };
    }
    present_mutex_give(presenter->gate);
    return error;
}
esp_err_t esp_display_presenter_submit_buffer(esp_display_presenter_t *presenter, const esp_display_presenter_buffer_t *buffer,
                                              const esp_display_present_area_t *area, size_t stride)
{
    if (!presenter || !buffer || !area) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)present_mutex_take(presenter->gate, UINT32_MAX);
    esp_err_t error = producer(presenter);
    if (error == ESP_OK && (!presenter->lease.public_id || buffer->lease_id != presenter->lease.public_id || buffer->surface.pixels != presenter->lease.pixels || presenter->state != ESP_DISPLAY_PRESENTER_STATE_OPEN)) {
        error = ESP_ERR_INVALID_STATE;
    }
    if (error == ESP_OK && !esp_display_present_geometry_area_is_valid(area, presenter->caps.draw_layout.width, presenter->caps.draw_layout.height)) {
        error = ESP_ERR_INVALID_ARG;
    }
    esp_display_present_engine_area_t a = {area->x1, area->y1, area->x2, area->y2};
    if (error == ESP_OK) {
        error = present_presenter_submit_lease(core(presenter), presenter->lease.native_id, buffer->surface.pixels, &a, stride);
    }
    if (error == ESP_OK) {
        memset(&presenter->lease, 0, sizeof(presenter->lease));
    }
    present_mutex_give(presenter->gate);
    return error;
}
esp_err_t esp_display_presenter_commit_frame_with_receipt(esp_display_presenter_t *presenter, const esp_display_presenter_submit_t *submit,
                                                          bool tracking, esp_display_presenter_frame_receipt_t *out)
{
    if (out) {
        memset(out, 0, sizeof(*out));
    }
    if (!presenter || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)present_mutex_take(presenter->gate, UINT32_MAX);
    esp_err_t error = producer(presenter);
    if (error == ESP_OK && (!presenter->active || presenter->lease.public_id || presenter->state != ESP_DISPLAY_PRESENTER_STATE_OPEN)) {
        error = ESP_ERR_INVALID_STATE;
    }
    if (error == ESP_OK && tracking && presenter->tracked.value) {
        error = ESP_ERR_NO_MEM;
    }
    if (error == ESP_OK && tracking && !presenter->caps.supports_present_confirmation) {
        error = ESP_ERR_NOT_SUPPORTED;
    }
    esp_display_present_rect_t areas[32];
    esp_display_present_frame_coverage_t coverage = {.full_coverage = true};
    if (error == ESP_OK && submit) {
        if (submit->coverage == ESP_DISPLAY_PRESENT_COVERAGE_AREAS) {
            if (!submit->areas || !submit->area_count ||
                    submit->area_count > (presenter->config.max_damage_areas ? presenter->config.max_damage_areas : 32)) {
                error = ESP_ERR_INVALID_ARG;
            }
            for (size_t i = 0; i < submit->area_count && error == ESP_OK; ++i) {
                esp_display_present_rect_t validated;
                error = rectangle(&submit->areas[i], presenter->caps.draw_layout.width, presenter->caps.draw_layout.height, &validated);
                if (error == ESP_OK && i < 32) {
                    areas[i] = validated;
                }
            }
            coverage = (esp_display_present_frame_coverage_t) {
                submit->area_count <= 32 ? areas : NULL,
                       submit->area_count <= 32 ? submit->area_count : 0, false
            };
            /* The core derives coverage/history from successful submissions.
             * Omitting oversized, validated hints never fabricates coverage. */
        } else if (submit->coverage != ESP_DISPLAY_PRESENT_COVERAGE_FULL) {
            error = ESP_ERR_INVALID_ARG;
        }
    }
    if (error == ESP_OK) {
        error = tracking ? esp_display_present_engine_commit_with_receipt(core(presenter), &coverage, &presenter->tracked) : esp_display_present_engine_commit_frame(core(presenter), &coverage);
    }
    if (error == ESP_OK) {
        presenter->active = false;
        presenter->full_next = false;
        /* Establish the legacy ID before observing a possibly early fence. */
        (void)observe_core_timeline(presenter);
        *out = (esp_display_presenter_frame_receipt_t) {
            presenter->timeline.last_submitted_frame, presenter->timeline.generation, tracking
        };
        if (tracking) {
            presenter->tracked_id = out->frame_id;
        }
        (void)poll(presenter);
    }
    present_mutex_give(presenter->gate);
    return error;
}
esp_err_t esp_display_presenter_commit_frame(esp_display_presenter_t *presenter, const esp_display_presenter_submit_t *submit)
{
    esp_display_presenter_frame_receipt_t receipt;
    return esp_display_presenter_commit_frame_with_receipt(presenter, submit, false, &receipt);
}
esp_err_t esp_display_presenter_cancel_frame_tracking(esp_display_presenter_t *presenter, uint64_t id, uint32_t generation)
{
    if (!presenter || !id || !generation) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)present_mutex_take(presenter->gate, UINT32_MAX);
    (void)poll(presenter); /* Observe a confirmation which already arrived. */
    esp_err_t error = ESP_ERR_NOT_FOUND;
    if (presenter->tracked_id == id && presenter->timeline.generation == generation) {
        /* Explicit observation cancellation leaves the backend receipt and
         * its hardware ownership untouched. It frees the observer immediately. */
        error = presenter->tracked.value ? present_presenter_control_cancel_tracking(core(presenter), presenter->tracked) : ESP_OK;
        if (error == ESP_OK) {
            presenter->tracked.value = 0;
            presenter->tracked_id = presenter->confirm_after = 0;
        }
    }
    present_mutex_give(presenter->gate);
    return error;
}
void esp_display_presenter_cancel_frame(esp_display_presenter_t *presenter)
{
    if (!presenter) {
        return;
    }
    (void)present_mutex_take(presenter->gate, UINT32_MAX);
    if (producer(presenter) == ESP_OK && (presenter->active || presenter->cancel_pending)) {
        presenter->active = false;
        memset(&presenter->lease, 0, sizeof(presenter->lease));
        presenter->full_next = true;
        presenter->cancel_pending = true;
        (void)finish_cancel(presenter, 0);
    }
    present_mutex_give(presenter->gate);
}
esp_err_t esp_display_presenter_quiesce(esp_display_presenter_t *presenter, uint32_t timeout)
{
    if (!presenter || !timeout) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)present_mutex_take(presenter->gate, UINT32_MAX);
    esp_err_t error = presenter->active || presenter->state != ESP_DISPLAY_PRESENTER_STATE_OPEN ? ESP_ERR_INVALID_STATE : ESP_OK;
    uint64_t start = present_now_ms();
    esp_display_present_engine_status_t status;
    if (error == ESP_OK) {
        error = finish_cancel(presenter, timeout);
    }
    if (error == ESP_OK) {
        error = present_presenter_control_get_status(core(presenter), &status);
        if (error == ESP_OK && (status.frame_active || status.cancel_pending)) {
            error = ESP_ERR_INVALID_STATE;
        }
    }
    while (error == ESP_OK) {
        error = poll(presenter);
        if (error == ESP_OK && presenter->state != ESP_DISPLAY_PRESENTER_STATE_OPEN) {
            error = ESP_ERR_INVALID_STATE;
        }
        if (error != ESP_OK || presenter->timeline.completed_transfer_frame >= presenter->timeline.last_submitted_frame) {
            break;
        }
        if (present_expired(start, timeout)) {
            error = ESP_ERR_TIMEOUT;
            break;
        }
        present_pause();
    }
    present_mutex_give(presenter->gate);
    return error;
}
esp_err_t esp_display_presenter_set_rotation(esp_display_presenter_t *presenter, esp_display_present_rotation_t rotation)
{
    if (!presenter || (rotation != 0 && rotation != 90 && rotation != 180 && rotation != 270)) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)present_mutex_take(presenter->gate, UINT32_MAX);
    esp_err_t error = producer(presenter);
    if (error == ESP_OK && (presenter->active || presenter->tracked_id || presenter->tracked.value || presenter->state != ESP_DISPLAY_PRESENTER_STATE_OPEN || presenter->timeline.generation == UINT32_MAX)) {
        error = ESP_ERR_INVALID_STATE;
    }
    if (error == ESP_OK) {
        error = finish_cancel(presenter, budget(presenter));
    }
    if (error == ESP_OK && presenter->config.target.hw.rotation == rotation) {
        present_mutex_give(presenter->gate);
        return ESP_OK;
    }
    if (error == ESP_OK) {
        error = present_resources_set_rotation(presenter->resources, (esp_display_present_engine_rotation_t)(rotation / 90), budget(presenter));
    }
    if (error == ESP_OK) {
        presenter->config.target.hw.rotation = rotation;
        (void)present_resources_select_executor(presenter->resources, (esp_display_present_engine_rotation_t)(rotation / 90));
        ++presenter->timeline.generation;
        presenter->full_next = true;
        error = refresh_caps(presenter);
        if (error == ESP_OK) {
            presenter->config.width = presenter->caps.draw_layout.width;
            presenter->config.height = presenter->caps.draw_layout.height;
        }
    }
    present_mutex_give(presenter->gate);
    return error;
}
esp_err_t esp_display_presenter_map_point(const esp_display_presenter_t *input, int32_t *x, int32_t *y)
{
    if (!input || !x || !y) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_display_presenter_t *presenter = (esp_display_presenter_t *)input;
    (void)present_mutex_take(presenter->gate, UINT32_MAX);
    const esp_display_present_size_t logical_size = {
        presenter->caps.draw_layout.width, presenter->caps.draw_layout.height
    };
    const esp_display_present_point_t physical = {*x, *y};
    esp_display_present_point_t logical;
    esp_err_t error = esp_display_present_geometry_map_physical_point_to_logical(
                          presenter->config.target.hw.rotation, logical_size, &physical, &logical);
    if (error == ESP_OK) {
        *x = logical.x;
        *y = logical.y;
    }
    present_mutex_give(presenter->gate);
    return error;
}
esp_err_t esp_display_presenter_stop(esp_display_presenter_t *presenter)
{
    if (!presenter) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)present_mutex_take(presenter->gate, UINT32_MAX);
    uint64_t start = present_now_ms();
    esp_err_t error = presenter->lease.public_id || presenter->lvgl_bound ? ESP_ERR_INVALID_STATE : ESP_OK;
    if (error == ESP_OK) {
        presenter->state = ESP_DISPLAY_PRESENTER_STATE_CLOSING;
        if (presenter->active) {
            presenter->active = false;
            presenter->cancel_pending = true;
            presenter->full_next = true;
        }
        error = finish_cancel(presenter, remaining_budget(presenter, start));
    }
    if (error == ESP_OK) {
        error = present_resources_stop(presenter->resources, remaining_budget(presenter, start));
    }
    if (error == ESP_OK) {
        (void)poll(presenter);
        if (presenter->tracked.value) {
            error = present_presenter_control_release(core(presenter), presenter->tracked);
            if (error == ESP_OK) {
                presenter->tracked.value = 0;
            }
        }
        if (error == ESP_OK) {
            presenter->cancel_pending = false;
            presenter->state = ESP_DISPLAY_PRESENTER_STATE_STOPPED;
            presenter->timeline.accepting_callbacks = false;
        }
    }
    present_mutex_give(presenter->gate);
    return error == ESP_ERR_TIMEOUT ? ESP_ERR_INVALID_STATE : error;
}
esp_err_t esp_display_presenter_delete(esp_display_presenter_t *presenter)
{
    if (!presenter) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t error = esp_display_presenter_stop(presenter);
    if (error != ESP_OK) {
        return error;
    }
    (void)present_mutex_take(presenter->gate, UINT32_MAX);
    error = present_resources_destroy(presenter->resources, budget(presenter));
    present_mutex_give(presenter->gate);
    if (error == ESP_OK) {
        present_mutex_destroy(presenter->gate);
        present_lock_destroy(&presenter->events);
        present_free(presenter);
    }
    return error == ESP_ERR_TIMEOUT ? ESP_ERR_INVALID_STATE : error;
}

esp_err_t present_session_bind(esp_display_presenter_t *presenter, esp_display_present_engine_t **out)
{
    if (!presenter || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = NULL;
    (void)present_mutex_take(presenter->gate, UINT32_MAX);
    esp_err_t error = presenter->active || presenter->lvgl_bound ? ESP_ERR_INVALID_STATE :
                      finish_cancel(presenter, budget(presenter));
    if (error == ESP_OK) {
        error = poll(presenter);
    }
    if (error == ESP_OK && (presenter->active || presenter->lvgl_bound || presenter->state != ESP_DISPLAY_PRESENTER_STATE_OPEN || presenter->tracked_id)) {
        error = ESP_ERR_INVALID_STATE;
    }
    if (error == ESP_OK) {
        error = present_resources_prepare(presenter->resources, budget(presenter));
    }
    if (error == ESP_OK) {
        error = present_presenter_control_bind_producer(core(presenter), budget(presenter));
    }
    if (error == ESP_OK) {
        presenter->producer = present_current_task();
        presenter->lvgl_bound = true;
        *out = core(presenter);
    }
    present_mutex_give(presenter->gate);
    return error;
}
void present_session_unbind(esp_display_presenter_t *presenter)
{
    (void)present_mutex_take(presenter->gate, UINT32_MAX);
    presenter->lvgl_bound = false;
    presenter->full_next = true;
    present_mutex_give(presenter->gate);
}

uint64_t present_session_committed(esp_display_presenter_t *presenter)
{
    (void)present_mutex_take(presenter->gate, UINT32_MAX);
    (void)poll(presenter);
    uint64_t committed = presenter->timeline.last_submitted_frame;
    present_mutex_give(presenter->gate);
    return committed;
}
