/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_cache.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io_additions.h"
#include "esp_lcd_panel_io_interface.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include "lvgl.h"
#include "lv_port.h"

#include "esp_lcd_st77903_rgb.h"

static const char *TAG = "lvgl_port";
static SemaphoreHandle_t lvgl_mux;
static TaskHandle_t lvgl_task_handle = NULL;

void flush_callback(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);

    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

    /* Just copy data from the color map to the RGB frame buffer */
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);

    lv_display_flush_ready(disp);
}

static void tick_increment(void *arg)
{
    /* Tell LVGL how many milliseconds have elapsed */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
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
    return esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000);
}

static void lv_port_task(void *arg)
{
    lv_init();
    tick_init();

    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)arg;

    ESP_LOGD(TAG, "Malloc memory for LVGL buffer");
    void *buf1 = NULL;
    int buffer_size = EXAMPLE_LCD_RGB_H_RES * EXAMPLE_LCD_RGB_V_RES;
    buf1 = heap_caps_malloc(buffer_size, EXAMPLE_LVGL_BUFFER_MALLOC);

    ESP_LOGD(TAG, "Register display driver to LVGL");
    lv_display_t *display = lv_display_create(EXAMPLE_LCD_RGB_H_RES, EXAMPLE_LCD_RGB_V_RES);
    lv_display_set_buffers(display, buf1, NULL, buffer_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush_callback);
    lv_display_set_user_data(display, panel_handle);

    uint32_t task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
    ESP_LOGI(TAG, "Starting LVGL task");
    while (1) {
        // Lock the mutex due to the LVGL APIs are not thread-safe
        if (lv_port_lock(-1)) {
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

esp_err_t lv_port_init(esp_lcd_panel_handle_t panel_handle)
{
    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    assert(lvgl_mux);
    xTaskCreate(lv_port_task, "lvgl", EXAMPLE_LVGL_TASK_STACK_SIZE, panel_handle, EXAMPLE_LVGL_TASK_PRIORITY, &lvgl_task_handle);

    vTaskDelay(pdMS_TO_TICKS(1000));

    return ESP_OK;
}

bool lv_port_lock(int timeout_ms)
{
    assert(lvgl_mux && "bsp_lvgl_port_init must be called first");

    const TickType_t timeout_ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

void lv_port_unlock(void)
{
    assert(lvgl_mux && "bsp_lvgl_port_init must be called first");
    xSemaphoreGiveRecursive(lvgl_mux);
}
