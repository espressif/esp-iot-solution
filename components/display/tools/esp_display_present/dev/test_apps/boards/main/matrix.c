/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0 */
/* Public presenter mode / rotation hardware matrix. Full physical TE only. */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "esp_display_present.h"
#include "esp_lcd_panel_ops.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#if CONFIG_IDF_TARGET_ESP32S31 && !defined(PRESENT_BOARD_S31_RGB)
#include "board_qspi.h"
#define WIDTH 480
#define HEIGHT 480
#define PANEL_KIND "S31_QSPI"
#else
#include "board.h"
#define PANEL_KIND BOARD_NAME
#endif
#define FRAMES 4
static const char *TAG = "mode_matrix";
static uint16_t *shadow;
static const uint16_t *last_full;
static unsigned full_calls, band_calls, errors, invalid_calls;
static bool gram_mode;
#ifdef PRESENT_P4_TRANSITION_MATRIX
static unsigned target_rotation, transition_passes, transition_failures;
static int invalid_x1, invalid_y1;
#endif
static const uint16_t palette[] = {0xf800, 0x07e0, 0x001f, 0xffe0, 0xf81f, 0x07ff, 0xffff, 0};

esp_err_t __real_esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t, int, int, int, int, const void *);
esp_err_t __wrap_esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t panel, int x0, int y0, int x1, int y1, const void *data)
{
    if (!data || x0 < 0 || y0 < 0 || x0 >= x1 || y0 >= y1 || x1 > WIDTH || y1 > HEIGHT) {
        ++invalid_calls;
#ifdef PRESENT_P4_TRANSITION_MATRIX
        invalid_x1 = x1;
        invalid_y1 = y1;
#endif
        ESP_LOGE(TAG, "OUT_OF_PANEL x0=%d y0=%d x1=%d y1=%d physical=%dx%d", x0, y0, x1, y1, WIDTH, HEIGHT);
    } else if (x0 == 0 && y0 == 0 && x1 == WIDTH && y1 == HEIGHT) {
        ++full_calls;
        last_full = data;
    } else {
        ++band_calls;
    }
#if CONFIG_IDF_TARGET_ESP32S31 && !defined(PRESENT_BOARD_S31_RGB)
    if ((x0 & 3) || ((x1 - x0) & 3)) {
        ++invalid_calls;
    }
    if (gram_mode && data && x0 >= 0 && y0 >= 0 && x0 < x1 && y0 < y1 && x1 <= WIDTH && y1 <= HEIGHT) {
        const uint16_t *p = data;
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                shadow[y * WIDTH + x] = __builtin_bswap16(*p++);
            }
        }
    }
#endif
    esp_err_t result = __real_esp_lcd_panel_draw_bitmap(panel, x0, y0, x1, y1, data);
    if (result != ESP_OK) {
        ++errors;
    }
    return result;
}
static void require(esp_err_t e, const char *what)
{
    while (e != ESP_OK) {
        ESP_LOGE(TAG, "FAIL step=%s error=%s", what, esp_err_to_name(e));
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
static uint16_t reference(unsigned x, unsigned y, unsigned width, unsigned height, unsigned frame, unsigned profile)
{
    if (x < 4 || x >= width - 4 || y < 4 || y >= height - 4) {
        return 0xffff;
    }
    if (y < 28 && x >= 12 && x < 12 + 12 * (profile + 1)) {
        return 0xffff;
    }
    unsigned bx = 24 + frame * 19 % (width - 88);
    if (x >= bx && x < bx + 48 && y >= height / 2 && y < height / 2 + 48) {
        return 0xffff;
    }
    return palette[x * 8 / width];
}
static void verify_pixels(unsigned rot, unsigned lw, unsigned lh, unsigned frame, unsigned profile)
{
    const uint16_t *pixels = gram_mode ? shadow : last_full;
    require(pixels ? ESP_OK : ESP_FAIL, "capture missing");
    for (unsigned ly = 0; ly < lh; ++ly) {
        for (unsigned lx = 0; lx < lw; ++lx) {
            unsigned px, py;
            switch (rot) {
            case 90: px = lh - 1 - ly; py = lx; break;
            case 180: px = lw - 1 - lx; py = lh - 1 - ly; break;
            case 270: px = ly; py = lw - 1 - lx; break;
            default: px = lx; py = ly; break;
            }
            uint16_t got = pixels[py * WIDTH + px];
#if CONFIG_IDF_TARGET_ESP32S31 && !defined(PRESENT_BOARD_S31_RGB)
            if (!gram_mode) {
                got = __builtin_bswap16(got);
            }
#endif
            uint16_t want = reference(lx, ly, lw, lh, frame, profile);
            if (got != want) {
                ESP_LOGE(TAG, "PIXEL_FAIL rot=%u frame=%u logical=%u,%u physical=%u,%u got=%04x expected=%04x",
                         rot, frame, lx, ly, px, py, got, want);
                require(ESP_FAIL, "pixel reference");
            }
        }
    }
}
typedef struct {
    const char *name;
    esp_display_present_mode_t mode;
    unsigned compose, tiles;
    bool te;
} profile_t;
#if CONFIG_IDF_TARGET_ESP32S31 && !defined(PRESENT_BOARD_S31_RGB)
static const profile_t profiles[] = {
    {"GRAM_T1", ESP_DISPLAY_PRESENT_MODE_NONE, 0, 1, false},
    {"GRAM_T2", ESP_DISPLAY_PRESENT_MODE_NONE, 0, 2, false},
    {"TE_C1_T1", ESP_DISPLAY_PRESENT_MODE_TE_SYNC, 1, 1, true},
    {"TE_C1_T2", ESP_DISPLAY_PRESENT_MODE_TE_SYNC, 1, 2, true},
    {"TE_C2_T1", ESP_DISPLAY_PRESENT_MODE_TE_SYNC, 2, 1, true},
    {"TE_C2_T2", ESP_DISPLAY_PRESENT_MODE_TE_SYNC, 2, 2, true},
    {"AUTO_GRAM", ESP_DISPLAY_PRESENT_MODE_AUTO, 0, 2, false},
    {"AUTO_TE", ESP_DISPLAY_PRESENT_MODE_AUTO, 2, 2, true},
};
#elif defined(PRESENT_P4_CROSS_AXIS_PROBE)
static const profile_t profiles[] = {
    {"DOUBLE_PARTIAL_CROSS", ESP_DISPLAY_PRESENT_MODE_DOUBLE_PARTIAL, 0, 2, false},
};
#else
static const profile_t profiles[] = {
    {"DOUBLE_FULL", ESP_DISPLAY_PRESENT_MODE_DOUBLE_FULL, 0, 2, false},
    {"TRIPLE_FULL", ESP_DISPLAY_PRESENT_MODE_TRIPLE_FULL, 0, 2, false},
    {"DOUBLE_DIRECT", ESP_DISPLAY_PRESENT_MODE_DOUBLE_DIRECT, 0, 2, false},
    {"DOUBLE_PARTIAL", ESP_DISPLAY_PRESENT_MODE_DOUBLE_PARTIAL, 0, 2, false},
    {"TRIPLE_PARTIAL", ESP_DISPLAY_PRESENT_MODE_TRIPLE_PARTIAL, 0, 2, false},
    {"AUTO", ESP_DISPLAY_PRESENT_MODE_AUTO, 0, 2, false},
};
#endif
static void run_case(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t io, unsigned id, unsigned rot)
{
    const profile_t *p = &profiles[id];
    unsigned lw = (rot == 90 || rot == 270) ? HEIGHT : WIDTH;
    unsigned lh = (rot == 90 || rot == 270) ? WIDTH : HEIGHT;
    esp_display_presenter_config_t config = {
        .width = lw, .height = lh, .max_damage_areas = 8,
        .transfer_timeout_ms = 5000, .pipeline_acquire_timeout_ms = 5000,
        .target = {
            .hw = {
                .panel = panel, .io = io, .rotation = rot,
#if CONFIG_IDF_TARGET_ESP32S31 && !defined(PRESENT_BOARD_S31_RGB)
                .panel_type = ESP_DISPLAY_PRESENT_PANEL_IO, .swap_bytes = true,
                .te_enabled = p->te, .spi_psram_dma_direct = true, .te_sync = {.gpio_num = QSPI_TE_GPIO, .bus_freq_hz = 40000000, .data_lines = 4},
#elif CONFIG_IDF_TARGET_ESP32S3 || defined(PRESENT_BOARD_S31_RGB)
                .panel_type = ESP_DISPLAY_PRESENT_PANEL_RGB,
#else
                .panel_type = ESP_DISPLAY_PRESENT_PANEL_MIPI_DSI,
#endif
            }, .fb = {.mode = p->mode},
            .drawbuf = {.lines = 16, .buffers = p->tiles, .te_compose_buffers = p->compose}
        }
    };
    gram_mode = !p->te;
#if !CONFIG_IDF_TARGET_ESP32S31 || defined(PRESENT_BOARD_S31_RGB)
    gram_mode = false;
#endif
    esp_display_presenter_t *presenter = NULL;
    ESP_LOGI(TAG, "CASE_BEGIN board=%s profile=%s rotation=%u logical=%ux%u physical=%ux%u", PANEL_KIND, p->name, rot, lw, lh, WIDTH, HEIGHT);
    require(esp_display_presenter_create(&config, &presenter), "create");
    if (p->te) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    esp_display_presenter_caps_t caps;
    require(esp_display_presenter_get_caps(presenter, &caps), "caps");
    require(caps.width == lw && caps.height == lh ? ESP_OK : ESP_FAIL, "logical dimensions");
    uint64_t duration_us = 0;
    unsigned current_rot = rot;
#ifdef PRESENT_P4_TRANSITION_MATRIX
    bool unsafe = false;
#endif
    for (unsigned f = 0; f < FRAMES; ++f) {
        if (f == FRAMES / 2) {
#ifdef PRESENT_P4_TRANSITION_MATRIX
            unsigned next = target_rotation;
#elif defined(PRESENT_P4_CROSS_AXIS_PROBE)
            unsigned next = (rot + 90) % 360;
#else
            unsigned next = (rot + 180) % 360;
#endif
            esp_err_t changed = esp_display_presenter_set_rotation(presenter, next);
            if (gram_mode && rot == 0 && changed == ESP_ERR_NOT_SUPPORTED) {
                ESP_LOGW(TAG, "ROTATION_LIMIT profile=%s from=%u to=%u reason=in_place", p->name, rot, next);
            } else {
                require(changed, "runtime rotation");
                current_rot = next;
#ifdef PRESENT_P4_CROSS_AXIS_PROBE
                require(esp_display_presenter_get_caps(presenter, &caps), "runtime caps");
                lw = caps.width;
                lh = caps.height;
#endif
                ESP_LOGI(TAG, "ROTATION_CHANGED profile=%s from=%u to=%u", p->name, rot, next);
            }
        }
        full_calls = band_calls = errors = invalid_calls = 0;
        last_full = NULL;
        if (gram_mode) {
            memset(shadow, 0, WIDTH * HEIGHT * sizeof(*shadow));
        }
        uint64_t start = esp_timer_get_time();
        size_t count;
        bool full;
        require(esp_display_presenter_begin_next_frame(presenter, NULL, NULL, 0, &count, &full), "begin");
        for (unsigned y = 0; y < lh;) {
            esp_display_presenter_buffer_t buf;
            require(esp_display_presenter_acquire_buffer(presenter, &buf), "acquire");
            size_t rows = lh - y;
            if (caps.contract == ESP_DISPLAY_PRESENT_CONTRACT_PARTITION) {
                require(buf.resolve_rows(buf.resolve_rows_ctx, buf.lease_id, lw * 2, rows, &rows), "resolve rows");
            }
            require(rows > 0 && rows <= lh - y && buf.capacity_bytes >= (size_t)lw * rows * 2 ? ESP_OK : ESP_FAIL, "tile bounds");
            uint16_t *pixels = buf.surface.pixels;
            for (unsigned yy = 0; yy < rows; ++yy)
                for (unsigned x = 0; x < lw; ++x) {
                    pixels[yy * lw + x] = reference(x, y + yy, lw, lh, f, id);
                }
            esp_display_present_area_t area = {0, y, lw - 1, y + rows - 1};
            require(esp_display_presenter_submit_buffer(presenter, &buf, &area, lw * 2), "submit");
            y += rows;
        }
        const esp_display_presenter_submit_t submit = {.coverage = ESP_DISPLAY_PRESENT_COVERAGE_FULL};
        require(esp_display_presenter_commit_frame(presenter, &submit), "commit");
        require(esp_display_presenter_quiesce(presenter, 10000), "quiesce");
        duration_us += esp_timer_get_time() - start;
#ifdef PRESENT_P4_TRANSITION_MATRIX
        if (invalid_calls) {
            ESP_LOGE(TAG, "TRANSITION_UNSAFE profile=%s from=%u to=%u output=%dx%d physical=%dx%d",
                     p->name, rot, current_rot, invalid_x1, invalid_y1, WIDTH, HEIGHT);
            unsafe = true;
            ++transition_failures;
            break;
        }
#endif
        if (errors || invalid_calls || (p->te && (full_calls != 1 || band_calls != 0)) ||
                (!gram_mode && !p->te && ((full_calls != 1 && !(f == 0 && full_calls == 2)) || band_calls != 0)) ||
                (gram_mode && !full_calls && !band_calls)) {
            ESP_LOGE(TAG, "GEOMETRY_FAIL full=%u bands=%u invalid=%u error=%u", full_calls, band_calls, invalid_calls, errors);
            require(ESP_FAIL, "physical transfer geometry");
        }
        verify_pixels(current_rot, lw, lh, f, id);
        vTaskDelay(pdMS_TO_TICKS(60));
    }
#if CONFIG_IDF_TARGET_ESP32S31 && !defined(PRESENT_BOARD_S31_RGB)
    /* The producer changes only the moving rectangle; TE must still submit
     * the complete physical frame. Coordinates are aligned to four pixels. */
    esp_display_present_area_t dirty = {80, lh / 2, 151, lh / 2 + 47};
    esp_display_present_surface_request_t request = {.dirty_areas = &dirty, .dirty_area_count = 1};
    esp_display_present_area_t render[2];
    size_t render_count = 0;
    bool full_coverage = false;
    full_calls = band_calls = errors = invalid_calls = 0;
    last_full = NULL;
    require(esp_display_presenter_begin_next_frame(presenter, &request, render, 2, &render_count, &full_coverage), "dirty begin");
    require(!full_coverage && render_count == 1 ? ESP_OK : ESP_FAIL, "dirty negotiation");
    esp_display_present_area_t area = render[0];
    unsigned aw = area.x2 - area.x1 + 1;
    for (unsigned y = area.y1; y <= area.y2;) {
        esp_display_presenter_buffer_t buf;
        require(esp_display_presenter_acquire_buffer(presenter, &buf), "dirty acquire");
        size_t rows = area.y2 - y + 1;
        require(buf.resolve_rows(buf.resolve_rows_ctx, buf.lease_id, aw * 2, rows, &rows), "dirty rows");
        require(rows > 0 && rows <= area.y2 - y + 1 && buf.capacity_bytes >= (size_t)aw * rows * 2 ? ESP_OK : ESP_FAIL, "dirty capacity");
        uint16_t *pixels = buf.surface.pixels;
        for (unsigned yy = 0; yy < rows; ++yy)
            for (unsigned x = 0; x < aw; ++x) {
                pixels[yy * aw + x] = reference(area.x1 + x, y + yy, lw, lh, FRAMES, id);
            }
        esp_display_present_area_t part = {area.x1, y, area.x2, y + rows - 1};
        require(esp_display_presenter_submit_buffer(presenter, &buf, &part, aw * 2), "dirty submit");
        y += rows;
    }
    const esp_display_presenter_submit_t dirty_submit = {.coverage = ESP_DISPLAY_PRESENT_COVERAGE_AREAS, .areas = &area, .area_count = 1};
    require(esp_display_presenter_commit_frame(presenter, &dirty_submit), "dirty commit");
    require(esp_display_presenter_quiesce(presenter, 10000), "dirty quiesce");
    require(!errors && !invalid_calls && (p->te ? full_calls == 1 && band_calls == 0 : band_calls != 0) ? ESP_OK : ESP_FAIL, "dirty physical geometry");
    verify_pixels(current_rot, lw, lh, FRAMES, id);
    ESP_LOGI(TAG, "DIRTY_PASS profile=%s rotation=%u producer=%u,%u-%u,%u full_calls=%u bands=%u pixels=ALL",
             p->name, current_rot, area.x1, area.y1, area.x2, area.y2, full_calls, band_calls);
#endif
    esp_display_presenter_frame_status_t status;
    require(esp_display_presenter_get_frame_status(presenter, &status), "status");
    require(status.completed_transfer_frame == status.last_submitted_frame ? ESP_OK : ESP_FAIL, "frames completed");
    require(esp_display_presenter_stop(presenter), "stop");
    require(esp_display_presenter_delete(presenter), "delete");
    require(heap_caps_check_integrity_all(true) ? ESP_OK : ESP_FAIL, "heap integrity");
#ifdef PRESENT_P4_TRANSITION_MATRIX
    if (!unsafe) {
        ++transition_passes;
        ESP_LOGI(TAG, "TRANSITION_PASS profile=%s from=%u to=%u pixels=ALL", p->name, rot, current_rot);
    }
#else
    ESP_LOGI(TAG, "CASE_PASS board=%s profile=%s rotation=%u frames=%u avg_frame_us=%"PRIu64" pixels=ALL last_full=%u last_bands=%u completed=%"PRIu64,
             PANEL_KIND, p->name, rot, FRAMES, duration_us / FRAMES, full_calls, band_calls, status.completed_transfer_frame);
#endif
}
static void run(void *unused)
{
    (void)unused;
    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_io_handle_t io = NULL;
    vTaskDelay(pdMS_TO_TICKS(3000)); /* Allow the host to attach a serial collector. */
    shadow = heap_caps_malloc(WIDTH * HEIGHT * sizeof(*shadow), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    require(shadow ? ESP_OK : ESP_ERR_NO_MEM, "shadow");
#if CONFIG_IDF_TARGET_ESP32S31 && !defined(PRESENT_BOARD_S31_RGB)
    require(qspi_board_create(&panel, &io), "board create");
    require(esp_lcd_panel_set_gap(panel, 0, 0), "physical full-screen gap");
#else
    require(board_panel_create(&panel, 3), "board create");
#endif
#ifdef PRESENT_P4_CROSS_AXIS_PROBE
    const unsigned rots[] = {0};
#else
    const unsigned rots[] = {0, 90, 180, 270};
#endif
    for (unsigned p = 0; p < sizeof(profiles) / sizeof(profiles[0]); ++p)
        for (unsigned r = 0; r < sizeof(rots) / sizeof(rots[0]); ++r)
#ifdef PRESENT_P4_TRANSITION_MATRIX
            for (unsigned t = 0; t < sizeof(rots) / sizeof(rots[0]); ++t) {
                if (r != t) {
                    target_rotation = rots[t];
                    run_case(panel, io, p, rots[r]);
                }
            }
    ESP_LOGI(TAG, "TRANSITION_MATRIX_DONE board=%s pass=%u unsafe=%u", PANEL_KIND, transition_passes, transition_failures);
#else
            run_case(panel, io, p, rots[r]);
    ESP_LOGI(TAG, "MATRIX_PASS board=%s cases=%u rotations=4 frames_per_case=%u", PANEL_KIND,
             (unsigned)((sizeof(profiles) / sizeof(profiles[0])) * (sizeof(rots) / sizeof(rots[0]))), FRAMES);
#endif
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
void app_main(void)
{
    if (xTaskCreate(run, "mode_matrix", 16384, NULL, 6, NULL) != pdPASS) {
        require(ESP_ERR_NO_MEM, "task");
    }
}
