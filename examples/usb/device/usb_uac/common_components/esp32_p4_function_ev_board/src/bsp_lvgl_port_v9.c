/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <inttypes.h>
#include "esp_lcd_panel_ops.h"
#include "esp_timer.h"
#include "esp_cache.h"
#include "esp_dma_utils.h"
#include "esp_private/esp_cache_private.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_lcd_mipi_dsi.h"
#include "driver/ppa.h"
#include "lvgl_private.h"
#include "lvgl.h"

#include "src/draw/sw/lv_draw_sw.h"
#include "bsp_lvgl_port.h"
#include "bsp_err_check.h"
#include "bsp/display.h"
#include "bsp/touch.h"
#include "bsp/esp32_p4_function_ev_board.h"

#define ALIGN_UP_BY(num, align) (((num) + ((align) - 1)) & ~((align) - 1))

static const char *TAG = "bsp_lvgl_port_v9";
static SemaphoreHandle_t lvgl_mux = NULL;                  // LVGL mutex
static TaskHandle_t lvgl_task_handle = NULL;
static uint32_t data_cache_line_size = 0;

static esp_lcd_panel_handle_t lcd = NULL;
static esp_lcd_touch_handle_t tp = NULL;
static lv_display_t *disp = NULL;
static lv_indev_t *indev = NULL;

#if LVGL_PORT_DIRECT_MODE
static void flush_callback(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);

    /* Action after last area refresh */
    if (lv_disp_flush_is_last(disp)) {
        /* Switch the current RGB frame buffer to `color_p` */
        esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, BSP_LCD_H_RES, BSP_LCD_V_RES, color_p);

        /* Waiting for the last frame buffer to complete transmission */
        ulTaskNotifyValueClear(NULL, ULONG_MAX);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }

    lv_disp_flush_ready(disp);
}

#elif LVGL_PORT_FULL_REFRESH && (LVGL_PORT_LCD_DPI_BUFFER_NUMS == 2)

static void flush_callback(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

    /* Switch the current RGB frame buffer to `color_p` */
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_p);

    /* Waiting for the last frame buffer to complete transmission */
    ulTaskNotifyValueClear(NULL, ULONG_MAX);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    lv_disp_flush_ready(disp);
}

#elif LVGL_PORT_FULL_REFRESH && (LVGL_PORT_LCD_DPI_BUFFER_NUMS == 3)

static void *lvgl_port_rgb_last_buf = NULL;
static void *lvgl_port_rgb_next_buf = NULL;
static void *lvgl_port_flush_next_buf = NULL;

static void flush_callback(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

    if (disp->buf_act == disp->buf_1) {
        disp->buf_2->data = lvgl_port_flush_next_buf;
    } else {
        disp->buf_1->data = lvgl_port_flush_next_buf;
    }
    lvgl_port_flush_next_buf = color_p;

    /* Switch the current RGB frame buffer to `color_p` */
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_p);

    lvgl_port_rgb_next_buf = color_p;

    lv_disp_flush_ready(disp);
}

#else

static void flush_callback(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

    /* Just copy data from the color map to the RGB frame buffer */
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_p);

#if !BSP_LCD_DSI_USE_DMA2D
    lv_disp_flush_ready(disp);
#endif
}
#endif

static lv_disp_t *display_init(esp_lcd_panel_handle_t lcd)
{
    BSP_NULL_CHECK(lcd, NULL);

    // alloc draw buffers used by LVGL
    size_t buffer_size;
    void *buf1 = NULL;
    void *buf2 = NULL;

    ESP_LOGD(TAG, "Malloc memory for LVGL buffer");
#if LVGL_PORT_AVOID_TEAR_ENABLE
    // To avoid the tearing effect, we should use at least two frame buffers: one for LVGL rendering and another for RGB output
    buffer_size = ALIGN_UP_BY(LVGL_PORT_H_RES * LVGL_PORT_V_RES * LV_COLOR_DEPTH / 8, data_cache_line_size);
#if (LVGL_PORT_LCD_DPI_BUFFER_NUMS == 3) && LVGL_PORT_FULL_REFRESH
    // With the usage of three buffers and full-refresh, we always have one buffer available for rendering, eliminating the need to wait for the RGB's sync signal
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(lcd, 3, &lvgl_port_rgb_last_buf, &buf1, &buf2));
    lvgl_port_rgb_next_buf = lvgl_port_rgb_last_buf;
    lvgl_port_flush_next_buf = buf2;
#else
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(lcd, 2, &buf1, &buf2));
#endif
#else
    buffer_size = LVGL_PORT_H_RES * LVGL_PORT_BUFFER_HEIGHT * LV_COLOR_DEPTH / 8;
    esp_dma_mem_info_t dma_mem_info = {
        .extra_heap_caps = MALLOC_CAP_SPIRAM,
    };
    BSP_ERROR_CHECK_RETURN_NULL(esp_dma_capable_calloc(1, buffer_size, &dma_mem_info, &buf1, &buffer_size));
    assert(buf1 && "Failed to allocate memory for LVGL display buffer");
#endif /* LVGL_PORT_AVOID_TEAR_ENABLE */

    ESP_LOGI(TAG, "LVGL buffer size: %dKB, 0x%p, 0x%p", buffer_size / 1024, buf1, buf2);

    ESP_LOGD(TAG, "Register display driver to LVGL");
    lv_display_t *display = lv_display_create(LVGL_PORT_H_RES, LVGL_PORT_V_RES);
    lv_display_set_buffers(
        display, buf1, buf2, buffer_size,
#if LVGL_PORT_FULL_REFRESH
        LV_DISPLAY_RENDER_MODE_FULL
#elif LVGL_PORT_DIRECT_MODE
        LV_DISPLAY_RENDER_MODE_DIRECT
#else
        LV_DISPLAY_RENDER_MODE_PARTIAL
#endif
    );
    lv_display_set_flush_cb(display, flush_callback);
    lv_display_set_user_data(display, lcd);

    return display;
}

static void touchpad_read(lv_indev_t *indev_drv, lv_indev_data_t *data)
{
    esp_lcd_touch_handle_t tp = (esp_lcd_touch_handle_t)lv_indev_get_user_data(indev);
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
        data->state = LV_INDEV_STATE_PRESSED;
        ESP_LOGD(TAG, "Touch position: %" PRIu16 ",%" PRIu16, touch_points[0].x, touch_points[0].y);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static lv_indev_t *indev_init(esp_lcd_touch_handle_t tp)
{
    BSP_NULL_CHECK(tp, NULL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);   /*See below.*/
    lv_indev_set_user_data(indev, tp);
    lv_indev_set_read_cb(indev, touchpad_read);  /*See below.*/

    return indev;
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
    BSP_ERROR_CHECK_RETURN_ERR(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    return esp_timer_start_periodic(lvgl_tick_timer, LVGL_PORT_TICK_PERIOD_MS * 1000);
}

static void lvgl_port_task(void *arg)
{
    BSP_ERROR_CHECK_RETURN_ERR(esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &data_cache_line_size));

    lv_init();
    BSP_ERROR_CHECK_RETURN_ERR(tick_init());
    BSP_NULL_CHECK(disp = display_init(lcd), ESP_FAIL);
    if (tp != NULL) {
        BSP_NULL_CHECK(indev = indev_init(tp), ESP_FAIL);
    }

    ESP_LOGI(TAG, "Starting LVGL task");
    while (1) {
        bsp_lvgl_port_lock(0);

        uint32_t task_delay_ms = lv_timer_handler();

        bsp_lvgl_port_unlock();
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
    lv_display_flush_ready(disp);

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

esp_err_t bsp_lvgl_port_init(esp_lcd_panel_handle_t lcd_in, esp_lcd_touch_handle_t tp_in, lv_display_t **disp_out, lv_indev_t **indev_out)
{
    BSP_NULL_CHECK(lcd_in, ESP_ERR_INVALID_ARG);
    BSP_NULL_CHECK(tp_in, ESP_ERR_INVALID_ARG);
    BSP_NULL_CHECK(disp_out, ESP_ERR_INVALID_ARG);
    BSP_NULL_CHECK(indev_out, ESP_ERR_INVALID_ARG);

    lcd = lcd_in;
    tp = tp_in;

    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    BSP_NULL_CHECK(lvgl_mux, ESP_FAIL);
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

    vTaskDelay(pdMS_TO_TICKS(1000));

    *disp_out = disp;
    *indev_out = indev;

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
