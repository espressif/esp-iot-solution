/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "soc/soc_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
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
#if CONFIG_IDF_TARGET_ESP32P4
#include "esp_private/esp_cache_private.h"
#include "driver/ppa.h"
#endif
#include "lvgl.h"
#include "lvgl_port_v8.h"

#define ALIGN_UP_BY(num, align)    (((num) + ((align) - 1)) & ~((align) - 1))
#define BLOCK_SIZE_SMALL           (32)
#define BLOCK_SIZE_LARGE           (256)

static const char *TAG = "lv_port";

typedef esp_err_t (*get_lcd_frame_buffer_cb_t)(esp_lcd_panel_handle_t panel, uint32_t fb_num, void **fb0, ...);

#if LVGL_PORT_PPA_ROTATION_ENABLE
static ppa_client_handle_t ppa_srm_handle = NULL;
static size_t data_cache_line_size = 0;
#endif

static SemaphoreHandle_t lvgl_mux;                  // LVGL mutex
static TaskHandle_t lvgl_task_handle = NULL;
static lvgl_port_interface_t lvgl_port_interface = LVGL_PORT_INTERFACE_RGB;

#if LVGL_PORT_AVOID_TEAR_ENABLE
static get_lcd_frame_buffer_cb_t lvgl_get_lcd_frame_buffer = NULL;
#endif

#if EXAMPLE_LVGL_PORT_ROTATION_DEGREE != 0
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
#endif

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

    ESP_ERROR_CHECK(ppa_do_scale_rotate_mirror(ppa_srm_handle, &oper_config));

#else
    // Fallback: optimized transpose for non-PPA systems
    rotate_image(from, to, w, h, rotation, LV_COLOR_DEPTH);
#endif
}

#endif /* EXAMPLE_LVGL_PORT_ROTATION_DEGREE */

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

#else

static void flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;

    /* Action after last area refresh */
    if (lv_disp_flush_is_last(drv)) {
        /* Switch the current LCD frame buffer to `color_map` */
        switch_lcd_frame_buffer_to(panel_handle, color_map);

        /* Waiting for the last frame buffer to complete transmission */
        ulTaskNotifyValueClear(NULL, ULONG_MAX);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }

    lv_disp_flush_ready(drv);
}
#endif /* EXAMPLE_LVGL_PORT_ROTATION_DEGREE */

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
#endif

#else

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
    ESP_ERROR_CHECK(esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &data_cache_line_size));
#endif

    assert(panel_handle);

    static lv_disp_draw_buf_t disp_buf = { 0 };     // Contains internal graphic buffer(s) called draw buffer(s)
    static lv_disp_drv_t disp_drv = { 0 };          // Contains LCD panel handle and callback functions

    // alloc draw buffers used by LVGL
    void *buf1 = NULL;
    void *buf2 = NULL;
    int buffer_size = 0;

    ESP_LOGD(TAG, "Malloc memory for LVGL buffer");
#if LVGL_PORT_AVOID_TEAR_ENABLE
    // To avoid the tearing effect, we should use at least two frame buffers: one for LVGL rendering and another for RGB output
    buffer_size = LVGL_PORT_H_RES * LVGL_PORT_V_RES;
#if (LVGL_PORT_LCD_BUFFER_NUMS == 3) && (EXAMPLE_LVGL_PORT_ROTATION_DEGREE == 0) && LVGL_PORT_FULL_REFRESH
    // With the usage of three buffers and full-refresh, we always have one buffer available for rendering, eliminating the need to wait for the RGB's sync signal
    ESP_ERROR_CHECK(lvgl_get_lcd_frame_buffer(panel_handle, 3, &lvgl_port_rgb_last_buf, &buf1, &buf2));
    lvgl_port_rgb_next_buf = lvgl_port_rgb_last_buf;
    lvgl_port_flush_next_buf = buf2;
#elif (LVGL_PORT_LCD_BUFFER_NUMS == 3) && (EXAMPLE_LVGL_PORT_ROTATION_DEGREE != 0)
    // Here we are using three frame buffers, one for LVGL rendering, and the other two for RGB driver (one of them is used for rotation)
    void *fbs[3];
    ESP_ERROR_CHECK(lvgl_get_lcd_frame_buffer(panel_handle, 3, &fbs[0], &fbs[1], &fbs[2]));
    buf1 = fbs[2];
#else
    ESP_ERROR_CHECK(lvgl_get_lcd_frame_buffer(panel_handle, 2, &buf1, &buf2));
#endif
#else
    // Normmaly, for RGB LCD, we just use one buffer for LVGL rendering
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
#else
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
        esp_lcd_touch_set_mirror_x(tp_handle, true);
#elif EXAMPLE_LVGL_PORT_ROTATION_180
        esp_lcd_touch_set_mirror_x(tp_handle, true);
        esp_lcd_touch_set_mirror_y(tp_handle, true);
#elif EXAMPLE_LVGL_PORT_ROTATION_270
        esp_lcd_touch_set_swap_xy(tp_handle, true);
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
