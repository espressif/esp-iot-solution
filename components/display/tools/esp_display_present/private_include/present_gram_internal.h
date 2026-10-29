/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_display_present_gram.h"
#include "esp_display_present_frame_tracker.h"
#include "esp_display_present_drawbuf.h"

#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

typedef enum {
    GRAM_STOPPED = 0,
    GRAM_RUNNING,
    GRAM_CLOSING,
} gram_life_t;

typedef struct {
    uint64_t frame_id;
    uint8_t buffer_index;
} gram_transfer_t;

struct esp_display_present_gram_endpoint {
    esp_lcd_panel_handle_t panel;
    /** Frame fences and producer callback lifetime. */
    esp_display_present_frame_tracker_t tracker;
    /** Borrowed drawbuf pool metadata; presenter owns the memory. */
    esp_display_present_drawbuf_pool_t drawbuf_pool;
    bool held[ESP_DISPLAY_PRESENT_DRAWBUF_MAX_BUFFERS];
    gram_transfer_t inflight[ESP_DISPLAY_PRESENT_DRAWBUF_MAX_BUFFERS];
    SemaphoreHandle_t available;
    StaticSemaphore_t available_storage;
    portMUX_TYPE lock;
    size_t drawbuf_bytes;
    /** Per-push transfer-done wait budget, resolved at create. */
    uint32_t transfer_timeout_ms;
    uint16_t width;
    uint16_t height;
    uint16_t lines;
    uint8_t buffer_count;
    uint8_t inflight_head;
    uint8_t inflight_count;
    uint8_t life; /**< gram_life_t */
    uint8_t color_bytes;
    bool swap_bytes;
};
