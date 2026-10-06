/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ESP_DISPLAY_PRESENT_PANEL_IF_RGB = 0,
    ESP_DISPLAY_PRESENT_PANEL_IF_MIPI_DSI = 1,
    ESP_DISPLAY_PRESENT_PANEL_IF_OTHER = 2,
} esp_display_present_panel_interface_t;

typedef enum {
    ESP_DISPLAY_PRESENT_ROTATE_0 = 0,
    ESP_DISPLAY_PRESENT_ROTATE_90 = 90,
    ESP_DISPLAY_PRESENT_ROTATE_180 = 180,
    ESP_DISPLAY_PRESENT_ROTATE_270 = 270,
} esp_display_present_rotation_t;

typedef enum {
    ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565 = 0,
    ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB888,
} esp_display_present_pixel_format_t;

/**
 * Why a presenter latched a fault. A transfer timeout is a hardware-level
 * condition (for example bus jitter or a slow panel) that the application
 * may recover from by deleting and recreating the presenter; a producer
 * protocol fault is a producer-side bug and has no in-place recovery.
 */
typedef enum {
    ESP_DISPLAY_PRESENT_FAULT_NONE = 0,
    /** Ticket/order violation in the producer build or fence protocol. */
    ESP_DISPLAY_PRESENT_FAULT_PRODUCER_PROTOCOL,
    /** A panel transfer failed to complete within the configured budget. */
    ESP_DISPLAY_PRESENT_FAULT_TRANSFER_TIMEOUT,
} esp_display_present_fault_reason_t;

/** Logical pixel coordinate. */
typedef struct {
    int32_t x; /**< Horizontal coordinate. */
    int32_t y; /**< Vertical coordinate. */
} esp_display_present_point_t;

/** Two-dimensional size in pixels. */
typedef struct {
    uint16_t width;  /**< Width in pixels. */
    uint16_t height; /**< Height in pixels. */
} esp_display_present_size_t;

/** Inclusive logical pixel area. */
typedef struct {
    /** Inclusive logical coordinates (closed interval): x2/y2 name the
     *  last affected pixel. The GSP-side counterpart gsp_rect_t
     *  (gsp_types.h) is open-interval [x1, x2) x [y1, y2). */
    int x1; /**< Left coordinate. */
    int y1; /**< Top coordinate. */
    int x2; /**< Right coordinate. */
    int y2; /**< Bottom coordinate. */
} esp_display_present_area_t;

/**
 * Producer raster-area requirements. Zero fields mean no extra alignment.
 * The presenter expands damage conservatively and clips it to the logical
 * display before returning render areas.
 */
typedef struct {
    uint16_t x_pixels;      /**< Required X-origin alignment in pixels. */
    uint16_t y_pixels;      /**< Required Y-origin alignment in pixels. */
    uint16_t width_pixels;  /**< Required area-width alignment in pixels. */
    uint16_t height_pixels; /**< Required area-height alignment in pixels. */
} esp_display_present_render_alignment_t;

/**
 * Logical damage supplied when preparing a surface frame.
 *
 * The presenter may copy the previous displayed surface into the new draw
 * surface and may conservatively request a full raster when scroll state or
 * buffer history cannot be reused safely.
 */
typedef struct {
    const esp_display_present_area_t *dirty_areas; /**< Logical dirty areas. */
    size_t dirty_area_count; /**< Number of entries in @c dirty_areas. */
    esp_display_present_render_alignment_t render_alignment; /**< Producer raster alignment. */
    bool has_scroll; /**< Whether this request contains a scroll operation. */
    esp_display_present_area_t scroll_viewport; /**< Logical viewport affected by scrolling. */
    int16_t scroll_dx; /**< Horizontal scroll displacement in pixels. */
    int16_t scroll_dy; /**< Vertical scroll displacement in pixels. */
} esp_display_present_surface_request_t;

/** Framebuffer storage returned by a panel driver. */
typedef struct {
    void *buffers[3]; /**< Framebuffer addresses. */
    uint8_t count;    /**< Number of valid framebuffer addresses. */
    size_t size_bytes; /**< Size of each framebuffer in bytes. */
} esp_display_present_frame_buffers_t;

#ifdef __cplusplus
}
#endif
