/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ESP_DISPLAY_PRESENT_FORMAT_RGB565,
    ESP_DISPLAY_PRESENT_FORMAT_RGB888,
} esp_display_present_engine_pixel_format_t;

/** Byte order of a multi-byte pixel scalar, not RGB/BGR channel order.
 * RGB888 uses R,G,B byte sequence; only NATIVE is valid for RGB888. */
typedef enum {
    ESP_DISPLAY_PRESENT_ORDER_NATIVE,
    ESP_DISPLAY_PRESENT_ORDER_SWAPPED,
} esp_display_present_byte_order_t;

typedef enum {
    ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0,
    ESP_DISPLAY_PRESENT_ENGINE_ROTATE_90,
    ESP_DISPLAY_PRESENT_ENGINE_ROTATE_180,
    ESP_DISPLAY_PRESENT_ENGINE_ROTATE_270,
} esp_display_present_engine_rotation_t;

/** x/y are inclusive origins, width/height are extents; no inclusive ends. */
typedef struct {
    uint32_t x, y, width, height;
} esp_display_present_rect_t;

typedef struct {
    uint32_t width, height;
    size_t stride_bytes;
    esp_display_present_engine_pixel_format_t format;
    esp_display_present_byte_order_t byte_order;
} esp_display_present_layout_t;

typedef struct {
    const void *pixels;
    size_t capacity_bytes;
    esp_display_present_layout_t layout;
} esp_display_present_source_t;

typedef struct {
    void *pixels;
    size_t capacity_bytes;
    esp_display_present_layout_t layout;
} esp_display_present_surface_t;

typedef struct {
    esp_display_present_source_t src;
    esp_display_present_surface_t dst;
    esp_display_present_rect_t src_area;
    uint32_t dst_x, dst_y;
    esp_display_present_engine_rotation_t rotation; /* Clockwise around src_area, then translate. */
} esp_display_present_copy_request_t;

/** Optional blocking executor, called only by the engine worker. On return
 * ALL reads/writes must have ended, even on failure. Hardware implementations
 * must drain/abort safely before returning; an uncertain DMA error cannot be
 * returned as terminal failure while hardware still owns memory. Borrowed
 * context stays valid until successful stop. NULL executes the CPU reference. */
typedef esp_err_t (*esp_display_present_pixel_execute_fn)(void *user, const esp_display_present_copy_request_t *request);

#ifdef __cplusplus
}
#endif
