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

/** Driver completion feedback. */
typedef enum {
    /** All frame transfers completed; no scanout event is available. */
    ESP_DISPLAY_PRESENT_COMPLETION_TRANSFER = 0,
    /** Hardware VSYNC available; confirmed_present_frame tracks display confirmation. */
    ESP_DISPLAY_PRESENT_COMPLETION_FRAMEBUFFER,
} esp_display_present_completion_t;

/** Frame and binding identity. Rotation changes generation; frame_id=0 means
 * commit failed. IDs are valid only during this presenter's lifetime. */
typedef struct {
    uint64_t frame_id; /**< Committed frame ID; zero on failure. */
    uint32_t generation; /**< Binding generation that issued this receipt. */
    bool display_tracked; /**< Only tracked receipts use confirmed_present_frame. */
} esp_display_presenter_frame_receipt_t;

/** Coherent frame timeline; zero means no frame has reached that point. */
typedef struct {
    uint32_t generation; /**< Current binding generation. */
    uint64_t last_submitted_frame; /**< Latest submitted frame ID. */
    uint64_t completed_transfer_frame; /**< Latest frame with completed transfers. */
    /** Ownership fence; coalesced submissions do not prove every frame was displayed. */
    uint64_t completed_present_frame;
    /** Tracked frames only: switch fence plus two later RGB/MIPI VSYNCs. */
    uint64_t confirmed_present_frame;
    esp_display_present_completion_t completion; /**< Available driver completion feedback. */
    esp_display_present_fault_reason_t fault_reason; /**< Latched transfer or protocol fault. */
    /** Separate transfer queue; otherwise transfer follows the present fence. */
    bool tracks_transfer;
    bool accepting_callbacks; /**< Whether completion callbacks are currently accepted. */
} esp_display_presenter_frame_status_t;

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
