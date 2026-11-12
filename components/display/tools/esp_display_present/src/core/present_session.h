/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "core/present_engine_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct esp_display_presenter;
/* Exclusive renderer session handoff for bindings such as esp_lv_present.
 * Failed bind preserves producer identity. Successful bind transfers ownership;
 * unbind releases the session but does not restore the previous producer. */
esp_err_t present_session_bind(struct esp_display_presenter *presenter, esp_display_present_engine_t **out_engine);
void present_session_unbind(struct esp_display_presenter *presenter);
uint64_t present_session_committed(struct esp_display_presenter *presenter);

#ifdef __cplusplus
}
#endif
