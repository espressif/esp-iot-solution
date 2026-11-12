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
#include "esp_lcd_panel_io.h"
#include "pixel/present_executor.h"
#include "stdatomic.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#if CONFIG_IDF_TARGET_ESP32S31
#include "board_qspi.h"
#define WIDTH 480
#define HEIGHT 480
#define PANEL_KIND "S31_QSPI"
#else
#include "board.h"
#define PANEL_KIND BOARD_NAME
#endif
#define FRAMES 5
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

static atomic_uint dma_pending, dma_done, dma_bad, copy_entered;
static atomic_bool hold_copy;
static esp_lcd_panel_io_color_trans_done_cb_t original_color;
static void *original_color_user;
static bool color_probe(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *event, void *user)
{
    (void)user;
    if (atomic_fetch_sub(&dma_pending, 1) == 0) {
        atomic_fetch_add(&dma_bad, 1);
    }
    atomic_fetch_add(&dma_done, 1);
    return original_color ? original_color(io, event, original_color_user) : false;
}
esp_err_t __real_esp_lcd_panel_io_register_event_callbacks(esp_lcd_panel_io_handle_t, const esp_lcd_panel_io_callbacks_t *, void *);
esp_err_t __wrap_esp_lcd_panel_io_register_event_callbacks(esp_lcd_panel_io_handle_t io, const esp_lcd_panel_io_callbacks_t *callbacks, void *user)
{
    if (!callbacks || !callbacks->on_color_trans_done) {
        return __real_esp_lcd_panel_io_register_event_callbacks(io, callbacks, user);
    }
    original_color = callbacks->on_color_trans_done;
    original_color_user = user;
    esp_lcd_panel_io_callbacks_t wrapped = *callbacks;
    wrapped.on_color_trans_done = color_probe;
    return __real_esp_lcd_panel_io_register_event_callbacks(io, &wrapped, NULL);
}
esp_err_t __real_esp_display_present_executor_run(const esp_display_present_executor_dispatch_t *, const esp_display_present_copy_request_t *, esp_display_present_executor_selection_t *);
esp_err_t __wrap_esp_display_present_executor_run(const esp_display_present_executor_dispatch_t *dispatch, const esp_display_present_copy_request_t *request, esp_display_present_executor_selection_t *out)
{
    if (atomic_load(&hold_copy)) {
        atomic_fetch_add(&copy_entered, 1);
        while (atomic_load(&hold_copy)) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
    return __real_esp_display_present_executor_run(dispatch, request, out);
}
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
#if CONFIG_IDF_TARGET_ESP32S31
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
    atomic_fetch_add(&dma_pending, 1);
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
#if CONFIG_IDF_TARGET_ESP32S31
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
#if CONFIG_IDF_TARGET_ESP32S31
static const profile_t profiles[] = {
    {"TE_C1_T1", ESP_DISPLAY_PRESENT_MODE_TE_SYNC, 1, 1, true},
    {"TE_C1_T2", ESP_DISPLAY_PRESENT_MODE_TE_SYNC, 1, 2, true},
    {"TE_C2_T1", ESP_DISPLAY_PRESENT_MODE_TE_SYNC, 2, 1, true},
    {"TE_C2_T2", ESP_DISPLAY_PRESENT_MODE_TE_SYNC, 2, 2, true},
};
#elif defined(PRESENT_P4_CROSS_AXIS_PROBE)
static const profile_t profiles[] = {
    {"DOUBLE_PARTIAL_CROSS", ESP_DISPLAY_PRESENT_MODE_DOUBLE_PARTIAL, 0, 2, false},
};
#else
static const profile_t profiles[] = {
    {"NONE_1FB", ESP_DISPLAY_PRESENT_MODE_NONE, 0, 2, false},
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
#if CONFIG_IDF_TARGET_ESP32S31
                .panel_type = ESP_DISPLAY_PRESENT_PANEL_IO, .swap_bytes = true,
                .te_enabled = p->te, .spi_psram_dma_direct = true, .te_sync = {.gpio_num = QSPI_TE_GPIO, .bus_freq_hz = 40000000, .data_lines = 4},
#elif CONFIG_IDF_TARGET_ESP32S3
                .panel_type = ESP_DISPLAY_PRESENT_PANEL_RGB,
#else
                .panel_type = ESP_DISPLAY_PRESENT_PANEL_MIPI_DSI,
#endif
            }, .fb = {.mode = p->mode},
            .drawbuf = {.lines = 16, .buffers = p->tiles, .te_compose_buffers = p->compose}
        }
    };
    gram_mode = !p->te;
#if !CONFIG_IDF_TARGET_ESP32S31
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
        if (f > 0) {
#ifdef PRESENT_P4_TRANSITION_MATRIX
            unsigned next = target_rotation;
#elif defined(PRESENT_P4_CROSS_AXIS_PROBE)
            unsigned next = (rot + 90) % 360;
#else
            unsigned next = (rot + f * 90) % 360;
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
        vTaskDelay(pdMS_TO_TICKS(600));
    }
#if CONFIG_IDF_TARGET_ESP32S31
    /* The producer changes only the moving rectangle; TE must still submit
     * the complete physical frame. Coordinates are aligned to four pixels. */
    esp_display_present_area_t dirty = {24, lh / 2, 255, lh / 2 + 47};
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
    vTaskDelay(pdMS_TO_TICKS(100)); /* Allow RTOS idle cleanup before heap comparison. */
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
static void wait_flag(atomic_uint *value, const char *what)
{
    int64_t start = esp_timer_get_time();
    while (!atomic_load(value) && esp_timer_get_time() - start < 3000000) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    require(atomic_load(value) ? ESP_OK : ESP_ERR_TIMEOUT, what);
}
static void release_copy(void *unused)
{
    (void)unused;
    vTaskDelay(pdMS_TO_TICKS(30));
    atomic_store(&hold_copy, false);
    vTaskDelete(NULL);
}
static void fill_lifecycle(esp_display_presenter_t *presenter, bool one_band)
{
    size_t count;
    bool full;
    require(esp_display_presenter_begin_next_frame(presenter, NULL, NULL, 0, &count, &full), "lifecycle begin");
    require(gpio_intr_enable(QSPI_TE_GPIO), "TE enable");
    for (unsigned y = 0; y < HEIGHT;) {
        esp_display_presenter_buffer_t buffer;
        require(esp_display_presenter_acquire_buffer(presenter, &buffer), "lifecycle acquire");
        size_t rows = HEIGHT - y;
        require(buffer.resolve_rows(buffer.resolve_rows_ctx, buffer.lease_id, WIDTH * 2, rows, &rows), "lifecycle rows");
        uint16_t *pixels = buffer.surface.pixels;
        for (unsigned n = 0; n < WIDTH * rows; ++n) {
            pixels[n] = 0x07e0;
        }
        esp_display_present_area_t area = {0, y, WIDTH - 1, y + rows - 1};
        require(esp_display_presenter_submit_buffer(presenter, &buffer, &area, WIDTH * 2), "lifecycle submit");
        y += rows;
        if (one_band) {
            return;
        }
    }
    require(esp_display_presenter_commit_frame(presenter, NULL), "lifecycle commit");
}
static void run_lifecycle(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t io)
{
    const char *phases[] = {"LEASED", "WAIT_TE", "COPY_PENDING", "DMA_PENDING"};
    for (unsigned profile = 0; profile < 4; ++profile) {
        const profile_t *test = &profiles[profile];
        for (unsigned phase = 0; phase < 4; ++phase) {
            esp_display_presenter_config_t config = {
                .width = WIDTH, .height = HEIGHT,
                .transfer_timeout_ms = 5000, .pipeline_acquire_timeout_ms = 5000,
                .target = {
                    .hw = {
                        .panel_type = ESP_DISPLAY_PRESENT_PANEL_IO, .panel = panel, .io = io,
                        .swap_bytes = true, .te_enabled = true,
                        .spi_psram_dma_direct = true, .te_sync = {.gpio_num = QSPI_TE_GPIO, .bus_freq_hz = 40000000, .data_lines = 4}
                    },
                    .fb = {.mode = ESP_DISPLAY_PRESENT_MODE_TE_SYNC},
                    .drawbuf = {.lines = 16, .buffers = test->tiles, .te_compose_buffers = test->compose}
                }
            };
            ESP_LOGI(TAG, "LIFECYCLE_BEGIN profile=%s phase=%s", test->name, phases[phase]);
            require(!atomic_load(&dma_pending) ? ESP_OK : ESP_FAIL, "clean phase ingress");
            esp_display_presenter_t *presenter = NULL;
            require(esp_display_presenter_create(&config, &presenter), "lifecycle create");
            if (phase == 0) {
                size_t count;
                bool full;
                require(esp_display_presenter_begin_next_frame(presenter, NULL, NULL, 0, &count, &full), "leased begin");
                esp_display_presenter_buffer_t buffer;
                require(esp_display_presenter_acquire_buffer(presenter, &buffer), "leased acquire");
                require(esp_display_presenter_stop(presenter) == ESP_ERR_INVALID_STATE ? ESP_OK : ESP_FAIL, "stop must not confiscate lease");
                esp_display_presenter_cancel_frame(presenter);
            } else if (phase == 1) {
                /* Warm up with real TE edges, then suppress only the ISR input.
                 * Let old windows expire before queuing the next sealed frame. */
                fill_lifecycle(presenter, false);
                require(esp_display_presenter_quiesce(presenter, 10000), "wait TE warmup");
                require(gpio_intr_disable(QSPI_TE_GPIO), "TE mask");
                vTaskDelay(pdMS_TO_TICKS(80));
                unsigned before = atomic_load(&dma_done);
                /* fill_lifecycle enables TE after begin, so mask again before
                 * rendering the queued frame using the explicit band loop. */
                size_t count;
                bool full;
                require(esp_display_presenter_begin_next_frame(presenter, NULL, NULL, 0, &count, &full), "wait TE begin");
                for (unsigned y = 0; y < HEIGHT;) {
                    esp_display_presenter_buffer_t buffer;
                    require(esp_display_presenter_acquire_buffer(presenter, &buffer), "wait TE acquire");
                    size_t rows = HEIGHT - y;
                    require(buffer.resolve_rows(buffer.resolve_rows_ctx, buffer.lease_id, WIDTH * 2, rows, &rows), "wait TE rows");
                    memset(buffer.surface.pixels, 0x5a, WIDTH * rows * 2);
                    esp_display_present_area_t area = {0, y, WIDTH - 1, y + rows - 1};
                    require(esp_display_presenter_submit_buffer(presenter, &buffer, &area, WIDTH * 2), "wait TE submit");
                    y += rows;
                }
                require(esp_display_presenter_commit_frame(presenter, NULL), "wait TE commit");
                vTaskDelay(pdMS_TO_TICKS(50));
                require(!atomic_load(&dma_pending) && before == atomic_load(&dma_done) ? ESP_OK : ESP_FAIL, "waiting TE must not send");
                ESP_LOGI(TAG, "PHASE_OBSERVED profile=%s phase=WAIT_TE irq_masked=1 no_dma=1", test->name);
            } else if (phase == 2) {
                atomic_store(&copy_entered, 0);
                atomic_store(&hold_copy, true);
                fill_lifecycle(presenter, true);
                wait_flag(&copy_entered, "copy entered");
                esp_display_presenter_cancel_frame(presenter);
                require(esp_display_presenter_quiesce(presenter, 5) == ESP_ERR_TIMEOUT ? ESP_OK : ESP_FAIL, "cancel retains pending copy");
                require(!atomic_load(&dma_pending) ? ESP_OK : ESP_FAIL, "unsealed copy must not send");
                require(xTaskCreate(release_copy, "release_copy", 2048, NULL, 7, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM, "release task");
                ESP_LOGI(TAG, "PHASE_OBSERVED profile=%s phase=COPY_PENDING delayed_executor=1", test->name);
            } else {
                fill_lifecycle(presenter, false);
                wait_flag(&dma_pending, "DMA entered");
                ESP_LOGI(TAG, "PHASE_OBSERVED profile=%s phase=DMA_PENDING pending=%u", test->name, atomic_load(&dma_pending));
            }
            int64_t start = esp_timer_get_time();
            require(esp_display_presenter_stop(presenter), "lifecycle stop");
            int64_t stopped_us = esp_timer_get_time() - start;
            require(!atomic_load(&dma_pending) && !atomic_load(&hold_copy) ? ESP_OK : ESP_FAIL, "stop drained ownership");
            require(esp_display_presenter_stop(presenter), "idempotent stop");
            require(esp_display_presenter_delete(presenter), "lifecycle delete");
            unsigned completed = atomic_load(&dma_done);
            vTaskDelay(pdMS_TO_TICKS(100));
            require(atomic_load(&dma_done) == completed && !atomic_load(&dma_pending) && !atomic_load(&dma_bad) ? ESP_OK : ESP_FAIL, "no callback after delete");
            require(heap_caps_check_integrity_all(true) ? ESP_OK : ESP_FAIL, "lifecycle heap");
            ESP_LOGI(TAG, "LIFECYCLE_PASS profile=%s phase=%s stop_us=%lld dma_pending=0 dma_bad=0 internal=%u psram=%u",
                     test->name, phases[phase], (long long)stopped_us,
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        }
    }
}

static void run(void *unused)
{
    (void)unused;
    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_io_handle_t io = NULL;
    vTaskDelay(pdMS_TO_TICKS(3000)); /* Allow the host to attach a serial collector. */
    shadow = heap_caps_malloc(WIDTH * HEIGHT * sizeof(*shadow), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    require(shadow ? ESP_OK : ESP_ERR_NO_MEM, "shadow");
#if CONFIG_IDF_TARGET_ESP32S31
    require(qspi_board_create(&panel, &io), "board create");
    require(esp_lcd_panel_set_gap(panel, 0, 0), "physical full-screen gap");
#else
    require(board_panel_create(&panel, 3), "board create");
#endif
    for (unsigned round = 0; round < 2; ++round) {
        for (unsigned p = 0; p < sizeof(profiles) / sizeof(profiles[0]); ++p) {
            run_case(panel, io, p, 0);
            require(!atomic_load(&dma_pending) && !atomic_load(&dma_bad) ? ESP_OK : ESP_FAIL, "DMA balanced");
        }
        ESP_LOGI(TAG, "ROUND_PASS round=%u internal=%u psram=%u dma_done=%u", round,
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM), atomic_load(&dma_done));
    }
    run_lifecycle(panel, io);
    ESP_LOGI(TAG, "TE_SUITE_PASS normal_cases=8 frames=48 lifecycle_cases=16 dma_pending=%u dma_bad=%u",
             atomic_load(&dma_pending), atomic_load(&dma_bad));
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
