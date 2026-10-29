/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#include "esp_display_present.h"
#include "esp_display_present_blit.h"
#include "esp_err.h"
#include "esp_display_present_target.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Resolve and validate the immutable target contract. */
esp_err_t present_target_resolve_info(
    const esp_display_present_target_config_t *config,
    esp_display_present_pixel_format_t pixel_format,
    uint16_t width,
    uint16_t height,
    esp_display_present_target_info_t *out_info);

esp_err_t present_target_blit_region(
    esp_display_present_target_t *target,
    const esp_display_present_blit_fb_t *dst,
    const esp_display_presenter_region_t *region);

#ifdef __cplusplus
}
#endif
