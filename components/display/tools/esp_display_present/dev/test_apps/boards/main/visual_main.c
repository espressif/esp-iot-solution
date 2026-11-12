/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* S31 TE visual test: diagnostic program, not a tear-free proof. */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include "board_qspi.h"
#include "esp_display_present.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define WIDTH 480
#define HEIGHT 480
#define BAND_ROWS 16
static const char *TAG = "te_visual";
static portMUX_TYPE stats_lock = portMUX_INITIALIZER_UNLOCKED;
static uint64_t full_submits, other_submits, submit_errors;

/* Linker wrap observes the actual LCD driver call, without changing the TE
 * completion callback owned by Presenter. The call return is acceptance,
 * not DMA completion: quiesce below supplies the completion check. */
esp_err_t __real_esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t panel, int x0, int y0,
                                           int x1, int y1, const void *color_data);
esp_err_t __wrap_esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t panel, int x0, int y0,
                                           int x1, int y1, const void *color_data)
{
    esp_err_t error = __real_esp_lcd_panel_draw_bitmap(panel, x0, y0, x1, y1, color_data);
    portENTER_CRITICAL(&stats_lock);
    if (error != ESP_OK) {
        submit_errors++;
    } else if (x0 == 0 && y0 == 0 && x1 == WIDTH && y1 == HEIGHT) {
        full_submits++;
    } else {
        other_submits++;
    }
    portEXIT_CRITICAL(&stats_lock);
    return error;
}

static void require_ok(esp_err_t error, const char *operation)
{
    while (error != ESP_OK) {
        ESP_LOGE(TAG, "FAIL operation=%s error=%s; retaining resources", operation, esp_err_to_name(error));
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static uint16_t pixel_at(int x, int y, unsigned step, bool te)
{
    if (y < 8) {
        return 0xf800;    /* red top */
    }
    if (y >= HEIGHT - 8) {
        return 0x001f;    /* blue bottom */
    }
    if (x < 8) {
        return 0x07e0;    /* green left */
    }
    if (x >= WIDTH - 8) {
        return 0xffe0;    /* yellow right */
    }
    /* Large upper-left marker identifies the active stage: cyan = GRAM,
     * magenta = automatic TE. A white block remains fixed in both stages. */
    if (y >= 32 && y < 64 && x >= 32 && x < 64) {
        return te ? 0xf81f : 0x07ff;
    }
    if (y >= 32 && y < 64 && x >= 80 && x < 112) {
        return 0xffff;
    }
    int box_x = 32 + (int)((step * 4U) % 352U);
    if (x >= box_x && x < box_x + 64 && y >= 208 && y < 272) {
        return 0xffff;
    }
    return 0;
}

static bool direct_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *event, void *user)
{
    (void)io;
    (void)event;
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)user, &woken);
    return woken == pdTRUE;
}

static void run_direct_stage(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t io,
                             unsigned duration_s, unsigned cycle, unsigned stage)
{
    const size_t bytes = (size_t)WIDTH * HEIGHT * sizeof(uint16_t);
    uint16_t *pixels = heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    SemaphoreHandle_t done = xSemaphoreCreateBinary();
    require_ok(pixels && done ? ESP_OK : ESP_ERR_NO_MEM, "direct PSRAM and completion");
    esp_lcd_panel_io_callbacks_t callbacks = {.on_color_trans_done = direct_done};
    require_ok(esp_lcd_panel_io_register_event_callbacks(io, &callbacks, done), "direct callback attach");
    portENTER_CRITICAL(&stats_lock);
    full_submits = other_submits = submit_errors = 0;
    portEXIT_CRITICAL(&stats_lock);
    ESP_LOGI(TAG, "STAGE_START cycle=%u stage=%u mode=DIRECT_NO_TE marker=CYAN duration_s=%u size=480x480 payload_bytes=460800 bus=40MHz lines=4 payload_estimate=23040us optical_safety=UNVERIFIED",
             cycle, stage, duration_s);
    const int64_t start_us = esp_timer_get_time();
    unsigned frame = 0;
    for (;;) {
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                pixels[y * WIDTH + x] = __builtin_bswap16(pixel_at(x, y, frame, false));
            }
        }
        esp_err_t sync = esp_cache_msync(pixels, bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
        require_ok(sync, "direct cache sync");
        require_ok(esp_lcd_panel_draw_bitmap(panel, 0, 0, WIDTH, HEIGHT, pixels), "direct panel draw");
        require_ok(xSemaphoreTake(done, pdMS_TO_TICKS(5000)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT, "direct transfer completion");
        frame++;
        uint64_t full, other, errors;
        portENTER_CRITICAL(&stats_lock);
        full = full_submits;
        other = other_submits;
        errors = submit_errors;
        portEXIT_CRITICAL(&stats_lock);
        if (full != frame || other || errors) {
            require_ok(ESP_FAIL, "direct transmission count");
        }
        int64_t elapsed = esp_timer_get_time() - start_us;
        if (frame % 100 == 0 || frame == 1) {
            ESP_LOGI(TAG, "PROGRESS cycle=%u stage=%u mode=DIRECT_NO_TE elapsed_ms=%"PRId64" frames=%u full_calls=%"PRIu64" other=%"PRIu64" errors=%"PRIu64,
                     cycle, stage, elapsed / 1000, frame, full, other, errors);
        }
        if (elapsed >= (int64_t)duration_s * 1000000) {
            ESP_LOGI(TAG, "STAGE_PASS cycle=%u stage=%u mode=DIRECT_NO_TE elapsed_ms=%"PRId64" frames=%u full_calls=%"PRIu64" other=%"PRIu64" optical_safety=UNVERIFIED",
                     cycle, stage, elapsed / 1000, frame, full, other);
            break;
        }
    }
    callbacks = (esp_lcd_panel_io_callbacks_t) {
        0
    };
    require_ok(esp_lcd_panel_io_register_event_callbacks(io, &callbacks, NULL), "direct callback detach");
    vSemaphoreDelete(done);
    heap_caps_free(pixels);
}

static void run_te_stage(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t io,
                         unsigned duration_s, unsigned cycle, unsigned stage)
{
    esp_display_presenter_config_t config = {
        .width = WIDTH, .height = HEIGHT,
        .pixel_format = ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565,
        .max_damage_areas = 8,
        .transfer_timeout_ms = 2000, .pipeline_acquire_timeout_ms = 2000,
        .target = {
            .hw = {
                .panel = panel, .io = io, .panel_type = ESP_DISPLAY_PRESENT_PANEL_IO,
                .input_pixel_format = ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565,
                .rotation = ESP_DISPLAY_PRESENT_ROTATE_0, .swap_bytes = true,
                .te_enabled = true,
                .te_sync = (esp_display_present_te_sync_config_t)
                {
                    .gpio_num = QSPI_TE_GPIO, .bus_freq_hz = 40000000, .data_lines = 4,
                },
            },
            .fb = {.mode = ESP_DISPLAY_PRESENT_MODE_TE_SYNC},
            .drawbuf = {
                .lines = BAND_ROWS, .buffers = 2,
                .in_psram = false, .te_compose_buffers = 2
            },
        },
    };
    esp_display_presenter_t *presenter = NULL;
    require_ok(esp_display_presenter_create(&config, &presenter), "presenter create");
    vTaskDelay(pdMS_TO_TICKS(100)); /* actual GPIO edges */
    portENTER_CRITICAL(&stats_lock);
    full_submits = other_submits = submit_errors = 0;
    portEXIT_CRITICAL(&stats_lock);
    ESP_LOGI(TAG, "STAGE_START cycle=%u stage=%u mode=%s marker=%s duration_s=%u size=480x480 payload_bytes=460800 bus=40MHz lines=4 payload_estimate=23040us optical_safety=UNVERIFIED",
             cycle, stage, "TE_AUTO", "MAGENTA", duration_s);
    const int64_t start_us = esp_timer_get_time();
    unsigned frame = 0;
    for (;;) {
        esp_display_present_surface_request_t request = {0};
        esp_display_present_area_t render_areas[8];
        size_t render_count = 0;
        bool full_coverage = false;
        require_ok(esp_display_presenter_begin_next_frame(presenter, &request, render_areas, 8,
                                                          &render_count, &full_coverage), "begin");
        const int rows_per_band = BAND_ROWS;
        for (int y = 0; y < HEIGHT; y += rows_per_band) {
            esp_display_presenter_buffer_t buf;
            require_ok(esp_display_presenter_acquire_buffer(presenter, &buf), "acquire");
            if (buf.capacity_bytes < (size_t)WIDTH * rows_per_band * sizeof(uint16_t)) {
                require_ok(ESP_ERR_NO_MEM, "tile capacity");
            }
            uint16_t *pixels = buf.surface.pixels;
            for (int row = 0; row < rows_per_band; row++) {
                for (int x = 0; x < WIDTH; x++) {
                    pixels[row * WIDTH + x] = pixel_at(x, y + row, frame, true);
                }
            }
            esp_display_present_area_t area = {0, y, WIDTH - 1, y + rows_per_band - 1};
            require_ok(esp_display_presenter_submit_buffer(presenter, &buf, &area, WIDTH * sizeof(uint16_t)), "submit");
        }
        esp_display_presenter_submit_t submit = {.coverage = ESP_DISPLAY_PRESENT_COVERAGE_FULL};
        require_ok(esp_display_presenter_commit_frame(presenter, &submit), "commit");
        require_ok(esp_display_presenter_quiesce(presenter, 5000), "quiesce");
        frame++;
        uint64_t full, other, errors;
        portENTER_CRITICAL(&stats_lock);
        full = full_submits;
        other = other_submits;
        errors = submit_errors;
        portEXIT_CRITICAL(&stats_lock);
        /* TE composes 16-row tiles into one full-screen driver call. */
        if (full != frame || other != 0 || errors != 0) {
            ESP_LOGE(TAG, "FAIL transfer geometry frame=%u full=%"PRIu64" bands=%"PRIu64" rejected=%"PRIu64,
                     frame, full, other, errors);
            require_ok(ESP_FAIL, "transmission count");
        }
        int64_t elapsed = esp_timer_get_time() - start_us;
        if (frame % 100 == 0 || frame == 1) {
            ESP_LOGI(TAG, "PROGRESS cycle=%u stage=%u mode=%s elapsed_ms=%"PRId64" frames=%u full_calls=%"PRIu64" band_calls=%"PRIu64" errors=%"PRIu64,
                     cycle, stage, "TE_AUTO", elapsed / 1000, frame, full, other, errors);
        }
        if (elapsed >= (int64_t)duration_s * 1000000) {
            ESP_LOGI(TAG, "STAGE_PASS cycle=%u stage=%u mode=%s elapsed_ms=%"PRId64" frames=%u full_calls=%"PRIu64" band_calls=%"PRIu64" optical_safety=UNVERIFIED",
                     cycle, stage, "TE_AUTO", elapsed / 1000, frame, full, other);
            break;
        }
    }
    require_ok(esp_display_presenter_stop(presenter), "stop");
    require_ok(esp_display_presenter_delete(presenter), "delete");
}

void app_main(void)
{
    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_io_handle_t io = NULL;
    require_ok(qspi_board_create(&panel, &io), "board create");
    for (unsigned cycle = 1;; cycle++) {
        run_direct_stage(panel, io, 30, cycle, 1);
        run_te_stage(panel, io, 60, cycle, 2);
        run_direct_stage(panel, io, 30, cycle, 3);
        ESP_LOGI(TAG, "COMPARISON_CYCLE_PASS cycle=%u", cycle);
    }
}
