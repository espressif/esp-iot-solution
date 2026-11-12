/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "board.h"
#include "boundary_trace.h"
#include "core/present_engine.h"
#include "platform/lcd/present_lcd_fb_scanout.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdatomic.h>
#include <string.h>
#include <stdio.h>

#define FRAMES 6
static atomic_bool pressure_stop;
static atomic_bool pressure_active;
static SemaphoreHandle_t pressure_done;
static void pressure(void *unused)
{
    (void)unused;
    size_t bytes = 512 * 1024;
    uint8_t *source = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    uint8_t *destination = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    assert(source && destination);
    memset(source, 0x5a, bytes);
    while (!atomic_load(&pressure_stop)) {
        if (!atomic_load(&pressure_active)) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        memcpy(destination, source, bytes);
        __asm__ volatile("" : : "r"(destination) : "memory");
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    free(source);
    free(destination);
    xSemaphoreGive(pressure_done);
    vTaskDelete(NULL);
}

static void draw(esp_display_present_engine_t *presenter, bool full, unsigned width, unsigned height, unsigned frame)
{
    for (unsigned y = 0; y < height;) {
        unsigned rows = full ? height : (height - y < 16 ? height - y : 16);
        esp_display_present_lease_t lease;
        ESP_ERROR_CHECK(esp_display_present_engine_acquire_buffer(presenter, &lease));
        uint16_t *pixels = lease.surface.pixels;
        size_t stride = lease.surface.layout.stride_bytes / 2;
        for (unsigned row = 0; row < rows; ++row) {
            for (unsigned x = 0; x < width; ++x) {
                pixels[row * stride + x] = ((x / 32 + frame) & 1) ? 0x07e0 : 0x001f;
            }
        }
        esp_display_present_engine_area_t submit = {0, y, width - 1, y + rows - 1};
        ESP_ERROR_CHECK(esp_display_present_engine_submit_buffer(presenter, &lease, &submit, stride * 2));
        y += rows;
    }
}

static void run_v2(unsigned count, bool full, unsigned rotation)
{
    char name[64];
    snprintf(name, sizeof(name), "%s_fb%u_%s_r%u", BOARD_NAME, count, full ? "full" : "partition", rotation * 90);
    boundary_reset();
    esp_lcd_panel_handle_t panel;
    ESP_ERROR_CHECK(board_panel_create(&panel, count));
    esp_display_present_lcd_fb_scanout_t *adapter;
    esp_display_present_lcd_fb_scanout_config_t scan = {
        .panel = panel, .kind = SCANOUT_KIND, .width = WIDTH, .height = HEIGHT,
        .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .buffer_count = count,
        .bounce = SCANOUT_KIND == ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_RGB,
        .delete_panel = true,
    };
    ESP_ERROR_CHECK(esp_display_present_lcd_fb_scanout_create(&scan, &adapter));
    void *buffers[3];
    size_t actual;
    ESP_ERROR_CHECK(esp_display_present_lcd_fb_scanout_get_buffers(adapter, buffers, 3, &actual));
    for (size_t i = 0; i < actual; ++i) {
        boundary_emit(BT_REGISTER, (uintptr_t)buffers[i], i);
    }
    bool quarter = rotation & 1;
    unsigned width = quarter ? HEIGHT : WIDTH;
    unsigned height = quarter ? WIDTH : HEIGHT;
    esp_display_present_engine_config_t config = {
        .backend_kind = ESP_DISPLAY_PRESENT_ENGINE_FB,
        .mode = full ? (count == 2 ? ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_FULL : ESP_DISPLAY_PRESENT_ENGINE_MODE_TRIPLE_FULL)
        : (count == 2 ? ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_PARTIAL : ESP_DISPLAY_PRESENT_ENGINE_MODE_TRIPLE_PARTIAL),
        .timeout_ms = 3000, .background_service = true,
        .backend = {
            .width = width, .height = height, .tile_rows = 16, .tile_count = 2,
            .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .buffer_count = count, .max_frames = 4,
            .rotation = (esp_display_present_engine_rotation_t)rotation, .async_copy = true,
            .tile_in_psram = true, .logical_in_psram = true,
            .ext_bufs = buffers, .ext_bytes = WIDTH * HEIGHT * 2,
            .scan_buf = buffers[0],
        },
    };
    ESP_ERROR_CHECK(esp_display_present_lcd_fb_scanout_get_interface(adapter, &config.backend.panel));
    esp_display_present_engine_t *presenter;
    ESP_ERROR_CHECK(esp_display_present_engine_create(&config, &presenter));
    atomic_store(&pressure_active, true);
    esp_display_present_engine_receipt_t receipts[FRAMES];
    for (unsigned frame = 0; frame < FRAMES; ++frame) {
        esp_display_present_frame_request_t request = {.full_redraw = true};
        ESP_ERROR_CHECK(esp_display_present_engine_begin_next_frame(presenter, &request));
        draw(presenter, full, width, height, frame);
        ESP_ERROR_CHECK(esp_display_present_engine_commit_with_receipt(presenter, NULL, &receipts[frame]));
#if CONFIG_IDF_TARGET_ESP32S3 && CONFIG_BOUNDARY_INJECT_RESTART
        boundary_emit(BT_INJECT, 0, frame);
        ESP_ERROR_CHECK(esp_lcd_rgb_panel_restart(panel));
#endif
        if (frame >= 2) {
            esp_display_present_frame_status_t status;
            ESP_ERROR_CHECK(esp_display_present_engine_wait_frame(presenter, receipts[frame - 2], ESP_DISPLAY_PRESENT_COMPLETION_PRESENT_CONFIRMED, 3000, &status));
            ESP_ERROR_CHECK(status.result);
            ESP_ERROR_CHECK(esp_display_present_engine_release_receipt(presenter, receipts[frame - 2]));
        }
    }
    for (unsigned frame = FRAMES - 2; frame < FRAMES; ++frame) {
        esp_display_present_frame_status_t status;
        ESP_ERROR_CHECK(esp_display_present_engine_wait_frame(presenter, receipts[frame], ESP_DISPLAY_PRESENT_COMPLETION_PRESENT_CONFIRMED, 3000, &status));
        ESP_ERROR_CHECK(status.result);
        ESP_ERROR_CHECK(esp_display_present_engine_release_receipt(presenter, receipts[frame]));
    }
    atomic_store(&pressure_active, false);
    ESP_ERROR_CHECK(esp_display_present_engine_stop(presenter, 3000));
    ESP_ERROR_CHECK(esp_display_present_engine_destroy(presenter));
    ESP_ERROR_CHECK(esp_display_present_lcd_fb_scanout_destroy(adapter));
    ESP_ERROR_CHECK(board_resources_destroy());
    boundary_emit(BT_STOP, 0, 0);
    boundary_freeze();
    boundary_dump(name, "v2");
}

#if CONFIG_IDF_TARGET_ESP32S3 && CONFIG_BOUNDARY_RAW_RGB
static SemaphoreHandle_t raw_done;
static SemaphoreHandle_t raw_vsync_done;
static bool IRAM_ATTR raw_frame(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *event, void *user)
{
    (void)panel;
    (void)event;
    (void)user;
    BaseType_t yield = pdFALSE;
    xSemaphoreGiveFromISR(raw_done, &yield);
    return yield;
}
static bool IRAM_ATTR raw_vsync(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *event, void *user)
{
    (void)panel;
    (void)event;
    (void)user;
    BaseType_t yield = pdFALSE;
    xSemaphoreGiveFromISR(raw_vsync_done, &yield);
    return yield;
}
static void run_raw(unsigned count)
{
    boundary_reset();
    raw_done = xSemaphoreCreateCounting(32, 0);
    raw_vsync_done = xSemaphoreCreateCounting(32, 0);
    assert(raw_done && raw_vsync_done);
    esp_lcd_panel_handle_t panel;
    ESP_ERROR_CHECK(board_panel_create(&panel, count));
    void *buffers[3] = {0};
    if (count == 2) {
        ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(panel, 2, &buffers[0], &buffers[1]));
    } else {
        ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(panel, 3, &buffers[0], &buffers[1], &buffers[2]));
    }
    for (unsigned i = 0; i < count; ++i) {
        boundary_emit(BT_REGISTER, (uintptr_t)buffers[i], i);
        /* Only initially inactive buffers are modified; no raw reuse proof implied. */
        if (i) {
            memset(buffers[i], i == 1 ? 0xff : 0, WIDTH * HEIGHT * 2);
        }
    }
    esp_lcd_rgb_panel_event_callbacks_t callbacks = {.on_frame_buf_complete = raw_frame, .on_vsync = raw_vsync};
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel, &callbacks, NULL));
    for (unsigned frame = 0; frame < FRAMES; ++frame) {
        void *target = buffers[(frame + 1) % count];
        while (xSemaphoreTake(raw_done, 0) == pdTRUE) { }
        while (xSemaphoreTake(raw_vsync_done, 0) == pdTRUE) { }
        boundary_emit(BT_RAW_REQUEST, (uintptr_t)target, frame);
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, 0, WIDTH, HEIGHT, target));
#if CONFIG_BOUNDARY_ON_DEMAND
        ESP_ERROR_CHECK(esp_lcd_rgb_panel_refresh(panel));
        assert(xSemaphoreTake(raw_done, pdMS_TO_TICKS(3000)) == pdTRUE);
        assert(xSemaphoreTake(raw_vsync_done, pdMS_TO_TICKS(3000)) == pdTRUE);
#else
        /* VSYNC/EOF absence is evidence, not a hang: direct S3 EOF may be unsuitable. */
        vTaskDelay(pdMS_TO_TICKS(50));
#endif
    }
    ESP_ERROR_CHECK(esp_lcd_panel_del(panel));
    ESP_ERROR_CHECK(board_resources_destroy());
    vSemaphoreDelete(raw_done);
    vSemaphoreDelete(raw_vsync_done);
    boundary_emit(BT_STOP, 0, 0);
    boundary_freeze();
#if CONFIG_BOUNDARY_ON_DEMAND
    boundary_dump("rgb_oneshot", "raw");
#else
    boundary_dump("rgb_stream", "raw");
#endif
}
#endif

void app_main(void)
{
#if CONFIG_IDF_TARGET_ESP32P4 && CONFIG_BOUNDARY_RAW_RGB
#error "Raw RGB tests require the S3 board"
#endif
#if CONFIG_BOUNDARY_STRESS
    pressure_done = xSemaphoreCreateBinary();
    assert(pressure_done);
    atomic_store(&pressure_stop, false);
    assert(xTaskCreate(pressure, "psram-pressure", 3072, NULL, 2, NULL) == pdPASS);
#else
    (void)pressure;
#endif
    for (unsigned count = 2; count <= 3; ++count) {
#if CONFIG_IDF_TARGET_ESP32S3 && CONFIG_BOUNDARY_RAW_RGB
        run_raw(count);
#else
        for (unsigned full = 0; full <= 1; ++full) {
            for (unsigned rotation = 0; rotation < 4; ++rotation) {
#if CONFIG_BOUNDARY_STRESS || CONFIG_BOUNDARY_INJECT_RESTART
                if (rotation != 1) {
                    continue; /* Stress representatives: both pools/contracts, 90 degrees. */
                }
#endif
                run_v2(count, full, rotation);
            }
        }
#endif
    }
#if CONFIG_BOUNDARY_STRESS
    atomic_store(&pressure_stop, true);
    assert(xSemaphoreTake(pressure_done, pdMS_TO_TICKS(3000)) == pdTRUE);
    vSemaphoreDelete(pressure_done);
#endif
    puts("BOUNDARY_SUITE_COMPLETE");
}
