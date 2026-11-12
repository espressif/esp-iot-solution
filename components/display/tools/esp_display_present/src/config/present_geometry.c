/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "config/present_geometry.h"
#include <limits.h>

static int32_t align_down(int32_t value, uint16_t alignment)
{
    return alignment > 1 ? value - value % alignment : value;
}

static int32_t align_up(int32_t value, uint16_t alignment)
{
    if (alignment <= 1) {
        return value;
    }
    int32_t remainder = value % alignment;
    return remainder != 0 ? value + alignment - remainder : value;
}

esp_err_t esp_display_present_geometry_align_area(
    esp_display_present_size_t logical_size,
    const esp_display_present_area_t *area,
    const esp_display_present_render_alignment_t *alignment,
    esp_display_present_area_t *out_area)
{
    if (alignment == NULL || out_area == NULL ||
            !esp_display_present_geometry_area_is_valid(
                area, logical_size.width, logical_size.height)) {
        return ESP_ERR_INVALID_ARG;
    }
    uint16_t x_alignment = alignment->x_pixels != 0
                           ? alignment->x_pixels : 1;
    uint16_t y_alignment = alignment->y_pixels != 0
                           ? alignment->y_pixels : 1;
    uint16_t w_align = alignment->width_pixels != 0
                       ? alignment->width_pixels : 1;
    uint16_t h_align = alignment->height_pixels != 0
                       ? alignment->height_pixels : 1;
    int32_t x1 = align_down(area->x1, x_alignment);
    int32_t y1 = align_down(area->y1, y_alignment);
    int32_t x2 = x1 + align_up(area->x2 - x1 + 1, w_align) - 1;
    int32_t y2 = y1 + align_up(area->y2 - y1 + 1, h_align) - 1;
    if (x2 >= logical_size.width) {
        x2 = logical_size.width - 1;
    }
    if (y2 >= logical_size.height) {
        y2 = logical_size.height - 1;
    }
    *out_area = (esp_display_present_area_t) {
        .x1 = x1,
        .y1 = y1,
        .x2 = x2,
        .y2 = y2,
    };
    return ESP_OK;
}

static void physical_size(esp_display_present_rotation_t rotation,
                          uint16_t logical_width, uint16_t logical_height,
                          int32_t *out_width, int32_t *out_height)
{
    if (rotation == ESP_DISPLAY_PRESENT_ROTATE_90 ||
            rotation == ESP_DISPLAY_PRESENT_ROTATE_270) {
        *out_width = logical_height;
        *out_height = logical_width;
    } else {
        *out_width = logical_width;
        *out_height = logical_height;
    }
}

esp_err_t esp_display_present_geometry_map_logical_point_to_physical(
    esp_display_present_rotation_t rotation,
    esp_display_present_size_t logical_size,
    const esp_display_present_point_t *logical_point,
    esp_display_present_point_t *out_physical_point)
{
    if (logical_size.width == 0 || logical_size.height == 0 ||
            logical_point == NULL || out_physical_point == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    int32_t physical_width;
    int32_t physical_height;
    physical_size(rotation, logical_size.width, logical_size.height,
                  &physical_width, &physical_height);

    switch (rotation) {
    case ESP_DISPLAY_PRESENT_ROTATE_90:
        *out_physical_point = (esp_display_present_point_t) {
            .x = physical_width - 1 - logical_point->y,
            .y = logical_point->x,
        };
        break;
    case ESP_DISPLAY_PRESENT_ROTATE_180:
        *out_physical_point = (esp_display_present_point_t) {
            .x = physical_width - 1 - logical_point->x,
            .y = physical_height - 1 - logical_point->y,
        };
        break;
    case ESP_DISPLAY_PRESENT_ROTATE_270:
        *out_physical_point = (esp_display_present_point_t) {
            .x = logical_point->y,
            .y = physical_height - 1 - logical_point->x,
        };
        break;
    case ESP_DISPLAY_PRESENT_ROTATE_0:
    default:
        *out_physical_point = *logical_point;
        break;
    }
    return ESP_OK;
}

esp_err_t esp_display_present_geometry_map_physical_point_to_logical(
    esp_display_present_rotation_t rotation,
    esp_display_present_size_t logical_size,
    const esp_display_present_point_t *physical_point,
    esp_display_present_point_t *out_logical_point)
{
    if (logical_size.width == 0 || logical_size.height == 0 ||
            physical_point == NULL || out_logical_point == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    int64_t x = physical_point->x, y = physical_point->y;
    switch (rotation) {
    case ESP_DISPLAY_PRESENT_ROTATE_90:
        x = physical_point->y;
        y = (int64_t)logical_size.height - 1 - physical_point->x;
        break;
    case ESP_DISPLAY_PRESENT_ROTATE_180:
        x = (int64_t)logical_size.width - 1 - physical_point->x;
        y = (int64_t)logical_size.height - 1 - physical_point->y;
        break;
    case ESP_DISPLAY_PRESENT_ROTATE_270:
        x = (int64_t)logical_size.width - 1 - physical_point->y;
        y = physical_point->x;
        break;
    case ESP_DISPLAY_PRESENT_ROTATE_0:
    default:
        break;
    }
    if (x < INT32_MIN || x > INT32_MAX || y < INT32_MIN || y > INT32_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_logical_point = (esp_display_present_point_t) {
        .x = (int32_t)x,
        .y = (int32_t)y,
    };
    return ESP_OK;
}
