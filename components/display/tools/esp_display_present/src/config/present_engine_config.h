/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "backend/present_backend_config.h"
#include "backend/present_output_caps.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ESP_DISPLAY_PRESENT_ENGINE_REGION = 0,
    ESP_DISPLAY_PRESENT_ENGINE_TE = 1,
    ESP_DISPLAY_PRESENT_ENGINE_FB = 2,
} esp_display_present_engine_backend_t;
typedef enum {
    ESP_DISPLAY_PRESENT_MODE_DEFAULT = 0, /* Use backend_kind/config verbatim; source compatibility. */
    ESP_DISPLAY_PRESENT_ENGINE_MODE_NONE,
    ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_FULL,
    ESP_DISPLAY_PRESENT_ENGINE_MODE_TRIPLE_FULL,
    ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_DIRECT,
    ESP_DISPLAY_PRESENT_ENGINE_MODE_TRIPLE_PARTIAL,
    ESP_DISPLAY_PRESENT_ENGINE_MODE_TE_SYNC,
    ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_PARTIAL,
    ESP_DISPLAY_PRESENT_ENGINE_MODE_AUTO,
} esp_display_present_present_mode_t;
typedef struct esp_display_present_engine_config {
    esp_display_present_backend_config_t backend; /* region/TE PARTITION or FB PARTITION/FULL/DIRECT. */
    uint32_t timeout_ms; /* 0 polls; UINT32_MAX waits indefinitely. */
    esp_display_present_engine_backend_t backend_kind; /* Zero preserves region default. */
    esp_display_present_present_mode_t mode;
    /** Optional backend worker; default false retains cooperative service.
     * With region deferred_submit, only this worker enters panel submission.
     * Host uses pthread; IDF defaults: 4096-byte stack, priority 5. */
    bool background_service;
    uint32_t worker_stack_bytes; /* 0 selects default; ignored by Host. */
    unsigned worker_priority; /* 0 selects default; ignored by Host. */
    /** FB modes require at least two buffers; NONE/active scanout writes are unsupported.
     * IO NONE selects region. AUTO resolves FB 2/3 buffers to DOUBLE/TRIPLE_PARTIAL;
     * a single FB is rejected. IO with both TE budgets selects TE, otherwise region.
     * Named FB modes set buffer_count; external array must have that many entries. */
    esp_display_present_output_caps_t output_caps; /* Zero region alignments mean 1. */
    /** Per physical panel.submit completion deadline, including driver call
     * and callback dispatch. Zero disables (legacy V2 behavior). Timeout is a
     * sticky fault, never synthetic completion or permission to free pixels.
     * Checked by service/task APIs and completion ingress; an existing backend worker
     * wakes at the next deadline. No extra task; a blocking driver call cannot
     * be interrupted. Not a deadline for drawing, copy, repair or waiting TE. */
    uint32_t transfer_timeout_ms;
} esp_display_present_engine_config_t;

#ifdef __cplusplus
}
#endif
