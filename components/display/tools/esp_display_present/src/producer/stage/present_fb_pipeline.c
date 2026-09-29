/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * COMMIT switch: present a coherent surface and rotate the FB pipeline.
 *
 * Barrier PENDING (mode5/6): wait_pending → blit → set_pending → acquire_next.
 * Barrier ORDERED (mode3): pool_commit before blit, no wait_pending, keeps
 * two in-flight presents; acquire_next failure is terminal.
 */

#include "present_fb_internal.h"
#include "present_mode_internal.h"

#include <inttypes.h>

#include "esp_display_present_cache.h"
#include "esp_display_present_geometry.h"
#include "esp_log.h"

static const char *TAG = "present_fb_switch";

static esp_err_t submit_pipeline_pending(
    esp_display_present_fb_endpoint_t *frame,
    void *panel_pixels,
    uint64_t ticket)
{
    esp_err_t ret = esp_display_present_blit_full_frame(
                        frame->panel, frame->physical_width, frame->physical_height,
                        panel_pixels);
    if (ret != ESP_OK) {
        return ret;
    }
    esp_display_present_tracker_pool_commit(
        &frame->tracker, frame->fb.disp_fb, panel_pixels, ticket);
    esp_display_present_tracker_set_pending(&frame->tracker,
                                            panel_pixels);
    if (frame->fb.submit_gate_enabled) {
        esp_display_present_tracker_mark_submit(&frame->tracker);
    }

    /* Rotated DOUBLE_FULL: do not wait here for a recycled scanout FB.
     * Drawing already used the single logical transform surface; the two
     * pipeline FBs only ping-pong as rotate destinations. VSYNC frees the
     * old scanout FB while the next logical frame renders; repair_rotate
     * acquires it immediately before the transform. */
    if (frame->fb.defer_draw_acquire_until_transform) {
        frame->fb.draw_fb = NULL;
        return ESP_OK;
    }

    esp_display_present_tracker_buf_t *next = NULL;
    ret = esp_display_present_tracker_pool_acquire_next(
              &frame->tracker, &next);
    if (ret != ESP_OK) {
        return ret == ESP_ERR_NOT_FOUND ? ESP_ERR_INVALID_STATE : ret;
    }
    frame->fb.draw_fb = next->buffer;
    return ESP_OK;
}

static esp_err_t submit_pipeline_ordered(
    esp_display_present_fb_endpoint_t *frame,
    uint64_t frame_id,
    void *panel_pixels,
    uint64_t previous_frame)
{
    void *old_display = frame->fb.disp_fb;
    esp_err_t ret = esp_display_present_blit_full_frame(
                        frame->panel, frame->physical_width, frame->physical_height,
                        panel_pixels);
    if (ret != ESP_OK) {
        (void)esp_display_present_tracker_pool_cancel_commit(
            &frame->tracker, old_display);
        esp_display_present_tracker_rollback_frame(
            &frame->tracker, frame_id, previous_frame, false);
        ESP_LOGE(TAG, "ordered blit failed: ticket=%" PRIu64 " error=%s",
                 frame_id, esp_err_to_name(ret));
        return ret;
    }
    /* Ordered PARTITION submissions are already represented by the inflight
     * ownership FIFO. Do not also publish through the single pending slot:
     * triple buffering may accept the next frame before VSYNC and would
     * overwrite that slot, while the ISR retires the inflight FIFO directly. */
    if (frame->fb.submit_gate_enabled) {
        esp_display_present_tracker_mark_submit(&frame->tracker);
    }
    ret = esp_display_present_tracker_end_frame(
              &frame->tracker, frame_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ordered frame close failed: ticket=%" PRIu64 " error=%s",
                 frame_id, esp_err_to_name(ret));
        return ret;
    }

    esp_display_present_tracker_buf_t *next = NULL;
    ret = esp_display_present_tracker_pool_acquire_next(
              &frame->tracker, &next);
    if (ret != ESP_OK) {
        /* Panel accepted this frame and the session is closed. Keep the
         * published buffer as display state; make the endpoint terminal. */
        frame->fb.disp_fb = panel_pixels;
        frame->fb.draw_fb = NULL;
        frame->repair.state.display_buffer_valid = true;
        portENTER_CRITICAL(&frame->partial_lock);
        frame->closing = true;
        portEXIT_CRITICAL(&frame->partial_lock);
        ESP_LOGE(TAG, "ordered acquire failed: ticket=%" PRIu64 " error=%s",
                 frame_id, esp_err_to_name(ret));
        return ret;
    }
    frame->fb.disp_fb = panel_pixels;
    frame->fb.draw_fb = next->buffer;
    frame->repair.state.display_buffer_valid = true;
    return ESP_OK;
}

esp_err_t present_fb_switch_commit(
    esp_display_present_fb_endpoint_t *frame,
    uint64_t frame_id,
    void *panel_pixels,
    present_switch_barrier_t barrier)
{
    if (frame == NULL || panel_pixels == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    bool uses_pipeline = present_fb_uses_pipeline(frame);
    if (barrier == PRESENT_SWITCH_BARRIER_ORDERED) {
        if (frame->fb.transform == NULL && panel_pixels != frame->fb.draw_fb) {
            return ESP_ERR_INVALID_STATE;
        }
        if (frame->fb.single_pending_switch &&
                frame->tracker.pool.retire_sem != NULL) {
            esp_err_t wait_ret =
                esp_display_present_tracker_pool_wait_retired(&frame->tracker);
            if (wait_ret != ESP_OK) {
                return wait_ret;
            }
        }
    } else if (uses_pipeline) {
        esp_err_t wait_ret = esp_display_present_tracker_wait_pending(
                                 &frame->tracker);
        if (wait_ret != ESP_OK) {
            return wait_ret;
        }
    }

    esp_display_present_cache_msync_framebuffer(
        panel_pixels,
        frame->physical_stride_bytes * frame->physical_height);

    void *old_display = frame->fb.disp_fb;
    if (barrier == PRESENT_SWITCH_BARRIER_ORDERED) {
        esp_display_present_tracker_pool_commit(
            &frame->tracker, old_display, panel_pixels, frame_id);
    }

    uint64_t previous_frame = 0;
    esp_err_t ret = esp_display_present_tracker_begin_build(
                        &frame->tracker, frame_id, xTaskGetCurrentTaskHandle(),
                        &previous_frame);
    if (ret == ESP_OK) {
        ret = esp_display_present_tracker_publish_frame(&frame->tracker, frame_id, &previous_frame);
    }
    if (ret != ESP_OK) {
        if (barrier == PRESENT_SWITCH_BARRIER_ORDERED) {
            (void)esp_display_present_tracker_pool_cancel_commit(
                &frame->tracker, old_display);
        }
        return ret;
    }

    if (barrier == PRESENT_SWITCH_BARRIER_ORDERED) {
        return submit_pipeline_ordered(
                   frame, frame_id, panel_pixels, previous_frame);
    }

    if (!uses_pipeline) {
        ret = esp_display_present_tracker_end_frame(
                  &frame->tracker, frame_id);
        if (ret != ESP_OK) {
            esp_display_present_tracker_rollback_frame(
                &frame->tracker, frame_id, previous_frame, false);
            return ret;
        }
        ret = esp_display_present_tracker_complete_frame(
                  &frame->tracker, frame_id, true, true);
    } else {
        ret = submit_pipeline_pending(frame, panel_pixels, frame_id);
        if (ret == ESP_OK) {
            ret = esp_display_present_tracker_end_frame(
                      &frame->tracker, frame_id);
        }
    }
    if (ret != ESP_OK) {
        bool submission_active = esp_display_present_tracker_has_pending(
                                     &frame->tracker);
        esp_display_present_tracker_rollback_frame(
            &frame->tracker, frame_id, previous_frame, submission_active);
        ESP_LOGE(TAG, "full/direct submit failed: ticket=%" PRIu64 " error=%s",
                 frame_id, esp_err_to_name(ret));
    }
    return ret;
}

static bool fb_area_list_is_valid(
    const esp_display_present_fb_endpoint_t *frame,
    const esp_display_present_area_t *areas,
    size_t count)
{
    if ((areas == NULL) != (count == 0) ||
            count > frame->max_damage_areas) {
        return false;
    }
    for (size_t index = 0; index < count; ++index) {
        if (!esp_display_present_geometry_area_is_valid(
                    &areas[index], frame->width, frame->height)) {
            return false;
        }
    }
    return true;
}

static void *fb_resolve_panel_pixels(
    const esp_display_present_fb_endpoint_t *frame,
    const esp_display_present_fb_lease_t *lease)
{
    if (frame->fb.transform != NULL) {
        return frame->fb.draw_fb;
    }
    return lease->surface.pixels;
}

static esp_err_t fb_commit_with_barrier(
    present_frame_ctx_t *ctx,
    const esp_display_presenter_submit_t *submit,
    present_switch_barrier_t barrier)
{
    esp_display_present_fb_endpoint_t *frame =
        ctx != NULL ? ctx->mode_ctx : NULL;
    if (frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_display_present_fb_lease_t *lease = &frame->lease.valet;
    esp_err_t copy_ret = present_async_copy_drain(&frame->fb.async_copy);
    if (copy_ret != ESP_OK) {
        present_fb_invalidate_lease(frame, lease);
        return copy_ret;
    }
    const esp_display_present_area_t *rendered_areas =
        ctx->repair.rendered_areas;
    size_t rendered_area_count = ctx->repair.rendered_area_count;
    if (rendered_areas == NULL && submit != NULL &&
            submit->coverage == ESP_DISPLAY_PRESENT_COVERAGE_AREAS) {
        rendered_areas = submit->areas;
        rendered_area_count = submit->area_count;
    }
    if (!present_fb_lease_is_valid(frame, lease) ||
            !fb_area_list_is_valid(frame, rendered_areas,
                                   rendered_area_count)) {
        return ESP_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL(&frame->partial_lock);
    bool closing = frame->closing;
    portEXIT_CRITICAL(&frame->partial_lock);
    if (closing) {
        present_fb_invalidate_lease(frame, lease);
        return ESP_ERR_INVALID_STATE;
    }

    void *panel_pixels = fb_resolve_panel_pixels(frame, lease);
    if (panel_pixels == NULL) {
        ESP_LOGE(TAG, "FB commit has no panel pixels");
        present_fb_invalidate_lease(frame, lease);
        return ESP_ERR_INVALID_STATE;
    }

    void *display_before = frame->fb.disp_fb;
    esp_err_t ret = present_fb_switch_commit(
                        frame, lease->frame_id, panel_pixels, barrier);
    if (present_fb_tracks_direct_history(frame)) {
        present_buffer_repair_view_t view = present_fb_repair_view(
                                                frame, display_before,
                                                panel_pixels);
        present_buffer_repair_record_draw(
            &frame->repair.state, &view,
            submit != NULL &&
            submit->coverage == ESP_DISPLAY_PRESENT_COVERAGE_AREAS
            ? rendered_areas : NULL,
            submit != NULL &&
            submit->coverage == ESP_DISPLAY_PRESENT_COVERAGE_AREAS
            ? rendered_area_count : 0,
            ret == ESP_OK);
    }
    if (ret == ESP_OK) {
        ++frame->submitted_frames;
    }
    present_fb_invalidate_lease(frame, lease);
    return ret;
}

esp_err_t esp_display_present_fb_commit(
    present_frame_ctx_t *ctx,
    const esp_display_presenter_submit_t *submit)
{
    return fb_commit_with_barrier(
               ctx, submit, PRESENT_SWITCH_BARRIER_PENDING);
}

esp_err_t esp_display_present_fb_commit_ordered(
    present_frame_ctx_t *ctx,
    const esp_display_presenter_submit_t *submit)
{
    return fb_commit_with_barrier(
               ctx, submit, PRESENT_SWITCH_BARRIER_ORDERED);
}
