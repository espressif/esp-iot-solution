/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_cache.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include "lvgl.h"
#include "esp_lcd_st77903.h"
#include "lv_port.h"

static const char *TAG = "lvgl_port";
static esp_lcd_panel_handle_t panel_handle = NULL;
static SemaphoreHandle_t lvgl_mux = NULL;                  // LVGL mutex
static TaskHandle_t lvgl_task_handle = NULL;

static void disp_init(void);
static void tick_init(void);
static void lv_task(void *arg);

void lv_port_init(void)
{
    lv_init();
    disp_init();
    tick_init();

    lvgl_mux = xSemaphoreCreateMutex();
    assert(lvgl_mux);
    xTaskCreate(lv_task, "lvgl", EXAMPLE_LVGL_TASK_STACK_SIZE, NULL, EXAMPLE_LVGL_TASK_PRIORITY, &lvgl_task_handle);
}

bool lv_port_lock(uint32_t timeout_ms)
{
    assert(lvgl_mux && "lv_port_start must be called first");

    const TickType_t timeout_ticks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(lvgl_mux, timeout_ticks) == pdTRUE;
}

void lv_port_unlock(void)
{
    assert(lvgl_mux && "lv_port_start must be called first");
    xSemaphoreGive(lvgl_mux);
}

#if CONFIG_EXAMPLE_DISPLAY_LVGL_ROTATION_DEGREE != 0
static void *get_next_frame_buffer(esp_lcd_panel_handle_t panel_handle)
{
    static void *next_fb = NULL;
    static void *fb[2] = { NULL };
    if (next_fb == NULL) {
        esp_lcd_st77903_qspi_get_frame_buffer(panel_handle, 2, &fb[0], &fb[1]);
        next_fb = fb[1];
    } else {
        next_fb = (next_fb == fb[0]) ? fb[1] : fb[0];
    }
    return next_fb;
}

typedef struct {
    uint8_t b;
    uint8_t g;
    uint8_t r;
} color_24bpp_t;

__attribute__((always_inline))
static inline void copy_pixel_16bpp_to_16bpp(void *to, const void *from)
{
    *(uint16_t *)to++ = *(const uint16_t *)from++;
}

__attribute__((always_inline))
static inline void copy_pixel_16bpp_to_24bpp(void *to, const void *from)
{
    color_24bpp_t to_piexl;
    uint16_t from_pixel = *(const uint16_t *)from;
    to_piexl.b = from_pixel << 3;
    to_piexl.r = (from_pixel & 0x07E0) >> 3;
    to_piexl.g = (from_pixel & 0xF800) >> 8;
    *(color_24bpp_t *)to = to_piexl;
}

__attribute__((always_inline))
static inline void copy_pixel_32bpp_to_24bpp(void *to, const void *from)
{
    color_24bpp_t to_piexl;
    lv_color32_t from_pixel = *(const lv_color32_t *)from;
    to_piexl.b = from_pixel.ch.blue;
    to_piexl.r = from_pixel.ch.green;
    to_piexl.g = from_pixel.ch.red;
    *(color_24bpp_t *)to = to_piexl;
}

#define _COPY_PIXEL(to_bpp, from_bpp, to, from) copy_pixel_ ## from_bpp ## bpp_to_ ## to_bpp ## bpp(to, from)
#define COPY_PIXEL(to_bpp, from_bpp, to, from)  _COPY_PIXEL(to_bpp, from_bpp, to, from)

IRAM_ATTR void rotate_copy_pixel(void *to, const void *from, uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint16_t w, uint16_t h, lv_disp_rot_t rotate)
{
    int from_bytes_per_piexl = CONFIG_LV_COLOR_DEPTH >> 3;
    int from_bytes_per_line = w * from_bytes_per_piexl;
    int from_index = x_start * from_bytes_per_piexl;
    int from_index_const = 0;

    int to_bytes_per_piexl = EXAMPLE_LCD_BIT_PER_PIXEL >> 3;
    int to_bytes_per_line;
    int to_index = 0;
    int to_index_const = 0;

    uint8_t *to_p = (uint8_t *)to;
    const uint8_t *from_p = (const uint8_t *)from;

    switch (rotate) {
    case LV_DISP_ROT_NONE:
        to_bytes_per_line = w * to_bytes_per_piexl;
        to_index_const = x_start * from_bytes_per_piexl;
        for (int from_y = y_start; from_y < y_end + 1; from_y++) {
            from_index = from_y * from_bytes_per_line + from_index_const;
            to_index = from_y * to_bytes_per_line + to_index_const;
            for (int from_x = x_start; from_x < x_end + 1; from_x++) {
                COPY_PIXEL(EXAMPLE_LCD_BIT_PER_PIXEL, CONFIG_LV_COLOR_DEPTH, to_p + to_index, from_p + from_index);
                from_index += from_bytes_per_piexl;
                to_index += to_bytes_per_piexl;
            }
        }
        break;
    case LV_DISP_ROT_90:
        to_bytes_per_line = h * to_bytes_per_piexl;
        to_index_const = x_start * to_bytes_per_line + (h - 1) * to_bytes_per_piexl;
        for (int from_y = y_start; from_y < y_end + 1; from_y++) {
            from_index = from_y * from_bytes_per_line + from_index_const;
            to_index = to_index_const - from_y * to_bytes_per_piexl;
            for (int from_x = x_start; from_x < x_end + 1; from_x++) {
                COPY_PIXEL(EXAMPLE_LCD_BIT_PER_PIXEL, CONFIG_LV_COLOR_DEPTH, to_p + to_index, from_p + from_index);
                from_index += from_bytes_per_piexl;
                to_index += to_bytes_per_line;
            }
        }
        break;
    case LV_DISP_ROT_180:
        to_bytes_per_line = w * to_bytes_per_piexl;
        to_index_const = (h - 1) * to_bytes_per_line + (w - x_start - 1) * to_bytes_per_piexl;
        for (int from_y = y_start; from_y < y_end + 1; from_y++) {
            from_index = from_y * from_bytes_per_line + from_index_const;
            to_index = to_index_const - from_y * to_bytes_per_line;
            for (int from_x = x_start; from_x < x_end + 1; from_x++) {
                COPY_PIXEL(EXAMPLE_LCD_BIT_PER_PIXEL, CONFIG_LV_COLOR_DEPTH, to_p + to_index, from_p + from_index);
                from_index += from_bytes_per_piexl;
                to_index -= to_bytes_per_piexl;
            }
        }
        break;
    case LV_DISP_ROT_270:
        to_bytes_per_line = h * to_bytes_per_piexl;
        to_index_const = (w - x_start - 1) * to_bytes_per_line;
        for (int from_y = y_start; from_y < y_end + 1; from_y++) {
            from_index = from_y * from_bytes_per_line + from_index_const;
            to_index = to_index_const + from_y * to_bytes_per_piexl;
            for (int from_x = x_start; from_x < x_end + 1; from_x++) {
                COPY_PIXEL(EXAMPLE_LCD_BIT_PER_PIXEL, CONFIG_LV_COLOR_DEPTH, to_p + to_index, from_p + from_index);
                from_index += from_bytes_per_piexl;
                to_index -= to_bytes_per_line;
            }
        }
        break;
    default:
        break;
    }
}

#endif /* CONFIG_EXAMPLE_DISPLAY_LVGL_ROTATION_DEGREE */

#if CONFIG_EXAMPLE_DISPLAY_LVGL_AVOID_TEAR
#if CONFIG_EXAMPLE_DISPLAY_LVGL_DIRECT_MODE
typedef struct {
    uint16_t inv_p;
    uint8_t inv_area_joined[LV_INV_BUF_SIZE];
    lv_area_t inv_areas[LV_INV_BUF_SIZE];
} lv_port_dirty_area_t;

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

typedef enum {
    FLUSH_STATUS_PART,
    FLUSH_STATUS_FULL
} lv_port_flush_status_t;

typedef enum {
    FLUSH_PROBE_PART_COPY,
    FLUSH_PROBE_SKIP_COPY,
    FLUSH_PROBE_FULL_COPY,
} lv_port_flush_probe_t;

/**
 * @brief Probe dirty area to copy
 *
 * @note This function is used to avoid tearing effect, and only work with LVGL direct-mode.
 *
 */
static lv_port_flush_probe_t flush_copy_probe(lv_disp_drv_t *drv)
{
    static lv_port_flush_status_t prev_status = FLUSH_PROBE_PART_COPY;
    lv_port_flush_status_t cur_status;
    uint8_t probe_result;
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

#if CONFIG_EXAMPLE_DISPLAY_LVGL_ROTATION_DEGREE != 0
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
    lv_disp_t *disp_refr = _lv_refr_get_disp_refreshing();

    lv_coord_t x_start, x_end, y_start, y_end;
    for (int i = 0; i < disp_refr->inv_p; i++) {
        /* Refresh the unjoined areas*/
        if (disp_refr->inv_area_joined[i] == 0) {
            x_start = disp_refr->inv_areas[i].x1;
            x_end = disp_refr->inv_areas[i].x2;
            y_start = disp_refr->inv_areas[i].y1;
            y_end = disp_refr->inv_areas[i].y2;

            rotate_copy_pixel(dst, src, x_start, y_start, x_end, y_end, LV_HOR_RES, LV_VER_RES, CONFIG_EXAMPLE_DISPLAY_LVGL_ROTATION_DEGREE);
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

    void *next_fb = flush_get_next_buf(panel_handle);

    lv_port_flush_probe_t probe_result;
    /* Action after last area refresh */
    if (lv_disp_flush_is_last(drv)) {
        if (drv->full_refresh) {
            drv->full_refresh = 0;
            rotate_copy_pixel(next_fb, color_map, offsetx1, offsety1, offsetx2, offsety2, LV_HOR_RES, LV_VER_RES, CONFIG_EXAMPLE_DISPLAY_LVGL_ROTATION_DEGREE);
            /* Due to direct-mode, here we just swtich driver's pointer of frame buffer rather than draw bitmap */
            esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, next_fb);
            /* Waiting for the current frame buffer to complete transmission */
            ulTaskNotifyValueClear(NULL, ULONG_MAX);
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            flush_dirty_copy(flush_get_next_buf(panel_handle), color_map, &dirty_area);
            flush_get_next_buf(panel_handle);
        } else {
            /* Probe and copy dirty area from the current LVGL's frame buffer to the next LVGL's frame buffer */
            probe_result = flush_copy_probe(drv);
            if (probe_result == FLUSH_PROBE_FULL_COPY) {
                flush_dirty_save(&dirty_area);
                /* Set LVGL full-refresh flag and force to refresh whole screen */
                drv->full_refresh = 1;
                lv_disp_flush_ready(drv);
                lv_refr_now(_lv_refr_get_disp_refreshing());
            } else {
                rotate_copy_pixel(next_fb, color_map, offsetx1, offsety1, offsetx2, offsety2, LV_HOR_RES, LV_VER_RES, CONFIG_EXAMPLE_DISPLAY_LVGL_ROTATION_DEGREE);
                /* Due to full-refresh mode, here we just swtich pointer of frame buffer rather than draw bitmap */
                esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, next_fb);
                /* Waiting for the current frame buffer to complete transmission */
                ulTaskNotifyValueClear(NULL, ULONG_MAX);
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
                if (probe_result == FLUSH_PROBE_PART_COPY) {
                    flush_dirty_save(&dirty_area);
                    flush_dirty_copy(flush_get_next_buf(panel_handle), color_map, &dirty_area);
                    flush_get_next_buf(panel_handle);
                }
            }
        }
    }

    lv_disp_flush_ready(drv);
}
#else
static inline void *flush_get_next_buf(void *buf)
{
    lv_disp_t *disp = _lv_refr_get_disp_refreshing();
    lv_disp_draw_buf_t *draw_buf = disp->driver->draw_buf;
    return (buf == draw_buf->buf1) ? draw_buf->buf2 : draw_buf->buf1;
}

/**
 * @brief Copy dirty area
 *
 * @note This function is used to avoid tearing effect, and only work with LVGL direct-mode.
 *
 */
static void flush_dirty_copy(void *dst, void *src, lv_port_dirty_area_t *dirty_area)
{
    lv_disp_t *disp_refr = _lv_refr_get_disp_refreshing();

    lv_coord_t x_start, x_end, y_start, y_end;
    uint32_t copy_bytes_per_line;
    uint32_t h_res = disp_refr->driver->hor_res;
    uint32_t bytes_per_line = h_res * 2;
    uint8_t *from, *to;
    for (int i = 0; i < disp_refr->inv_p; i++) {
        /* Refresh the unjoined areas*/
        if (disp_refr->inv_area_joined[i] == 0) {
            x_start = disp_refr->inv_areas[i].x1;
            x_end = disp_refr->inv_areas[i].x2 + 1;
            y_start = disp_refr->inv_areas[i].y1;
            y_end = disp_refr->inv_areas[i].y2 + 1;

            copy_bytes_per_line = (x_end - x_start) * 2;
            from = src + (y_start * h_res + x_start) * 2;
            to = dst + (y_start * h_res + x_start) * 2;
            for (int y = y_start; y < y_end; y++) {
                memcpy(to, from, copy_bytes_per_line);
                from += bytes_per_line;
                to += bytes_per_line;
            }
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

    lv_port_flush_probe_t probe_result;
    /* Action after last area refresh */
    if (lv_disp_flush_is_last(drv)) {
        if (drv->full_refresh) {
            drv->full_refresh = 0;
            /* Due to direct-mode, here we just swtich driver's pointer of frame buffer rather than draw bitmap */
            esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
            /* Waiting for the current frame buffer to complete transmission */
            ulTaskNotifyValueClear(NULL, ULONG_MAX);
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            flush_dirty_copy(flush_get_next_buf(color_map), color_map, &dirty_area);
            drv->draw_buf->buf_act = (color_map == drv->draw_buf->buf1) ? drv->draw_buf->buf2 : drv->draw_buf->buf1;
        } else {
            /* Probe and copy dirty area from the current LVGL's frame buffer to the next LVGL's frame buffer */
            probe_result = flush_copy_probe(drv);
            if (probe_result == FLUSH_PROBE_FULL_COPY) {
                flush_dirty_save(&dirty_area);
                /* Set LVGL full-refresh flag and force to refresh whole screen */
                drv->full_refresh = 1;
                lv_disp_flush_ready(drv);
                lv_refr_now(_lv_refr_get_disp_refreshing());
            } else {
                /* Due to direct-mode, here we just swtich driver's pointer of frame buffer rather than draw bitmap */
                esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
                /* Waiting for the current frame buffer to complete transmission */
                ulTaskNotifyValueClear(NULL, ULONG_MAX);
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
                if (probe_result == FLUSH_PROBE_PART_COPY) {
                    flush_dirty_save(&dirty_area);
                    flush_dirty_copy(flush_get_next_buf(color_map), color_map, &dirty_area);
                }
            }
        }
    }

    lv_disp_flush_ready(drv);
}
#endif /* CONFIG_EXAMPLE_DISPLAY_LVGL_ROTATION_DEGREE */

#elif CONFIG_EXAMPLE_DISPLAY_LVGL_FULL_REFRESH && CONFIG_EXAMPLE_LCD_BUFFER_NUMS == 2

static void flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

    /* Due to full-refresh mode, here we just swtich pointer of frame buffer rather than draw bitmap */
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
    /* Waiting for the current frame buffer to complete transmission */
    ulTaskNotifyValueClear(NULL, ULONG_MAX);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    lv_disp_flush_ready(drv);
}

#elif CONFIG_EXAMPLE_DISPLAY_LVGL_FULL_REFRESH && CONFIG_EXAMPLE_LCD_BUFFER_NUMS == 3

#if CONFIG_EXAMPLE_DISPLAY_LVGL_ROTATION_DEGREE == 0
static void *lvgl_port_rgb_last_buf = NULL;
static void *lvgl_port_rgb_next_buf = NULL;
static void *lvgl_port_flush_next_buf = NULL;
#endif

void flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

#if CONFIG_EXAMPLE_DISPLAY_LVGL_ROTATION_DEGREE != 0
    void *next_fb = get_next_frame_buffer(panel_handle);
    rotate_copy_pixel(next_fb, (const uint8_t *)color_map, offsetx1, offsety1, offsetx2, offsety2, LV_HOR_RES, LV_VER_RES, CONFIG_EXAMPLE_DISPLAY_LVGL_ROTATION_DEGREE);
    /* Due to full-refresh mode, here we just swtich pointer of frame buffer rather than draw bitmap */
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, next_fb);
    // Since the time of full-refresh is normally longer than the time of one frame output, we don't have to wait for the current frame buffer to complete transmission
#else
    drv->draw_buf->buf1 = color_map;
    drv->draw_buf->buf2 = lvgl_port_flush_next_buf;
    lvgl_port_flush_next_buf = color_map;
    /* Due to full-refresh mode, here we just swtich pointer of frame buffer rather than draw bitmap */
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
    lvgl_port_rgb_next_buf = color_map;
#endif

    lv_disp_flush_ready(drv);
}
#endif

static bool lcd_on_trans(esp_lcd_panel_handle_t panel, const st77903_qspi_event_data_t *edata, void *user_ctx)
{
    BaseType_t need_yield = pdFALSE;
#if CONFIG_EXAMPLE_DISPLAY_LVGL_FULL_REFRESH && (CONFIG_EXAMPLE_LCD_BUFFER_NUMS == 3) && (CONFIG_EXAMPLE_DISPLAY_LVGL_ROTATION_DEGREE == 0)
    if (lvgl_port_rgb_next_buf != lvgl_port_rgb_last_buf) {
        lvgl_port_flush_next_buf = lvgl_port_rgb_last_buf;
        lvgl_port_rgb_last_buf = lvgl_port_rgb_next_buf;
    }
#else
    xTaskNotifyFromISR(lvgl_task_handle, ULONG_MAX, eNoAction, &need_yield);
#endif
    return (need_yield == pdTRUE);
}

#else

void flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

#if CONFIG_LV_COLOR_DEPTH == 32
    uint8_t *to = (uint8_t *)color_map;
    uint16_t w = offsetx2 - offsetx1 + 1;
    uint16_t h = offsety2 - offsety1 + 1;
    for (int i = 0; i < w * h; i++) {
        if (i == 0) {
            color_map[i].ch.alpha = color_map[i].ch.blue;
            to[i * 3] = color_map[i].ch.red;
            to[i * 3 + 2] = color_map[i].ch.alpha;
        } else {
            to[i * 3] = color_map[i].ch.red;
            to[i * 3 + 1] = color_map[i].ch.green;
            to[i * 3 + 2] = color_map[i].ch.blue;
        }
    }
#endif

    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);

    lv_disp_flush_ready(drv);
}

#endif /* CONFIG_EXAMPLE_DISPLAY_LVGL_AVOID_TEAR */

static void update_callback(lv_disp_drv_t *drv)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;

    switch (drv->rotated) {
    case LV_DISP_ROT_NONE:
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, false);
        break;
    case LV_DISP_ROT_90:
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, false, true);
        break;
    case LV_DISP_ROT_180:
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, true, true);
        break;
    case LV_DISP_ROT_270:
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, true, false);
        break;
    }
}

static void disp_init(void)
{
    ESP_LOGI(TAG, "Install ST77903 panel driver");
    st77903_qspi_config_t qspi_config = ST77903_QSPI_CONFIG_DEFAULT(EXAMPLE_LCD_HOST,
                                                                    EXAMPLE_PIN_NUM_LCD_CS,
                                                                    EXAMPLE_PIN_NUM_LCD_SCK,
                                                                    EXAMPLE_PIN_NUM_LCD_D0,
                                                                    EXAMPLE_PIN_NUM_LCD_D1,
                                                                    EXAMPLE_PIN_NUM_LCD_D2,
                                                                    EXAMPLE_PIN_NUM_LCD_D3,
                                                                    CONFIG_EXAMPLE_LCD_BUFFER_NUMS,
                                                                    EXAMPLE_LCD_H_RES,
                                                                    EXAMPLE_LCD_V_RES);
    st77903_vendor_config_t vendor_config = {
        .qspi_config = &qspi_config,
        .flags = {
            .use_qspi_interface = 1,
            .mirror_by_cmd = 1,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_endian = LCD_RGB_ENDIAN_BGR,
        .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st77903(NULL, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
#if CONFIG_EXAMPLE_DISPLAY_LVGL_AVOID_TEAR
    st77903_qspi_event_callbacks_t cbs = {
        .on_bounce_frame_finish = lcd_on_trans,
    };
    esp_lcd_st77903_qspi_register_event_callbacks(panel_handle, &cbs, NULL);
#endif
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    ESP_LOGI(TAG, "Initialize LVGL library");
    static lv_disp_drv_t disp_drv;
    static lv_disp_draw_buf_t disp_buf;
    lv_disp_drv_init(&disp_drv);

    void *buf1 = NULL;
    void *buf2 = NULL;
    int buffer_size = 0;
#ifndef CONFIG_EXAMPLE_DISPLAY_LVGL_AVOID_TEAR
    // Normmaly, we just use one buffer for LVGL rendering
    buffer_size = EXAMPLE_LCD_H_RES * EXAMPLE_LVGL_BUFFER_HEIGHT;
    buf1 = heap_caps_malloc(buffer_size * sizeof(lv_color_t), EXAMPLE_LVGL_BUFFER_MALLOC);
    assert(buf1);
    ESP_LOGI(TAG, "LVGL buffer size: %dKB", buffer_size * sizeof(lv_color_t) / 1024);
#else
    // To avoid the tearing effect, we should use at least two frame buffers: one for LVGL rendering and another for LCD output
    buffer_size = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES;
#if (CONFIG_EXAMPLE_LCD_BUFFER_NUMS == 3) && (CONFIG_EXAMPLE_DISPLAY_LVGL_ROTATION_DEGREE == 0) && CONFIG_EXAMPLE_DISPLAY_LVGL_FULL_REFRESH
    // With the usage of three buffers and full-refresh, we always have one buffer available for rendering, eliminating the need to wait for the RGB's sync signal
    ESP_ERROR_CHECK(esp_lcd_st77903_qspi_get_frame_buffer(panel_handle, 3, &lvgl_port_rgb_last_buf, &buf1, &buf2));
    lvgl_port_rgb_next_buf = lvgl_port_rgb_last_buf;
    lvgl_port_flush_next_buf = buf2;
#elif (CONFIG_EXAMPLE_LCD_BUFFER_NUMS == 3) && (CONFIG_EXAMPLE_DISPLAY_LVGL_ROTATION_DEGREE != 0)
    // Here we are using three frame buffers, one for LVGL rendering, and the other two for RGB driver (one of them is used for rotation)
    void *fbs[3];
    ESP_ERROR_CHECK(esp_lcd_st77903_qspi_get_frame_buffer(panel_handle, 3, &fbs[0], &fbs[1], &fbs[2]));
    buf1 = fbs[2];
#else
    ESP_ERROR_CHECK(esp_lcd_st77903_qspi_get_frame_buffer(panel_handle, 2, &buf1, &buf2));
#endif
#endif /* CONFIG_EXAMPLE_DISPLAY_LVGL_AVOID_TEAR */
    // initialize LVGL draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, buffer_size);

    disp_drv.hor_res = EXAMPLE_LCD_H_RES;
    disp_drv.ver_res = EXAMPLE_LCD_V_RES;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    disp_drv.flush_cb = flush_callback;
    disp_drv.drv_update_cb = update_callback;
#if CONFIG_EXAMPLE_DISPLAY_LVGL_FULL_REFRESH
    disp_drv.full_refresh = 1;
#elif CONFIG_EXAMPLE_DISPLAY_LVGL_DIRECT_MODE
    disp_drv.direct_mode = 1;
#endif
    lv_disp_drv_register(&disp_drv);
}

static void tick_increment(void *arg)
{
    /* Tell LVGL how many milliseconds have elapsed */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

static void tick_init(void)
{
    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &tick_increment,
        .name = "LVGL tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));
}

static void lv_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
    while (1) {
        // Lock the mutex due to the LVGL APIs are not thread-safe
        if (lv_port_lock(0)) {
            task_delay_ms = lv_timer_handler();
            // Release the mutex
            lv_port_unlock();
        }
        if (task_delay_ms > EXAMPLE_LVGL_TASK_MAX_DELAY_MS) {
            task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < EXAMPLE_LVGL_TASK_MIN_DELAY_MS) {
            task_delay_ms = EXAMPLE_LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}
