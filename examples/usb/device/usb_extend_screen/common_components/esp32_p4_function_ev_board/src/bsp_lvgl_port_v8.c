/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_timer.h"
#include "esp_cache.h"
#include "esp_dma_utils.h"
#include "esp_private/esp_cache_private.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "lvgl.h"

#include "esp_intr_alloc.h"
#include "rom/cache.h"
#include "driver/ppa.h"

#include "src/draw/sw/lv_draw_sw.h"
#include "bsp_lvgl_port.h"
#include "bsp_err_check.h"
#include "bsp/display.h"
#include "bsp/touch.h"
#include "bsp/esp32_p4_function_ev_board.h"

#define ALIGN_UP_BY(num, align) (((num) + ((align) - 1)) & ~((align) - 1))

static const char *TAG = "bsp_lvgl_port_v8";
static SemaphoreHandle_t lvgl_mux;                  // LVGL mutex
static TaskHandle_t lvgl_task_handle = NULL;
static uint32_t data_cache_line_size = 0;

static lv_disp_drv_t disp_drv;

typedef lv_draw_sw_ctx_t lv_draw_ppa_ctx_t;

/*OPTIONAL: GPU INTERFACE*/
#if LVGL_PORT_USE_GPU

#define ENABLE_PPA_LOG      (0)

#if ENABLE_PPA_LOG
#define ESP_LOG_PPA(fmt, ...)   esp_rom_printf("%s", fmt, ##__VA_ARGS__)
#else
#define ESP_LOG_PPA(fmt, ...)   {}
#endif

static ppa_client_handle_t ppa_client_blend_handle;
static ppa_client_handle_t ppa_client_fill_handle;

/* If your MCU has hardware accelerator (GPU) then you can use it to blend to memories using opacity
 * It can be used only in buffered mode (LV_VDB_SIZE != 0 in lv_conf.h)*/
static void ppa_blend(lv_color_t *bg_buf, const lv_area_t *bg_area,
                      const lv_color_t *fg_buf, const lv_area_t *fg_area,
                      const lv_area_t *block_area, lv_opa_t opa)
{
    ESP_LOG_PPA("ppa_blend start\n");

    uint16_t bg_w = lv_area_get_width(bg_area);
    uint16_t bg_h = lv_area_get_height(bg_area);
    uint16_t bg_offset_x = block_area->x1 - bg_area->x1;
    uint16_t bg_offset_y = block_area->y1 - bg_area->y1;

    uint16_t fg_w = lv_area_get_width(fg_area);
    uint16_t fg_h = lv_area_get_height(fg_area);
    uint16_t fg_offset_x = block_area->x1 - fg_area->x1;
    uint16_t fg_offset_y = block_area->y1 - fg_area->y1;

    uint16_t block_w = lv_area_get_width(block_area);
    uint16_t block_h = lv_area_get_height(block_area);

    ppa_blend_oper_config_t blend_trans_config = {
        .in_bg.buffer = bg_buf,
        .in_bg.pic_w = bg_w,
        .in_bg.pic_h = bg_h,
        .in_bg.block_w = block_w,
        .in_bg.block_h = block_h,
        .in_bg.block_offset_x = bg_offset_x,
        .in_bg.block_offset_y = bg_offset_y,
        .in_bg.blend_cm = PPA_BLEND_COLOR_MODE_RGB565,

        .in_fg.buffer = fg_buf,
        .in_fg.pic_w = fg_w,
        .in_fg.pic_h = fg_h,
        .in_fg.block_w = block_w,
        .in_fg.block_h = block_h,
        .in_fg.block_offset_x = fg_offset_x,
        .in_fg.block_offset_y = fg_offset_y,
        .in_fg.blend_cm = PPA_BLEND_COLOR_MODE_RGB565,

        .out.buffer = bg_buf,
        .out.buffer_size = ALIGN_UP_BY(sizeof(lv_color_t) * bg_w * bg_h, data_cache_line_size),
            .out.pic_w = bg_w,
            .out.pic_h = bg_h,
            .out.block_offset_x = bg_offset_x,
            .out.block_offset_y = bg_offset_y,
            .out.blend_cm = PPA_BLEND_COLOR_MODE_RGB565,

            .bg_rgb_swap = 0,
            .bg_byte_swap = 0,
            .bg_alpha_update_mode = PPA_ALPHA_FIX_VALUE,
            .bg_alpha_fix_val = 255 - opa,

            .fg_rgb_swap = 0,
            .fg_byte_swap = 0,
            .fg_alpha_update_mode = PPA_ALPHA_FIX_VALUE,
            .fg_alpha_fix_val = opa,

            .mode = PPA_TRANS_MODE_BLOCKING,
    };

    assert(((uintptr_t)bg_buf & 127) == 0 && (blend_trans_config.out.buffer_size & 127) == 0);

    ESP_ERROR_CHECK(ppa_do_blend(ppa_client_blend_handle, &blend_trans_config));

    ESP_LOG_PPA("ppa_blend end\n");
}

static void ppa_fill(lv_color_t *bg_buf, const lv_area_t *bg_area,
                     const lv_area_t *block_area, lv_color_t color)
{
    ESP_LOG_PPA("ppa_fill start\n");

    uint16_t bg_w = lv_area_get_width(bg_area);
    uint16_t bg_h = lv_area_get_height(bg_area);
    uint16_t bg_offset_x = block_area->x1 - bg_area->x1;
    uint16_t bg_offset_y = block_area->y1 - bg_area->y1;

    uint16_t block_w = lv_area_get_width(block_area);
    uint16_t block_h = lv_area_get_height(block_area);

    ppa_fill_oper_config_t fill_trans_config = {
        .out.buffer = bg_buf,
        .out.buffer_size = ALIGN_UP_BY(sizeof(lv_color_t) * bg_w * bg_h, data_cache_line_size),
            .out.pic_w = bg_w,
            .out.pic_h = bg_h,
            .out.block_offset_x = bg_offset_x,
            .out.block_offset_y = bg_offset_y,
            .out.fill_cm = PPA_FILL_COLOR_MODE_RGB565,

            .fill_block_w = block_w,
            .fill_block_h = block_h,
        .fill_argb_color = {
            .val = lv_color_to32(color),
        },

        .mode = PPA_TRANS_MODE_BLOCKING,
    };
    ESP_ERROR_CHECK(ppa_do_fill(ppa_client_fill_handle, &fill_trans_config));

    ESP_LOG_PPA("ppa_fill end\n");
}

static void lv_draw_ppa_buffer_copy(lv_draw_ctx_t *draw_ctx,
                                    void *dest_buf, lv_coord_t dest_stride, const lv_area_t *dest_area,
                                    void *src_buf, lv_coord_t src_stride, const lv_area_t *src_area)
{
    ESP_LOG_PPA("ppa_buffer_copy start\n");

    // ESP_LOGW(TAG, "lv_draw_ppa_buffer_copy TO BE IMPLEMENTED!");

    lv_draw_sw_buffer_copy(draw_ctx,
                           dest_buf, dest_stride, dest_area,
                           src_buf, src_stride, src_area);

    ESP_LOG_PPA("ppa_buffer_copy end\n");
}

static void lv_draw_ppa_blend(lv_draw_ctx_t *draw_ctx, const lv_draw_sw_blend_dsc_t *dsc)
{
    ESP_LOG_PPA("draw_ppa_blend start\n");

    lv_area_t block_area;
    if (!_lv_area_intersect(&block_area, dsc->blend_area, draw_ctx->clip_area)) {
        return;
    }

    bool done = false;

    if ((dsc->mask_buf == NULL) && (lv_area_get_size(&block_area) > 100)) {
        lv_color_t *bg_buf = draw_ctx->buf;
        const lv_area_t *bg_area = draw_ctx->buf_area;
        lv_color_t *fg_buf = dsc->src_buf;
        const lv_area_t *fg_area = dsc->blend_area;

        if ((intptr_t)bg_buf % data_cache_line_size == 0) {
            if (fg_buf) {
                ppa_blend(bg_buf, bg_area, fg_buf, fg_area, &block_area, dsc->opa);
                done = true;
            } else if (dsc->opa >= LV_OPA_MAX) {
                ppa_fill(bg_buf, bg_area, &block_area, dsc->color);
                done = true;
            }
        }
    }

    if (!done) {
        lv_draw_sw_blend_basic(draw_ctx, dsc);
    }

    ESP_LOG_PPA("draw_ppa_blend end\n");
}

static void lv_draw_ppa_ctx_init(lv_disp_drv_t *drv, lv_draw_ctx_t *draw_ctx)
{
    ESP_LOG_PPA("draw_ppa_init start\n");

    lv_draw_sw_init_ctx(drv, draw_ctx);

    if (ppa_client_blend_handle || ppa_client_fill_handle) {
        goto end;
    }

    ppa_client_config_t ppa_blend_config = {
        .oper_type = PPA_OPERATION_BLEND,
    };

    ppa_client_config_t ppa_fill_config = {
        .oper_type = PPA_OPERATION_FILL,
    };

    ESP_ERROR_CHECK(ppa_register_client(&ppa_blend_config, &ppa_client_blend_handle));
    ESP_ERROR_CHECK(ppa_register_client(&ppa_fill_config, &ppa_client_fill_handle));

end:
    lv_draw_ppa_ctx_t * ppa_draw_ctx = (lv_draw_sw_ctx_t *)draw_ctx;
    ppa_draw_ctx->blend = lv_draw_ppa_blend;
    ppa_draw_ctx->base_draw.buffer_copy = lv_draw_ppa_buffer_copy;

    ESP_LOG_PPA("draw_ppa_init end\n");
}

static void lv_draw_ppa_ctx_deinit(lv_disp_drv_t *drv, lv_draw_ctx_t *draw_ctx)
{
    ESP_LOG_PPA("draw_ppa_deinit start\n");

    lv_draw_sw_deinit_ctx(drv, draw_ctx);

    if (drv != &disp_drv) {
        goto end;
    }
    ESP_ERROR_CHECK(ppa_unregister_client(ppa_client_blend_handle));
    ESP_ERROR_CHECK(ppa_unregister_client(ppa_client_fill_handle));

end:
    ESP_LOG_PPA("draw_ppa_deinit end\n");
}
#endif  /*LVGL_PORT_USE_GPU*/

#if LVGL_PORT_AVOID_TEAR_ENABLE
#if LVGL_PORT_DIRECT_MODE

static void flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;

    /* Action after last area refresh */
    if (lv_disp_flush_is_last(drv)) {
        /* Switch the current RGB frame buffer to `color_map` */
        esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, BSP_LCD_H_RES, BSP_LCD_V_RES, color_map);

        /* Waiting for the last frame buffer to complete transmission */
        ulTaskNotifyValueClear(NULL, ULONG_MAX);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }

    lv_disp_flush_ready(drv);
}

#elif LVGL_PORT_FULL_REFRESH && LVGL_PORT_LCD_DPI_BUFFER_NUMS == 2

static void flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

    /* Switch the current RGB frame buffer to `color_map` */
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);

    /* Waiting for the last frame buffer to complete transmission */
    ulTaskNotifyValueClear(NULL, ULONG_MAX);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    lv_disp_flush_ready(drv);
}

#elif LVGL_PORT_FULL_REFRESH && LVGL_PORT_LCD_DPI_BUFFER_NUMS == 3

static void *lvgl_port_rgb_last_buf = NULL;
static void *lvgl_port_rgb_next_buf = NULL;
static void *lvgl_port_flush_next_buf = NULL;

static void flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

    drv->draw_buf->buf1 = color_map;
    drv->draw_buf->buf2 = lvgl_port_flush_next_buf;
    lvgl_port_flush_next_buf = color_map;

    /* Switch the current RGB frame buffer to `color_map` */
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);

    lvgl_port_rgb_next_buf = color_map;

    lv_disp_flush_ready(drv);
}
#endif

#else

static void flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

    /* Just copy data from the color map to the RGB frame buffer */
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);

#if !BSP_LCD_DSI_USE_DMA2D
    lv_disp_flush_ready(disp_drv);
#endif
}

#endif /* LVGL_PORT_AVOID_TEAR_ENABLE */

static lv_disp_t *display_init(esp_lcd_panel_handle_t panel_handle)
{
    assert(panel_handle);

    static lv_disp_draw_buf_t disp_buf = { 0 };     // Contains internal graphic buffer(s) called draw buffer(s)

    // alloc draw buffers used by LVGL
    void *buf1 = NULL;
    void *buf2 = NULL;
    size_t buffer_size = 0;

    ESP_LOGD(TAG, "Malloc memory for LVGL buffer");
#if LVGL_PORT_AVOID_TEAR_ENABLE
    // To avoid the tearing effect, we should use at least two frame buffers: one for LVGL rendering and another for RGB output
    buffer_size = LVGL_PORT_H_RES * LVGL_PORT_V_RES;
#if (LVGL_PORT_LCD_DPI_BUFFER_NUMS == 3) && LVGL_PORT_FULL_REFRESH
    // With the usage of three buffers and full-refresh, we always have one buffer available for rendering, eliminating the need to wait for the RGB's sync signal
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(panel_handle, 3, &lvgl_port_rgb_last_buf, &buf1, &buf2));
    lvgl_port_rgb_next_buf = lvgl_port_rgb_last_buf;
    lvgl_port_flush_next_buf = buf2;
#else
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(panel_handle, 2, &buf1, &buf2));
#endif
#else
    // Normmaly, for RGB LCD, we just use one buffer for LVGL rendering
    buffer_size = LVGL_PORT_H_RES * LVGL_PORT_BUFFER_HEIGHT * LV_COLOR_DEPTH / 8;
    esp_dma_mem_info_t dma_mem_info = {
        .extra_heap_caps = MALLOC_CAP_SPIRAM,
    };
    BSP_ERROR_CHECK_RETURN_NULL(esp_dma_capable_calloc(1, buffer_size, &dma_mem_info, &buf1, &buffer_size));
    assert(buf1);
    ESP_LOGI(TAG, "LVGL buffer size: %dKB", buffer_size * LV_COLOR_DEPTH / 8 / 1024);
#endif /* LVGL_PORT_AVOID_TEAR_ENABLE */

    // initialize LVGL draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, buffer_size);

    ESP_LOGD(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LVGL_PORT_H_RES;
    disp_drv.ver_res = LVGL_PORT_V_RES;
    disp_drv.flush_cb = flush_callback;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
#if LVGL_PORT_FULL_REFRESH
    disp_drv.full_refresh = 1;
#elif LVGL_PORT_DIRECT_MODE
    disp_drv.direct_mode = 1;
#endif
#if LVGL_PORT_USE_GPU
    disp_drv.draw_ctx_init = lv_draw_ppa_ctx_init;
    disp_drv.draw_ctx_deinit = lv_draw_ppa_ctx_deinit;
    disp_drv.draw_ctx_size = sizeof(lv_draw_ppa_ctx_t);

    ESP_LOGI(TAG, "Enable GPU interface for LVGL");
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
    bool touchpad_pressed = esp_lcd_touch_get_coordinates(tp, &touchpad_x, &touchpad_y, NULL, NULL, &touchpad_cnt, 1);
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
        if (bsp_lvgl_port_lock(-1)) {
            task_delay_ms = lv_timer_handler();
            bsp_lvgl_port_unlock();
        }
        if (task_delay_ms > LVGL_PORT_TASK_MAX_DELAY_MS) {
            task_delay_ms = LVGL_PORT_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < LVGL_PORT_TASK_MIN_DELAY_MS) {
            task_delay_ms = LVGL_PORT_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

#if BSP_LCD_DSI_USE_DMA2D
IRAM_ATTR bool display_trans_done_cb(esp_lcd_panel_handle_t handle)
{
    lv_disp_flush_ready(&disp_drv);

    return false;
}
#endif

#if LVGL_PORT_AVOID_TEAR_ENABLE
IRAM_ATTR bool display_vsync_cb(esp_lcd_panel_handle_t handle)
{
    BaseType_t need_yield = pdFALSE;
#if LVGL_PORT_FULL_REFRESH && (LVGL_PORT_LCD_DPI_BUFFER_NUMS == 3)
    if (lvgl_port_rgb_next_buf != lvgl_port_rgb_last_buf) {
        lvgl_port_flush_next_buf = lvgl_port_rgb_last_buf;
        lvgl_port_rgb_last_buf = lvgl_port_rgb_next_buf;
    }
#else
    // Notify that the current RGB frame buffer has been transmitted
    xTaskNotifyFromISR(lvgl_task_handle, ULONG_MAX, eNoAction, &need_yield);
#endif

    return (need_yield == pdTRUE);
}
#endif

esp_err_t bsp_lvgl_port_init(esp_lcd_panel_handle_t lcd, esp_lcd_touch_handle_t tp, lv_disp_t **disp, lv_indev_t **indev)
{
    BSP_ERROR_CHECK_RETURN_ERR(esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &data_cache_line_size));

    lv_init();
    ESP_ERROR_CHECK(tick_init());

    *disp = display_init(lcd);
    assert(*disp);

    if (tp) {
        *indev = indev_init(tp);
        assert(*indev);
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

    bsp_display_callback_t display_cb = {
#if LVGL_PORT_AVOID_TEAR_ENABLE
        .on_vsync_cb = display_vsync_cb,
#elif BSP_LCD_DSI_USE_DMA2D
        .on_trans_done_cb = display_trans_done_cb,
#endif
    };
    bsp_display_register_callback(&display_cb);

    return ESP_OK;
}

bool bsp_lvgl_port_lock(uint32_t timeout_ms)
{
    assert(lvgl_mux && "bsp_lvgl_port_init must be called first");

    const TickType_t timeout_ticks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

void bsp_lvgl_port_unlock(void)
{
    assert(lvgl_mux && "bsp_lvgl_port_init must be called first");
    xSemaphoreGiveRecursive(lvgl_mux);
}
