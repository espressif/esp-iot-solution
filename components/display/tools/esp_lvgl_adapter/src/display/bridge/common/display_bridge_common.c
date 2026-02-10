/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Display Bridge Common - Shared implementation for display bridge operations
 */

/*********************
 *      INCLUDES
 *********************/
#include "display_bridge_common.h"

#include <string.h>

#include "esp_cache.h"
#include "esp_private/esp_cache_private.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "esp_lvgl:bridge";

/**********************
 *   CACHE PROFILE
 **********************/

static size_t s_bridge_cache_line_size = 0;
static int s_bridge_block_size_small = ESP_LV_ADAPTER_BRIDGE_BLOCK_SIZE_SMALL_DEFAULT;
static int s_bridge_block_size_large = ESP_LV_ADAPTER_BRIDGE_BLOCK_SIZE_LARGE_DEFAULT;

static void display_bridge_init_cache_profile(void)
{
    if (s_bridge_cache_line_size) {
        return;
    }

    size_t align = 0;
    if (esp_cache_get_alignment(MALLOC_CAP_INTERNAL, &align) != ESP_OK || align == 0) {
        if (esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &align) != ESP_OK || align == 0) {
            align = ESP_LV_ADAPTER_BRIDGE_BLOCK_SIZE_SMALL_DEFAULT;
        }
    }

    if (align < ESP_LV_ADAPTER_BRIDGE_BLOCK_SIZE_SMALL_DEFAULT) {
        align = ESP_LV_ADAPTER_BRIDGE_BLOCK_SIZE_SMALL_DEFAULT;
    }

    s_bridge_cache_line_size = align;

    int block_small = (int)align;
    if (block_small <= 0) {
        block_small = ESP_LV_ADAPTER_BRIDGE_BLOCK_SIZE_SMALL_DEFAULT;
    }

    int block_large = block_small * 8;
    if (block_large < ESP_LV_ADAPTER_BRIDGE_BLOCK_SIZE_LARGE_DEFAULT) {
        block_large = ESP_LV_ADAPTER_BRIDGE_BLOCK_SIZE_LARGE_DEFAULT;
    }
    if (block_large > 512) {
        block_large = 512;
    }

    s_bridge_block_size_small = block_small;
    s_bridge_block_size_large = block_large;
}

/**********************
 *   DIRTY REGION MANAGEMENT
 **********************/

/**
 * @brief Capture current dirty region from LVGL
 */
void display_dirty_region_capture(esp_lv_adapter_display_dirty_region_t *dst,
                                  const lv_area_t *areas,
                                  const uint8_t *joined,
                                  uint16_t count)
{
    if (!dst) {
        return;
    }

    uint16_t stored = LV_MIN(count, (uint16_t)LV_INV_BUF_SIZE);

    if (!areas || !joined || stored == 0) {
        dst->inv_p = 0;
        memset(dst->inv_area_joined, 0, sizeof(dst->inv_area_joined));
        return;
    }

    dst->inv_p = stored;
    for (uint16_t i = 0; i < stored; i++) {
        dst->inv_area_joined[i] = joined[i];
        dst->inv_areas[i] = areas[i];
    }
}

/**********************
 *   ROTATION & COPY OPERATIONS
 **********************/

/**
 * @brief Rotate and copy a region with stride support (IRAM optimized)
 *
 * Uses block-based copying for better cache locality
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
                                int block_size_large)
{
    const int rect_w = lv_x_end - lv_x_start + 1;
    const int rect_h = lv_y_end - lv_y_start + 1;
    const int phys_w = hor_res;

    if (rotation == ESP_LV_ADAPTER_ROTATE_0) {
        const uint8_t *src_base = (const uint8_t *)src;
        uint8_t *dst_base = (uint8_t *)dst_fb + (size_t)(lv_y_start * phys_w + lv_x_start) * color_bytes;
        size_t row_bytes = rect_w * color_bytes;
        size_t dst_stride_bytes = phys_w * color_bytes;
        size_t src_stride_bytes = src_stride_px * color_bytes;

        bool is_contiguous = (src_stride_px == rect_w) && (rect_w == phys_w) && (lv_x_start == 0);

        if (is_contiguous) {
            size_t total_bytes = rect_h * row_bytes;
            memcpy(dst_base, src_base, total_bytes);
        } else {
            for (int y = 0; y < rect_h; y++) {
                memcpy(dst_base, src_base, row_bytes);
                src_base += src_stride_bytes;
                dst_base += dst_stride_bytes;
            }
        }
        return;
    }

    if (rotation == ESP_LV_ADAPTER_ROTATE_180) {
        for (int y = 0; y < rect_h; y++) {
            const uint8_t *src_row = (const uint8_t *)src + (size_t)y * src_stride_px * color_bytes;
            int dst_y = ver_res - 1 - (lv_y_start + y);
            int dst_x_start = hor_res - 1 - lv_x_end;
            uint8_t *dst_row = (uint8_t *)dst_fb + (size_t)(dst_y * phys_w + dst_x_start) * color_bytes;

            if (color_bytes == 2) {
                uint16_t *dst16 = (uint16_t *)(dst_row + (rect_w - 1) * 2);
                const uint16_t *src16 = (const uint16_t *)src_row;
                for (int x = 0; x < rect_w; x++) {
                    *dst16-- = *src16++;
                }
            } else if (color_bytes == 4) {
                uint32_t *dst32 = (uint32_t *)(dst_row + (rect_w - 1) * 4);
                const uint32_t *src32 = (const uint32_t *)src_row;
                for (int x = 0; x < rect_w; x++) {
                    *dst32-- = *src32++;
                }
            } else {
                for (int x = 0; x < rect_w; x++) {
                    const uint8_t *src_pixel = src_row + (size_t)x * color_bytes;
                    uint8_t *dst_pixel = dst_row + (size_t)(rect_w - 1 - x) * color_bytes;
                    memcpy(dst_pixel, src_pixel, color_bytes);
                }
            }
        }
        return;
    }

    /* Select block size based on rotation for optimal cache usage */
    int block_w = (rotation == ESP_LV_ADAPTER_ROTATE_90 || rotation == ESP_LV_ADAPTER_ROTATE_270) ? block_size_small : block_size_large;
    int block_h = (rotation == ESP_LV_ADAPTER_ROTATE_90 || rotation == ESP_LV_ADAPTER_ROTATE_270) ? block_size_large : block_size_small;

    /* Process in blocks for better cache locality */
    for (int i = 0; i < rect_h; i += block_h) {
        int max_height = (i + block_h > rect_h) ? rect_h : i + block_h;

        for (int j = 0; j < rect_w; j += block_w) {
            int max_width = (j + block_w > rect_w) ? rect_w : j + block_w;

            /* Copy pixels within current block - fully inlined for performance */
            for (int y = i; y < max_height; y++) {
                for (int x = j; x < max_width; x++) {
                    int gx = lv_x_start + x;
                    int gy = lv_y_start + y;
                    int dx, dy;

                    /* Inline coordinate transformation - eliminates function call overhead */
                    if (rotation == ESP_LV_ADAPTER_ROTATE_90) {
                        dx = phys_w - 1 - gy;
                        dy = gx;
                    } else { /* ESP_LV_ADAPTER_ROTATE_270 */
                        dx = gy;
                        dy = ver_res - 1 - gx;
                    }

                    const uint8_t *src_pixel = (const uint8_t *)src + (size_t)(y * src_stride_px + x) * color_bytes;
                    uint8_t *dst_pixel = (uint8_t *)dst_fb + (size_t)(dy * phys_w + dx) * color_bytes;

                    /* Inline pixel copy - eliminates function call overhead */
                    if (color_bytes == 2) {
                        *(uint16_t *)dst_pixel = *(const uint16_t *)src_pixel;
                    } else if (color_bytes == 3) {
                        dst_pixel[0] = src_pixel[0];
                        dst_pixel[1] = src_pixel[1];
                        dst_pixel[2] = src_pixel[2];
                    }
                }
            }
        }
    }
}

/**
 * @brief Rotate an entire image (IRAM optimized)
 *
 * Uses block-based rotation for better cache locality
 */
void display_rotate_image(const void *src,
                          void *dst,
                          int width,
                          int height,
                          int rotation,
                          uint8_t color_bytes,
                          int block_size_small,
                          int block_size_large)
{
    /* Select block size based on rotation */
    int block_w = (rotation == 90 || rotation == 270) ? block_size_small : block_size_large;
    int block_h = (rotation == 90 || rotation == 270) ? block_size_large : block_size_small;

    /* Process in blocks */
    for (int i = 0; i < height; i += block_h) {
        int max_height = (i + block_h > height) ? height : i + block_h;

        for (int j = 0; j < width; j += block_w) {
            int max_width = (j + block_w > width) ? width : j + block_w;

            /* Rotate pixels within current block - fully inlined for performance */
            for (int x = i; x < max_height; x++) {
                for (int y = j; y < max_width; y++) {
                    const uint8_t *src_pixel = (const uint8_t *)src + (size_t)(x * width + y) * color_bytes;
                    uint8_t *dst_pixel;

                    switch (rotation) {
                    case 270:
                        dst_pixel = (uint8_t *)dst + (size_t)((width - 1 - y) * height + x) * color_bytes;
                        break;
                    case 180:
                        dst_pixel = (uint8_t *)dst + (size_t)((height - 1 - x) * width + (width - 1 - y)) * color_bytes;
                        break;
                    case 90:
                        dst_pixel = (uint8_t *)dst + (size_t)(y * height + (height - 1 - x)) * color_bytes;
                        break;
                    default:
                        return;
                    }

                    /* Inline pixel copy - eliminates function call overhead */
                    if (color_bytes == 2) {
                        *(uint16_t *)dst_pixel = *(const uint16_t *)src_pixel;
                    } else if (color_bytes == 3) {
                        dst_pixel[0] = src_pixel[0];
                        dst_pixel[1] = src_pixel[1];
                        dst_pixel[2] = src_pixel[2];
                    }
                }
            }
        }
    }
}

/**********************
 *   CACHE MANAGEMENT
 **********************/

/**
 * @brief Synchronize cache for a specific memory range
 *
 * Note: Not placed in IRAM as it calls esp_cache_msync() which is also not in IRAM
 */
void display_cache_msync_range(const void *addr,
                               size_t size,
                               size_t cache_line_size)
{
    if (!addr || size == 0 || cache_line_size == 0) {
        return;
    }

    /* Align to cache line boundaries */
    uintptr_t start_addr = (uintptr_t)addr;
    uintptr_t aligned_start = start_addr & ~((uintptr_t)cache_line_size - 1);
    size_t align_padding = start_addr - aligned_start;
    size_t aligned_size = ((size + align_padding + cache_line_size - 1) / cache_line_size) * cache_line_size;

    esp_cache_msync((void *)aligned_start, aligned_size, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
}

/**
 * @brief Synchronize cache for a framebuffer (allows unaligned)
 *
 * Note: Not placed in IRAM as it calls esp_cache_msync() which is also not in IRAM
 */
void display_cache_msync_framebuffer(void *buffer,
                                     size_t size)
{
    if (!buffer || size == 0) {
        return;
    }

    esp_cache_msync(buffer, size,
                    ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
}

/**********************
 *   LCD OPERATIONS
 **********************/

/**
 * @brief Send full framebuffer to LCD panel
 */
esp_err_t display_lcd_blit_full(esp_lcd_panel_handle_t panel,
                                const esp_lv_adapter_display_runtime_info_t *runtime,
                                void *frame_buffer)
{
    if (!panel || !runtime || !frame_buffer) {
        return ESP_ERR_INVALID_ARG;
    }

    return esp_lcd_panel_draw_bitmap(panel,
                                     0,
                                     0,
                                     runtime->hor_res,
                                     runtime->ver_res,
                                     frame_buffer);
}

/**
 * @brief Send framebuffer region to LCD panel
 */
esp_err_t display_lcd_blit_area(esp_lcd_panel_handle_t panel,
                                int x_start,
                                int y_start,
                                int x_end,
                                int y_end,
                                const void *frame_buffer)
{
    if (!panel || !frame_buffer) {
        return ESP_ERR_INVALID_ARG;
    }

    return esp_lcd_panel_draw_bitmap(panel,
                                     x_start,
                                     y_start,
                                     x_end,
                                     y_end,
                                     frame_buffer);
}

/**
 * @brief Acquire next available frame buffer for double/triple buffering
 */
void *display_runtime_acquire_next_buffer(esp_lv_adapter_display_runtime_info_t *runtime,
                                          void **toggle_slot)
{
    if (!runtime || !toggle_slot) {
        return NULL;
    }

    void **frame_buffers = runtime->frame_buffers;
    uint8_t count = runtime->frame_buffer_count;

    /* No buffers available */
    if (count == 0 || frame_buffers[0] == NULL) {
        *toggle_slot = NULL;
        return NULL;
    }

    /* Single buffer mode */
    if (count == 1 || frame_buffers[1] == NULL) {
        *toggle_slot = frame_buffers[0];
        return frame_buffers[0];
    }

    /* Double buffering - toggle between two buffers */
    void *primary = frame_buffers[0];
    void *secondary = frame_buffers[1];
    void *current = *toggle_slot;

    if (current != primary && current != secondary) {
        current = secondary;
    } else {
        current = (current == primary) ? secondary : primary;
    }

    *toggle_slot = current;
    return current;
}

void display_bridge_get_block_sizes(int *block_small, int *block_large)
{
    display_bridge_init_cache_profile();
    if (block_small) {
        *block_small = s_bridge_block_size_small;
    }
    if (block_large) {
        *block_large = s_bridge_block_size_large;
    }
}

size_t display_bridge_get_cache_line_size(void)
{
    display_bridge_init_cache_profile();
    return s_bridge_cache_line_size;
}

/******************************************************************************
 *                   Bridge-Specific Common Functions (v8/v9)
 ******************************************************************************/

#include "esp_log.h"
#include "esp_check.h"

#if SOC_MIPI_DSI_SUPPORTED
#include "esp_lcd_mipi_dsi.h"
#endif

#if SOC_LCDCAM_RGB_LCD_SUPPORTED
#include "esp_lcd_panel_rgb.h"
#endif

#if CONFIG_SOC_PPA_SUPPORTED
#include "driver/ppa.h"
#endif

#if SOC_DMA2D_SUPPORTED
#include "esp_async_fbcpy.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#endif

/**
 * @brief Initialize runtime information from configuration
 *
 * This function is 100% identical in both v8 and v9 implementations
 */
void display_bridge_init_runtime_info(esp_lv_adapter_display_runtime_info_t *runtime,
                                      const esp_lv_adapter_display_runtime_config_t *cfg)
{
    if (!runtime || !cfg) {
        return;
    }

    runtime->hor_res = cfg->base.profile.hor_res;
    runtime->ver_res = cfg->base.profile.ver_res;
    runtime->rotation = cfg->base.profile.rotation;
    runtime->frame_buffer_count = cfg->frame_buffer_count;
    runtime->frame_buffer_size = cfg->frame_buffer_size;

    for (int i = 0; i < 3; i++) {
        runtime->frame_buffers[i] = cfg->frame_buffers[i];
    }

    /* Calculate color bytes from buffer size */
    size_t total_pixels = (size_t)runtime->hor_res * runtime->ver_res;
    uint8_t color_bytes = 0;

    if (runtime->frame_buffer_size && total_pixels) {
        color_bytes = runtime->frame_buffer_size / total_pixels;
    }
    if (color_bytes == 0 && LV_COLOR_DEPTH % 8 == 0) {
        color_bytes = LV_COLOR_DEPTH / 8;
    }
    if (color_bytes == 0) {
        color_bytes = sizeof(lv_color_t);
    }

    /* Ensure color_bytes has a valid value (final fallback to RGB565) */
    if (color_bytes == 0) {
        color_bytes = 2;  /* RGB565: 2 bytes per pixel */
        ESP_LOGW(TAG, "Unable to determine color bytes, defaulting to RGB565 (2 bytes)");
    }

    runtime->color_bytes = color_bytes;

    /* Recalculate frame_buffer_size if it wasn't set */
    if (runtime->frame_buffer_size == 0 && total_pixels && color_bytes) {
        runtime->frame_buffer_size = total_pixels * color_bytes;
        ESP_LOGD(TAG, "Calculated frame_buffer_size: %zu bytes (%zux%u pixels, %u bpp)",
                 runtime->frame_buffer_size, total_pixels, color_bytes * 8);
    }

    if (runtime->color_bytes != 2 && runtime->color_bytes != 3 &&
            cfg->base.profile.mono_layout == ESP_LV_ADAPTER_MONO_LAYOUT_NONE) {
        ESP_LOGW(TAG, "color depth %u bytes not HW-accelerated; using CPU fallback",
                 runtime->color_bytes);
    }
}

/**
 * @brief Initialize frame buffer pointers
 *
 * This function is 100% identical in both v8 and v9 implementations
 */
void display_bridge_init_frame_buffer_pointers(
    void **front_fb,
    void **back_fb,
    void **spare_fb,
    void **rgb_last_buf,
    void **rgb_next_buf,
    void **rgb_flush_next_buf,
    const esp_lv_adapter_display_runtime_info_t *runtime)
{
    if (!runtime) {
        return;
    }

    uint8_t fb_count = runtime->frame_buffer_count;
    void * const *frame_buffers = runtime->frame_buffers;

    if (front_fb) {
        *front_fb = (fb_count > 0) ? frame_buffers[0] : NULL;
    }
    if (back_fb) {
        *back_fb = (fb_count > 1) ? frame_buffers[1] : NULL;
    }
    if (spare_fb) {
        *spare_fb = (fb_count > 2) ? frame_buffers[2] : NULL;
    }

    void *rgb_last = (fb_count > 0) ? frame_buffers[0] : NULL;
    if (rgb_last_buf) {
        *rgb_last_buf = rgb_last;
    }
    if (rgb_next_buf) {
        *rgb_next_buf = rgb_last;
    }
    if (rgb_flush_next_buf) {
        *rgb_flush_next_buf = (fb_count > 2) ? frame_buffers[2] : NULL;
    }
}

#if SOC_DMA2D_SUPPORTED

/* Global hardware resource - shared between v8 and v9 */
static esp_lv_adapter_display_bridge_hw_resource_t s_hw_resource = {0};
static bool s_hw_resource_initialized = false;
static uint8_t s_hw_resource_ref_count = 0;

/**
 * @brief Get global hardware resource singleton
 *
 * This function implements reference counting for hardware resources.
 * Multiple displays can share the same PPA and DMA2D hardware.
 */
esp_lv_adapter_display_bridge_hw_resource_t *display_bridge_get_hw_resource(void)
{
    if (!s_hw_resource_initialized) {
        ESP_LOGI(TAG, "Initializing hardware resources");

#if CONFIG_SOC_PPA_SUPPORTED
        esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &s_hw_resource.data_cache_line_size);
        if (s_hw_resource.data_cache_line_size == 0) {
            /* Default PPA alignment: 128 bytes for cache line optimization */
            s_hw_resource.data_cache_line_size = 128;
        }
#endif

#if CONFIG_SOC_DMA2D_SUPPORTED
        esp_async_fbcpy_config_t cfg_dma = { };
        esp_err_t ret = esp_async_fbcpy_install(&cfg_dma,
                                                (esp_async_fbcpy_handle_t *)&s_hw_resource.fbcpy_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to install DMA2D (ret=%d)", ret);
            return NULL;
        }

        s_hw_resource.dma2d_mutex = xSemaphoreCreateMutex();
        s_hw_resource.dma2d_done_sem = xSemaphoreCreateBinary();

        if (!s_hw_resource.dma2d_mutex || !s_hw_resource.dma2d_done_sem) {
            ESP_LOGE(TAG, "Failed to create DMA2D semaphores");
            /* Cleanup on failure */
            if (s_hw_resource.dma2d_mutex) {
                vSemaphoreDelete((SemaphoreHandle_t)s_hw_resource.dma2d_mutex);
            }
            if (s_hw_resource.dma2d_done_sem) {
                vSemaphoreDelete((SemaphoreHandle_t)s_hw_resource.dma2d_done_sem);
            }
            esp_async_fbcpy_uninstall((esp_async_fbcpy_handle_t)s_hw_resource.fbcpy_handle);
            return NULL;
        }
#endif

#if CONFIG_SOC_PPA_SUPPORTED
        if (s_hw_resource.ppa_handle == NULL) {
            ppa_client_config_t ppa_srm_config = {
                .oper_type = PPA_OPERATION_SRM,
                .max_pending_trans_num = LVGL_PORT_PPA_MAX_PENDING_TRANS,
            };
            ret = ppa_register_client(&ppa_srm_config,
                                      (ppa_client_handle_t *)&s_hw_resource.ppa_handle);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to register PPA client (ret=%d)", ret);
#if CONFIG_SOC_DMA2D_SUPPORTED
                /* Cleanup DMA2D resources */
                vSemaphoreDelete((SemaphoreHandle_t)s_hw_resource.dma2d_mutex);
                vSemaphoreDelete((SemaphoreHandle_t)s_hw_resource.dma2d_done_sem);
                esp_async_fbcpy_uninstall((esp_async_fbcpy_handle_t)s_hw_resource.fbcpy_handle);
#endif
                return NULL;
            }
        }
#endif

        s_hw_resource_initialized = true;
        ESP_LOGI(TAG, "Hardware resources initialized successfully");
    }

    /* Increment reference count */
    s_hw_resource_ref_count++;
    ESP_LOGD(TAG, "Hardware resource acquired (ref_count=%d)", s_hw_resource_ref_count);

    return &s_hw_resource;
}

/**
 * @brief Release hardware resource reference
 *
 * Decrements the reference count and cleans up hardware resources
 * when the count reaches zero.
 */
esp_err_t display_bridge_release_hw_resource(void)
{
    if (!s_hw_resource_initialized) {
        ESP_LOGW(TAG, "Hardware resources not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_hw_resource_ref_count == 0) {
        ESP_LOGW(TAG, "Hardware resource reference count already zero");
        return ESP_ERR_INVALID_STATE;
    }

    /* Decrement reference count */
    s_hw_resource_ref_count--;
    ESP_LOGD(TAG, "Hardware resource released (ref_count=%d)", s_hw_resource_ref_count);

    /* Cleanup when no more references */
    if (s_hw_resource_ref_count == 0) {
        ESP_LOGI(TAG, "Cleaning up hardware resources");

#if CONFIG_SOC_PPA_SUPPORTED
        if (s_hw_resource.ppa_handle) {
            ppa_unregister_client((ppa_client_handle_t)s_hw_resource.ppa_handle);
            s_hw_resource.ppa_handle = NULL;
            ESP_LOGI(TAG, "PPA client unregistered");
        }
#endif

#if CONFIG_SOC_DMA2D_SUPPORTED
        if (s_hw_resource.dma2d_mutex) {
            vSemaphoreDelete((SemaphoreHandle_t)s_hw_resource.dma2d_mutex);
            s_hw_resource.dma2d_mutex = NULL;
        }

        if (s_hw_resource.dma2d_done_sem) {
            vSemaphoreDelete((SemaphoreHandle_t)s_hw_resource.dma2d_done_sem);
            s_hw_resource.dma2d_done_sem = NULL;
        }

        if (s_hw_resource.fbcpy_handle) {
            esp_async_fbcpy_uninstall((esp_async_fbcpy_handle_t)s_hw_resource.fbcpy_handle);
            s_hw_resource.fbcpy_handle = NULL;
            ESP_LOGI(TAG, "DMA2D resources released");
        }
#endif

        s_hw_resource_initialized = false;
        memset(&s_hw_resource, 0, sizeof(s_hw_resource));
        ESP_LOGI(TAG, "Hardware resources cleaned up successfully");
    }

    return ESP_OK;
}

/**
 * @brief DMA2D completion callback (ISR context)
 *
 * IRAM: Executed in ISR context for fast response
 */
bool display_bridge_dma2d_done_callback(esp_async_fbcpy_handle_t mcp,
                                        esp_async_fbcpy_event_data_t *event_data,
                                        void *cb_args)
{
    BaseType_t high_task_woken = pdFALSE;
    (void)mcp;
    (void)event_data;
    (void)cb_args;

    esp_lv_adapter_display_bridge_hw_resource_t *hw = &s_hw_resource;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)hw->dma2d_done_sem, &high_task_woken);

    return (high_task_woken == pdTRUE);
}

/**
 * @brief Synchronous DMA2D copy operation
 *
 * Note: Mutex held during entire transfer to serialize access to shared
 * dma2d_done_sem semaphore (global resource shared by all displays)
 */
esp_err_t display_bridge_dma2d_copy_sync(void *trans_desc, uint32_t timeout_ms)
{
    esp_err_t ret = ESP_OK;
    esp_lv_adapter_display_bridge_hw_resource_t *hw = display_bridge_get_hw_resource();
    ESP_RETURN_ON_FALSE(hw, ESP_ERR_INVALID_STATE, TAG, "DMA2D resource not initialized");

    ESP_GOTO_ON_FALSE(xSemaphoreTake((SemaphoreHandle_t)hw->dma2d_mutex,
                                     pdMS_TO_TICKS(timeout_ms)) == pdTRUE,
                      ESP_ERR_TIMEOUT, out, TAG, "Acquire DMA2D mutex timeout");

    /* Clear completion semaphore */
    xSemaphoreTake((SemaphoreHandle_t)hw->dma2d_done_sem, 0);

    ret = esp_async_fbcpy(hw->fbcpy_handle,
                          trans_desc,
                          display_bridge_dma2d_done_callback,
                          NULL);
    ESP_GOTO_ON_ERROR(ret, release_mutex, TAG, "DMA2D transfer start failed (%d)", ret);

    ESP_GOTO_ON_FALSE(xSemaphoreTake((SemaphoreHandle_t)hw->dma2d_done_sem,
                                     pdMS_TO_TICKS(timeout_ms)) == pdTRUE,
                      ESP_ERR_TIMEOUT, release_mutex, TAG, "DMA2D transfer timeout");

release_mutex:
    xSemaphoreGive((SemaphoreHandle_t)hw->dma2d_mutex);

out:
    return ret;
}

#endif /* SOC_DMA2D_SUPPORTED */

/**
 * @brief Destroy display bridge and release resources
 *
 * This function is shared between v8 and v9 implementations.
 * It releases hardware resources (if any) and frees the bridge structure.
 *
 * @param bridge Display bridge structure to destroy
 */
void display_bridge_common_destroy(esp_lv_adapter_display_bridge_t *bridge)
{
    /* NULL pointer check - safe to call with NULL */
    if (!bridge) {
        return;
    }

#if CONFIG_SOC_DMA2D_SUPPORTED || CONFIG_SOC_PPA_SUPPORTED
    /* Release shared hardware resources with reference counting */
    display_bridge_release_hw_resource();
#endif

    /* Free the bridge implementation structure */
    free(bridge);
}

/**
 * @brief Probe flush type to determine copy strategy (thread-safe)
 *
 * Thread-safe: prev_status is now per-display instead of global static
 * IRAM: Called every frame during rendering for performance
 */
esp_lv_adapter_display_flush_probe_t display_bridge_flush_copy_probe(
    const lv_area_t *inv_areas,
    const uint8_t *inv_area_joined,
    uint16_t inv_p,
    uint16_t hor_res,
    uint16_t ver_res,
    uint8_t *prev_status)
{
    esp_lv_adapter_display_flush_status_t cur_status;
    esp_lv_adapter_display_flush_probe_t probe_result;

    if (!prev_status) {
        return ESP_LV_ADAPTER_DISPLAY_FLUSH_PROBE_PART_COPY;
    }

    uint32_t flush_ver = 0;
    uint32_t flush_hor = 0;

    for (int i = 0; i < inv_p; i++) {
        if (inv_area_joined[i] == 0) {
            flush_ver = (inv_areas[i].y2 + 1 - inv_areas[i].y1);
            flush_hor = (inv_areas[i].x2 + 1 - inv_areas[i].x1);
            break;
        }
    }

    /* Check if the current full screen refreshes */
    cur_status = ((flush_ver == ver_res) && (flush_hor == hor_res)) ?
                 ESP_LV_ADAPTER_DISPLAY_FLUSH_STATUS_FULL : ESP_LV_ADAPTER_DISPLAY_FLUSH_STATUS_PART;

    if (*prev_status == ESP_LV_ADAPTER_DISPLAY_FLUSH_STATUS_FULL) {
        if (cur_status == ESP_LV_ADAPTER_DISPLAY_FLUSH_STATUS_PART) {
            probe_result = ESP_LV_ADAPTER_DISPLAY_FLUSH_PROBE_FULL_COPY;
        } else {
            probe_result = ESP_LV_ADAPTER_DISPLAY_FLUSH_PROBE_SKIP_COPY;
        }
    } else {
        probe_result = ESP_LV_ADAPTER_DISPLAY_FLUSH_PROBE_PART_COPY;
    }
    *prev_status = cur_status;

    return probe_result;
}
