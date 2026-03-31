/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Display Bridge Common - Shared utilities for v8 and v9 bridge implementations
 */

#pragma once

/*********************
 *      INCLUDES
 *********************/
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lvgl.h"
#include "esp_lcd_panel_ops.h"
#if LVGL_VERSION_MAJOR >= 9
#include "lvgl_private.h"
#endif
#include "adapter_internal.h"
#include "esp_attr.h"

/*********************
 *      DEFINES
 *********************/

#define ESP_LV_ADAPTER_BRIDGE_BLOCK_SIZE_SMALL_DEFAULT  (32)
#define ESP_LV_ADAPTER_BRIDGE_BLOCK_SIZE_LARGE_DEFAULT  (256)

#if SOC_DMA2D_SUPPORTED
#include "esp_async_fbcpy.h"
#endif

/* Forward declaration */
typedef struct esp_lv_adapter_display_bridge esp_lv_adapter_display_bridge_t;

/*********************
 *      TYPEDEFS
 *********************/

/**
 * @brief Runtime display information
 */
typedef struct {
    uint16_t hor_res;               /*!< Horizontal resolution in pixels */
    uint16_t ver_res;               /*!< Vertical resolution in pixels */
    esp_lv_adapter_rotation_t rotation;   /*!< Display rotation */
    uint8_t frame_buffer_count;     /*!< Number of frame buffers */
    size_t frame_buffer_size;       /*!< Size of each frame buffer in bytes */
    uint8_t color_bytes;            /*!< Bytes per pixel */
    void *frame_buffers[3];         /*!< Frame buffer pointers (max 3) */
} esp_lv_adapter_display_runtime_info_t;

/**
 * @brief VSYNC timing measurement and jitter detection
 */
typedef struct {
    int64_t vsync_prev_ts_us;       /*!< Previous vsync interrupt timestamp (us) */
    uint32_t refresh_period_us;     /*!< Measured refresh period (us) */
    int64_t flush_post_ts_us;       /*!< Timestamp after last blit/flush (us) */
    uint32_t shield_threshold_us;   /*!< Jitter shield threshold (0 to disable) */
    bool shield_enabled;            /*!< Jitter shield enabled flag */
} esp_lv_adapter_vsync_timing_t;

/**
 * @brief Dirty region tracking for partial updates
 */
typedef struct {
    uint16_t inv_p;                         /*!< Number of invalid areas */
    uint8_t inv_area_joined[LV_INV_BUF_SIZE];  /*!< Join flags for areas */
    lv_area_t inv_areas[LV_INV_BUF_SIZE];  /*!< Invalid area coordinates */
} esp_lv_adapter_display_dirty_region_t;

/**********************
 *   INLINE HELPERS
 **********************/

/**
 * @brief Get display width from runtime info
 */
static inline uint16_t display_runtime_width_px(const esp_lv_adapter_display_runtime_info_t *runtime)
{
    return runtime->hor_res;
}

/**
 * @brief Get display height from runtime info
 */
static inline uint16_t display_runtime_height_px(const esp_lv_adapter_display_runtime_info_t *runtime)
{
    return runtime->ver_res;
}

/**
 * @brief Get display rotation from runtime info
 */
static inline esp_lv_adapter_rotation_t display_runtime_rotation(const esp_lv_adapter_display_runtime_info_t *runtime)
{
    return runtime->rotation;
}

/**
 * @brief Get bytes per pixel from runtime info
 */
static inline uint8_t display_runtime_color_bytes(const esp_lv_adapter_display_runtime_info_t *runtime)
{
    return runtime->color_bytes;
}

/**
 * @brief Reset dirty region tracker
 */
static inline void display_dirty_region_reset(esp_lv_adapter_display_dirty_region_t *region)
{
    region->inv_p = 0;
    memset(region->inv_area_joined, 0, sizeof(region->inv_area_joined));
}

/**
 * @brief Convert LVGL coordinates to physical coordinates with rotation
 *
 * @param lv_x LVGL X coordinate
 * @param lv_y LVGL Y coordinate
 * @param phy_x Output physical X coordinate
 * @param phy_y Output physical Y coordinate
 * @param rotation Display rotation
 * @param hor_res Horizontal resolution
 * @param ver_res Vertical resolution
 */
static inline void display_coord_to_phy(int lv_x, int lv_y, int *phy_x, int *phy_y,
                                        esp_lv_adapter_rotation_t rotation,
                                        uint16_t hor_res,
                                        uint16_t ver_res)
{
    switch (rotation) {
    case ESP_LV_ADAPTER_ROTATE_90:
        *phy_x = hor_res - 1 - lv_y;
        *phy_y = lv_x;
        break;
    case ESP_LV_ADAPTER_ROTATE_180:
        *phy_x = hor_res - 1 - lv_x;
        *phy_y = ver_res - 1 - lv_y;
        break;
    case ESP_LV_ADAPTER_ROTATE_270:
        *phy_x = lv_y;
        *phy_y = ver_res - 1 - lv_x;
        break;
    default:
        *phy_x = lv_x;
        *phy_y = lv_y;
        break;
    }
}

/**********************
 *   PUBLIC API
 **********************/

/* Dirty region management */

/**
 * @brief Capture current dirty region from LVGL
 *
 * @param dst Destination dirty region structure
 * @param areas Source area array from LVGL
 * @param joined Join flags array from LVGL
 * @param count Number of areas
 */
void display_dirty_region_capture(esp_lv_adapter_display_dirty_region_t *dst,
                                  const lv_area_t *areas,
                                  const uint8_t *joined,
                                  uint16_t count);

/* Rotation and copy operations */

/**
 * @brief Rotate and copy a region with stride support (IRAM optimized)
 *
 * @param src Source buffer
 * @param dst_fb Destination framebuffer
 * @param lv_x_start Start X in LVGL coordinates
 * @param lv_y_start Start Y in LVGL coordinates
 * @param lv_x_end End X in LVGL coordinates
 * @param lv_y_end End Y in LVGL coordinates
 * @param src_stride_px Source stride in pixels
 * @param hor_res Horizontal resolution
 * @param ver_res Vertical resolution
 * @param rotation Display rotation
 * @param color_bytes Bytes per pixel
 * @param block_size_small Small block size for cache optimization
 * @param block_size_large Large block size for cache optimization
 */
void display_rotate_copy_region(const void *src,
                                void *dst_fb,
                                uint16_t lv_x_start,
                                uint16_t lv_y_start,
                                uint16_t lv_x_end,
                                uint16_t lv_y_end,
                                uint16_t src_stride_px,
                                uint16_t hor_res,
                                uint16_t ver_res,
                                esp_lv_adapter_rotation_t rotation,
                                uint8_t color_bytes,
                                int block_size_small,
                                int block_size_large);

/**
 * @brief Rotate an entire image (IRAM optimized)
 *
 * @param src Source image buffer
 * @param dst Destination buffer
 * @param width Image width
 * @param height Image height
 * @param rotation Rotation angle (0, 90, 180, 270)
 * @param color_bytes Bytes per pixel
 * @param block_size_small Small block size for cache optimization
 * @param block_size_large Large block size for cache optimization
 */
void display_rotate_image(const void *src,
                          void *dst,
                          int width,
                          int height,
                          int rotation,
                          uint8_t color_bytes,
                          int block_size_small,
                          int block_size_large);

/* Cache management */

/**
 * @brief Synchronize cache for a specific memory range
 *
 * @param addr Starting address
 * @param size Size in bytes
 * @param cache_line_size Cache line size for alignment
 */
void display_cache_msync_range(const void *addr,
                               size_t size,
                               size_t cache_line_size);

/**
 * @brief Synchronize cache for a framebuffer (allows unaligned)
 *
 * @param buffer Framebuffer pointer
 * @param size Framebuffer size in bytes
 */
void display_cache_msync_framebuffer(void *buffer,
                                     size_t size);

/* LCD operations */

/**
 * @brief Send full framebuffer to LCD panel
 *
 * @param panel LCD panel handle
 * @param runtime Runtime display information
 * @param frame_buffer Framebuffer to send
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t display_lcd_blit_full(esp_lcd_panel_handle_t panel,
                                const esp_lv_adapter_display_runtime_info_t *runtime,
                                void *frame_buffer);

/**
 * @brief Send framebuffer region to LCD panel
 *
 * @param panel LCD panel handle
 * @param x_start Starting X coordinate (inclusive)
 * @param y_start Starting Y coordinate (inclusive)
 * @param x_end Ending X coordinate (exclusive)
 * @param y_end Ending Y coordinate (exclusive)
 * @param frame_buffer Framebuffer data for the specified region
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t display_lcd_blit_area(esp_lcd_panel_handle_t panel,
                                int x_start,
                                int y_start,
                                int x_end,
                                int y_end,
                                const void *frame_buffer);

/**
 * @brief Acquire next available frame buffer for double/triple buffering
 *
 * @param runtime Runtime display information
 * @param toggle_slot Pointer to toggle state variable
 * @return void* Next available buffer, or NULL if unavailable
 */
void *display_runtime_acquire_next_buffer(esp_lv_adapter_display_runtime_info_t *runtime,
                                          void **toggle_slot);

void display_bridge_get_block_sizes(int *block_small, int *block_large);

size_t display_bridge_get_cache_line_size(void);

/* Pipeline management */

/**
 * @brief Initialize pipeline from runtime config (call from bridge create with impl's pipeline and cfg)
 *
 * Decides whether pipeline is used from cfg->base.tear_avoid_mode and rotation.
 * If used, inits the pipeline (lock, lists, elems) and sets *out_disp_fb and *out_draw_fb.
 * Caller must free pipeline->elems in bridge destroy.
 *
 * @param pipeline Pipeline to init (e.g. &impl->pipeline)
 * @param cfg Runtime config (base, frame_buffers, frame_buffer_count must be set)
 * @param out_disp_fb Output display (front) buffer pointer (e.g. &impl->disp_fb)
 * @param out_draw_fb Output draw (back) buffer pointer (e.g. &impl->draw_fb)
 * @return 1 if pipeline in use and inited, 0 if pipeline not used, -1 on alloc failure
 */
int display_bridge_pipeline_init_from_cfg(esp_lv_adapter_display_pipeline_t *pipeline,
                                          esp_lv_adapter_display_runtime_config_t *cfg,
                                          void **out_disp_fb, void **out_draw_fb);

/**
 * @brief Mark an element in the pipeline as busy
 */
void display_bridge_pipeline_mark_buf_busy(esp_lv_adapter_display_pipeline_t *p, void *buf);

/**
 * @brief Release a buffer from the busy list to the empty list (ISR context)
 *
 * Caller contract: when this is called from an ISR, the caller MUST subsequently
 * call vTaskNotifyGiveFromISR() on the task that may be blocked in
 * display_bridge_pipeline_wait_free_buf() (typically the LVGL adapter task).
 * Otherwise the waiter will never be woken. See v8/v9 VSync handlers for the
 * pattern: release_buf_isr(pipeline) then vTaskNotifyGiveFromISR(notify_task, &need_yield).
 */
void display_bridge_pipeline_release_buf_isr(esp_lv_adapter_display_pipeline_t *p);

/**
 * @brief Take a free buffer from the pipeline (no wait)
 */
struct display_pipeline_buf *display_bridge_pipeline_take_free_buf(esp_lv_adapter_display_pipeline_t *p);

/**
 * @brief Wait for a free buffer from the pipeline (uses ulTaskNotifyTake)
 *
 * Blocks the current task until a buffer is moved to the empty list. The task is
 * woken by the same notification sent after display_bridge_pipeline_release_buf_isr()
 * in the VSync (or equivalent) ISR via vTaskNotifyGiveFromISR(adapter_task).
 */
struct display_pipeline_buf *display_bridge_pipeline_wait_free_buf(esp_lv_adapter_display_pipeline_t *p);

/**
 * @brief Initialize runtime information from configuration
 *
 * @note This function is 100% identical in v8 and v9
 *
 * @param runtime Runtime info structure to initialize
 * @param cfg Display runtime configuration
 */
void display_bridge_init_runtime_info(esp_lv_adapter_display_runtime_info_t *runtime,
                                      const esp_lv_adapter_display_runtime_config_t *cfg);

/**
 * @brief Record timestamp after a successful flush/blit operation
 */
void display_bridge_vsync_record_flush_post(esp_lv_adapter_vsync_timing_t *t);

/**
 * @brief Update VSYNC timing and check for jitter (ISR context)
 *
 * @param t VSYNC timing context
 * @return true if jitter detected (should skip/delay), false otherwise
 */
bool display_bridge_vsync_on_isr(esp_lv_adapter_vsync_timing_t *t);

/**
 * @brief Configure VSYNC jitter shield
 *
 * @param t VSYNC timing context
 * @param threshold_us Threshold in microseconds (e.g. 1000), 0 to disable
 */
void display_bridge_vsync_config_jitter_shield(esp_lv_adapter_vsync_timing_t *t, uint32_t threshold_us);

/**
 * @brief Get the measured refresh rate for the interface in Hz
 *
 * @param t VSYNC timing context
 * @return Refresh rate in Hz, or 0 if not measured yet
 */
uint32_t display_bridge_vsync_get_refresh_rate_hz(esp_lv_adapter_vsync_timing_t *t);

#if SOC_DMA2D_SUPPORTED
/**
 * @brief Hardware resource structure for DMA2D/PPA
 */
typedef struct {
    size_t data_cache_line_size;
#if CONFIG_SOC_PPA_SUPPORTED
    void *ppa_handle;  /* ppa_client_handle_t */
#endif
#if CONFIG_SOC_DMA2D_SUPPORTED
    void *fbcpy_handle;  /* esp_async_fbcpy_handle_t */
    void *dma2d_mutex;   /* SemaphoreHandle_t */
    void *dma2d_done_sem; /* SemaphoreHandle_t */
#endif
} esp_lv_adapter_display_bridge_hw_resource_t;

/**
 * @brief Get global hardware resource singleton
 *
 * Acquires hardware resources with reference counting. The first call
 * initializes PPA and DMA2D hardware. Subsequent calls increment the
 * reference count.
 *
 * @return Pointer to hardware resource structure, or NULL on failure
 */
esp_lv_adapter_display_bridge_hw_resource_t *display_bridge_get_hw_resource(void);

/**
 * @brief Release hardware resource reference
 *
 * Decrements the reference count. When the count reaches zero, all hardware
 * resources (PPA, DMA2D, semaphores) are freed.
 *
 * This function should be called when a display is destroyed.
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Resources not initialized
 */
esp_err_t display_bridge_release_hw_resource(void);

/**
 * @brief DMA2D completion callback
 *
 * @note This function is 100% identical in v8 and v9
 */
bool display_bridge_dma2d_done_callback(esp_async_fbcpy_handle_t mcp,
                                        esp_async_fbcpy_event_data_t *event_data,
                                        void *cb_args);

/**
 * @brief Synchronous DMA2D copy operation
 *
 * @note This function is 100% identical in v8 and v9
 *
 * @param trans_desc DMA transfer descriptor
 * @param timeout_ms Timeout in milliseconds
 * @return esp_err_t ESP_OK on success
 */
esp_err_t display_bridge_dma2d_copy_sync(void *trans_desc, uint32_t timeout_ms);
#endif /* SOC_DMA2D_SUPPORTED */

/**
 * @brief Destroy display bridge and release resources
 *
 * Releases hardware resources (PPA, DMA2D) with reference counting and
 * frees the bridge structure. Safe to call with NULL pointer.
 *
 * @note This function is 100% identical in v8 and v9
 *
 * @param bridge Display bridge structure to destroy
 */
void display_bridge_common_destroy(esp_lv_adapter_display_bridge_t *bridge);
