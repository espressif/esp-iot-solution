/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
/* Driver event characterization only, not a present non-bounce implementation.
 * No private IDF fields or patches. Visual validation remains necessary.
 */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include "board.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_cache.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define FRAMES 80
static const char *TAG = "rgb_events";
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static uint32_t completed, vsyncs;
static TaskHandle_t controller;
static esp_lcd_panel_handle_t panel;
static void *buffers[3];
static unsigned fb_count, bounce, worker_core;
static bool timed_out;
static uint64_t wait_sum;
static uint32_t wait_max;

static void log_driver_caps(void)
{
#ifdef ESP_LCD_RGB_PANEL_HAS_FRAME_BUFFER_INFO
    esp_lcd_rgb_panel_frame_buffer_info_t info;
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer_info(panel, &info));
    ESP_ERROR_CHECK((info.width == WIDTH && info.height == HEIGHT &&
                     info.frame_buffer_count >= fb_count &&
                     info.frame_buffer_bytes == (size_t)WIDTH * HEIGHT * 2 &&
                     info.bits_per_pixel == 16 && info.continuous_refresh &&
                     !info.custom_bounce_callback && !info.transformed) ? ESP_OK : ESP_ERR_INVALID_STATE);
    ESP_ERROR_CHECK(((bounce != 0) == (info.bounce_buffer_bytes != 0)) ? ESP_OK : ESP_ERR_INVALID_STATE);
    ESP_LOGI(TAG, "RGB_CAPS bounce=%u fbs=%u bounce_bytes=%u retirement=%u continuous=%u",
             bounce, fb_count, (unsigned)info.bounce_buffer_bytes,
             info.frame_buffer_retirement, info.continuous_refresh);
#else
    ESP_LOGW(TAG, "RGB_CAPS unavailable bounce=%u fbs=%u", bounce, fb_count);
#endif
}

esp_err_t board_s31_rgb_create(esp_lcd_panel_handle_t *, size_t, unsigned);
static bool IRAM_ATTR frame_done(esp_lcd_panel_handle_t p, const esp_lcd_rgb_panel_event_data_t *e, void *u)
{
    (void)p; (void)e; (void)u;
    portENTER_CRITICAL_ISR(&lock); ++completed; portEXIT_CRITICAL_ISR(&lock);
    return false;
}
static bool IRAM_ATTR vsync_done(esp_lcd_panel_handle_t p, const esp_lcd_rgb_panel_event_data_t *e, void *u)
{
    (void)p; (void)e; (void)u;
    portENTER_CRITICAL_ISR(&lock); ++vsyncs; portEXIT_CRITICAL_ISR(&lock);
    return false;
}
static uint32_t snapshot(bool frames)
{
    portENTER_CRITICAL(&lock);
    uint32_t n = frames ? completed : vsyncs;
    portEXIT_CRITICAL(&lock);
    return n;
}
static void submit_frames(void *unused)
{
    (void)unused;
    unsigned active = 0;
    const uint16_t colors[] = {0xf800, 0x07e0, 0x001f, 0xffe0, 0xf81f, 0x07ff};
    for (unsigned frame = 0; frame < FRAMES; ++frame) {
        unsigned next = fb_count == 1 ? 0 : (active + 1) % fb_count;
        uint16_t *pixels = buffers[next];
        for (unsigned y = 0; y < HEIGHT; ++y)
            for (unsigned x = 0; x < WIDTH; ++x) {
                pixels[y * WIDTH + x] = x < 16 + (frame % 48) * 16 ? 0xffff : colors[frame % 6];
            }
        ESP_ERROR_CHECK(esp_cache_msync(pixels, WIDTH * HEIGHT * 2, ESP_CACHE_MSYNC_FLAG_DIR_C2M));
        uint32_t before = snapshot(true);
        int64_t start = esp_timer_get_time();
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, 0, WIDTH, HEIGHT, pixels));
        // Bounce is periodic: two completions conservatively cover a callback
        // racing submission. Direct mode: exactly one requested link switch;
        // no second-period assumption and no task-context arming after draw.
        unsigned needed = bounce ? 2 : 1;
        while ((uint32_t)(snapshot(true) - before) < needed) {
            if (esp_timer_get_time() - start > 500000) {
                timed_out = true;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        if (timed_out) {
            break;
        }
        uint32_t us = esp_timer_get_time() - start;
        wait_sum += us; if (us > wait_max) {
            wait_max = us;
        }
        active = next;
    }
    xTaskNotifyGive(controller);
    vTaskDelete(NULL);
}
void app_main(void)
{
    controller = xTaskGetCurrentTaskHandle();
    vTaskDelay(pdMS_TO_TICKS(2000));
    unsigned passed = 0;
    for (unsigned b = 0; b < 2; ++b)
        for (unsigned n = 1; n <= 3; ++n)
            for (unsigned core = 0; core < 2; ++core) {
                bounce = b ? 10 : 0; fb_count = n; worker_core = core;
                timed_out = false; wait_sum = 0; wait_max = 0;
                ESP_ERROR_CHECK(board_s31_rgb_create(&panel, fb_count, bounce));
                log_driver_caps();
                ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(panel, fb_count, &buffers[0], &buffers[1], &buffers[2]));
                const esp_lcd_rgb_panel_event_callbacks_t cb = {.on_frame_buf_complete = frame_done, .on_vsync = vsync_done};
                ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel, &cb, NULL));
                uint32_t c = snapshot(true), v = snapshot(false);
                vTaskDelay(pdMS_TO_TICKS(250));
                unsigned idle_c = snapshot(true) - c, idle_v = snapshot(false) - v;
                ESP_LOGI(TAG, "RGB_EVENT_BEGIN bounce=%u fbs=%u submit_core=%u init_core=%u idle_frame_events=%u idle_vsync=%u", bounce, fb_count, worker_core, xPortGetCoreID(), idle_c, idle_v);
                ESP_ERROR_CHECK(xTaskCreatePinnedToCore(submit_frames, "rgb_submit", 4096, NULL, 5, NULL, worker_core) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
                ESP_ERROR_CHECK(esp_lcd_panel_del(panel));
                panel = NULL;
                ESP_LOGI(TAG, "%s mode=RGB_%s_FB%u_CORE%u rotation=0 frames=%u wait_avg_us=%u wait_max_us=%u idle_frame_events=%u idle_vsync=%u active_write=%u", timed_out ? "FAIL" : "PASS", bounce ? "BOUNCE" : "DIRECT", fb_count, worker_core, timed_out ? 0 : FRAMES, (unsigned)(wait_sum / FRAMES), wait_max, idle_c, idle_v, fb_count == 1);
                ESP_ERROR_CHECK(timed_out ? ESP_ERR_TIMEOUT : ESP_OK);
                ++passed;
            }
    ESP_LOGI(TAG, "RGB_EVENT_SUITE_PASS board=s31-rgb cases=%u frames_per_case=%u", passed, FRAMES);
}
