/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_display_present.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_display_present_geometry.h"
#include "esp_display_present_target.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_display_present_endpoint.h"

static const char *TAG = "presenter";

struct esp_display_presenter {
    esp_display_presenter_config_t config;
    esp_display_present_target_t *target;
    const present_endpoint_ops_t *endpoint_ops;
    const present_mode_ops_t *mode_ops;
    void *mode_ctx;
    esp_display_present_frame_tracker_t *tracker;
    esp_display_present_drawbuf_pool_t drawbuf_pool;
    esp_display_presenter_caps_t caps;
    struct {
        bool active;
        bool region_held;
        bool repaired;
        bool presented;
        uint64_t id;
        uint32_t lease_id;
        esp_display_present_render_alignment_t render_alignment;
        esp_display_presenter_region_t region;
    } frame;
    uint64_t next_ticket;
    uint32_t next_lease_id;
    portMUX_TYPE lifecycle_lock;
    uint16_t active_calls;
    esp_display_presenter_state_t state;
};

static uint64_t presenter_next_internal_ticket(
    const esp_display_presenter_t *presenter);

static present_frame_ctx_t presenter_frame_ctx(
    const esp_display_presenter_t *presenter,
    uint64_t frame_id)
{
    return (present_frame_ctx_t) {
        .mode_ctx = presenter->mode_ctx,
        .tracker = presenter->tracker,
        .target = presenter->target,
        .frame_id = frame_id,
        .render_alignment = presenter->frame.render_alignment,
    };
}

static void frame_reset_region(esp_display_presenter_t *presenter)
{
    presenter->frame.region_held = false;
    presenter->frame.lease_id = 0;
    memset(&presenter->frame.region, 0, sizeof(presenter->frame.region));
}

static uint8_t presenter_color_bytes(
    esp_display_present_pixel_format_t pixel_format)
{
    switch (pixel_format) {
    case ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565:
        return 2;
    case ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB888:
        return 3;
    default:
        return 0;
    }
}

static uint32_t presenter_next_lease_id(
    esp_display_presenter_t *presenter)
{
    ++presenter->next_lease_id;
    if (presenter->next_lease_id == 0) {
        ++presenter->next_lease_id;
    }
    return presenter->next_lease_id;
}

static uint64_t ticket_advance(uint64_t ticket)
{
    ++ticket;
    return ticket == 0 ? 1 : ticket;
}

static esp_err_t presenter_enter(esp_display_presenter_t *presenter,
                                 bool allow_closing);
static void presenter_leave(esp_display_presenter_t *presenter);

static esp_err_t presenter_resolve_buffer_rows(
    void *user_ctx, uint32_t lease_id, size_t stride_bytes,
    size_t requested_rows, size_t *out_rows)
{
    esp_display_presenter_t *presenter = user_ctx;
    if (presenter == NULL || lease_id == 0 || stride_bytes == 0 ||
            requested_rows == 0 || out_rows == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = presenter_enter(presenter, false);
    if (ret != ESP_OK) {
        return ret;
    }
    if (!presenter->frame.active || !presenter->frame.region_held ||
            presenter->frame.lease_id != lease_id) {
        presenter_leave(presenter);
        return ESP_ERR_INVALID_STATE;
    }
    size_t capacity = presenter->caps.contract ==
                      ESP_DISPLAY_PRESENT_CONTRACT_PARTITION
                      ? presenter->caps.drawbuf_bytes
                      : presenter->frame.region.surface.stride_bytes *
                      presenter->frame.region.surface.height;
    size_t leased_layout_bytes =
        presenter->frame.region.surface.stride_bytes *
        presenter->frame.region.surface.height;
    if (leased_layout_bytes < capacity) {
        capacity = leased_layout_bytes;
    }
    size_t rows = capacity / stride_bytes;
    if (rows > requested_rows) {
        rows = requested_rows;
    }
    size_t alignment =
        presenter->frame.render_alignment.height_pixels != 0
        ? presenter->frame.render_alignment.height_pixels : 1;
    if (presenter->caps.contract == ESP_DISPLAY_PRESENT_CONTRACT_PARTITION &&
            rows < requested_rows && alignment > 1) {
        rows -= rows % alignment;
    }
    if (rows == 0 || rows > UINT16_MAX) {
        presenter_leave(presenter);
        return ESP_ERR_INVALID_SIZE;
    }
    *out_rows = rows;
    presenter_leave(presenter);
    return ESP_OK;
}

static void frame_reset(esp_display_presenter_t *presenter)
{
    presenter->frame.active = false;
    presenter->frame.repaired = false;
    presenter->frame.id = 0;
    presenter->frame.render_alignment =
    (esp_display_present_render_alignment_t) {
        0
    };
    frame_reset_region(presenter);
}

static bool submit_is_valid(const esp_display_presenter_t *presenter,
                            const esp_display_presenter_submit_t *submit)
{
    return presenter != NULL && submit != NULL &&
           (submit->areas == NULL) == (submit->area_count == 0) &&
           submit->area_count <= presenter->caps.max_damage_areas &&
           (submit->coverage == ESP_DISPLAY_PRESENT_COVERAGE_FULL ||
            submit->coverage == ESP_DISPLAY_PRESENT_COVERAGE_AREAS) &&
           (submit->coverage != ESP_DISPLAY_PRESENT_COVERAGE_FULL ||
            submit->area_count == 0) &&
           (submit->coverage != ESP_DISPLAY_PRESENT_COVERAGE_AREAS ||
            submit->area_count != 0);
}

static void frame_cancel(esp_display_presenter_t *presenter)
{
    /* Ordinary cancel covers acquire/raster/submit only. After REPAIR the
     * frame is past the abort boundary (design 2.9). */
    if (presenter->frame.active && presenter->mode_ops != NULL &&
            !presenter->frame.repaired) {
        present_frame_ctx_t ctx =
            presenter_frame_ctx(presenter, presenter->frame.id);
        if (presenter->frame.region_held &&
                presenter->mode_ops->cancel_region != NULL) {
            (void)presenter->mode_ops->cancel_region(
                &ctx, presenter->frame.region.surface.pixels);
        }
        if (presenter->mode_ops->cancel_frame != NULL) {
            presenter->mode_ops->cancel_frame(&ctx);
        }
    }
    frame_reset(presenter);
}

static esp_err_t presenter_enter(esp_display_presenter_t *presenter,
                                 bool allow_closing)
{
    portENTER_CRITICAL(&presenter->lifecycle_lock);
    bool allowed = presenter->state == ESP_DISPLAY_PRESENTER_STATE_OPEN ||
                   (allow_closing &&
                    presenter->state == ESP_DISPLAY_PRESENTER_STATE_CLOSING);
    if (allowed) {
        ++presenter->active_calls;
    }
    portEXIT_CRITICAL(&presenter->lifecycle_lock);
    return allowed ? ESP_OK : ESP_ERR_INVALID_STATE;
}

static void presenter_leave(esp_display_presenter_t *presenter)
{
    portENTER_CRITICAL(&presenter->lifecycle_lock);
    if (presenter->active_calls != 0) {
        --presenter->active_calls;
    }
    portEXIT_CRITICAL(&presenter->lifecycle_lock);
}

static const char *present_contract_name(
    esp_display_present_contract_t contract)
{
    switch (contract) {
    case ESP_DISPLAY_PRESENT_CONTRACT_PARTITION:
        return "PARTITION";
    case ESP_DISPLAY_PRESENT_CONTRACT_DIRECT:
        return "DIRECT";
    case ESP_DISPLAY_PRESENT_CONTRACT_FULL:
        return "FULL";
    default:
        return "UNKNOWN";
    }
}

static bool IRAM_ATTR presenter_transfer_done(void *user_ctx)
{
    esp_display_presenter_t *presenter = user_ctx;
    return presenter != NULL && presenter->endpoint_ops != NULL
           ? presenter->endpoint_ops->transfer_done_isr(presenter->mode_ctx)
           : false;
}

static bool IRAM_ATTR presenter_frame_done(void *user_ctx)
{
    esp_display_presenter_t *presenter = user_ctx;
    if (presenter == NULL || presenter->endpoint_ops == NULL) {
        return false;
    }
    BaseType_t need_yield = pdFALSE;
    bool request = presenter->endpoint_ops->frame_done_isr(
                       presenter->mode_ctx, &need_yield);
    return request || need_yield == pdTRUE;
}

static const esp_display_present_target_callbacks_t s_target_callbacks = {
    .on_transfer_done = presenter_transfer_done,
    .on_frame_done = presenter_frame_done,
};

static void log_presenter_caps(const esp_display_presenter_t *presenter)
{
    ESP_LOGI(TAG,
             "presenter: contract=%s endpoint=%s mode=%s areas=%u retain=%u"
             " previous=%u drawbuf=%zu",
             present_contract_name(presenter->caps.contract),
             presenter->endpoint_ops != NULL
             ? presenter->endpoint_ops->name : "none",
             presenter->mode_ops != NULL ? presenter->mode_ops->name : "none",
             (unsigned)presenter->caps.supports_coverage_areas,
             (unsigned)presenter->caps.surface_retains_content,
             (unsigned)presenter->caps.previous_surface_readable,
             presenter->caps.drawbuf_bytes);
}

static esp_err_t presenter_release_binding(esp_display_presenter_t *presenter);

static esp_err_t presenter_bind_config(
    esp_display_presenter_t *presenter,
    const esp_display_presenter_config_t *config)
{
    esp_err_t ret = esp_display_present_target_create(
                        &config->target, config->pixel_format, config->width, config->height,
                        &presenter->target);
    if (ret != ESP_OK) {
        return ret;
    }

    esp_display_present_endpoint_binding_t endpoint = {0};
    ret = esp_display_present_endpoint_bind(
              presenter->target, config, &endpoint);
    if (ret != ESP_OK) {
        (void)esp_display_present_target_delete(presenter->target);
        presenter->target = NULL;
        return ret;
    }
    presenter->endpoint_ops = endpoint.ops;
    presenter->mode_ops = endpoint.mode_ops;
    presenter->mode_ctx = endpoint.mode_ctx;
    presenter->tracker = endpoint.tracker;
    presenter->drawbuf_pool = endpoint.drawbuf_pool;
    presenter->caps = endpoint.caps;

    ret = esp_display_present_target_set_callbacks(
              presenter->target, &s_target_callbacks, presenter);
    if (ret != ESP_OK) {
        (void)presenter_release_binding(presenter);
        return ret;
    }
    presenter->config = *config;
    log_presenter_caps(presenter);
    return ESP_OK;
}

static esp_err_t presenter_release_binding(esp_display_presenter_t *presenter)
{
    esp_err_t ret = presenter->mode_ctx != NULL && presenter->endpoint_ops != NULL
                    ? presenter->endpoint_ops->destroy(presenter->mode_ctx) : ESP_OK;
    if (ret != ESP_OK) {
        return ret;
    }
    presenter->mode_ctx = NULL;
    presenter->tracker = NULL;
    esp_display_present_drawbuf_pool_free(&presenter->drawbuf_pool);
    ret = presenter->target != NULL
          ? esp_display_present_target_delete(presenter->target) : ESP_OK;
    if (ret != ESP_OK) {
        return ret;
    }
    presenter->target = NULL;
    presenter->endpoint_ops = NULL;
    presenter->mode_ops = NULL;
    memset(&presenter->caps, 0, sizeof(presenter->caps));
    return ESP_OK;
}

esp_err_t esp_display_presenter_create(
    const esp_display_presenter_config_t *config,
    esp_display_presenter_t **out_presenter)
{
    if (out_presenter != NULL) {
        *out_presenter = NULL;
    }
    ESP_RETURN_ON_FALSE(config && out_presenter &&
                        config->width && config->height,
                        ESP_ERR_INVALID_ARG, TAG, "invalid config");

    esp_display_presenter_t *presenter = heap_caps_calloc(
                                             1, sizeof(*presenter), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(presenter, ESP_ERR_NO_MEM, TAG, "no state memory");

    presenter->lifecycle_lock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
    presenter->state = ESP_DISPLAY_PRESENTER_STATE_OPEN;
    esp_err_t ret = presenter_bind_config(presenter, config);
    if (ret != ESP_OK) {
        free(presenter);
        return ret;
    }
    *out_presenter = presenter;
    return ESP_OK;
}

esp_err_t esp_display_presenter_stop(
    esp_display_presenter_t *presenter)
{
    if (presenter == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    portENTER_CRITICAL(&presenter->lifecycle_lock);
    if (presenter->state == ESP_DISPLAY_PRESENTER_STATE_STOPPED) {
        portEXIT_CRITICAL(&presenter->lifecycle_lock);
        return ESP_OK;
    }
    if (presenter->active_calls != 0) {
        portEXIT_CRITICAL(&presenter->lifecycle_lock);
        return ESP_ERR_INVALID_STATE;
    }
    presenter->state = ESP_DISPLAY_PRESENTER_STATE_CLOSING;
    bool tile_return_blocks_stop =
        presenter->frame.region_held &&
        presenter->endpoint_ops != NULL &&
        presenter->endpoint_ops->stop_after_facade_returns_tiles;
    portEXIT_CRITICAL(&presenter->lifecycle_lock);

    /* An endpoint declaring stop_after_facade_returns_tiles has no
     * backend-held tile state and no blocked acquire to wake. Keep it open
     * until the facade returns its current tile. */
    if (tile_return_blocks_stop) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = presenter->endpoint_ops != NULL
                    ? presenter->endpoint_ops->stop(presenter->mode_ctx)
                    : ESP_ERR_INVALID_STATE;
    bool finish_after_callbacks =
        ret == ESP_DISPLAY_PRESENT_STOP_NEEDS_CALLBACK_DETACH &&
        presenter->endpoint_ops->finish_stop_after_callbacks != NULL;
    if (ret != ESP_OK && !finish_after_callbacks) {
        return ret;
    }
    ret = esp_display_present_target_clear_callbacks(presenter->target);
    if (ret == ESP_OK && finish_after_callbacks) {
        ret = presenter->endpoint_ops->finish_stop_after_callbacks(
                  presenter->mode_ctx);
    }
    if (ret == ESP_OK) {
        portENTER_CRITICAL(&presenter->lifecycle_lock);
        presenter->state = ESP_DISPLAY_PRESENTER_STATE_STOPPED;
        portEXIT_CRITICAL(&presenter->lifecycle_lock);
    }
    return ret;
}

esp_err_t esp_display_presenter_delete(
    esp_display_presenter_t *presenter)
{
    if (presenter == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = esp_display_presenter_stop(presenter);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = presenter_release_binding(presenter);
    if (ret != ESP_OK) {
        return ret;
    }
    free(presenter);
    return ESP_OK;
}

esp_err_t esp_display_presenter_set_rotation(
    esp_display_presenter_t *presenter,
    esp_display_present_rotation_t rotation)
{
    if (presenter == NULL ||
            (rotation != ESP_DISPLAY_PRESENT_ROTATE_0 &&
             rotation != ESP_DISPLAY_PRESENT_ROTATE_90 &&
             rotation != ESP_DISPLAY_PRESENT_ROTATE_180 &&
             rotation != ESP_DISPLAY_PRESENT_ROTATE_270)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (presenter->config.target.hw.rotation == rotation) {
        return ESP_OK;
    }

    esp_display_presenter_config_t previous = presenter->config;
    esp_display_presenter_config_t next = previous;
    next.target.hw.rotation = rotation;

    esp_err_t ret = esp_display_presenter_stop(presenter);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = presenter_release_binding(presenter);
    if (ret != ESP_OK) {
        portENTER_CRITICAL(&presenter->lifecycle_lock);
        presenter->state = ESP_DISPLAY_PRESENTER_STATE_FAULTED;
        portEXIT_CRITICAL(&presenter->lifecycle_lock);
        ESP_LOGE(TAG, "rotation teardown failed: %s", esp_err_to_name(ret));
        return ret;
    }
    frame_reset(presenter);
    presenter->next_ticket = 0;
    presenter->next_lease_id = 0;
    ret = presenter_bind_config(presenter, &next);
    if (ret == ESP_OK) {
        portENTER_CRITICAL(&presenter->lifecycle_lock);
        presenter->state = ESP_DISPLAY_PRESENTER_STATE_OPEN;
        portEXIT_CRITICAL(&presenter->lifecycle_lock);
        ESP_LOGI(TAG, "runtime rotation=%d", (int)rotation);
        return ESP_OK;
    }

    ESP_LOGE(TAG, "rotation reconfigure failed: %s; restoring",
             esp_err_to_name(ret));
    esp_err_t restore_ret = presenter_bind_config(presenter, &previous);
    if (restore_ret == ESP_OK) {
        portENTER_CRITICAL(&presenter->lifecycle_lock);
        presenter->state = ESP_DISPLAY_PRESENTER_STATE_OPEN;
        portEXIT_CRITICAL(&presenter->lifecycle_lock);
    } else {
        portENTER_CRITICAL(&presenter->lifecycle_lock);
        presenter->state = ESP_DISPLAY_PRESENTER_STATE_FAULTED;
        portEXIT_CRITICAL(&presenter->lifecycle_lock);
        ESP_LOGE(TAG, "rotation restore failed: %s",
                 esp_err_to_name(restore_ret));
    }
    return ret;
}

esp_err_t esp_display_presenter_get_caps(
    const esp_display_presenter_t *presenter,
    esp_display_presenter_caps_t *out_caps)
{
    if (presenter == NULL || out_caps == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_caps = presenter->caps;
    return ESP_OK;
}

esp_err_t esp_display_presenter_get_drawbuf(
    const esp_display_presenter_t *presenter,
    esp_display_presenter_drawbuf_t *out_drawbuf)
{
    if (presenter == NULL || out_drawbuf == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (presenter->caps.contract != ESP_DISPLAY_PRESENT_CONTRACT_PARTITION ||
            presenter->drawbuf_pool.count == 0 ||
            presenter->drawbuf_pool.bytes == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    memset(out_drawbuf, 0, sizeof(*out_drawbuf));
    out_drawbuf->count = presenter->drawbuf_pool.count;
    out_drawbuf->lines = presenter->drawbuf_pool.lines;
    out_drawbuf->bytes = presenter->drawbuf_pool.bytes;
    for (uint8_t index = 0; index < out_drawbuf->count && index < 2; ++index) {
        out_drawbuf->buffers[index] = presenter->drawbuf_pool.buffers[index];
        if (out_drawbuf->buffers[index] == NULL) {
            return ESP_ERR_INVALID_STATE;
        }
    }
    return ESP_OK;
}

esp_err_t esp_display_presenter_rebind_producer(
    esp_display_presenter_t *presenter)
{
    if (presenter == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
    if (esp_display_present_tracker_get_producer_task(presenter->tracker) ==
            current_task) {
        return ESP_OK;
    }
    if (presenter_enter(presenter, false) != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = presenter->frame.active
                    ? ESP_ERR_INVALID_STATE
                    : esp_display_present_tracker_rebind_producer_task(
                        presenter->tracker, current_task);
    presenter_leave(presenter);
    return ret;
}

esp_err_t esp_display_presenter_get_state(
    const esp_display_presenter_t *presenter,
    esp_display_presenter_state_t *out_state)
{
    if (presenter == NULL || out_state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    portENTER_CRITICAL((portMUX_TYPE *)&presenter->lifecycle_lock);
    *out_state = presenter->state;
    portEXIT_CRITICAL((portMUX_TYPE *)&presenter->lifecycle_lock);
    return ESP_OK;
}

esp_err_t esp_display_presenter_get_fault_reason(
    const esp_display_presenter_t *presenter,
    esp_display_present_fault_reason_t *out_reason)
{
    if (presenter == NULL || out_reason == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_reason =
        esp_display_present_tracker_get_fault_reason(presenter->tracker);
    return ESP_OK;
}

esp_err_t esp_display_presenter_map_point(
    const esp_display_presenter_t *presenter,
    int32_t *x,
    int32_t *y)
{
    return presenter != NULL
           ? esp_display_present_target_map_physical_point_to_logical(presenter->target, x, y)
           : ESP_ERR_INVALID_ARG;
}

static bool scroll_request_is_valid(
    const esp_display_present_surface_request_t *request,
    uint16_t width,
    uint16_t height)
{
    if (request == NULL || !request->has_scroll) {
        return true;
    }
    return request->scroll_viewport.x1 >= 0 &&
           request->scroll_viewport.y1 >= 0 &&
           request->scroll_viewport.x2 >= request->scroll_viewport.x1 &&
           request->scroll_viewport.y2 >= request->scroll_viewport.y1 &&
           request->scroll_viewport.x2 < width &&
           request->scroll_viewport.y2 < height &&
           (request->scroll_dx != 0 || request->scroll_dy != 0);
}

/**
 * FramePlan: dirty/scroll validation + coverage negotiation for every
 * contract. begin_frame (SEED) must not rewrite these outputs.
 */
static esp_err_t presenter_plan_frame(
    const esp_display_presenter_t *presenter,
    const esp_display_present_surface_request_t *request,
    esp_display_present_area_t *out_render_areas,
    size_t render_area_capacity,
    size_t *out_render_area_count,
    bool *out_full_coverage)
{
    size_t dirty_count = request != NULL ? request->dirty_area_count : 0;
    const esp_display_present_area_t *dirty_areas =
        request != NULL ? request->dirty_areas : NULL;
    bool has_scroll = request != NULL && request->has_scroll;
    esp_display_present_contract_t contract = presenter->caps.contract;

    *out_render_area_count = 0;
    *out_full_coverage = false;

    if ((dirty_areas == NULL) != (dirty_count == 0) ||
            dirty_count > presenter->caps.max_damage_areas ||
            dirty_count > render_area_capacity ||
            (dirty_count != 0 && out_render_areas == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }
    for (size_t index = 0; index < dirty_count; ++index) {
        if (!esp_display_present_geometry_area_is_valid(
                    &dirty_areas[index], presenter->caps.width,
                    presenter->caps.height)) {
            return ESP_ERR_INVALID_ARG;
        }
    }
    if (!scroll_request_is_valid(
                request, presenter->caps.width, presenter->caps.height)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (contract == ESP_DISPLAY_PRESENT_CONTRACT_PARTITION) {
        if (has_scroll) {
            return ESP_ERR_NOT_SUPPORTED;
        }
        /* First frame, empty dirty, or an explicit whole-display dirty area
         * is a full raster. Treating the latter as PARTITION makes producers
         * report every draw-buffer band as coverage, can overflow their area
         * list, and incorrectly preserves pixels from the previous producer
         * during a handoff. */
        bool full_dirty = dirty_count == 1 && dirty_areas[0].x1 == 0 &&
                          dirty_areas[0].y1 == 0 &&
                          dirty_areas[0].x2 == (int32_t)presenter->caps.width - 1 &&
                          dirty_areas[0].y2 == (int32_t)presenter->caps.height - 1;
        bool full = !presenter->frame.presented || dirty_count == 0 ||
                    full_dirty;
        if (!full) {
            for (size_t index = 0; index < dirty_count; ++index) {
                esp_err_t ret = esp_display_present_geometry_align_area(
                (esp_display_present_size_t) {
                    .width = presenter->caps.width,
                    .height = presenter->caps.height,
                }, &dirty_areas[index], &request->render_alignment,
                &out_render_areas[index]);
                if (ret != ESP_OK) {
                    return ret;
                }
            }
            *out_render_area_count = dirty_count;
        }
        *out_full_coverage = full;
        return ESP_OK;
    }

    if (contract == ESP_DISPLAY_PRESENT_CONTRACT_FULL ||
            !presenter->caps.supports_coverage_areas ||
            has_scroll || dirty_count == 0 ||
            !presenter->frame.presented ||
            !presenter->caps.previous_surface_readable) {
        /* FULL, transform (previous_surface_readable=false), first frame,
         * scroll, or empty dirty → complete producer raster. */
        *out_full_coverage = true;
        return ESP_OK;
    }

    for (size_t index = 0; index < dirty_count; ++index) {
        esp_err_t ret = esp_display_present_geometry_align_area(
        (esp_display_present_size_t) {
            .width = presenter->caps.width,
            .height = presenter->caps.height,
        }, &dirty_areas[index], &request->render_alignment,
        &out_render_areas[index]);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    *out_render_area_count = dirty_count;
    *out_full_coverage = false;
    return ESP_OK;
}

static esp_err_t presenter_begin_frame_id(
    esp_display_presenter_t *presenter,
    uint64_t frame_id,
    const esp_display_present_surface_request_t *request,
    esp_display_present_area_t *out_render_areas,
    size_t render_area_capacity,
    size_t *out_render_area_count,
    bool *out_full_coverage)
{
    if (presenter == NULL || frame_id == 0 ||
            out_render_area_count == NULL || out_full_coverage == NULL ||
            presenter->mode_ops == NULL || presenter->mode_ctx == NULL ||
            presenter->tracker == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = presenter_plan_frame(
                        presenter, request, out_render_areas,
                        render_area_capacity, out_render_area_count,
                        out_full_coverage);
    if (ret != ESP_OK) {
        return ret;
    }

    bool partition = presenter->caps.contract ==
                     ESP_DISPLAY_PRESENT_CONTRACT_PARTITION;
    ret = presenter_enter(presenter, false);
    if (ret != ESP_OK) {
        return ret;
    }
    if (presenter->frame.active ||
            (partition && presenter->caps.drawbuf_bytes == 0)) {
        presenter_leave(presenter);
        return ESP_ERR_INVALID_STATE;
    }
    /* The mode context belongs to the frame being opened.  Publish its
     * alignment before invoking any mode callback so begin/acquire stages
     * observe the same contract that FramePlan used. */
    presenter->frame.render_alignment = request != NULL
                                        ? request->render_alignment
    : (esp_display_present_render_alignment_t) {
        0
    };
    present_frame_ctx_t ctx = presenter_frame_ctx(presenter, frame_id);
    /* SEED: begin_frame must not rewrite FramePlan outputs. */
    ret = presenter->mode_ops->begin_frame != NULL
          ? presenter->mode_ops->begin_frame(
              &ctx, request, NULL, 0, NULL)
          : ESP_OK;
    if (ret != ESP_OK) {
        presenter->frame.render_alignment =
        (esp_display_present_render_alignment_t) {
            0
        };
        presenter_leave(presenter);
        return ret;
    }
    presenter->frame.active = true;
    presenter->frame.id = frame_id;
    presenter->next_ticket = ticket_advance(frame_id);
    presenter_leave(presenter);
    return ESP_OK;
}

esp_err_t esp_display_presenter_begin_next_frame(
    esp_display_presenter_t *presenter,
    const esp_display_present_surface_request_t *request,
    esp_display_present_area_t *out_render_areas,
    size_t render_area_capacity,
    size_t *out_render_area_count,
    bool *out_full_coverage)
{
    if (presenter == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint64_t frame_id = presenter->next_ticket;
    if (frame_id == 0) {
        frame_id = presenter_next_internal_ticket(presenter);
    }
    return presenter_begin_frame_id(
               presenter, frame_id, request, out_render_areas,
               render_area_capacity, out_render_area_count,
               out_full_coverage);
}

esp_err_t esp_display_presenter_acquire_buffer(
    esp_display_presenter_t *presenter,
    esp_display_presenter_buffer_t *out_buffer)
{
    if (presenter == NULL || out_buffer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = presenter_enter(presenter, false);
    if (ret != ESP_OK) {
        return ret;
    }
    if (!presenter->frame.active || presenter->frame.region_held) {
        presenter_leave(presenter);
        return ESP_ERR_INVALID_STATE;
    }

    esp_display_present_area_t full_area = {
        .x1 = 0,
        .y1 = 0,
        .x2 = presenter->caps.width - 1,
        .y2 = presenter->caps.height - 1,
    };
    present_frame_ctx_t ctx = presenter_frame_ctx(
                                  presenter, presenter->frame.id);
    ret = presenter->mode_ops->acquire_region(
              &ctx, &full_area, presenter->caps.pixel_format,
              &presenter->frame.region);
    if (ret != ESP_OK) {
        presenter_leave(presenter);
        return ret;
    }

    size_t capacity = presenter->caps.contract ==
                      ESP_DISPLAY_PRESENT_CONTRACT_PARTITION
                      ? presenter->caps.drawbuf_bytes
                      : presenter->frame.region.surface.stride_bytes *
                      presenter->frame.region.surface.height;
    if (capacity == 0) {
        (void)presenter->mode_ops->cancel_region(
            &ctx, presenter->frame.region.surface.pixels);
        frame_reset_region(presenter);
        presenter_leave(presenter);
        return ESP_ERR_INVALID_STATE;
    }
    presenter->frame.region_held = true;
    presenter->frame.lease_id = presenter_next_lease_id(presenter);
    *out_buffer = (esp_display_presenter_buffer_t) {
        .surface = presenter->frame.region.surface,
        .capacity_bytes = capacity,
        .lease_id = presenter->frame.lease_id,
        .resolve_rows = presenter_resolve_buffer_rows,
        .resolve_rows_ctx = presenter,
    };
    presenter_leave(presenter);
    return ESP_OK;
}

esp_err_t esp_display_presenter_submit_buffer(
    esp_display_presenter_t *presenter,
    const esp_display_presenter_buffer_t *buffer,
    const esp_display_present_area_t *area,
    size_t stride_bytes)
{
    if (presenter == NULL || buffer == NULL ||
            area == NULL || buffer->surface.pixels == NULL ||
            buffer->lease_id == 0 ||
            !esp_display_present_geometry_area_is_valid(
                area, presenter->caps.width, presenter->caps.height)) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t color_bytes = presenter_color_bytes(
                              presenter->caps.pixel_format);
    size_t width = (size_t)(area->x2 - area->x1 + 1);
    size_t height = (size_t)(area->y2 - area->y1 + 1);
    if (color_bytes == 0 || stride_bytes < width * color_bytes ||
            height > buffer->capacity_bytes / stride_bytes) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = presenter_enter(presenter, true);
    if (ret != ESP_OK) {
        return ret;
    }
    if (!presenter->frame.active || !presenter->frame.region_held ||
            presenter->frame.lease_id != buffer->lease_id ||
            buffer->surface.pixels !=
            presenter->frame.region.surface.pixels ||
            buffer->surface.pixel_format != presenter->caps.pixel_format ||
            buffer->surface.width !=
            presenter->frame.region.surface.width ||
            buffer->surface.height !=
            presenter->frame.region.surface.height ||
            buffer->capacity_bytes !=
            (presenter->caps.contract ==
             ESP_DISPLAY_PRESENT_CONTRACT_PARTITION
             ? presenter->caps.drawbuf_bytes
             : presenter->frame.region.surface.stride_bytes *
             presenter->frame.region.surface.height)) {
        presenter_leave(presenter);
        return ESP_ERR_INVALID_STATE;
    }

    esp_display_presenter_region_t rendered = {
        .surface = {
            .pixels = buffer->surface.pixels,
            .stride_bytes = stride_bytes,
            .width = (uint16_t)width,
            .height = (uint16_t)height,
            .pixel_format = buffer->surface.pixel_format,
        },
        .origin_x = (uint16_t)area->x1,
        .origin_y = (uint16_t)area->y1,
    };
    present_frame_ctx_t ctx = presenter_frame_ctx(
                                  presenter, presenter->frame.id);
    ret = presenter->mode_ops->submit_region(&ctx, &rendered);
    if (ret != ESP_OK) {
        frame_cancel(presenter);
        presenter_leave(presenter);
        return ret;
    }
    frame_reset_region(presenter);
    presenter_leave(presenter);
    return ESP_OK;
}

esp_err_t esp_display_presenter_commit_frame(
    esp_display_presenter_t *presenter,
    const esp_display_presenter_submit_t *submit)
{
    if (presenter == NULL || !submit_is_valid(presenter, submit)) {
        return ESP_ERR_INVALID_ARG;
    }
    for (size_t index = 0; index < submit->area_count; ++index) {
        if (!esp_display_present_geometry_area_is_valid(
                    &submit->areas[index], presenter->caps.width,
                    presenter->caps.height)) {
            return ESP_ERR_INVALID_ARG;
        }
    }
    if (submit->coverage == ESP_DISPLAY_PRESENT_COVERAGE_AREAS &&
            presenter->caps.contract !=
            ESP_DISPLAY_PRESENT_CONTRACT_PARTITION &&
            !presenter->caps.supports_coverage_areas) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    esp_err_t ret = presenter_enter(presenter, true);
    if (ret != ESP_OK) {
        return ret;
    }
    if (!presenter->frame.active || presenter->frame.region_held) {
        presenter_leave(presenter);
        return ESP_ERR_INVALID_STATE;
    }
    present_frame_ctx_t ctx = presenter_frame_ctx(
                                  presenter, presenter->frame.id);
    if (submit->coverage == ESP_DISPLAY_PRESENT_COVERAGE_AREAS) {
        ctx.repair.rendered_areas = submit->areas;
        ctx.repair.rendered_area_count = submit->area_count;
    }
    if (presenter->mode_ops->repair_frame != NULL) {
        bool repaired = false;
        ret = presenter->mode_ops->repair_frame(&ctx, &repaired);
        if (ret != ESP_OK) {
            frame_cancel(presenter);
            presenter_leave(presenter);
            return ret;
        }
        presenter->frame.repaired = repaired;
    }
    ret = presenter->mode_ops->commit_frame(&ctx, submit);
    if (ret != ESP_OK && !presenter->frame.repaired &&
            presenter->mode_ops->cancel_frame != NULL) {
        presenter->mode_ops->cancel_frame(&ctx);
    }
    if (ret != ESP_OK &&
            esp_display_present_tracker_is_faulted(presenter->tracker)) {
        portENTER_CRITICAL(&presenter->lifecycle_lock);
        presenter->state = ESP_DISPLAY_PRESENTER_STATE_FAULTED;
        portEXIT_CRITICAL(&presenter->lifecycle_lock);
    }
    if (ret == ESP_OK) {
        presenter->frame.presented = true;
    }
    frame_reset(presenter);
    presenter_leave(presenter);
    return ret;
}

void esp_display_presenter_cancel_frame(
    esp_display_presenter_t *presenter)
{
    if (presenter == NULL || presenter_enter(presenter, true) != ESP_OK) {
        return;
    }
    TaskHandle_t producer = presenter->tracker != NULL
                            ? esp_display_present_tracker_get_producer_task(presenter->tracker)
                            : NULL;
    if (presenter->frame.active && producer != NULL &&
            producer != xTaskGetCurrentTaskHandle()) {
        presenter_leave(presenter);
        return;
    }
    /* Only an active, pre-commit lease can be cancelled. commit_frame() is
     * synchronous through the endpoint boundary and resets frame.active, so
     * hardware-accepted frames are owned by their fence/terminal path. */
    frame_cancel(presenter);
    presenter_leave(presenter);
}

static uint64_t presenter_next_internal_ticket(
    const esp_display_presenter_t *presenter)
{
    if (presenter == NULL) {
        return 0;
    }
    uint64_t latest = presenter->endpoint_ops != NULL
                      ? presenter->endpoint_ops->last_submitted(presenter->mode_ctx) : 0;
    uint64_t transfer = presenter->endpoint_ops != NULL
                        ? presenter->endpoint_ops->completed_transfer(presenter->mode_ctx) : 0;
    uint64_t present = presenter->endpoint_ops != NULL
                       ? presenter->endpoint_ops->completed_present(presenter->mode_ctx) : 0;
    if (transfer > latest) {
        latest = transfer;
    }
    if (present > latest) {
        latest = present;
    }
    ++latest;
    return latest == 0 ? 1 : latest;
}

esp_err_t esp_display_presenter_quiesce(
    esp_display_presenter_t *presenter,
    uint32_t timeout_ms)
{
    if (presenter == NULL || timeout_ms == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = presenter_enter(presenter, false);
    if (ret != ESP_OK) {
        return ret;
    }
    if (presenter->frame.active || presenter->frame.region_held ||
            presenter->endpoint_ops == NULL) {
        presenter_leave(presenter);
        return ESP_ERR_INVALID_STATE;
    }
    uint64_t fence = presenter->endpoint_ops->last_submitted(
                         presenter->mode_ctx);
    presenter_leave(presenter);
    if (fence == 0) {
        return ESP_OK;
    }

    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    if (timeout_ticks == 0) {
        timeout_ticks = 1;
    }
    TickType_t start = xTaskGetTickCount();
    for (;;) {
        bool tracks_transfer =
            esp_display_present_tracker_tracks_transfer(presenter->tracker);
        uint64_t transfer = presenter->endpoint_ops->completed_transfer(
                                presenter->mode_ctx);
        uint64_t present = presenter->endpoint_ops->completed_present(
                               presenter->mode_ctx);
        const uint64_t present_tail =
            presenter->endpoint_ops->quiesce_present_tail;
        const bool present_quiescent = present >= fence ||
                                       (fence > present && fence - present <= present_tail);
        if ((!tracks_transfer || transfer >= fence) && present_quiescent) {
            return ESP_OK;
        }
        if ((TickType_t)(xTaskGetTickCount() - start) >= timeout_ticks) {
            ESP_LOGE(TAG,
                     "quiesce timeout: fence=%" PRIu64
                     " transfer=%" PRIu64 " present=%" PRIu64
                     " tracks_transfer=%u present_tail=%" PRIu64,
                     fence, transfer, present, (unsigned)tracks_transfer,
                     present_tail);
            return ESP_ERR_TIMEOUT;
        }
        TickType_t elapsed = (TickType_t)(xTaskGetTickCount() - start);
        TickType_t remaining = timeout_ticks - elapsed;
        (void)esp_display_present_tracker_wait_completion(
            presenter->tracker, remaining);
    }
}
