/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_display_present_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_DISPLAY_PRESENT_MAX_FRAME_BUFFERS 3U

typedef enum {
    ESP_DISPLAY_PRESENT_PANEL_AUTO = 0,
    ESP_DISPLAY_PRESENT_PANEL_MIPI_DSI,
    ESP_DISPLAY_PRESENT_PANEL_RGB,
    ESP_DISPLAY_PRESENT_PANEL_IO,
} esp_display_present_panel_t;

typedef enum {
    /** No tearing avoidance; single buffer or direct transfer. */
    ESP_DISPLAY_PRESENT_MODE_NONE = 0,
    /** Double buffering with full frame buffers. */
    ESP_DISPLAY_PRESENT_MODE_DOUBLE_FULL,
    /** Triple buffering with full frame buffers. */
    ESP_DISPLAY_PRESENT_MODE_TRIPLE_FULL,
    /** Double buffering with direct dirty-area rendering into full buffers. */
    ESP_DISPLAY_PRESENT_MODE_DOUBLE_DIRECT,
    /** Triple buffering with partial/partition refresh. */
    ESP_DISPLAY_PRESENT_MODE_TRIPLE_PARTIAL,
    /** TE GPIO synchronized direct transfer for SPI/I80/QSPI panels. */
    ESP_DISPLAY_PRESENT_MODE_TE_SYNC,
    /** Double buffering with partial/partition refresh. */
    ESP_DISPLAY_PRESENT_MODE_DOUBLE_PARTIAL,
    /** Let esp_display_present choose from panel type, TE config and buffers. */
    ESP_DISPLAY_PRESENT_MODE_AUTO,
} esp_display_present_mode_t;

/** Optional panel TE sync. A zeroed config is disabled; gpio 0 is valid, so
 *  set @c te_enabled when a TE line is wired.
 *
 *  Frame/blanking/transfer timing is measured at runtime. @c bus_freq_hz and
 *  @c data_lines only seed the first-frame transfer estimate before a real
 *  transfer duration has been observed. */
typedef struct {
    int gpio_num;         /**< TE input GPIO number, or -1 when disabled. */
    uint32_t bus_freq_hz; /**< Initial panel-bus frequency estimate in hertz. */
    uint8_t data_lines;   /**< Number of panel-bus data lines. */
} esp_display_present_te_sync_config_t;

#define ESP_DISPLAY_PRESENT_TE_SYNC_DISABLED() \
    (esp_display_present_te_sync_config_t) { .gpio_num = -1 }

/**
 * Panel / bus hardware fact. Borrowed handles stay valid until presenter
 * deletion.
 *
 * RGB and MIPI-DPI have no distinct generic handle shape, so set
 * @c panel_type explicitly for them. Panel-IO/GRAM may use AUTO when @c io
 * is supplied.
 */
typedef struct {
    esp_lcd_panel_handle_t panel; /**< Borrowed initialized panel handle. */
    /** Required for panel-IO/GRAM transfers; otherwise optional. */
    esp_lcd_panel_io_handle_t io;
    esp_display_present_panel_t panel_type; /**< Panel transport category. */
    /** Pixel format accepted by the initialized panel data path. */
    esp_display_present_pixel_format_t input_pixel_format;
    esp_display_present_rotation_t rotation; /**< Logical-to-physical rotation. */
    bool swap_bytes; /**< Whether RGB565 bytes are swapped on transfer. */
    bool te_enabled; /**< Whether TE synchronization is enabled. */
    esp_display_present_te_sync_config_t te_sync; /**< TE synchronization settings. */
} esp_display_present_hw_config_t;

/**
 * Frame-buffer present profile (RGB / MIPI paths).
 *
 * frame_buffers are borrowed for the presenter's lifetime; each buffer must
 * hold the resolved physical frame and satisfy the panel driver's DMA/cache
 * alignment contract. A zero frame_buffer_count asks the component to fetch
 * driver-owned RGB/MIPI framebuffers. Ignored for GRAM partition targets.
 */
typedef struct {
    esp_display_present_mode_t mode; /**< Requested presentation mode. */
    void *frame_buffers[ESP_DISPLAY_PRESENT_MAX_FRAME_BUFFERS]; /**< Borrowed framebuffer addresses. */
    uint8_t frame_buffer_count; /**< Number of valid framebuffer addresses. */
} esp_display_present_fb_config_t;

/** Shared producer drawbuf policy for every partition rendering mode. */
typedef struct {
    uint16_t lines; /**< Preferred height of each partition draw buffer. */
    /**
     * Number of partition draw buffers (1 or 2). 0 selects two for GRAM
     * without TE, and for FB repair and TE only on multi-core targets whose
     * internal DMA heap keeps 128 KiB free after both; two buffers overlap
     * tile copies with rendering at the cost of one more buffer and a 4 KiB
     * worker stack.
     */
    uint8_t buffers;
    bool in_psram; /**< Allocate draw buffers in PSRAM when possible. */
    /**
     * TE_SYNC full-screen compose buffers. 0/1 keeps the synchronous,
     * single-buffer path; 2 enables a rotating compose pool so composition
     * can overlap an in-flight GRAM transfer.
     */
    uint8_t te_compose_buffers;
} esp_display_present_drawbuf_config_t;

/** Target hardware, presentation policy and endpoint memory configuration. */
typedef struct {
    esp_display_present_hw_config_t hw; /**< Panel and bus configuration. */
    esp_display_present_fb_config_t fb; /**< Framebuffer presentation policy. */
    esp_display_present_drawbuf_config_t drawbuf; /**< Partition draw-buffer policy. */
} esp_display_present_target_config_t;

#ifdef __cplusplus
}
#endif
