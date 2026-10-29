/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_display_present_rotate.h"
#include "esp_display_present_target.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_display_present_transform esp_display_present_transform_t;

/** Component-owned logical draw surface for a rotated display target. */
typedef struct {
    void *pixels;
    size_t stride_bytes;
    uint16_t width;
    uint16_t height;
} esp_display_present_transform_surface_t;

/** Create the logical-to-physical transform selected by @p target.
 *  The component owns the logical buffer and optional PPA client. */
esp_err_t esp_display_present_transform_create(
    esp_display_present_target_t *target,
    uint8_t bytes_per_pixel,
    esp_display_present_transform_t **out_transform);

void esp_display_present_transform_delete(
    esp_display_present_transform_t *transform);

esp_err_t esp_display_present_transform_get_logical_surface(
    const esp_display_present_transform_t *transform,
    esp_display_present_transform_surface_t *out_surface);

esp_err_t esp_display_present_transform_get_physical_geometry(
    const esp_display_present_transform_t *transform,
    uint16_t *out_width,
    uint16_t *out_height,
    size_t *out_stride_bytes);

/**
 * Copy one logical area through a component-owned transform.
 *
 * The transform owns rotation and destination geometry. The request therefore
 * carries only the source plane, where it starts in logical space, the
 * destination pixels, and the logical area to update.
 */
typedef struct {
    esp_display_present_blit_plane_t source;
    esp_display_present_point_t source_origin;
    void *destination_pixels;
    esp_display_present_area_t logical_area;
} esp_display_present_transform_copy_request_t;

esp_err_t esp_display_present_transform_copy(
    esp_display_present_transform_t *transform,
    const esp_display_present_transform_copy_request_t *request);

#ifdef __cplusplus
}
#endif
