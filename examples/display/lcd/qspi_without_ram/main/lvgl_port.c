/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <inttypes.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "lvgl.h"
#include "lvgl_port.h"
#include "esp_lcd_st77903_qspi.h"

static const char *TAG = "lv_port";
static SemaphoreHandle_t lvgl_mux;                  // LVGL mutex
static TaskHandle_t lvgl_task_handle = NULL;
static SemaphoreHandle_t touch_isr_mux = NULL;

#if EXAMPLE_LVGL_PORT_ROTATION_DEGREE != 0
static void *get_next_frame_buffer(esp_lcd_panel_handle_t panel_handle)
{
    static void *next_fb = NULL;
    static void *fb[2] = { NULL };
    if (next_fb == NULL) {
        ESP_ERROR_CHECK(esp_lcd_st77903_qspi_get_frame_buffer(panel_handle, 2, &fb[0], &fb[1]));
        next_fb = fb[1];
    } else {
        next_fb = (next_fb == fb[0]) ? fb[1] : fb[0];
    }
    return next_fb;
}

IRAM_ATTR static void rotate_copy_pixel(const uint16_t *from, uint16_t *to, uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint16_t w, uint16_t h, uint16_t rotation)
{
    int from_index = 0;

    int to_index = 0;
    int to_index_const = 0;

    switch (rotation) {
    case 90:
        to_index_const = (w - x_start - 1) * h;
        for (int from_y = y_start; from_y < y_end + 1; from_y++) {
            from_index = from_y * w + x_start;
            to_index = to_index_const + from_y;
            for (int from_x = x_start; from_x < x_end + 1; from_x++) {
                *(to + to_index) = *(from + from_index);
                from_index += 1;
                to_index -= h;
            }
        }
        break;
    case 180:
        to_index_const = h * w - x_start - 1;
        for (int from_y = y_start; from_y < y_end + 1; from_y++) {
            from_index = from_y * w + x_start;
            to_index = to_index_const - from_y * w;
            for (int from_x = x_start; from_x < x_end + 1; from_x++) {
                *(to + to_index) = *(from + from_index);
                from_index += 1;
                to_index -= 1;
            }
        }
        break;
    case 270:
        to_index_const = (x_start + 1) * h - 1;
        for (int from_y = y_start; from_y < y_end + 1; from_y++) {
            from_index = from_y * w + x_start;
            to_index = to_index_const - from_y;
            for (int from_x = x_start; from_x < x_end + 1; from_x++) {
                *(to + to_index) = *(from + from_index);
                from_index += 1;
                to_index += h;
            }
        }
        break;
    default:
        break;
    }
}
#endif /* EXAMPLE_LVGL_PORT_ROTATION_DEGREE */

#if LVGL_PORT_AVOID_TEAR_ENABLE
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

            /* Switch the current QSPI frame buffer to `next_fb` */
            esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, next_fb);

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

                /* Switch the current QSPI frame buffer to `next_fb` */
                esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, next_fb);

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
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

    /* Action after last area refresh */
    if (lv_disp_flush_is_last(drv)) {
        /* Switch the current QSPI frame buffer to `color_map` */
        esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);

        /* Waiting for the last frame buffer to complete transmission */
        ulTaskNotifyValueClear(NULL, ULONG_MAX);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }

    lv_disp_flush_ready(drv);
}
#endif /* EXAMPLE_LVGL_PORT_ROTATION_DEGREE */

#elif LVGL_PORT_FULL_REFRESH && LVGL_PORT_LCD_QSPI_BUFFER_NUMS == 2

static void flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

    /* Switch the current QSPI frame buffer to `color_map` */
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);

    /* Waiting for the last frame buffer to complete transmission */
    ulTaskNotifyValueClear(NULL, ULONG_MAX);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    lv_disp_flush_ready(drv);
}

#elif LVGL_PORT_FULL_REFRESH && LVGL_PORT_LCD_QSPI_BUFFER_NUMS == 3

#if EXAMPLE_LVGL_PORT_ROTATION_DEGREE == 0
static void *lvgl_port_qspi_last_buf = NULL;
static void *lvgl_port_qspi_next_buf = NULL;
static void *lvgl_port_flush_next_buf = NULL;
#endif

void flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

#if EXAMPLE_LVGL_PORT_ROTATION_DEGREE != 0
    void *next_fb = get_next_frame_buffer(panel_handle);

    /* Rotate and copy dirty area from the current LVGL's buffer to the next QSPI frame buffer */
    rotate_copy_pixel((uint16_t *)color_map, next_fb, offsetx1, offsety1, offsetx2, offsety2, LV_HOR_RES, LV_VER_RES, EXAMPLE_LVGL_PORT_ROTATION_DEGREE);

    /* Switch the current QSPI frame buffer to `next_fb` */
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, next_fb);
#else
    drv->draw_buf->buf1 = color_map;
    drv->draw_buf->buf2 = lvgl_port_flush_next_buf;
    lvgl_port_flush_next_buf = color_map;

    /* Switch the current QSPI frame buffer to `color_map` */
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);

    lvgl_port_qspi_next_buf = color_map;
#endif

    lv_disp_flush_ready(drv);
}
#endif

IRAM_ATTR static bool qspi_lcd_on_trans_event(esp_lcd_panel_handle_t panel, const st77903_qspi_event_data_t *edata, void *user_ctx)
{
    BaseType_t need_yield = pdFALSE;
#if LVGL_PORT_FULL_REFRESH && (LVGL_PORT_LCD_QSPI_BUFFER_NUMS == 3) && (EXAMPLE_LVGL_PORT_ROTATION_DEGREE == 0)
    if (lvgl_port_qspi_next_buf != lvgl_port_qspi_last_buf) {
        lvgl_port_flush_next_buf = lvgl_port_qspi_last_buf;
        lvgl_port_qspi_last_buf = lvgl_port_qspi_next_buf;
    }
#elif LVGL_PORT_AVOID_TEAR_ENABLE
    // Notify that the current QSPI frame buffer has been transmitted
    if (lvgl_task_handle) {
        xTaskNotifyFromISR(lvgl_task_handle, ULONG_MAX, eNoAction, &need_yield);
    } else {
        ESP_LOGE(TAG, "lvgl_task_handle is NULL");
    }
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

    /* Just copy data from the color map to the QSPI frame buffer */
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);

    lv_disp_flush_ready(drv);
}

#endif /* LVGL_PORT_AVOID_TEAR_ENABLE */

static lv_disp_t *display_init(esp_lcd_panel_handle_t panel_handle)
{
    assert(panel_handle);

    static lv_disp_draw_buf_t disp_buf = { 0 };     // Contains internal graphic buffer(s) called draw buffer(s)
    static lv_disp_drv_t disp_drv = { 0 };          // Contains LCD panel handle and callback functions

    // alloc draw buffers used by LVGL
    void *buf1 = NULL;
    void *buf2 = NULL;
    int buffer_size = 0;

    ESP_LOGD(TAG, "Malloc memory for LVGL buffer");
#if LVGL_PORT_AVOID_TEAR_ENABLE
    // To avoid the tearing effect, we should use at least two frame buffers: one for LVGL rendering and another for QSPI output
    buffer_size = LVGL_PORT_H_RES * LVGL_PORT_V_RES;
#if (LVGL_PORT_LCD_QSPI_BUFFER_NUMS == 3) && (EXAMPLE_LVGL_PORT_ROTATION_DEGREE == 0) && LVGL_PORT_FULL_REFRESH
    // With the usage of three buffers and full-refresh, we always have one buffer available for rendering, eliminating the need to wait for the QSPI's sync signal
    ESP_ERROR_CHECK(esp_lcd_st77903_qspi_get_frame_buffer(panel_handle, 3, &lvgl_port_qspi_last_buf, &buf1, &buf2));
    lvgl_port_qspi_next_buf = lvgl_port_qspi_last_buf;
    lvgl_port_flush_next_buf = buf2;
#elif (LVGL_PORT_LCD_QSPI_BUFFER_NUMS == 3) && (EXAMPLE_LVGL_PORT_ROTATION_DEGREE != 0)
    // Here we are using three frame buffers, one for LVGL rendering, and the other two for QSPI driver (one of them is used for rotation)
    void *fbs[3];
    ESP_ERROR_CHECK(esp_lcd_st77903_qspi_get_frame_buffer(panel_handle, 3, &fbs[0], &fbs[1], &fbs[2]));
    buf1 = fbs[2];
#else
    ESP_ERROR_CHECK(esp_lcd_st77903_qspi_get_frame_buffer(panel_handle, 2, &buf1, &buf2));
#endif

    // Register the QSPI event callback to notify LVGL that the current frame buffer has been transmitted
    st77903_qspi_event_callbacks_t cbs = {
        .on_vsync = qspi_lcd_on_trans_event,
    };
    esp_lcd_st77903_qspi_register_event_callbacks(panel_handle, &cbs, NULL);
#else
    // Normmaly, for QSPI LCD, we just use one buffer for LVGL rendering
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

    uint8_t touchpad_cnt = 0;

    /* Read data from touch controller into memory */
    if (touch_isr_mux == NULL || xSemaphoreTake(touch_isr_mux, 0) == pdTRUE) {
        esp_lcd_touch_read_data(tp);
    }

    /* Read data from touch controller */
    esp_lcd_touch_point_data_t touch_points[1] = {0};
    esp_err_t ret = esp_lcd_touch_get_data(tp, touch_points, &touchpad_cnt, 1);
    if (ret == ESP_OK && touchpad_cnt > 0) {
        data->point.x = touch_points[0].x;
        data->point.y = touch_points[0].y;
        data->state = LV_INDEV_STATE_PRESSED;
        ESP_LOGD(TAG, "Touch position: %" PRIu16 ",%" PRIu16, touch_points[0].x, touch_points[0].y);
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

void lvgl_port_touch_isr_cb(esp_lcd_touch_handle_t tp)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(touch_isr_mux, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

SemaphoreHandle_t init_touch_isr_mux(void)
{
    touch_isr_mux = xSemaphoreCreateBinary();
    assert(touch_isr_mux);

    return touch_isr_mux;
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

esp_err_t lvgl_port_init(esp_lcd_panel_handle_t lcd_handle, esp_lcd_touch_handle_t tp_handle)
{
    lv_init();
    ESP_ERROR_CHECK(tick_init());

    lv_disp_t *disp = display_init(lcd_handle);
    assert(disp);

    if (tp_handle) {
        lv_indev_t *indev = indev_init(tp_handle);
        assert(indev);
#if EXAMPLE_LVGL_PORT_ROTATION_90
        esp_lcd_touch_set_swap_xy(tp_handle, true);
        esp_lcd_touch_set_mirror_y(tp_handle, true);
#elif EXAMPLE_LVGL_PORT_ROTATION_180
        esp_lcd_touch_set_mirror_x(tp_handle, true);
        esp_lcd_touch_set_mirror_y(tp_handle, true);
#elif EXAMPLE_LVGL_PORT_ROTATION_270
        esp_lcd_touch_set_swap_xy(tp_handle, true);
        esp_lcd_touch_set_mirror_x(tp_handle, true);
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
