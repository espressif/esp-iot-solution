/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "core/present_engine_types.h"
#include "backend/present_backend_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Owner-only facade helpers. Budget variants retain the native producer
 * contract and never grant controller access to frame/lease operations. */
esp_err_t present_presenter_begin_budget(esp_display_present_engine_t *, const esp_display_present_frame_request_t *, uint32_t);
esp_err_t present_presenter_cancel_budget(esp_display_present_engine_t *, uint32_t);

/* Facade identity bridge. Owner-only, same gate and argument validation as
 * native resolve/submit. Tokens retain full width; the facade separately
 * validates its public ID and pixel pointer. No lease/ownership is acquired. */
esp_err_t present_presenter_resolve_lease_rows(esp_display_present_engine_t *, esp_display_present_lease_id_t,
                                               size_t stride, uint32_t requested, uint32_t *out);
esp_err_t present_presenter_submit_lease(esp_display_present_engine_t *, esp_display_present_lease_id_t, void *pixels,
                                         const esp_display_present_engine_area_t *, size_t stride);

#ifdef __cplusplus
}
#endif
