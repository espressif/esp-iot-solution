/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "pixel/present_pixel_types.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Explicitly injected, statically linked executor. No global registration.
 * Ops/context are immutable for the lifetime of all borrowing engines.
 * Calls may execute concurrently: built-in executors serialize per instance.
 * Destroy is owner-serialized AFTER every borrowing engine has stopped/joined.
 * On destroy failure the executor remains valid and retryable. */
typedef struct esp_display_present_executor esp_display_present_executor_t;
typedef enum {
    ESP_DISPLAY_PRESENT_EXEC_SUPPORTED = 0,
    ESP_DISPLAY_PRESENT_EXEC_INVALID_REQUEST,
    ESP_DISPLAY_PRESENT_EXEC_TRANSFORM,
    ESP_DISPLAY_PRESENT_EXEC_ALIGNMENT,
    ESP_DISPLAY_PRESENT_EXEC_CAPACITY,
    ESP_DISPLAY_PRESENT_EXEC_LAYOUT,
    ESP_DISPLAY_PRESENT_EXEC_MEMORY,
    ESP_DISPLAY_PRESENT_EXEC_IN_PLACE,
} esp_display_present_executor_reason_t;

typedef struct {
    const char *name; /* Static lifetime; identity is the executor pointer. */
    uint32_t format_mask; /* Bit indexed by esp_display_present_engine_pixel_format_t. Same-format only. */
    uint32_t rotation_mask;
    bool byte_swap;
    bool in_place;
    size_t base_alignment; /* Minimum; actual addresses may require more. */
    size_t dst_cap_align;
    size_t stride_align; /* 1: no fixed byte constraint beyond pixel size. */
    uint32_t max_stride_pixels; /* 0: only checked size_t/capacity limits. */
    uint32_t max_height;
    bool addr_constraints; /* probe remains mandatory. */
    bool full_stride_cap;
} esp_display_present_executor_caps_t;

typedef struct {
    esp_err_t (*get_caps)(void *context, esp_display_present_executor_caps_t *caps);
    /* No writes, cache maintenance, submission or allocation. Unsupported returns
     * ESP_ERR_NOT_SUPPORTED with a reason; invalid requests are NOT fallback. */
    esp_err_t (*probe)(void *context, const esp_display_present_copy_request_t *request, esp_display_present_executor_reason_t *reason);
    /* Returning, even on error, guarantees ALL reads/writes have ended.
     * Must revalidate requests; never fall back internally. */
    esp_err_t (*execute)(void *context, const esp_display_present_copy_request_t *request);
    esp_err_t (*destroy)(void *context);
} esp_display_present_executor_ops_t;
struct esp_display_present_executor {
    const esp_display_present_executor_ops_t *ops;
    void *context;
};

esp_err_t esp_display_present_executor_cpu_create(esp_display_present_executor_t **out);
esp_err_t esp_display_present_executor_ppa_create(esp_display_present_executor_t **out);
esp_err_t esp_display_present_executor_dma2d_create(esp_display_present_executor_t **out);
esp_err_t esp_display_present_executor_get_caps(const esp_display_present_executor_t *executor, esp_display_present_executor_caps_t *caps);
esp_err_t esp_display_present_executor_probe(const esp_display_present_executor_t *executor, const esp_display_present_copy_request_t *request, esp_display_present_executor_reason_t *reason);
/* Direct Pixel Engine callback; context is esp_display_present_executor_t*. */
esp_err_t esp_display_present_executor_execute(void *context, const esp_display_present_copy_request_t *request);
esp_err_t esp_display_present_executor_destroy(esp_display_present_executor_t *executor);

/* Borrows both executors. NULL fallback means strict primary-only policy.
 * Only explicit probe NOT_SUPPORTED permits selecting fallback. No retry or
 * fallback after execute starts, including errors from cache/driver operations. */
typedef struct {
    esp_display_present_executor_t *primary;
    esp_display_present_executor_t *fallback;
} esp_display_present_executor_dispatch_t;
typedef struct {
    esp_display_present_executor_t *executor; /* NULL if selection failed; not an owning reference. */
    esp_display_present_executor_reason_t primary_reason;
    esp_display_present_executor_reason_t fallback_reason;
    bool used_fallback;
} esp_display_present_executor_selection_t;
/* Selection is advisory for this unchanged request, not a hardware reservation. */
esp_err_t esp_display_present_executor_select(const esp_display_present_executor_dispatch_t *dispatch, const esp_display_present_copy_request_t *request, esp_display_present_executor_selection_t *selection);
/* Per-call report avoids shared last-result races. Selected != successfully executed. */
esp_err_t esp_display_present_executor_run(const esp_display_present_executor_dispatch_t *dispatch, const esp_display_present_copy_request_t *request, esp_display_present_executor_selection_t *selection);
/* Pixel Engine callback; context is esp_display_present_executor_dispatch_t*. */
esp_err_t esp_display_present_executor_dispatch_execute(void *context, const esp_display_present_copy_request_t *request);
#ifdef __cplusplus
}
#endif
