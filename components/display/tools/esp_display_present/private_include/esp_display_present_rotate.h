/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#include "esp_display_present_blit.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * One logical area copied from a source plane into a physical destination.
 *
 * @p source_origin identifies the logical coordinate stored at source[0, 0].
 * This lets a tight tile and a full logical frame use the same operation.
 */
typedef struct {
    esp_display_present_blit_plane_t source;
    esp_display_present_point_t source_origin;
    esp_display_present_blit_plane_t destination;
    esp_display_present_area_t logical_area;
    esp_display_present_rotation_t rotation;
} esp_display_present_rotate_copy_request_t;

/** Software fallback for one rotate-copy request. Not a public API. */
void esp_display_present_rotate_copy(
    const esp_display_present_rotate_copy_request_t *request);

#ifdef __cplusplus
}
#endif
