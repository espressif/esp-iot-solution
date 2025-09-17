/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_heap_caps.h"
#include "esp_async_fbcpy.h"
#include "esp_lcd_panel_ops.h"
#if SOC_LCDCAM_RGB_LCD_SUPPORTED
#include "esp_lcd_panel_rgb.h"
#endif
#if SOC_MIPI_DSI_SUPPORTED
#include "esp_lcd_mipi_dsi.h"
#endif
#include "esp_lcd_touch.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_cache.h"
#if CONFIG_IDF_TARGET_ESP32P4
#include "esp_private/esp_cache_private.h"
#include "driver/ppa.h"
#endif
#include "lvgl.h"
#include "lvgl_port.h"
#include "lvgl_fps_stats.h"

#define ALIGN_UP_BY(num, align)    (((num) + ((align) - 1)) & ~((align) - 1))
#define BLOCK_SIZE_SMALL           (32)
#define BLOCK_SIZE_LARGE           (256)

static const char *TAG = "lv_port_v8";

typedef esp_err_t (*get_lcd_frame_buffer_cb_t)(esp_lcd_panel_handle_t panel, uint32_t fb_num, void **fb0, ...);

static SemaphoreHandle_t lvgl_mux;                  // LVGL mutex
static TaskHandle_t lvgl_task_handle = NULL;
static lvgl_port_interface_t lvgl_port_interface = LVGL_PORT_INTERFACE_RGB;

#if SOC_DMA2D_SUPPORTED
static size_t data_cache_line_size = 0;
#endif /* SOC_DMA2D_SUPPORTED */

#if LVGL_PORT_PPA_ROTATION_ENABLE
static ppa_client_handle_t ppa_srm_handle = NULL;
#endif /* LVGL_PORT_PPA_ROTATION_ENABLE */

#if LVGL_PORT_AVOID_TEAR_ENABLE
static get_lcd_frame_buffer_cb_t lvgl_get_lcd_frame_buffer = NULL;
#if !LVGL_PORT_DIRECT_MODE && !LVGL_PORT_FULL_REFRESH && LVGL_PORT_LCD_BUFFER_NUMS == 3
static void *front_fb = NULL;
static void *back_fb = NULL;
static void *spare_fb = NULL;
static size_t fb_size = 0;
static uint8_t lvgl_color_format_bytes = 2;  // RGB565 default
#if SOC_DMA2D_SUPPORTED
static esp_async_fbcpy_handle_t fbcpy_handle = NULL;

// DMA synchronization improvements
static SemaphoreHandle_t dma2d_mutex = NULL;       // DMA resource protection
static SemaphoreHandle_t dma2d_done_sem = NULL;    // DMA completion notification
#endif /* SOC_DMA2D_SUPPORTED */
#endif /* !LVGL_PORT_DIRECT_MODE && !LVGL_PORT_FULL_REFRESH && LVGL_PORT_LCD_BUFFER_NUMS == 3 */
#endif /* LVGL_PORT_AVOID_TEAR_ENABLE */

#if EXAMPLE_LVGL_PORT_ROTATION_DEGREE != 0
#if !LVGL_PORT_DIRECT_MODE && !LVGL_PORT_FULL_REFRESH && LVGL_PORT_LCD_BUFFER_NUMS == 3
static inline void map_coords_rotated_global(int gx, int gy, int rotation, int *dx, int *dy)
{
    const int PHYS_W = LVGL_PORT_H_RES;
    const int PHYS_H = LVGL_PORT_V_RES;

    switch (rotation) {
    case 90:
        *dx = PHYS_W - 1 - gy;
        *dy = gx;
        break;
    case 180:
        *dx = PHYS_W - 1 - gx;
        *dy = PHYS_H - 1 - gy;
        break;
    case 270:
        *dx = gy;
        *dy = PHYS_H - 1 - gx;
        break;
    default:
        *dx = gx;
        *dy = gy;
        break;
    }
}

static inline void copy_color_pixel(void *dst_pixel, const void *src_pixel)
{
    if (lvgl_color_format_bytes == 2) {
        *(uint16_t *)dst_pixel = *(const uint16_t *)src_pixel;
    } else if (lvgl_color_format_bytes == 3) {
        ((uint8_t *)dst_pixel)[0] = ((const uint8_t *)src_pixel)[0];
        ((uint8_t *)dst_pixel)[1] = ((const uint8_t *)src_pixel)[1];
        ((uint8_t *)dst_pixel)[2] = ((const uint8_t *)src_pixel)[2];
    }
}

IRAM_ATTR static void rotate_copy_pixel_ex(const void *src, void *dst_fb,
                                           uint16_t lv_x_start, uint16_t lv_y_start,
                                           uint16_t lv_x_end,   uint16_t lv_y_end,
                                           uint16_t src_stride_px,
                                           uint16_t rotation)
{
#if LVGL_PORT_PPA_ROTATION_ENABLE
    const size_t rect_w = lv_x_end - lv_x_start + 1;
    const size_t rect_h = lv_y_end - lv_y_start + 1;

    int block_w_final    = rect_w;
    int block_h_final    = rect_h;
    int block_off_x_final = 0;
    int block_off_y_final = 0;

    ppa_srm_rotation_angle_t ppa_rotation;
    int x_offset = 0, y_offset = 0;

    switch (rotation) {
    case 90:
        ppa_rotation = PPA_SRM_ROTATION_ANGLE_270;
        x_offset = LVGL_PORT_H_RES - 1 - lv_y_end;
        y_offset = lv_x_start;
        break;
    case 180:
        ppa_rotation = PPA_SRM_ROTATION_ANGLE_180;
        x_offset = LVGL_PORT_H_RES - 1 - lv_x_end;
        y_offset = LVGL_PORT_V_RES - 1 - lv_y_end;
        break;
    case 270:
        ppa_rotation = PPA_SRM_ROTATION_ANGLE_90;
        x_offset = lv_y_start;
        y_offset = LVGL_PORT_V_RES - 1 - lv_x_end;
        break;
    default:
        ppa_rotation = PPA_SRM_ROTATION_ANGLE_0;
        break;
    }

    /* Configure and start PPA */
    ppa_srm_oper_config_t oper_config = {
        .in.buffer          = src,
        .in.pic_w           = src_stride_px,
        .in.pic_h           = rect_h,
        .in.block_w         = block_w_final,
        .in.block_h         = block_h_final,
        .in.block_offset_x  = block_off_x_final,
        .in.block_offset_y  = block_off_y_final,
        .in.srm_cm          = (LV_COLOR_DEPTH == 24) ? PPA_SRM_COLOR_MODE_RGB888 : PPA_SRM_COLOR_MODE_RGB565,

           .out.buffer         = dst_fb,
           .out.buffer_size    = ALIGN_UP_BY(sizeof(lv_color_t) * LVGL_PORT_H_RES * LVGL_PORT_V_RES, data_cache_line_size),
           .out.pic_w          = LVGL_PORT_H_RES,
           .out.pic_h          = LVGL_PORT_V_RES,
           .out.block_offset_x = x_offset,
           .out.block_offset_y = y_offset,
           .out.srm_cm         = (LV_COLOR_DEPTH == 24) ? PPA_SRM_COLOR_MODE_RGB888 : PPA_SRM_COLOR_MODE_RGB565,

           .rotation_angle     = ppa_rotation,
           .scale_x            = 1.0,
           .scale_y            = 1.0,
           .rgb_swap           = 0,
           .byte_swap          = 0,
           .mode               = PPA_TRANS_MODE_BLOCKING,
    };
    esp_err_t ret = ppa_do_scale_rotate_mirror(ppa_srm_handle, &oper_config);
    ESP_ERROR_CHECK(ret);
    return;
#else /* !LVGL_PORT_PPA_ROTATION_ENABLE */

    const int rect_w = lv_x_end - lv_x_start + 1;
    const int rect_h = lv_y_end - lv_y_start + 1;
    const int PHYS_W = LVGL_PORT_H_RES;

    int block_w = (rotation == 90 || rotation == 270) ? BLOCK_SIZE_SMALL : BLOCK_SIZE_LARGE;
    int block_h = (rotation == 90 || rotation == 270) ? BLOCK_SIZE_LARGE : BLOCK_SIZE_SMALL;

    for (int i = 0; i < rect_h; i += block_h) {
        int max_height = (i + block_h > rect_h) ? rect_h : i + block_h;

        for (int j = 0; j < rect_w; j += block_w) {
            int max_width = (j + block_w > rect_w) ? rect_w : j + block_w;

            for (int y = i; y < max_height; y++) {
                for (int x = j; x < max_width; x++) {
                    int gx = lv_x_start + x;
                    int gy = lv_y_start + y;
                    int dx, dy;
                    map_coords_rotated_global(gx, gy, rotation, &dx, &dy);

                    void *src_pixel = (uint8_t *)src + (y * src_stride_px + x) * lvgl_color_format_bytes;
                    void *dst_pixel = (uint8_t *)dst_fb + (dy * PHYS_W + dx) * lvgl_color_format_bytes;

                    copy_color_pixel(dst_pixel, src_pixel);
                }
            }
        }
    }

#endif /* LVGL_PORT_PPA_ROTATION_ENABLE */
}

#else /* LVGL_PORT_DIRECT_MODE || LVGL_PORT_FULL_REFRESH || LVGL_PORT_LCD_BUFFER_NUMS != 3 */
static void *get_next_frame_buffer(esp_lcd_panel_handle_t panel_handle)
{
    static void *next_fb = NULL;
    static void *fb[2] = { NULL };
    if (next_fb == NULL) {
        ESP_ERROR_CHECK(lvgl_get_lcd_frame_buffer(panel_handle, 2, &fb[0], &fb[1]));
        next_fb = fb[1];
    } else {
        next_fb = (next_fb == fb[0]) ? fb[1] : fb[0];
    }
    return next_fb;
}

#if !LVGL_PORT_PPA_ROTATION_ENABLE
static void rotate_image(const void *src, void *dst, int width, int height, int rotation, int bpp)
{
    int bytes_per_pixel = bpp / 8;
    int block_w = rotation == 90 || rotation == 270 ? BLOCK_SIZE_SMALL : BLOCK_SIZE_LARGE;
    int block_h = rotation == 90 || rotation == 270 ? BLOCK_SIZE_LARGE : BLOCK_SIZE_SMALL;

    for (int i = 0; i < height; i += block_h) {
        int max_height = i + block_h > height ? height : i + block_h;

        for (int j = 0; j < width; j += block_w) {
            int max_width = j + block_w > width ? width : j + block_w;

            for (int x = i; x < max_height; x++) {
                for (int y = j; y < max_width; y++) {
                    void *src_pixel = (uint8_t *)src + (x * width + y) * bytes_per_pixel;
                    void *dst_pixel;

                    switch (rotation) {
                    case 270:
                        dst_pixel = (uint8_t *)dst + ((width - 1 - y) * height + x) * bytes_per_pixel;
                        break;
                    case 180:
                        dst_pixel = (uint8_t *)dst + ((height - 1 - x) * width + (width - 1 - y)) * bytes_per_pixel;
                        break;
                    case 90:
                        dst_pixel = (uint8_t *)dst + (y * height + (height - 1 - x)) * bytes_per_pixel;
                        break;
                    default:
                        return;
                    }

                    if (bpp == 16) {
                        *(uint16_t *)dst_pixel = *(uint16_t *)src_pixel;
                    } else if (bpp == 24) {
                        ((uint8_t *)dst_pixel)[0] = ((uint8_t *)src_pixel)[0];
                        ((uint8_t *)dst_pixel)[1] = ((uint8_t *)src_pixel)[1];
                        ((uint8_t *)dst_pixel)[2] = ((uint8_t *)src_pixel)[2];
                    }
                }
            }
        }
    }
}
#endif /* !LVGL_PORT_PPA_ROTATION_ENABLE */

IRAM_ATTR static void rotate_copy_pixel(const uint16_t *from, uint16_t *to, uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint16_t w, uint16_t h, uint16_t rotation)
{
#if LVGL_PORT_PPA_ROTATION_ENABLE
    ppa_srm_rotation_angle_t ppa_rotation;
    int x_offset = 0, y_offset = 0;

    // Determine rotation settings once and reuse
    switch (rotation) {
    case 90:
        ppa_rotation = PPA_SRM_ROTATION_ANGLE_270;
        x_offset = h - y_end - 1;
        y_offset = x_start;
        break;
    case 180:
        ppa_rotation = PPA_SRM_ROTATION_ANGLE_180;
        x_offset = w - x_end - 1;
        y_offset = h - y_end - 1;
        break;
    case 270:
        ppa_rotation = PPA_SRM_ROTATION_ANGLE_90;
        x_offset = y_start;
        y_offset = w - x_end - 1;
        break;
    default:
        ppa_rotation = PPA_SRM_ROTATION_ANGLE_0;
        break;
    }

    // Fill operation config for PPA rotation, without recalculating each time
    ppa_srm_oper_config_t oper_config = {
        .in.buffer = from,
        .in.pic_w = w,
        .in.pic_h = h,
        .in.block_w = x_end - x_start + 1,
        .in.block_h = y_end - y_start + 1,
        .in.block_offset_x = x_start,
        .in.block_offset_y = y_start,
        .in.srm_cm = (LV_COLOR_DEPTH == 24) ? PPA_SRM_COLOR_MODE_RGB888 : PPA_SRM_COLOR_MODE_RGB565,

           .out.buffer = to,
           .out.buffer_size = ALIGN_UP_BY(sizeof(lv_color_t) * w * h, data_cache_line_size),
           .out.pic_w = (ppa_rotation == PPA_SRM_ROTATION_ANGLE_90 || ppa_rotation == PPA_SRM_ROTATION_ANGLE_270) ? h : w,
           .out.pic_h = (ppa_rotation == PPA_SRM_ROTATION_ANGLE_90 || ppa_rotation == PPA_SRM_ROTATION_ANGLE_270) ? w : h,
           .out.block_offset_x = x_offset,
           .out.block_offset_y = y_offset,
           .out.srm_cm = (LV_COLOR_DEPTH == 24) ? PPA_SRM_COLOR_MODE_RGB888 : PPA_SRM_COLOR_MODE_RGB565,

           .rotation_angle = ppa_rotation,
           .scale_x = 1.0,
           .scale_y = 1.0,
           .rgb_swap = 0,
           .byte_swap = 0,
           .mode = PPA_TRANS_MODE_BLOCKING,
    };
    esp_err_t ret = ppa_do_scale_rotate_mirror(ppa_srm_handle, &oper_config);
    ESP_ERROR_CHECK(ret);

#else /* !LVGL_PORT_PPA_ROTATION_ENABLE */
    rotate_image(from, to, w, h, rotation, LV_COLOR_DEPTH);
#endif /* LVGL_PORT_PPA_ROTATION_ENABLE */
}
#endif /* LVGL_PORT_DIRECT_MODE || LVGL_PORT_FULL_REFRESH || LVGL_PORT_LCD_BUFFER_NUMS != 3 */
#endif /* !EXAMPLE_LVGL_PORT_ROTATION_DEGREE */

#if LVGL_PORT_AVOID_TEAR_ENABLE
static void switch_lcd_frame_buffer_to(esp_lcd_panel_handle_t panel_handle, void *fb)
{
    esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LVGL_PORT_H_RES, LVGL_PORT_V_RES, fb);
}
#if LVGL_PORT_DIRECT_MODE
#if EXAMPLE_LVGL_PORT_ROTATION_DEGREE != 0
typedef struct {
    uint16_t inv_p;
    uint8_t inv_area_joined[LV_INV_BUF_SIZE];
    lv_area_t inv_areas[LV_INV_BUF_SIZE];
} lv_port_dirty_area_t;

typedef enum {
    FLUSH_STATUS_PART,
    FLUSH_STATUS_FULL
} lv_port_flush_status_t;

typedef enum {
    FLUSH_PROBE_PART_COPY,
    FLUSH_PROBE_SKIP_COPY,
    FLUSH_PROBE_FULL_COPY,
} lv_port_flush_probe_t;

static lv_port_dirty_area_t dirty_area;

static void flush_dirty_save(lv_port_dirty_area_t *dirty_area)
{
    lv_disp_t *disp = _lv_refr_get_disp_refreshing();
    dirty_area->inv_p = disp->inv_p;
    for (int i = 0; i < disp->inv_p; i++) {
        dirty_area->inv_area_joined[i] = disp->inv_area_joined[i];
        dirty_area->inv_areas[i] = disp->inv_areas[i];
    }
}

/**
 * @brief Probe dirty area to copy
 *
 * @note This function is used to avoid tearing effect, and only work with LVGL direct-mode.
 *
 */
static lv_port_flush_probe_t flush_copy_probe(lv_disp_drv_t *drv)
{
    static lv_port_flush_status_t prev_status = FLUSH_STATUS_PART;
    lv_port_flush_status_t cur_status;
    lv_port_flush_probe_t probe_result;
    lv_disp_t *disp_refr = _lv_refr_get_disp_refreshing();

    uint32_t flush_ver = 0;
    uint32_t flush_hor = 0;
    for (int i = 0; i < disp_refr->inv_p; i++) {
        if (disp_refr->inv_area_joined[i] == 0) {
            flush_ver = (disp_refr->inv_areas[i].y2 + 1 - disp_refr->inv_areas[i].y1);
            flush_hor = (disp_refr->inv_areas[i].x2 + 1 - disp_refr->inv_areas[i].x1);
            break;
        }
    }
    /* Check if the current full screen refreshes */
    cur_status = ((flush_ver == drv->ver_res) && (flush_hor == drv->hor_res)) ? (FLUSH_STATUS_FULL) : (FLUSH_STATUS_PART);

    if (prev_status == FLUSH_STATUS_FULL) {
        if ((cur_status == FLUSH_STATUS_PART)) {
            probe_result = FLUSH_PROBE_FULL_COPY;
        } else {
            probe_result = FLUSH_PROBE_SKIP_COPY;
        }
    } else {
        probe_result = FLUSH_PROBE_PART_COPY;
    }
    prev_status = cur_status;

    return probe_result;
}

static inline void *flush_get_next_buf(void *panel_handle)
{
    return get_next_frame_buffer(panel_handle);
}

/**
 * @brief Copy dirty area
 *
 * @note This function is used to avoid tearing effect, and only work with LVGL direct-mode.
 *
 */
static void flush_dirty_copy(void *dst, void *src, lv_port_dirty_area_t *dirty_area)
{
    lv_coord_t x_start, x_end, y_start, y_end;
    for (int i = 0; i < dirty_area->inv_p; i++) {
        /* Refresh the unjoined areas*/
        if (dirty_area->inv_area_joined[i] == 0) {
            x_start = dirty_area->inv_areas[i].x1;
            x_end = dirty_area->inv_areas[i].x2;
            y_start = dirty_area->inv_areas[i].y1;
            y_end = dirty_area->inv_areas[i].y2;

            rotate_copy_pixel(src, dst, x_start, y_start, x_end, y_end, LV_HOR_RES, LV_VER_RES, EXAMPLE_LVGL_PORT_ROTATION_DEGREE);
        }
    }
}

static void flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;
    void *next_fb = NULL;
    lv_port_flush_probe_t probe_result = FLUSH_PROBE_PART_COPY;
    lv_disp_t *disp = lv_disp_get_default();

    /* Action after last area refresh */
    if (lv_disp_flush_is_last(drv)) {
        LVGL_FPS_STATS_RECORD_FRAME();
        /* Check if the `full_refresh` flag has been triggered */
        if (drv->full_refresh) {
            /* Reset flag */
            drv->full_refresh = 0;

            // Rotate and copy data from the whole screen LVGL's buffer to the next frame buffer
            next_fb = flush_get_next_buf(panel_handle);
            rotate_copy_pixel((uint16_t *)color_map, next_fb, offsetx1, offsety1, offsetx2, offsety2, LV_HOR_RES, LV_VER_RES, EXAMPLE_LVGL_PORT_ROTATION_DEGREE);

            /* Switch the current LCD frame buffer to `next_fb` */
            switch_lcd_frame_buffer_to(panel_handle, next_fb);

            /* Waiting for the current frame buffer to complete transmission */
            ulTaskNotifyValueClear(NULL, ULONG_MAX);
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

            /* Synchronously update the dirty area for another frame buffer */
            flush_dirty_copy(flush_get_next_buf(panel_handle), color_map, &dirty_area);
            flush_get_next_buf(panel_handle);
        } else {
            /* Probe the copy method for the current dirty area */
            probe_result = flush_copy_probe(drv);

            if (probe_result == FLUSH_PROBE_FULL_COPY) {
                /* Save current dirty area for next frame buffer */
                flush_dirty_save(&dirty_area);

                /* Set LVGL full-refresh flag and set flush ready in advance */
                drv->full_refresh = 1;
                disp->rendering_in_progress = false;
                lv_disp_flush_ready(drv);

                /* Force to refresh whole screen, and will invoke `flush_callback` recursively */
                lv_refr_now(_lv_refr_get_disp_refreshing());
            } else {
                /* Update current dirty area for next frame buffer */
                next_fb = flush_get_next_buf(panel_handle);
                flush_dirty_save(&dirty_area);
                flush_dirty_copy(next_fb, color_map, &dirty_area);

                /* Switch the current LCD frame buffer to `next_fb` */
                switch_lcd_frame_buffer_to(panel_handle, next_fb);

                /* Waiting for the current frame buffer to complete transmission */
                ulTaskNotifyValueClear(NULL, ULONG_MAX);
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

                if (probe_result == FLUSH_PROBE_PART_COPY) {
                    /* Synchronously update the dirty area for another frame buffer */
                    flush_dirty_save(&dirty_area);
                    flush_dirty_copy(flush_get_next_buf(panel_handle), color_map, &dirty_area);
                    flush_get_next_buf(panel_handle);
                }
            }
        }
    }

    lv_disp_flush_ready(drv);
}

#else /* EXAMPLE_LVGL_PORT_ROTATION_DEGREE */

static void flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;

    /* Action after last area refresh */
    if (lv_disp_flush_is_last(drv)) {
        LVGL_FPS_STATS_RECORD_FRAME();
        /* Switch the current LCD frame buffer to `color_map` */
        switch_lcd_frame_buffer_to(panel_handle, color_map);

        /* Waiting for the last frame buffer to complete transmission */
        ulTaskNotifyValueClear(NULL, ULONG_MAX);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }

    lv_disp_flush_ready(drv);
}
#endif /* !EXAMPLE_LVGL_PORT_ROTATION_DEGREE */

#elif LVGL_PORT_FULL_REFRESH && LVGL_PORT_LCD_BUFFER_NUMS == 2

static void flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;

    /* Switch the current LCD frame buffer to `color_map` */
    switch_lcd_frame_buffer_to(panel_handle, color_map);

    /* Waiting for the last frame buffer to complete transmission */
    ulTaskNotifyValueClear(NULL, ULONG_MAX);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    lv_disp_flush_ready(drv);
}

#elif LVGL_PORT_FULL_REFRESH && LVGL_PORT_LCD_BUFFER_NUMS == 3

#if EXAMPLE_LVGL_PORT_ROTATION_DEGREE == 0
static void *lvgl_port_rgb_last_buf = NULL;
static void *lvgl_port_rgb_next_buf = NULL;
static void *lvgl_port_flush_next_buf = NULL;
#endif

void flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;

#if EXAMPLE_LVGL_PORT_ROTATION_DEGREE != 0
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;
    void *next_fb = get_next_frame_buffer(panel_handle);

    /* Rotate and copy dirty area from the current LVGL's buffer to the next LCD frame buffer */
    rotate_copy_pixel((uint16_t *)color_map, next_fb, offsetx1, offsety1, offsetx2, offsety2, LV_HOR_RES, LV_VER_RES, EXAMPLE_LVGL_PORT_ROTATION_DEGREE);

    /* Switch the current LCD frame buffer to `next_fb` */
    switch_lcd_frame_buffer_to(panel_handle, next_fb);
#else
    drv->draw_buf->buf1 = color_map;
    drv->draw_buf->buf2 = lvgl_port_flush_next_buf;
    lvgl_port_flush_next_buf = color_map;

    /* Switch the current LCD frame buffer to `color_map` */
    switch_lcd_frame_buffer_to(panel_handle, color_map);

    lvgl_port_rgb_next_buf = color_map;
#endif

    lv_disp_flush_ready(drv);
}

#elif !LVGL_PORT_DIRECT_MODE && !LVGL_PORT_FULL_REFRESH && LVGL_PORT_LCD_BUFFER_NUMS == 3

#if SOC_DMA2D_SUPPORTED
static bool IRAM_ATTR dma_done_callback(esp_async_fbcpy_handle_t mcp, esp_async_fbcpy_event_data_t *event_data, void *cb_args)
{
    BaseType_t high_task_woken = pdFALSE;

    // Signal completion via semaphore instead of task notification
    xSemaphoreGiveFromISR(dma2d_done_sem, &high_task_woken);

    return (high_task_woken == pdTRUE);
}

static esp_err_t dma2d_copy_sync(esp_async_fbcpy_trans_desc_t *trans_desc, uint32_t timeout_ms)
{
    esp_err_t ret;

    // Acquire DMA resource lock
    if (xSemaphoreTake(dma2d_mutex, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    // Clear completion semaphore
    xSemaphoreTake(dma2d_done_sem, 0);

    // Start DMA operation
    ret = esp_async_fbcpy(fbcpy_handle, trans_desc, dma_done_callback, NULL);
    if (ret != ESP_OK) {
        xSemaphoreGive(dma2d_mutex);
        return ret;
    }

    // Wait for completion
    if (xSemaphoreTake(dma2d_done_sem, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        xSemaphoreGive(dma2d_mutex);
        return ESP_ERR_TIMEOUT;
    }

    // Release DMA resource lock
    xSemaphoreGive(dma2d_mutex);

    return ESP_OK;
}
#endif /* SOC_DMA2D_SUPPORTED */

static inline void lv_coord_to_phy(int lv_x, int lv_y, int *phy_x, int *phy_y)
{
#if   EXAMPLE_LVGL_PORT_ROTATION_DEGREE == 90
    *phy_x = LVGL_PORT_H_RES - 1 - lv_y;
    *phy_y = lv_x;
#elif EXAMPLE_LVGL_PORT_ROTATION_DEGREE == 180
    *phy_x = LVGL_PORT_H_RES - 1 - lv_x;
    *phy_y = LVGL_PORT_V_RES - 1 - lv_y;
#elif EXAMPLE_LVGL_PORT_ROTATION_DEGREE == 270
    *phy_x = lv_y;
    *phy_y = LVGL_PORT_V_RES - 1 - lv_x;
#else /* EXAMPLE_LVGL_PORT_ROTATION_DEGREE == 0 */
    *phy_x = lv_x;
    *phy_y = lv_y;
#endif /* EXAMPLE_LVGL_PORT_ROTATION_DEGREE */
}

static void copy_unrendered_area_from_front_to_back(lv_disp_t *disp_refr)
{
#if EXAMPLE_LVGL_PORT_ROTATION_DEGREE != 0
    for (int i = 0; i < disp_refr->inv_p; i++) {
        lv_area_t *a = &disp_refr->inv_areas[i];
        int x1, y1, x2, y2;
        lv_coord_to_phy(a->x1, a->y1, &x1, &y1);
        lv_coord_to_phy(a->x2, a->y2, &x2, &y2);
        a->x1 = LV_MIN(x1, x2);
        a->x2 = LV_MAX(x1, x2);
        a->y1 = LV_MIN(y1, y2);
        a->y2 = LV_MAX(y1, y2);
    }
#endif /* EXAMPLE_LVGL_PORT_ROTATION_DEGREE != 0 */

    typedef struct {
        int x1, y1, x2, y2;
    } rect_t;

    /*------------------------------------------------------------------
     * Step-1: Build "unsynced" list  =  FullScreen  –  Union(dirty).
     *----------------------------------------------------------------*/
    rect_t unsync_rects[LV_INV_BUF_SIZE * 4 + 4];
    int    unsync_cnt = 1;
    unsync_rects[0] = (rect_t) {
        0, 0, LVGL_PORT_H_RES - 1, LVGL_PORT_V_RES - 1
    };

    for (int i = 0; i < disp_refr->inv_p; i++) {
        if (disp_refr->inv_area_joined[i]) {
            continue; // skip already-joined areas
        }

        rect_t d = {
            .x1 = disp_refr->inv_areas[i].x1,
            .y1 = disp_refr->inv_areas[i].y1,
            .x2 = disp_refr->inv_areas[i].x2,
            .y2 = disp_refr->inv_areas[i].y2,
        };

        int j = 0;
        while (j < unsync_cnt) {
            rect_t r = unsync_rects[j];

            // no intersection => keep going
            bool overlap = !(d.x2 < r.x1 || d.x1 > r.x2 || d.y2 < r.y1 || d.y1 > r.y2);
            if (!overlap) {
                j++;
                continue;
            }

            // remove r from list
            for (int k = j; k < unsync_cnt - 1; k++) {
                unsync_rects[k] = unsync_rects[k + 1];
            }
            unsync_cnt--;

            // split r − d (up to four rectangles) and push back
            if (d.y1 > r.y1) { // top slice
                unsync_rects[unsync_cnt++] = (rect_t) {
                    r.x1, r.y1, r.x2, d.y1 - 1
                };
            }
            if (d.y2 < r.y2) { // bottom slice
                unsync_rects[unsync_cnt++] = (rect_t) {
                    r.x1, d.y2 + 1, r.x2, r.y2
                };
            }

            int overlap_y1 = LV_MAX(r.y1, d.y1);
            int overlap_y2 = LV_MIN(r.y2, d.y2);

            if (d.x1 > r.x1) { // left slice
                unsync_rects[unsync_cnt++] = (rect_t) {
                    r.x1, overlap_y1, d.x1 - 1, overlap_y2
                };
            }
            if (d.x2 < r.x2) { // right slice
                unsync_rects[unsync_cnt++] = (rect_t) {
                    d.x2 + 1, overlap_y1, r.x2, overlap_y2
                };
            }
        }
    }

    if (unsync_cnt == 0) {
        return; // whole screen was rendered this frame
    }

    /*------------------------------------------------------------------
     * Step-2: Merge rectangles which share one axis span to reduce copy.
     *----------------------------------------------------------------*/
    bool merged;
    do {
        merged = false;
        for (int i = 0; i < unsync_cnt; i++) {
            for (int j = i + 1; j < unsync_cnt; j++) {
                rect_t *a = &unsync_rects[i];
                rect_t *b = &unsync_rects[j];

                // Horizontal merge: identical vertical span, x ranges touch/overlap
                if (a->y1 == b->y1 && a->y2 == b->y2 &&
                        b->x1 <= a->x2 + 1 && b->x2 >= a->x1 - 1) {
                    a->x1 = LV_MIN(a->x1, b->x1);
                    a->x2 = LV_MAX(a->x2, b->x2);

                    for (int k = j; k < unsync_cnt - 1; k++) {
                        unsync_rects[k] = unsync_rects[k + 1];
                    }
                    unsync_cnt--;
                    merged = true;
                    goto MERGE_RESTART; // restart outer loops
                }

                // Vertical merge: identical horizontal span, y ranges touch/overlap
                if (a->x1 == b->x1 && a->x2 == b->x2 &&
                        b->y1 <= a->y2 + 1 && b->y2 >= a->y1 - 1) {
                    a->y1 = LV_MIN(a->y1, b->y1);
                    a->y2 = LV_MAX(a->y2, b->y2);

                    for (int k = j; k < unsync_cnt - 1; k++) {
                        unsync_rects[k] = unsync_rects[k + 1];
                    }
                    unsync_cnt--;
                    merged = true;
                    goto MERGE_RESTART;
                }
            }
        }
MERGE_RESTART:;
    } while (merged);

    /*------------------------------------------------------------------
     * Step-3: Copy using DMA2D when available, fallback to CPU memcpy.
     *----------------------------------------------------------------*/
#if SOC_DMA2D_SUPPORTED

    if (unsync_cnt > 0) {
        esp_cache_msync(front_fb, fb_size,
                        ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
    }

    for (int idx = 0; idx < unsync_cnt; idx++) {
        rect_t r = unsync_rects[idx];
        size_t copy_w_px = r.x2 - r.x1 + 1;
        size_t copy_h_px = r.y2 - r.y1 + 1;

        int offset_x = r.x1;

        esp_async_fbcpy_trans_desc_t tr = {
            .src_buffer        = front_fb,
            .dst_buffer        = back_fb,
            .src_buffer_size_x = LVGL_PORT_H_RES,
            .src_buffer_size_y = LVGL_PORT_V_RES,
            .dst_buffer_size_x = LVGL_PORT_H_RES,
            .dst_buffer_size_y = LVGL_PORT_V_RES,
            .src_offset_x      = offset_x,
            .src_offset_y      = r.y1,
            .dst_offset_x      = offset_x,
            .dst_offset_y      = r.y1,
            .copy_size_x       = copy_w_px,
            .copy_size_y       = copy_h_px,
            .pixel_format_unique_id = {
                .color_type_id = (lvgl_color_format_bytes == 2) ?
                COLOR_TYPE_ID(COLOR_SPACE_RGB, COLOR_PIXEL_RGB565) :
                COLOR_TYPE_ID(COLOR_SPACE_RGB, COLOR_PIXEL_RGB888)
            },
        };

        /* submit and wait */
        ESP_ERROR_CHECK(dma2d_copy_sync(&tr, portMAX_DELAY));
    }
#else /* !SOC_DMA2D_SUPPORTED */

    const int bytes_per_pixel = lvgl_color_format_bytes;
    for (int idx = 0; idx < unsync_cnt; idx++) {
        rect_t r = unsync_rects[idx];
        int width_px        = r.x2 - r.x1 + 1;
        int bytes_per_line  = width_px * bytes_per_pixel;
        uint8_t *src_line = (uint8_t *)front_fb + (r.y1 * LVGL_PORT_H_RES + r.x1) * bytes_per_pixel;
        uint8_t *dst_line = (uint8_t *)back_fb  + (r.y1 * LVGL_PORT_H_RES + r.x1) * bytes_per_pixel;

        for (int y = r.y1; y <= r.y2; y++) {
            memcpy(dst_line, src_line, bytes_per_line);
            src_line += LVGL_PORT_H_RES * bytes_per_pixel;
            dst_line += LVGL_PORT_H_RES * bytes_per_pixel;
        }
    }

#endif /* !SOC_DMA2D_SUPPORTED */
}

void flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)drv->user_data;

#if EXAMPLE_LVGL_PORT_ROTATION_DEGREE != 0
    size_t rect_w = area->x2 - area->x1 + 1;

    // Fix stride calculation: detect actual stride from buffer layout
    uint8_t *row0 = (uint8_t*)color_map;
    uint8_t *row1 = row0 + rect_w * lvgl_color_format_bytes;
    size_t stride_bytes = row1 - row0;
    size_t src_stride_px = stride_bytes / lvgl_color_format_bytes;

    // Check if this is full-width buffer or line buffer
    bool use_full_stride = (src_stride_px == LVGL_PORT_H_RES);
    if (use_full_stride) {
        src_stride_px = LVGL_PORT_H_RES;
    } else {
        src_stride_px = rect_w;
    }

    rotate_copy_pixel_ex(color_map,
                         back_fb,
                         area->x1, area->y1,
                         area->x2, area->y2,
                         src_stride_px,
                         EXAMPLE_LVGL_PORT_ROTATION_DEGREE);

#else /* EXAMPLE_LVGL_PORT_ROTATION_DEGREE == 0 */
#if SOC_DMA2D_SUPPORTED
    // Detect line buffer stride dynamically
    size_t rect_w = area->x2 - area->x1 + 1;
    size_t rect_h = area->y2 - area->y1 + 1;

    uint8_t *row0 = (uint8_t*)color_map;
    uint8_t *row1 = (uint8_t*)color_map + rect_w * lvgl_color_format_bytes;
    size_t stride_bytes = row1 - row0;
    size_t stride_px = stride_bytes / lvgl_color_format_bytes;

    // Determine if using full-width stride or compact stride
    bool use_full_stride = (stride_px == LVGL_PORT_H_RES);
    size_t src_stride_px = use_full_stride ? LVGL_PORT_H_RES : rect_w;
    size_t src_offset_x = use_full_stride ? area->x1 : 0;

    // Perform cache-aligned synchronization
    const size_t CACHE_LINE = data_cache_line_size;
    size_t flush_size = src_stride_px * rect_h * lvgl_color_format_bytes;
    uintptr_t start_addr = (uintptr_t)color_map;
    uintptr_t aligned_start = start_addr & ~(CACHE_LINE - 1);
    size_t align_padding = start_addr - aligned_start;
    size_t aligned_size = ((flush_size + align_padding + CACHE_LINE - 1) / CACHE_LINE) * CACHE_LINE;
    esp_cache_msync((void *)aligned_start, aligned_size, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

    esp_async_fbcpy_trans_desc_t blit = {
        .src_buffer        = color_map,
        .dst_buffer        = back_fb,
        .src_buffer_size_x = src_stride_px,
        .src_buffer_size_y = rect_h,
        .src_offset_x      = src_offset_x,
        .src_offset_y      = 0,
        .dst_buffer_size_x = LVGL_PORT_H_RES,
        .dst_buffer_size_y = LVGL_PORT_V_RES,
        .dst_offset_x      = area->x1,
        .dst_offset_y      = area->y1,
        .copy_size_x       = rect_w,
        .copy_size_y       = rect_h,
        .pixel_format_unique_id = {
            .color_type_id = (lvgl_color_format_bytes == 2) ?
            COLOR_TYPE_ID(COLOR_SPACE_RGB, COLOR_PIXEL_RGB565) :
            COLOR_TYPE_ID(COLOR_SPACE_RGB, COLOR_PIXEL_RGB888)
        },
    };

    ESP_ERROR_CHECK(dma2d_copy_sync(&blit, portMAX_DELAY));

#else /* !SOC_DMA2D_SUPPORTED */
    // Fallback to memcpy for non-DMA2D platforms
    const int bytes_per_pixel = lvgl_color_format_bytes;
    const int bytes_per_line = LVGL_PORT_H_RES * bytes_per_pixel;
    const int copy_bytes = (area->x2 - area->x1 + 1) * bytes_per_pixel;
    uint8_t *src = (uint8_t*)color_map;
    uint8_t *dst = (uint8_t *)back_fb + (area->y1 * LVGL_PORT_H_RES + area->x1) * bytes_per_pixel;

    for (int y = area->y1; y <= area->y2; y++) {
        memcpy(dst, src, copy_bytes);
        dst += bytes_per_line;
        src += copy_bytes;
    }

#endif /* SOC_DMA2D_SUPPORTED */

#endif /* !EXAMPLE_LVGL_PORT_ROTATION_DEGREE */

    if (lv_disp_flush_is_last(drv)) {
        LVGL_FPS_STATS_RECORD_FRAME();

#if EXAMPLE_LVGL_PORT_ROTATION_DEGREE != 0 && !LVGL_PORT_PPA_ROTATION_ENABLE
        esp_cache_msync(back_fb, fb_size,
                        ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
#endif /* EXAMPLE_LVGL_PORT_ROTATION_DEGREE != 0 && !LVGL_PORT_PPA_ROTATION_ENABLE */

        lv_disp_t *disp_refr = _lv_refr_get_disp_refreshing();
        copy_unrendered_area_from_front_to_back(disp_refr);

        // Display back buffer and wait for VSYNC
        switch_lcd_frame_buffer_to(panel, back_fb);

        /* front→back, back→spare, spare→front */
        void *tmp = front_fb;
        front_fb = back_fb;
        back_fb  = spare_fb;
        spare_fb = tmp;
    }

    /* Notify LVGL that flush is complete */
    lv_disp_flush_ready(drv);
}

#endif /* !LVGL_PORT_DIRECT_MODE && !LVGL_PORT_FULL_REFRESH && LVGL_PORT_LCD_BUFFER_NUMS == 3 */

#else /* LVGL_PORT_DIRECT_MODE || LVGL_PORT_FULL_REFRESH || LVGL_PORT_LCD_BUFFER_NUMS != 3 */

void flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

    /* Just copy data from the color map to the LCD frame buffer */
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);

    if (lvgl_port_interface != LVGL_PORT_INTERFACE_MIPI_DSI_DMA) {
        lv_disp_flush_ready(drv);
    }
}

#endif /* LVGL_PORT_AVOID_TEAR_ENABLE */

static lv_disp_t *display_init(esp_lcd_panel_handle_t panel_handle)
{
#if LVGL_PORT_PPA_ROTATION_ENABLE
    // Initialize the PPA
    ppa_client_config_t ppa_srm_config = {
        .oper_type = PPA_OPERATION_SRM,
    };
    ESP_ERROR_CHECK(ppa_register_client(&ppa_srm_config, &ppa_srm_handle));
#endif /* LVGL_PORT_PPA_ROTATION_ENABLE */

#if SOC_DMA2D_SUPPORTED
    ESP_ERROR_CHECK(esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &data_cache_line_size));
#endif /* SOC_DMA2D_SUPPORTED */

    assert(panel_handle);

    static lv_disp_draw_buf_t disp_buf = { 0 };     // Contains internal graphic buffer(s) called draw buffer(s)
    static lv_disp_drv_t disp_drv = { 0 };          // Contains LCD panel handle and callback functions

    // alloc draw buffers used by LVGL
    void *buf1 = NULL;
    void *buf2 = NULL;
    int buffer_size = 0;

    ESP_LOGD(TAG, "Malloc memory for LVGL buffer");
#if LVGL_PORT_AVOID_TEAR_ENABLE
    buffer_size = LVGL_PORT_H_RES * LVGL_PORT_V_RES;
#if (LVGL_PORT_LCD_BUFFER_NUMS == 3) && (EXAMPLE_LVGL_PORT_ROTATION_DEGREE == 0) && LVGL_PORT_FULL_REFRESH
    ESP_ERROR_CHECK(lvgl_get_lcd_frame_buffer(panel_handle, 3, &lvgl_port_rgb_last_buf, &buf1, &buf2));
    lvgl_port_rgb_next_buf = lvgl_port_rgb_last_buf;
    lvgl_port_flush_next_buf = buf2;
#elif (LVGL_PORT_LCD_BUFFER_NUMS == 3) && (EXAMPLE_LVGL_PORT_ROTATION_DEGREE != 0) && (LVGL_PORT_DIRECT_MODE || LVGL_PORT_FULL_REFRESH || LVGL_PORT_LCD_BUFFER_NUMS != 3)
    void *fbs[3];
    ESP_ERROR_CHECK(lvgl_get_lcd_frame_buffer(panel_handle, 3, &fbs[0], &fbs[1], &fbs[2]));
    buf1 = fbs[2];
#elif (LVGL_PORT_LCD_BUFFER_NUMS == 3) && (!LVGL_PORT_DIRECT_MODE && !LVGL_PORT_FULL_REFRESH && LVGL_PORT_LCD_BUFFER_NUMS == 3)
    void *fb0 = NULL;
    void *fb1 = NULL;
    void *fb2 = NULL;

    buffer_size = LVGL_PORT_H_RES * LVGL_PORT_BUFFER_HEIGHT;
    buf1 = heap_caps_malloc(buffer_size * sizeof(lv_color_t), (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    assert(buf1);
    buf2 = NULL;

    ESP_ERROR_CHECK(lvgl_get_lcd_frame_buffer(panel_handle, 3, &fb0, &fb1, &fb2));
    front_fb = fb0;
    back_fb  = fb1;
    spare_fb = fb2;
    fb_size = LVGL_PORT_H_RES * LVGL_PORT_V_RES * sizeof(lv_color_t);

    lvgl_color_format_bytes = sizeof(lv_color_t);

#if SOC_DMA2D_SUPPORTED
    esp_async_fbcpy_config_t cfg = { };
    ESP_ERROR_CHECK(esp_async_fbcpy_install(&cfg, &fbcpy_handle));

    dma2d_mutex = xSemaphoreCreateMutex();
    dma2d_done_sem = xSemaphoreCreateBinary();
    assert(dma2d_mutex && dma2d_done_sem);

#endif /* SOC_DMA2D_SUPPORTED */

#else /* LVGL_PORT_DIRECT_MODE */
    ESP_ERROR_CHECK(lvgl_get_lcd_frame_buffer(panel_handle, 2, &buf1, &buf2));
#endif

#else /* !LVGL_PORT_AVOID_TEAR_ENABLE */
    buffer_size = LVGL_PORT_H_RES * LVGL_PORT_BUFFER_HEIGHT;
    buf1 = heap_caps_malloc(buffer_size * sizeof(lv_color_t), LVGL_PORT_BUFFER_MALLOC_CAPS);
    assert(buf1);
    ESP_LOGI(TAG, "LVGL buffer size: %dKB", buffer_size * sizeof(lv_color_t) / 1024);
#endif /* LVGL_PORT_AVOID_TEAR_ENABLE */

    // initialize LVGL draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, buffer_size);

    ESP_LOGD(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
#if EXAMPLE_LVGL_PORT_ROTATION_90 || EXAMPLE_LVGL_PORT_ROTATION_270
    disp_drv.hor_res = LVGL_PORT_V_RES;
    disp_drv.ver_res = LVGL_PORT_H_RES;
#else /* EXAMPLE_LVGL_PORT_ROTATION_0 || EXAMPLE_LVGL_PORT_ROTATION_180 */
    disp_drv.hor_res = LVGL_PORT_H_RES;
    disp_drv.ver_res = LVGL_PORT_V_RES;
#endif
    disp_drv.flush_cb = flush_callback;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
#if LVGL_PORT_FULL_REFRESH
    disp_drv.full_refresh = 1;
#elif LVGL_PORT_DIRECT_MODE
    disp_drv.direct_mode = 1;
#endif

    return lv_disp_drv_register(&disp_drv);
}

static void touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    esp_lcd_touch_handle_t tp = (esp_lcd_touch_handle_t)indev_drv->user_data;
    assert(tp);

    uint16_t touchpad_x;
    uint16_t touchpad_y;
    uint8_t touchpad_cnt = 0;
    /* Read data from touch controller into memory */
    esp_lcd_touch_read_data(tp);

    /* Read data from touch controller */
    bool touchpad_pressed = esp_lcd_touch_get_coordinates(tp, &touchpad_x, &touchpad_y, NULL, &touchpad_cnt, 1);
    if (touchpad_pressed && touchpad_cnt > 0) {
        data->point.x = touchpad_x;
        data->point.y = touchpad_y;
        data->state = LV_INDEV_STATE_PRESSED;
        ESP_LOGD(TAG, "Touch position: %d,%d", touchpad_x, touchpad_y);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static lv_indev_t *indev_init(esp_lcd_touch_handle_t tp)
{
    assert(tp);

    static lv_indev_drv_t indev_drv_tp;

    /* Register a touchpad input device */
    lv_indev_drv_init(&indev_drv_tp);
    indev_drv_tp.type = LV_INDEV_TYPE_POINTER;
    indev_drv_tp.read_cb = touchpad_read;
    indev_drv_tp.user_data = tp;

    return lv_indev_drv_register(&indev_drv_tp);
}

static void tick_increment(void *arg)
{
    /* Tell LVGL how many milliseconds have elapsed */
    lv_tick_inc(LVGL_PORT_TICK_PERIOD_MS);
}

static esp_err_t tick_init(void)
{
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &tick_increment,
        .name = "LVGL tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    return esp_timer_start_periodic(lvgl_tick_timer, LVGL_PORT_TICK_PERIOD_MS * 1000);
}

static void lvgl_port_task(void *arg)
{
    ESP_LOGD(TAG, "Starting LVGL task");

    uint32_t task_delay_ms = LVGL_PORT_TASK_MAX_DELAY_MS;
    while (1) {
        if (lvgl_port_lock(-1)) {
            task_delay_ms = lv_timer_handler();
            lvgl_port_unlock();
        }

        if (task_delay_ms > LVGL_PORT_TASK_MAX_DELAY_MS) {
            task_delay_ms = LVGL_PORT_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < LVGL_PORT_TASK_MIN_DELAY_MS) {
            task_delay_ms = LVGL_PORT_TASK_MIN_DELAY_MS;
        }

        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

esp_err_t lvgl_port_init(esp_lcd_panel_handle_t lcd_handle, esp_lcd_touch_handle_t tp_handle, lvgl_port_interface_t interface)
{
    lv_init();
    ESP_ERROR_CHECK(tick_init());

    lvgl_port_interface = interface;
#if LVGL_PORT_AVOID_TEAR_ENABLE
    switch (interface) {
#if SOC_LCDCAM_RGB_LCD_SUPPORTED
    case LVGL_PORT_INTERFACE_RGB:
        lvgl_get_lcd_frame_buffer = esp_lcd_rgb_panel_get_frame_buffer;
        break;
#endif

#if SOC_MIPI_DSI_SUPPORTED
    case LVGL_PORT_INTERFACE_MIPI_DSI_DMA:
    case LVGL_PORT_INTERFACE_MIPI_DSI_NO_DMA:
        lvgl_get_lcd_frame_buffer = esp_lcd_dpi_panel_get_frame_buffer;
        break;
#endif

    default:
        ESP_LOGE(TAG, "Invalid interface type");
        return ESP_ERR_INVALID_ARG;
    }
#endif

    lv_disp_t *disp = display_init(lcd_handle);
    assert(disp);

    if (tp_handle) {
        lv_indev_t *indev = indev_init(tp_handle);
        assert(indev);
#if EXAMPLE_LVGL_PORT_ROTATION_90
        esp_lcd_touch_set_swap_xy(tp_handle, true);
        esp_lcd_touch_set_mirror_y(tp_handle, true);
#elif EXAMPLE_LVGL_PORT_ROTATION_180
        esp_lcd_touch_set_mirror_x(tp_handle, false);
        esp_lcd_touch_set_mirror_y(tp_handle, false);
#elif EXAMPLE_LVGL_PORT_ROTATION_270
        esp_lcd_touch_set_swap_xy(tp_handle, true);
        esp_lcd_touch_set_mirror_x(tp_handle, true);
#else
        esp_lcd_touch_set_mirror_x(tp_handle, true);
        esp_lcd_touch_set_mirror_y(tp_handle, true);
#endif
    }

    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    assert(lvgl_mux);

    ESP_LOGI(TAG, "Create LVGL task");
    BaseType_t core_id = (LVGL_PORT_TASK_CORE < 0) ? tskNO_AFFINITY : LVGL_PORT_TASK_CORE;
    BaseType_t ret = xTaskCreatePinnedToCore(lvgl_port_task, "lvgl", LVGL_PORT_TASK_STACK_SIZE, NULL,
                                             LVGL_PORT_TASK_PRIORITY, &lvgl_task_handle, core_id);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LVGL task");
        return ESP_FAIL;
    }

    LVGL_FPS_STATS_INIT();

    return ESP_OK;
}

bool lvgl_port_lock(int timeout_ms)
{
    assert(lvgl_mux && "lvgl_port_init must be called first");

    const TickType_t timeout_ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

void lvgl_port_unlock(void)
{
    assert(lvgl_mux && "lvgl_port_init must be called first");
    xSemaphoreGiveRecursive(lvgl_mux);
}

bool lvgl_port_notify_lcd_vsync(void)
{
    BaseType_t need_yield = pdFALSE;

#if LVGL_PORT_FULL_REFRESH && (LVGL_PORT_LCD_RGB_BUFFER_NUMS == 3) && (EXAMPLE_LVGL_PORT_ROTATION_DEGREE == 0)
    if (lvgl_port_rgb_next_buf != lvgl_port_rgb_last_buf) {
        lvgl_port_flush_next_buf = lvgl_port_rgb_last_buf;
        lvgl_port_rgb_last_buf = lvgl_port_rgb_next_buf;
    }
#elif LVGL_PORT_AVOID_TEAR_ENABLE
    // Notify that the current LCD frame buffer has been transmitted
    if (lvgl_task_handle) {
        xTaskNotifyFromISR(lvgl_task_handle, ULONG_MAX, eNoAction, &need_yield);
    }
#else
    if (lvgl_port_interface == LVGL_PORT_INTERFACE_MIPI_DSI_DMA) {
        lv_disp_drv_t *drv = lv_disp_get_default()->driver;
        lv_disp_flush_ready(drv);
    }
#endif
    return (need_yield == pdTRUE);
}
