/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "rom/cache.h"
#include "driver/ppa.h"
#include "esp_timer.h"
#include "esp_cache.h"
#include "esp_dma_utils.h"
#include "esp_private/esp_cache_private.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "esp_memory_utils.h"
#include "src/draw/sw/lv_draw_sw.h"
#include "lvgl.h"
#include "lvgl_private.h"
#include "ppa_blend.h"
#include "ppa_blend_private.h"
#include "lcd_ppa_blend.h"
#include "lcd_ppa_blend_private.h"
#include "lvgl_ppa_blend.h"

#define ALIGN_UP_BY(num, align) (((num) + ((align) - 1)) & ~((align) - 1))

static const char *TAG = "lvgl_ppa_blend";
static SemaphoreHandle_t lvgl_mux;                  // LVGL mutex
static TaskHandle_t lvgl_task_handle = NULL;
static size_t data_cache_line_size = 0;
static void *ppa_blend_fg_buf[LVGL_PPA_BLEND_FG_BUFFER_NUMS] = { NULL };
static void *ppa_blend_fg_buf_cur = NULL;

static user_data_flow_cb_t data_flow_switch_on_cb = NULL;
static user_data_flow_cb_t data_flow_switch_off_cb = NULL;

static lv_display_t *disp = NULL;
static lv_indev_t *indev = NULL;
static void *lvgl_cur_lcd_buf = NULL;

static user_ppa_blend_notify_done_cb_t user_ppa_blend_notify_done = NULL;

static ppa_blend_in_layer_t ppa_blend_fg_layer = {
    .buffer = NULL,
    .w = LVGL_PPA_BLEND_FG_H_RES,
    .h = LVGL_PPA_BLEND_FG_V_RES,
    .color_mode = PPA_BLEND_COLOR_MODE_ARGB8888,
    .alpha_update_mode = PPA_ALPHA_NO_CHANGE,
};

static ppa_blend_block_t ppa_block = {
    .bg_offset_x = 0,
    .bg_offset_y = 0,
    .fg_offset_x = 0,
    .fg_offset_y = 0,
    .out_offset_x = 0,
    .out_offset_y = 0,
    .w = LVGL_PPA_BLEND_FG_H_RES,
    .h = LVGL_PPA_BLEND_FG_V_RES,
    .update_type = PPA_BLEND_UPDATE_FG,
};

static esp_err_t lvgl_ppa_blend_fg_init(void)
{
    size_t ppa_blend_fg_buf_size = sizeof(lv_color32_t) * LVGL_PPA_BLEND_FG_H_RES * LVGL_PPA_BLEND_FG_V_RES;
    esp_dma_mem_info_t dma_mem_info = {
        .extra_heap_caps = MALLOC_CAP_SPIRAM,
    };
    for (int i = 0; i < LVGL_PPA_BLEND_FG_BUFFER_NUMS; i++) {
        ESP_RETURN_ON_ERROR(esp_dma_capable_calloc(1, ppa_blend_fg_buf_size, &dma_mem_info, &ppa_blend_fg_buf[i], &ppa_blend_fg_buf_size),
                            TAG, "Failed to allocate memory for PPA FG");
    }
    ppa_blend_fg_buf_cur = ppa_blend_fg_buf[0];

    ppa_blend_fg_layer.buffer = ppa_blend_fg_buf_cur;
    ppa_blend_set_fg_layer(&ppa_blend_fg_layer);

    return ESP_OK;
}

static esp_err_t ppa_blend_fg_draw_bitmap(void *handle, int x_start, int y_start, int x_end, int y_end, const uint8_t *color_data)
{
    (void)handle;   // Unused, just for compatibility
    ESP_RETURN_ON_FALSE((x_start < x_end) && (y_start < y_end), ESP_ERR_INVALID_ARG, TAG,
                        "start position must be smaller than end position");

    // check if we need to copy the draw buffer (pointed by the color_data) to the driver's frame buffer
    bool do_copy = true;
    for (int i = 0; i < LVGL_PPA_BLEND_FG_BUFFER_NUMS; i++) {
        // ESP_LOGE(TAG, "ppa_blend_fg_buf: %p", ppa_blend_fg_buf[i]);
        if (color_data == ppa_blend_fg_buf[i]) {
            ppa_blend_fg_buf_cur = (void *)color_data;
            ppa_blend_fg_layer.buffer = ppa_blend_fg_buf_cur;
            ppa_blend_set_fg_layer(&ppa_blend_fg_layer);
            do_copy = false;
            break;
        }
    }

    if (do_copy) {
        int h_res = LVGL_PPA_BLEND_FG_H_RES;
        // int v_res = LVGL_PPA_BLEND_FG_V_RES;

        int bytes_per_pixel = sizeof(lv_color32_t);
        uint32_t bytes_per_line = bytes_per_pixel * h_res;
        uint8_t *fb = ppa_blend_fg_buf_cur;

        // copy the UI draw buffer into internal frame buffer
        const uint8_t *from = (const uint8_t *)color_data;
        uint32_t copy_bytes_per_line = (x_end - x_start) * bytes_per_pixel;
        // size_t offset = y_start * copy_bytes_per_line + x_start * bytes_per_pixel;
        uint8_t *to = fb + (y_start * h_res + x_start) * bytes_per_pixel;

        for (int y = y_start; y < y_end; y++) {
            // TO DO: use dma2d memcpy!!!
            memcpy(to, from, copy_bytes_per_line);
            to += bytes_per_line;
            from += copy_bytes_per_line;
        }
    }

    return ESP_OK;
}

static void flush_callback(lv_display_t *disp, const lv_area_t *area, uint8_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);

    uint8_t color_format = lv_display_get_color_format(disp);

    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

    if (lv_display_flush_is_last(disp)) {
        if (color_format == LV_COLOR_FORMAT_RGB565 || color_format == LV_COLOR_FORMAT_RGB888) {
            /* Just copy data from the color map to the RGB frame buffer */
            esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LVGL_PPA_BLEND_H_RES, LVGL_PPA_BLEND_V_RES, color_map);
            lvgl_cur_lcd_buf = color_map;
            /* Waiting for the last frame buffer to complete transmission */
            ulTaskNotifyValueClear(NULL, ULONG_MAX);
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        } else if (color_format == LV_COLOR_FORMAT_ARGB8888) {
            /* Action after last area refresh */
            /* Switch the current PPA frame buffer to `color_map` */
            ppa_blend_fg_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
            if (!ppa_blend_user_data_is_enabled() && !ppa_blend_is_enabled()) {
                ppa_blend_enable(true);
            }
            ppa_blend_run(&ppa_block, -1);
            /* Waiting for the last frame buffer to complete transmission */
            ulTaskNotifyValueClear(NULL, ULONG_MAX);
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }
    }

    lv_display_flush_ready(disp);
}

void  lvgl_mode_change(lvgl_ppa_blend_color_mode_t mode)
{
    lvgl_ppa_blend_lock(0);
    ESP_LOGD(TAG, "change mode %d", mode);
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);

    if (mode == LVGL_PPA_BLEND_COLOR_MODE_RGB565 || mode == LVGL_PPA_BLEND_COLOR_MODE_RGB888) {
        ppa_blend_user_data_enable(false);
        ppa_blend_enable(false);
        if (data_flow_switch_off_cb) {
            data_flow_switch_off_cb();
        }

        ppa_blend_check_done(50);

        if (mode == LVGL_PPA_BLEND_COLOR_MODE_RGB565) {
            lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
            ESP_LOGI(TAG, "Set color format to RGB565");
        } else {
            lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB888);
            ESP_LOGI(TAG, "Set color format to RGB888");
        }

        void *lvgl_buf1;
        void *lvgl_buf2;
        ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(panel_handle, 2, &lvgl_buf1, &lvgl_buf2));
        size_t buffer_size = LVGL_PPA_BLEND_H_RES * LVGL_PPA_BLEND_V_RES * LCD_PPA_BLEND_OUT_COLOR_BITS / 8;

        void *lcd_buf = lcd_ppa_blend_get_buffer();
        if (lcd_buf == lvgl_buf2) {
            lv_display_set_buffers(disp, lvgl_buf2, lvgl_buf1, buffer_size, LV_DISPLAY_RENDER_MODE_DIRECT);
        } else {
            lv_display_set_buffers(disp, lvgl_buf1, lvgl_buf2, buffer_size, LV_DISPLAY_RENDER_MODE_DIRECT);
        }
    } else if (mode == LVGL_PPA_BLEND_COLOR_MODE_ARGB8888) {
        ppa_blend_user_data_enable(false);
        if (data_flow_switch_on_cb) {
            data_flow_switch_on_cb();
        }

        lv_display_set_color_format(disp, LV_COLOR_FORMAT_ARGB8888);
        ESP_LOGI(TAG, "Set color format to ARGB8888");

        lcd_ppa_blend_set_buffer(lvgl_cur_lcd_buf == disp->buf_1->data ? disp->buf_2->data : disp->buf_1->data);
        void *buf1 = ppa_blend_fg_buf[1];
        void *buf2 = ppa_blend_fg_buf[0];
        size_t buffer_size = LVGL_PPA_BLEND_H_RES * LVGL_PPA_BLEND_V_RES * sizeof(lv_color32_t);
        lv_display_set_buffers(disp, buf1, buf2, buffer_size, LV_DISPLAY_RENDER_MODE_DIRECT);

        ppa_blend_fg_layer.buffer = disp->buf_act->data;
        ppa_blend_set_fg_layer(&ppa_blend_fg_layer);
    } else if (mode == LVGL_PPA_BLEND_USER_DATA_MODE) {
        ppa_blend_enable(false);
        ESP_LOGI(TAG, "Set color format to USER DATA MODE");

        if (data_flow_switch_on_cb) {
            data_flow_switch_on_cb();
        }

        ppa_blend_check_done(50);

        ppa_blend_user_data_enable(true);
    }
    lvgl_ppa_blend_unlock();
}

esp_err_t lvgl_ppa_blend_register_switch_on_callback(user_data_flow_cb_t callback)
{
    data_flow_switch_on_cb = callback;
    return ESP_OK;
}

esp_err_t lvgl_ppa_blend_register_switch_off_callback(user_data_flow_cb_t callback)
{
    data_flow_switch_off_cb = callback;
    return ESP_OK;
}

static lv_display_t *lv_port_disp_init(esp_lcd_panel_handle_t lcd)
{
    size_t buffer_size = 0;

    ESP_LOGD(TAG, "Malloc memory for LVGL buffer");

    // Normmaly, for PPA LCD, we just use one buffer for LVGL rendering
    buffer_size = sizeof(lv_color32_t) * LVGL_PPA_BLEND_H_RES * LVGL_PPA_BLEND_BUFFER_HEIGHT;

    lv_display_t *display = lv_display_create(LVGL_PPA_BLEND_H_RES, LVGL_PPA_BLEND_V_RES);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(
        display, ppa_blend_fg_buf[0], ppa_blend_fg_buf[1], buffer_size,
        LV_DISPLAY_RENDER_MODE_DIRECT
    );
    // set the callback which can copy the rendered image to an area of the display
    lv_display_set_flush_cb(display, flush_callback);
    lv_display_set_user_data(display, lcd);
    return display;
}

static void touchpad_read(lv_indev_t *indev_drv, lv_indev_data_t *data)
{
    esp_lcd_touch_handle_t tp = lv_indev_get_user_data(indev_drv);
    assert(tp);

    uint8_t touchpad_cnt = 0;
    /* Read data from touch controller into memory */
    esp_lcd_touch_read_data(tp);

    /* Read data from touch controller */
    esp_lcd_touch_point_data_t touch_points[1] = {0};
    esp_err_t ret = esp_lcd_touch_get_data(tp, touch_points, &touchpad_cnt, 1);
    if (ret == ESP_OK && touchpad_cnt > 0) {
        data->point.x = touch_points[0].x;
        data->point.y = touch_points[0].y;
        data->state = LV_INDEV_STATE_PR;
        ESP_LOGD(TAG, "Touch position: %" PRIu16 ",%" PRIu16, touch_points[0].x, touch_points[0].y);
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

static lv_indev_t *lv_port_indev_init(esp_lcd_touch_handle_t tp)
{
    assert(tp);
    lv_indev_t * indev_touchpad = lv_indev_create();
    lv_indev_set_type(indev_touchpad, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev_touchpad, touchpad_read);
    lv_indev_set_user_data(indev_touchpad, tp);
    return indev_touchpad;
}

static void tick_increment(void *arg)
{
    /* Tell LVGL how many milliseconds have elapsed */
    lv_tick_inc(LVGL_PPA_BLEND_TICK_PERIOD_MS);
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
    return esp_timer_start_periodic(lvgl_tick_timer, LVGL_PPA_BLEND_TICK_PERIOD_MS * 1000);
}

static void lvgl_port_task(void *arg)
{
    ESP_LOGD(TAG, "Starting LVGL task");

    uint32_t task_delay_ms = LVGL_PPA_BLEND_TASK_MAX_DELAY_MS;
    while (1) {
        if (lvgl_ppa_blend_lock(0)) {
            task_delay_ms = lv_timer_handler();
            lvgl_ppa_blend_unlock();
        }
        if (task_delay_ms > LVGL_PPA_BLEND_TASK_MAX_DELAY_MS) {
            task_delay_ms = LVGL_PPA_BLEND_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < LVGL_PPA_BLEND_TASK_MIN_DELAY_MS) {
            task_delay_ms = LVGL_PPA_BLEND_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

esp_err_t lvgl_ppa_blend_init(esp_lcd_panel_handle_t lcd_in, esp_lcd_touch_handle_t tp_in, lv_display_t **disp_out, lv_indev_t **indev_out)
{
    ESP_ERROR_CHECK(esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &data_cache_line_size));

#if MIPI_DSI_LCD_COLOR_DEPTH == 24
    uint8_t *lcd_fb;
    esp_lcd_dpi_panel_get_frame_buffer(lcd_in, 1, &lcd_fb);
    rgb_convert_init(lcd_fb);
#endif

    lv_init();
    ESP_ERROR_CHECK(tick_init());

    lvgl_ppa_blend_fg_init();

    if (lcd_in) {
        ESP_LOGD(TAG, "lcd_in %p", lcd_in);
        disp = lv_port_disp_init(lcd_in);
        assert(disp);
    }

    if (tp_in) {
        ESP_LOGD(TAG, "tp_in %p", tp_in);
        indev = lv_port_indev_init(tp_in);
        assert(indev);
    }

    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    assert(lvgl_mux);
    ESP_LOGI(TAG, "Create LVGL task");
    BaseType_t core_id = (LVGL_PPA_BLEND_TASK_CORE < 0) ? tskNO_AFFINITY : LVGL_PPA_BLEND_TASK_CORE;
    BaseType_t ret = xTaskCreatePinnedToCoreWithCaps(lvgl_port_task, "lvgl", LVGL_PPA_BLEND_TASK_STACK_SIZE, NULL,
                                                     LVGL_PPA_BLEND_TASK_PRIORITY, &lvgl_task_handle, core_id, MALLOC_CAP_SPIRAM);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LVGL task");
        return ESP_FAIL;
    }

    const esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_refresh_done = on_lcd_vsync_cb,
    };
    esp_lcd_dpi_panel_register_event_callbacks(lcd_in, &cbs, NULL);

    *disp_out = disp;
    *indev_out = indev;

    return ESP_OK;
}

bool lvgl_ppa_blend_lock(uint32_t timeout_ms)
{
    assert(lvgl_mux && "lvgl_ppa_blend_fg_init must be called first");

    const TickType_t timeout_ticks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

void lvgl_ppa_blend_unlock(void)
{
    assert(lvgl_mux && "lvgl_ppa_blend_fg_init must be called first");
    xSemaphoreGiveRecursive(lvgl_mux);
}

IRAM_ATTR bool lvgl_ppa_blend_notify_done(bool is_isr)
{
    BaseType_t need_yield = pdFALSE;

    // Notify that the current PPA frame buffer has been transmitted
    if (is_isr) {
        xTaskNotifyFromISR(lvgl_task_handle, ULONG_MAX, eNoAction, &need_yield);
    } else {
        xTaskNotify(lvgl_task_handle, ULONG_MAX, eNoAction);
    }

    return (need_yield == pdTRUE);
}

void lvgl_ppa_blend_set_notify_done_cb(user_ppa_blend_notify_done_cb_t cb)
{
    user_ppa_blend_notify_done = cb;
}

bool on_lcd_vsync_cb(esp_lcd_panel_handle_t handle, esp_lcd_dpi_panel_event_data_t *event_data, void *user_data)
{
    BaseType_t need_yield = pdFALSE;

#if LCD_PPA_BLEND_AVOID_TEAR_ENABLE
    lcd_ppa_on_lcd_vsync_cb();
#endif

    if (ppa_blend_user_data_is_enabled() && user_ppa_blend_notify_done) {
        user_ppa_blend_notify_done();
    }

    xTaskNotifyFromISR(lvgl_task_handle, ULONG_MAX, eNoAction, &need_yield);

    return (need_yield == pdTRUE);
}
