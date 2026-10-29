/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_display_present.h"
#include "esp_gsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Run the GSP/LVGL presenter-ownership handoff stress loop. */
void handoff_stress_run(esp_display_presenter_t *presenter,
                        esp_gsp_handle_t gsp) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif
