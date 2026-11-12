/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "backend/present_backend_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Shared backend configuration constraints. Only checks that the plan and
 * the backend create paths already enforced with the same error code live
 * here: dimensions, pixel format, byte orders, rotation, render-contract
 * range and frame-record bounds, all as ESP_ERR_INVALID_ARG.
 *
 * Layer-specific rules deliberately stay with their owners, including where
 * they intentionally disagree with this helper's callers:
 * - resources: defaults, memory preference and shortage fallback policy;
 * - plan: normalization, output capability, layout and budget constraints
 *   (including the INT_MAX geometry bound);
 * - backend create: panel callbacks, per-backend buffer/tile/compose limits,
 *   external-buffer rules, and couplings such as tx_staging/in_place whose
 *   error codes differ per layer (INVALID_ARG vs NOT_SUPPORTED). TE create
 *   keeps its own draw-side checks and does not call this helper; the planned
 *   path still applies the shared checks through plan resolve.
 * Adding a check here must not turn a previously ignored field into a new
 * rejection at any layer. */
esp_err_t present_backend_config_validate_shared(const esp_display_present_backend_config_t *config);

#ifdef __cplusplus
}
#endif
