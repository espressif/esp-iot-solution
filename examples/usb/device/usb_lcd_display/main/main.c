/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_memory_utils.h"
#include "esp_lcd_usb_display.h"
#include "sdkconfig.h"
#include "lv_demos.h"
#include "lvgl.h"

static const char *TAG = "lvgl_usb_display";

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your Application ///////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define EXAMPLE_LVGL_DRAW_BUF_LINES    CONFIG_UVC_CAM1_FRAMESIZE_HEIGT // number of display lines in each draw buffer
#define EXAMPLE_LVGL_TICK_PERIOD_MS    (2)
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS (500)
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS (10)
#define EXAMPLE_LVGL_TASK_STACK_SIZE   (10 * 1024)
#define EXAMPLE_LVGL_TASK_PRIORITY     (4)

#define EXAMPLE_DISPL_BIT_PER_PIXEL    (24)

#define EXAMPLE_DISPL_H_RES            CONFIG_UVC_CAM1_FRAMESIZE_WIDTH
#define EXAMPLE_DISPL_V_RES            CONFIG_UVC_CAM1_FRAMESIZE_HEIGT

#define EXAMPLE_DISP_SIZE              (EXAMPLE_DISPL_V_RES * EXAMPLE_DISPL_H_RES * EXAMPLE_DISPL_BIT_PER_PIXEL / 8)
#define EXAMPLE_DISPLAY_BUF_NUMS       (1)
#define EXAMPLE_JPEG_SUB_SAMPLE        (JPEG_DOWN_SAMPLING_YUV420)
#define EXAMPLE_JPEG_ENC_QUALITY       (80)
#define EXAMPLE_JPEG_TASK_PRIORITY     (4)
#define EXAMPLE_JPEG_TASK_CORE         (-1)

static SemaphoreHandle_t lvgl_api_mux = NULL;

static void example_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // pass the draw buffer to the driver
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);

    lv_display_flush_ready(disp);
}

static void example_increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

static bool example_lvgl_lock(int timeout_ms)
{
    // Convert timeout in milliseconds to FreeRTOS ticks
    // If `timeout_ms` is set to -1, the program will block until the condition is met
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_api_mux, timeout_ticks) == pdTRUE;
}

static void example_lvgl_unlock(void)
{
    xSemaphoreGiveRecursive(lvgl_api_mux);
}

static void example_lvgl_port_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
    while (1) {
        // Lock the mutex due to the LVGL APIs are not thread-safe
        if (example_lvgl_lock(-1)) {
            task_delay_ms = lv_timer_handler();
            // Release the mutex
            example_lvgl_unlock();
        }
        if (task_delay_ms > EXAMPLE_LVGL_TASK_MAX_DELAY_MS) {
            task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < EXAMPLE_LVGL_TASK_MIN_DELAY_MS) {
            task_delay_ms = EXAMPLE_LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Install LCD data panel");
    esp_lcd_panel_handle_t display_panel;
    usb_display_vendor_config_t vendor_config = DEFAULT_USB_DISPLAY_VENDOR_CONFIG(EXAMPLE_DISPL_H_RES, EXAMPLE_DISPL_V_RES, EXAMPLE_DISPL_BIT_PER_PIXEL, display_panel);
    ESP_ERROR_CHECK(esp_lcd_new_panel_usb_display(&vendor_config, &display_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(display_panel));

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    // create a lvgl display
    lv_display_t *display = lv_display_create(EXAMPLE_DISPL_H_RES, EXAMPLE_DISPL_V_RES);
    // associate the mipi panel handle to the display
    lv_display_set_user_data(display, display_panel);
    // create draw buffer
    void *buf1 = NULL;
    void *buf2 = NULL;
    ESP_LOGI(TAG, "Allocate separate LVGL draw buffers");
    // Note:
    // Keep the display buffer in **internal** RAM can speed up the UI because LVGL uses it a lot and it should have a fast access time
    // This example allocate the buffer from PSRAM mainly because we want to save the internal RAM
    size_t draw_buffer_sz = EXAMPLE_DISPL_H_RES * EXAMPLE_LVGL_DRAW_BUF_LINES * EXAMPLE_DISPL_BIT_PER_PIXEL / 8;
    buf1 = heap_caps_malloc(draw_buffer_sz, MALLOC_CAP_SPIRAM);
    assert(buf1);
    buf2 = heap_caps_malloc(draw_buffer_sz, MALLOC_CAP_SPIRAM);
    assert(buf2);
    // initialize LVGL draw buffers
    lv_display_set_buffers(display, buf1, buf2, draw_buffer_sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
    // // set color depth
#if EXAMPLE_DISPL_BIT_PER_PIXEL == 16
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
#elif EXAMPLE_DISPL_BIT_PER_PIXEL == 24
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB888);
#else
    ESP_LOGE(TAG, "Unsupported color depth");
    abort();
#endif
    // set the callback which can copy the rendered image to an area of the display
    lv_display_set_flush_cb(display, example_lvgl_flush_cb);

    ESP_LOGI(TAG, "Use esp_timer as LVGL tick timer");
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

    // LVGL APIs are meant to be called across the threads without protection, so we use a mutex here
    lvgl_api_mux = xSemaphoreCreateRecursiveMutex();
    assert(lvgl_api_mux);

    ESP_LOGI(TAG, "Create LVGL task");
    xTaskCreate(example_lvgl_port_task, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, NULL, EXAMPLE_LVGL_TASK_PRIORITY, NULL);

    ESP_LOGI(TAG, "Display LVGL Music Demo");
    // Lock the mutex due to the LVGL APIs are not thread-safe
    if (example_lvgl_lock(-1)) {

        lv_demo_music();

        example_lvgl_unlock();
    }
}
