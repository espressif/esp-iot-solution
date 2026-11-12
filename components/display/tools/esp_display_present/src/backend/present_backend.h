/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "backend/present_backend_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Private backend dispatch. Configuration remains an opaque input here;
 * include present_backend_config.h explicitly when constructing/reading it. */
typedef struct {
    const char *name;
    esp_err_t (*create)(const esp_display_present_backend_config_t *config, present_backend_t **out_backend);
    /** Optional creation with an autonomous wake source. The engine passes
     * its background-service policy; NULL preserves the ordinary create path.
     * Direct create retains each backend's existing default wake policy. */
    esp_err_t (*create_scheduled)(const esp_display_present_backend_config_t *config, present_backend_t **out_backend, bool autonomous);
    /** Task-context progress, under the engine gate when engine-owned.
     * Advances existing work without manufacturing completion events. */
    esp_err_t (*service)(present_backend_t *backend);
    esp_err_t (*get_caps)(present_backend_t *backend, esp_display_present_backend_caps_t *caps);
    esp_err_t (*begin)(present_backend_t *backend, const esp_display_present_frame_request_t *request, esp_display_present_frame_t *frame);
    esp_err_t (*acquire)(present_backend_t *backend, esp_display_present_frame_t frame, const esp_display_present_rect_t *area, uint32_t timeout_ms, esp_display_present_lease_t *lease);
    esp_err_t (*get_draw_buffers)(present_backend_t *backend, void **out_buffers, size_t max_buffers, size_t *out_count, size_t *out_buffer_bytes);
    esp_err_t (*submit_region)(present_backend_t *backend, esp_display_present_frame_t frame, esp_display_present_lease_id_t lease_id);
    esp_err_t (*submit_region_ex)(present_backend_t *backend, esp_display_present_frame_t frame, esp_display_present_lease_id_t lease_id, const esp_display_present_region_submit_t *submit);
    esp_err_t (*cancel_region)(present_backend_t *backend, esp_display_present_frame_t frame, esp_display_present_lease_id_t lease_id);
    esp_err_t (*end)(present_backend_t *backend, esp_display_present_frame_t frame, esp_display_present_receipt_t *out_receipt);
    esp_err_t (*end_ex)(present_backend_t *backend, esp_display_present_frame_t frame, const esp_display_present_frame_coverage_t *coverage, esp_display_present_receipt_t *out_receipt);
    esp_err_t (*cancel_frame)(present_backend_t *backend, esp_display_present_frame_t frame, uint32_t timeout_ms);
    esp_err_t (*query)(present_backend_t *backend, esp_display_present_receipt_t receipt, esp_display_present_frame_status_t *status);
    esp_err_t (*get_fault)(present_backend_t *backend, esp_display_present_backend_fault_t *fault);
    esp_err_t (*wait)(present_backend_t *backend, esp_display_present_receipt_t receipt, esp_display_present_completion_point_t point, uint32_t timeout_ms, esp_display_present_frame_status_t *status);
    esp_err_t (*release_receipt)(present_backend_t *backend, esp_display_present_receipt_t receipt);
    bool (*on_event_isr)(present_backend_t *backend, const esp_display_present_backend_event_t *event);
    void (*on_event)(present_backend_t *backend, const esp_display_present_backend_event_t *event);
    esp_err_t (*stop)(present_backend_t *backend, uint32_t timeout_ms);
    esp_err_t (*destroy)(present_backend_t *backend);
    esp_err_t (*set_rotation)(present_backend_t *backend, esp_display_present_engine_rotation_t rotation);
} present_backend_ops_t;

extern const present_backend_ops_t present_region_backend;
extern const present_backend_ops_t present_te_backend;
extern const present_backend_ops_t present_fb_backend;

#ifdef __cplusplus
}
#endif
