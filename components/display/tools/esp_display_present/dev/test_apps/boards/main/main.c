/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include <inttypes.h>
#include <string.h>
#include "board.h"
#include "esp_display_present.h"
#include "core/present_engine.h"
#include "platform/lcd/present_lcd_fb_scanout.h"
#include "config/present_plan.h"
#include "pixel/present_executor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "present_board";
static const uint16_t colors[] = {0xf800, 0x07e0, 0x001f, 0xffff, 0x0000, 0xffe0, 0x07ff, 0xf81f};
static const struct {
    const char *name;
    esp_display_present_present_mode_t mode;
    size_t buffers;
    esp_display_present_render_contract_t contract;
} cases[] = {
    {"DOUBLE_FULL", ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_FULL, 2, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL},
    {"TRIPLE_FULL", ESP_DISPLAY_PRESENT_ENGINE_MODE_TRIPLE_FULL, 3, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL},
    {"DOUBLE_DIRECT", ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_DIRECT, 2, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT},
    {"DOUBLE_PARTIAL", ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_PARTIAL, 2, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION},
    {"TRIPLE_PARTIAL", ESP_DISPLAY_PRESENT_ENGINE_MODE_TRIPLE_PARTIAL, 3, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION},
};
#define CASE_COUNT (sizeof(cases) / sizeof(cases[0]))
#define FRAME_COUNT 48
#define TILE_ROWS 11

static void check(esp_err_t err, const char *operation)
{
    if (err == ESP_OK) {
        return;
    }
    while (true) {
        ESP_LOGE(TAG, "FAIL %s: %s; resources retained, reset to retry", operation, esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/* Tap only descriptors; all actual callbacks and submissions stay in V2. */
typedef struct {
    esp_display_present_panel_adapter_t real;
    esp_display_present_surface_t submitted;
    unsigned submissions;
    void *known[3];
    size_t known_count;
    unsigned used_mask;
} tap_t;
static esp_err_t tap_attach(void *context, esp_display_present_panel_event_fn callback, void *user)
{
    tap_t *tap = context;
    return tap->real.attach(tap->real.context, callback, user);
}
static esp_display_present_tx_result_t tap_submit(void *context, const esp_display_present_surface_t *surface,
                                                  const esp_display_present_rect_t *area, uint64_t cookie)
{
    tap_t *tap = context;
    esp_display_present_tx_result_t result = tap->real.submit(tap->real.context, surface, area, cookie);
    if (result.acceptance == ESP_DISPLAY_PRESENT_TX_ACCEPTED) {
        tap->submitted = *surface;
        tap->submissions++;
        for (size_t i = 0; i < tap->known_count; ++i) {
            if (tap->known[i] == surface->pixels) {
                tap->used_mask |= (1u << i);
            }
        }
    }
    return result;
}
static esp_err_t tap_quiesce(void *context, uint32_t timeout)
{
    tap_t *tap = context;
    return tap->real.quiesce(tap->real.context, timeout);
}

static esp_display_present_rect_t moving(unsigned frame)
{
    return (esp_display_present_rect_t) {
        7 + frame * 13 % (WIDTH - 60), 65 + frame * 7 % (HEIGHT - 150), 37, 59
    };
}
static uint16_t reference(unsigned x, unsigned y, unsigned frame, unsigned test)
{
    esp_display_present_rect_t box = moving(frame);
    if (x >= box.x && x < box.x + box.width && y >= box.y && y < box.y + box.height) {
        return 0xffff;
    }
    if (x < 3 || y < 3 || x >= WIDTH - 3 || y >= HEIGHT - 3) {
        return 0xffff;
    }
    /* Top row identifies mode by 1..5 white squares, left to right. */
    if (y >= 12 && y < 36 && x >= 12 && x < 12 + CASE_COUNT * 32) {
        return (x - 12) / 32 <= test && (x - 12) % 32 < 24 ? 0xffff : 0;
    }
    return colors[x * 8 / WIDTH];
}

static void render_region(esp_display_present_engine_t *p, esp_display_present_render_contract_t contract,
                          esp_display_present_rect_t area, unsigned frame, unsigned test)
{
    for (unsigned offset = 0; offset < area.height;) {
        esp_display_present_lease_t lease;
        check(esp_display_present_engine_acquire_buffer(p, &lease), "acquire");
        unsigned rows = area.height - offset;
        if (contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION && rows > TILE_ROWS) {
            rows = TILE_ROWS;
        }
        /* Padded, odd-width dirty tiles exercise submit-time stride packing. */
        size_t stride = contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION ? (area.width + (area.width < WIDTH ? 3 : 0)) * 2
                        : lease.surface.layout.stride_bytes;
        for (unsigned y = 0; y < rows; ++y) {
            unsigned dy = area.y + offset + y;
            uint16_t *row = (uint16_t *)((uint8_t *)lease.surface.pixels +
                                         (contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION ? y : dy) * stride);
            for (unsigned x = 0; x < area.width; ++x) {
                row[contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION ? x : area.x + x] = reference(area.x + x, dy, frame, test);
            }
        }
        esp_display_present_engine_area_t submit = {area.x, area.y + offset, area.x + area.width - 1, area.y + offset + rows - 1};
        check(esp_display_present_engine_submit_buffer(p, &lease, &submit, stride), "submit region");
        offset += rows;
    }
}

static void verify(const esp_display_present_surface_t *surface, unsigned frame, unsigned test)
{
    for (unsigned y = 0; y < HEIGHT; ++y) {
        const uint16_t *row = (const uint16_t *)((const uint8_t *)surface->pixels + y * surface->layout.stride_bytes);
        for (unsigned x = 0; x < WIDTH; ++x) {
            uint16_t expected = reference(x, y, frame, test);
            if (row[x] != expected) {
                ESP_LOGE(TAG, "PIXEL mode=%s frame=%u x=%u y=%u actual=%04x expected=%04x",
                         cases[test].name, frame, x, y, row[x], expected);
                check(ESP_FAIL, "pixel compare");
            }
        }
    }
}

static void run_case(unsigned cycle, unsigned test)
{
    ESP_LOGI(TAG, "BEGIN cycle=%u mode=%s buffers=%u", cycle, cases[test].name, (unsigned)cases[test].buffers);
    esp_lcd_panel_handle_t panel;
    check(board_panel_create(&panel, cases[test].buffers), "board panel create");
    esp_display_present_lcd_fb_scanout_t *adapter;
    esp_display_present_lcd_fb_scanout_config_t scan = {.panel = panel, .kind = SCANOUT_KIND,
                                                        .width = WIDTH, .height = HEIGHT, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .buffer_count = cases[test].buffers,
                                                        .bounce = SCANOUT_KIND == ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_RGB, .delete_panel = true
                                                       };
    check(esp_display_present_lcd_fb_scanout_create(&scan, &adapter), "scanout create");
    void *buffers[3];
    size_t count;
    check(esp_display_present_lcd_fb_scanout_get_buffers(adapter, buffers, 3, &count), "get buffers");
    tap_t tap = {0};
    check(esp_display_present_lcd_fb_scanout_get_interface(adapter, &tap.real), "interface");
    esp_display_present_engine_config_t cfg = {.backend_kind = ESP_DISPLAY_PRESENT_ENGINE_FB, .mode = cases[test].mode,
                                               .timeout_ms = 2000, .backend = {.width = WIDTH, .height = HEIGHT, .tile_rows = TILE_ROWS,
                                                                               .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .buffer_count = count, .max_frames = 3,
                                                                               .ext_bufs = buffers, .ext_bytes = WIDTH * HEIGHT * 2,
                                                                               .scan_buf = buffers[0], .panel = {&tap, tap_attach, tap_submit, tap_quiesce}
                                                                              }
                                              };
    esp_display_present_engine_t *presenter;
    esp_display_present_plan_config_t planned = {.presenter = cfg,
                                                 .resources = {.tile_count = cases[test].contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION ? 2 : 0,
                                                               .image_memory = ESP_DISPLAY_PRESENT_MEMORY_EXTERNAL
                                                              }
                                                };
    check(esp_display_present_lcd_fb_scanout_get_output_caps(adapter, &planned.output), "output caps");
    esp_display_present_resolved_plan_t resolved;
    check(esp_display_present_plan_resolve(&planned, &resolved), "resolve plan");
    ESP_LOGI(TAG, "PLAN images=%u tiles=%u logical=%u pixel_bytes=%u coverage=%u pending=%u",
             (unsigned)resolved.images.count, (unsigned)resolved.tiles.count, (unsigned)resolved.logical.count,
             (unsigned)resolved.pixel_bytes, (unsigned)resolved.coverage_bytes, (unsigned)resolved.pending_runtime);
    check(esp_display_present_engine_create_planned(&planned, &presenter), "planned presenter create");
    uint64_t max_wait = 0;
    for (unsigned frame = 0; frame < FRAME_COUNT; ++frame) {
        bool full = frame == 0 || cases[test].contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL;
        esp_display_present_rect_t dirty[2] = {moving(frame ? frame - 1 : 0), moving(frame)};
        esp_display_present_frame_request_t request = {.full_redraw = full, .damage = full ? NULL : dirty, .damage_count = full ? 0 : 2};
        check(esp_display_present_engine_begin_next_frame(presenter, &request), "begin");
        if (full) {
            render_region(presenter, cases[test].contract, (esp_display_present_rect_t) {
                0, 0, WIDTH, HEIGHT
            }, frame, test);
        } else {
            render_region(presenter, cases[test].contract, dirty[0], frame, test);
            render_region(presenter, cases[test].contract, dirty[1], frame, test);
        }
        esp_display_present_engine_receipt_t receipt;
        int64_t start = esp_timer_get_time();
        check(esp_display_present_engine_commit_with_receipt(presenter, NULL, &receipt), "commit");
        esp_display_present_frame_status_t status;
        check(esp_display_present_engine_wait_frame(presenter, receipt, ESP_DISPLAY_PRESENT_COMPLETION_PRESENT_CONFIRMED, 2000, &status), "wait");
        check(status.result, "frame result");
        check(status.present_confirmed && status.terminal ? ESP_OK : ESP_FAIL, "confirmed terminal");
        uint64_t elapsed = esp_timer_get_time() - start;
        if (elapsed > max_wait) {
            max_wait = elapsed;
        }
        check(tap.submissions == frame + 1 ? ESP_OK : ESP_FAIL, "submission count");
        verify(&tap.submitted, frame, test);
        check(esp_display_present_engine_release_receipt(presenter, receipt), "release");
        check(esp_display_present_engine_service(presenter), "service");
        vTaskDelay(pdMS_TO_TICKS(30));
    }
    esp_display_present_engine_status_t state;
    check(esp_display_present_engine_get_status(presenter, &state), "status");
    check(state.first_error, "sticky fault");
    check(state.pending_frames == 0 ? ESP_OK : ESP_FAIL, "receipts drained");
    ESP_LOGI(TAG, "PASS cycle=%u mode=%s frames=%u pixels=ALL max_wait_us=%" PRIu64 " pending=0 sticky=ESP_OK",
             cycle, cases[test].name, FRAME_COUNT, max_wait);
    check(esp_display_present_engine_stop(presenter, 2000), "stop");
    check(esp_display_present_engine_destroy(presenter), "presenter destroy");
    check(esp_display_present_lcd_fb_scanout_destroy(adapter), "adapter destroy");
    check(board_resources_destroy(), "board resources destroy");
    check(heap_caps_check_integrity_all(true) ? ESP_OK : ESP_FAIL, "heap integrity");
    ESP_LOGI(TAG, "CLEAN cycle=%u mode=%s internal=%u psram=%u", cycle, cases[test].name,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}
static void verify_rotated(const esp_display_present_surface_t *surface, uint32_t lw, uint32_t lh,
                           esp_display_present_engine_rotation_t rot, unsigned frame, unsigned test)
{
    for (unsigned ly = 0; ly < lh; ++ly) {
        for (unsigned lx = 0; lx < lw; ++lx) {
            /* Independent mapping: do not use the implementation geometry
             * helper as the reference for the pixels being verified. */
            esp_display_present_rect_t phys = {0};
            switch (rot) {
            case ESP_DISPLAY_PRESENT_ENGINE_ROTATE_90: phys.x = lh - 1 - ly; phys.y = lx; break;
            case ESP_DISPLAY_PRESENT_ENGINE_ROTATE_180: phys.x = lw - 1 - lx; phys.y = lh - 1 - ly; break;
            case ESP_DISPLAY_PRESENT_ENGINE_ROTATE_270: phys.x = ly; phys.y = lw - 1 - lx; break;
            default: phys.x = lx; phys.y = ly; break;
            }
            const uint16_t *row = (const uint16_t *)((const uint8_t *)surface->pixels +
                                                     phys.y * surface->layout.stride_bytes);
            esp_display_present_rect_t box = {7 + frame * 13 % (lw - 60), 65 + frame * 7 % (lh - 150), 37, 59};
            uint16_t expected = colors[lx * 8 / lw];
            if (lx >= box.x && lx < box.x + box.width && ly >= box.y && ly < box.y + box.height) {
                expected = 0xffff;
            } else if (lx < 3 || ly < 3 || lx >= lw - 3 || ly >= lh - 3) {
                expected = 0xffff;
            } else if (ly >= 12 && ly < 36 && lx >= 12 && lx < 12 + CASE_COUNT * 32) {
                expected = (lx - 12) / 32 <= test && (lx - 12) % 32 < 24 ? 0xffff : 0;
            }
            if (row[phys.x] != expected) {
                ESP_LOGE(TAG, "ROT_PIXEL mode=%s rot=%u frame=%u lx=%u ly=%u px=%u py=%u actual=%04x expected=%04x",
                         cases[test].name, (unsigned)rot, frame, lx, ly,
                         (unsigned)phys.x, (unsigned)phys.y, row[phys.x], expected);
                check(ESP_FAIL, "rotated pixel compare");
            }
        }
    }
}

static void render_region_dim(esp_display_present_engine_t *p, esp_display_present_render_contract_t contract,
                              uint32_t lw, uint32_t lh, esp_display_present_rect_t area, unsigned frame, unsigned test)
{
    for (unsigned offset = 0; offset < area.height;) {
        esp_display_present_lease_t lease;
        check(esp_display_present_engine_acquire_buffer(p, &lease), "acquire");
        unsigned rows = area.height - offset;
        if (contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION && rows > lease.surface.layout.height) {
            rows = lease.surface.layout.height;
        }
        size_t stride = contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION ? (area.width + (area.width < lw ? 3 : 0)) * 2
                        : lease.surface.layout.stride_bytes;
        esp_display_present_rect_t box = {7 + frame * 13 % (lw - 60), 65 + frame * 7 % (lh - 150), 37, 59};
        for (unsigned y = 0; y < rows; ++y) {
            unsigned dy = area.y + offset + y;
            uint16_t *row = (uint16_t *)((uint8_t *)lease.surface.pixels +
                                         (contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION ? y : dy) * stride);
            for (unsigned x = 0; x < area.width; ++x) {
                unsigned lx = area.x + x;
                uint16_t c = colors[lx * 8 / lw];
                if (lx >= box.x && lx < box.x + box.width && dy >= box.y && dy < box.y + box.height) {
                    c = 0xffff;
                } else if (lx < 3 || dy < 3 || lx >= lw - 3 || dy >= lh - 3) {
                    c = 0xffff;
                } else if (dy >= 12 && dy < 36 && lx >= 12 && lx < 12 + CASE_COUNT * 32) {
                    c = (lx - 12) / 32 <= test && (lx - 12) % 32 < 24 ? 0xffff : 0;
                }
                row[contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION ? x : lx] = c;
            }
        }
        esp_display_present_engine_area_t submit = {area.x, area.y + offset, area.x + area.width - 1, area.y + offset + rows - 1};
        check(esp_display_present_engine_submit_buffer(p, &lease, &submit, stride), "submit region");
        offset += rows;
    }
}

static void run_combo_case(unsigned cycle, unsigned test, bool async_copy, unsigned accel_kind)
{
    const char *exec_name = !async_copy ? "sync-cpu" : accel_kind == 0 ? "async-cpu" : accel_kind == 1 ? "async-ppa" : "async-dma2d";
    ESP_LOGI(TAG, "COMBO_BEGIN cycle=%u mode=%s exec=%s", cycle, cases[test].name, exec_name);
    esp_lcd_panel_handle_t panel;
    check(board_panel_create(&panel, cases[test].buffers), "board panel create");
    esp_display_present_lcd_fb_scanout_t *adapter;
    esp_display_present_lcd_fb_scanout_config_t scan = {.panel = panel, .kind = SCANOUT_KIND,
                                                        .width = WIDTH, .height = HEIGHT, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .buffer_count = cases[test].buffers,
                                                        .bounce = SCANOUT_KIND == ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_RGB, .delete_panel = true
                                                       };
    check(esp_display_present_lcd_fb_scanout_create(&scan, &adapter), "scanout create");
    void *buffers[3];
    size_t count;
    check(esp_display_present_lcd_fb_scanout_get_buffers(adapter, buffers, 3, &count), "get buffers");
    tap_t tap = {0};
    tap.known_count = count;
    memcpy(tap.known, buffers, count * sizeof(*buffers));
    check(esp_display_present_lcd_fb_scanout_get_interface(adapter, &tap.real), "interface");
    esp_display_present_executor_t *cpu = NULL, *hw = NULL;
    esp_display_present_executor_dispatch_t dispatch = {0};
    if (async_copy) {
        check(esp_display_present_executor_cpu_create(&cpu), "cpu executor");
#ifdef PRESENT_BOARD_EXECUTOR_CHECK
        if (accel_kind == 1) {
            check(esp_display_present_executor_ppa_create(&hw), "ppa executor");
        } else if (accel_kind == 2) {
            check(esp_display_present_executor_dma2d_create(&hw), "dma2d executor");
        }
#endif
        dispatch.primary = hw ? hw : cpu;
        dispatch.fallback = hw ? cpu : NULL;
    }
    esp_display_present_engine_config_t cfg = {.backend_kind = ESP_DISPLAY_PRESENT_ENGINE_FB, .mode = cases[test].mode,
                                               .timeout_ms = 2000, .background_service = async_copy,
                                               .backend = {.width = WIDTH, .height = HEIGHT, .tile_rows = 16,
                                                           .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .buffer_count = count, .max_frames = 3,
                                                           .async_copy = async_copy,
                                                           .ext_bufs = buffers, .ext_bytes = WIDTH * HEIGHT * 2,
                                                           .scan_buf = buffers[0], .panel = {&tap, tap_attach, tap_submit, tap_quiesce}
                                                          }
                                              };
    esp_display_present_plan_config_t planned = {.presenter = cfg,
                                                 .executor = async_copy ? &dispatch : NULL,
                                                 .resources = {.tile_count = cases[test].contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION ? 2 : 0,
                                                               .image_memory = ESP_DISPLAY_PRESENT_MEMORY_EXTERNAL,
                                                               .logical_memory = ESP_DISPLAY_PRESENT_MEMORY_EXTERNAL
                                                              }
                                                };
    check(esp_display_present_lcd_fb_scanout_get_output_caps(adapter, &planned.output), "output caps");
    esp_display_present_resolved_plan_t resolved;
    check(esp_display_present_plan_resolve(&planned, &resolved), "resolve combo plan");
    esp_display_present_engine_t *presenter;
    check(esp_display_present_engine_create_planned(&planned, &presenter), "create combo presenter");
    const esp_display_present_engine_rotation_t rots[] = {ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0, ESP_DISPLAY_PRESENT_ENGINE_ROTATE_90, ESP_DISPLAY_PRESENT_ENGINE_ROTATE_180, ESP_DISPLAY_PRESENT_ENGINE_ROTATE_270, ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0};
    uint64_t max_wait = 0, total_wait = 0;
    unsigned total_frames = 0;
    for (size_t r = 0; r < sizeof(rots) / sizeof(rots[0]); ++r) {
        esp_display_present_engine_rotation_t rot = rots[r];
        check(esp_display_present_engine_set_rotation(presenter, rot, 2000), "set_rotation");
        esp_display_present_backend_caps_t caps;
        check(esp_display_present_engine_get_caps(presenter, &caps), "get_caps");
        uint32_t lw = caps.draw_layout.width, lh = caps.draw_layout.height;
        for (unsigned f = 0; f < 2; ++f) {
            bool full = f == 0 || cases[test].contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL;
            esp_display_present_rect_t prev = {7 + (f ? f - 1 : 0) * 13 % (lw - 60), 65 + (f ? f - 1 : 0) * 7 % (lh - 150), 37, 59};
            esp_display_present_rect_t curr = {7 + f * 13 % (lw - 60), 65 + f * 7 % (lh - 150), 37, 59};
            esp_display_present_rect_t dirty[2] = {prev, curr};
            esp_display_present_frame_request_t req = {.full_redraw = full, .damage = full ? NULL : dirty, .damage_count = full ? 0 : 2};
            check(esp_display_present_engine_begin_next_frame(presenter, &req), "combo begin");
            if (full) {
                render_region_dim(presenter, cases[test].contract, lw, lh, (esp_display_present_rect_t) {
                    0, 0, lw, lh
                }, f, test);
            } else {
                render_region_dim(presenter, cases[test].contract, lw, lh, dirty[0], f, test);
                render_region_dim(presenter, cases[test].contract, lw, lh, dirty[1], f, test);
            }
            esp_display_present_engine_receipt_t receipt;
            int64_t start = esp_timer_get_time();
            check(esp_display_present_engine_commit_with_receipt(presenter, NULL, &receipt), "combo commit");
            esp_display_present_frame_status_t status;
            check(esp_display_present_engine_wait_frame(presenter, receipt, ESP_DISPLAY_PRESENT_COMPLETION_PRESENT_CONFIRMED, 2000, &status), "combo wait");
            check(status.result, "combo result");
            uint64_t elapsed = esp_timer_get_time() - start;
            total_wait += elapsed;
            if (elapsed > max_wait) {
                max_wait = elapsed;
            }
            verify_rotated(&tap.submitted, lw, lh, rot, f, test);
            check(esp_display_present_engine_release_receipt(presenter, receipt), "combo release");
            check(esp_display_present_engine_service(presenter), "combo service");
            total_frames++;
        }
        ESP_LOGI(TAG, "ROT_PASS mode=%s exec=%s rotation=%u pixels=ALL", cases[test].name, exec_name, (unsigned)rot * 90);
    }
    ESP_LOGI(TAG, "POOL_USED mode=%s mask=0x%x count=%u", cases[test].name, tap.used_mask, (unsigned)count);
    ESP_LOGI(TAG, "COMBO_PASS cycle=%u mode=%s exec=%s frames=%u rots=5 pixels=ALL avg_wait_us=%" PRIu64 " max_wait_us=%" PRIu64 " min_int=%u min_psram=%u",
             cycle, cases[test].name, exec_name, total_frames, total_wait / total_frames, max_wait,
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
    check(esp_display_present_engine_stop(presenter, 2000), "combo stop");
    check(esp_display_present_engine_destroy(presenter), "combo destroy");
    if (hw) {
        check(esp_display_present_executor_destroy(hw), "hw executor destroy");
    }
    if (cpu) {
        check(esp_display_present_executor_destroy(cpu), "cpu executor destroy");
    }
    check(esp_display_present_lcd_fb_scanout_destroy(adapter), "combo adapter destroy");
    check(board_resources_destroy(), "combo board destroy");
    check(heap_caps_check_integrity_all(true) ? ESP_OK : ESP_FAIL, "combo heap integrity");
    ESP_LOGI(TAG, "COMBO_CLEAN cycle=%u mode=%s exec=%s internal=%u psram=%u",
             cycle, cases[test].name, exec_name,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

static void run_fb_handoff_profile(unsigned cycle, unsigned test)
{
    esp_lcd_panel_handle_t panel;
    check(board_panel_create(&panel, cases[test].buffers), "handoff panel create");
    esp_display_present_lcd_fb_scanout_t *adapter;
    esp_display_present_lcd_fb_scanout_config_t scan = {.panel = panel, .kind = SCANOUT_KIND,
                                                        .width = WIDTH, .height = HEIGHT, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .buffer_count = cases[test].buffers,
                                                        .bounce = SCANOUT_KIND == ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_RGB, .delete_panel = true
                                                       };
    check(esp_display_present_lcd_fb_scanout_create(&scan, &adapter), "handoff scanout create");
    void *buffers[3] = {0};
    size_t count = 0;
    check(esp_display_present_lcd_fb_scanout_get_buffers(adapter, buffers, 3, &count), "handoff get buffers");
    tap_t tap = {.known = {buffers[0], buffers[1], buffers[2]}, .known_count = count, .used_mask = 1u /* buffers[0] is initial scanout */};
    check(esp_display_present_lcd_fb_scanout_get_interface(adapter, &tap.real), "handoff interface");

    /* For FULL modes, set draw_order = SWAPPED and panel_order = NATIVE so FB_SLOT_CONVERT runs through async_copy on every frame! */
    esp_display_present_engine_config_t cfg = {.backend_kind = ESP_DISPLAY_PRESENT_ENGINE_FB, .mode = cases[test].mode,
                                               .timeout_ms = 2000, .background_service = true,
                                               .backend = {.width = WIDTH, .height = HEIGHT, .tile_rows = 16,
                                                           .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565,
                                                           .draw_order = cases[test].contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL ? ESP_DISPLAY_PRESENT_ORDER_SWAPPED : ESP_DISPLAY_PRESENT_ORDER_NATIVE,
                                                           .panel_order = ESP_DISPLAY_PRESENT_ORDER_NATIVE,
                                                           .buffer_count = count, .max_frames = 3,
                                                           .async_copy = true,
                                                           .ext_bufs = buffers, .ext_bytes = WIDTH * HEIGHT * 2,
                                                           .scan_buf = buffers[0], .panel = {&tap, tap_attach, tap_submit, tap_quiesce}
                                                          }
                                              };
    esp_display_present_plan_config_t planned = {.presenter = cfg,
                                                 .resources = {.tile_count = cases[test].contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION ? 2 : 0,
                                                               .image_memory = ESP_DISPLAY_PRESENT_MEMORY_EXTERNAL,
                                                               .logical_memory = ESP_DISPLAY_PRESENT_MEMORY_EXTERNAL
                                                              }
                                                };
    check(esp_display_present_lcd_fb_scanout_get_output_caps(adapter, &planned.output), "handoff output caps");
    esp_display_present_engine_t *p;
    check(esp_display_present_engine_create_planned(&planned, &p), "handoff create planned");

    uint64_t sum_begin_wait = 0, max_begin_wait = 0;
    uint64_t sum_acq_wait = 0, max_acq_wait = 0;
    uint64_t sum_draw_sub = 0, max_draw_sub = 0;
    uint64_t sum_commit = 0, max_commit = 0;
    uint64_t sum_vsync_wait = 0, max_vsync_wait = 0;

    /* Pipelined frame queue: do NOT wait after every commit; keep up to (count - 1) frames in-flight
     * while building the next frame so 3-buffer modes simultaneously occupy SCANNING + PENDING + BUILDING! */
    esp_display_present_engine_receipt_t inflight[3] = {0};
    size_t head = 0, tail = 0, queued = 0;
    const unsigned total_frames = 12;

    for (unsigned f = 0; f < total_frames; ++f) {
        /* Only retire the oldest in-flight receipt when the pipeline is full. */
        if (queued >= count - 1) {
            esp_display_present_frame_status_t st;
            uint64_t tv0 = esp_timer_get_time();
            check(esp_display_present_engine_wait_frame(p, inflight[head], ESP_DISPLAY_PRESENT_COMPLETION_PRESENT_CONFIRMED, 2000, &st), "handoff wait oldest");
            uint64_t tv1 = esp_timer_get_time();
            check(st.result, "handoff oldest result");
            check(esp_display_present_engine_release_receipt(p, inflight[head]), "handoff release oldest");
            head = (head + 1) % 3;
            queued--;
            uint64_t vsync_w = tv1 - tv0;
            sum_vsync_wait += vsync_w;
            if (vsync_w > max_vsync_wait) {
                max_vsync_wait = vsync_w;
            }
        }

        bool full = f == 0 || cases[test].contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL;
        esp_display_present_rect_t dirty[2] = {moving(f ? f - 1 : 0), moving(f)};
        esp_display_present_frame_request_t req = {.full_redraw = full, .damage = full ? NULL : dirty, .damage_count = full ? 0 : 2};

        uint64_t tb0 = esp_timer_get_time();
        check(esp_display_present_engine_begin_next_frame(p, &req), "handoff begin");
        uint64_t tb1 = esp_timer_get_time();
        uint64_t bw = tb1 - tb0;
        sum_begin_wait += bw;
        if (bw > max_begin_wait) {
            max_begin_wait = bw;
        }

        uint64_t f_acq = 0, f_draw = 0;
        unsigned n_rects = full ? 1 : 2;
        esp_display_present_rect_t full_rect = {0, 0, WIDTH, HEIGHT};
        const esp_display_present_rect_t *rects = full ? &full_rect : dirty;
        for (unsigned r_idx = 0; r_idx < n_rects; ++r_idx) {
            esp_display_present_rect_t area = rects[r_idx];
            for (unsigned offset = 0; offset < area.height;) {
                esp_display_present_lease_t lease;
                uint64_t ta0 = esp_timer_get_time();
                check(esp_display_present_engine_acquire_buffer(p, &lease), "handoff acquire");
                uint64_t ta1 = esp_timer_get_time();
                f_acq += (ta1 - ta0);
                unsigned rows = area.height - offset;
                if (cases[test].contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION && rows > 16) {
                    rows = 16;
                }
                size_t stride = cases[test].contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION ? area.width * 2 : lease.surface.layout.stride_bytes;
                for (unsigned y = 0; y < rows; ++y) {
                    unsigned dy = area.y + offset + y;
                    uint16_t *row = (uint16_t *)((uint8_t *)lease.surface.pixels +
                                                 (cases[test].contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION ? y : dy) * stride);
                    for (unsigned x = 0; x < area.width; ++x) {
                        row[cases[test].contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION ? x : area.x + x] = reference(area.x + x, dy, f, test);
                    }
                }
                esp_display_present_engine_area_t sub = {area.x, area.y + offset, area.x + area.width - 1, area.y + offset + rows - 1};
                check(esp_display_present_engine_submit_buffer(p, &lease, &sub, stride), "handoff submit");
                uint64_t ta2 = esp_timer_get_time();
                f_draw += (ta2 - ta1);
                offset += rows;
            }
        }
        sum_acq_wait += f_acq;
        if (f_acq > max_acq_wait) {
            max_acq_wait = f_acq;
        }
        sum_draw_sub += f_draw;
        if (f_draw > max_draw_sub) {
            max_draw_sub = f_draw;
        }

        uint64_t tc0 = esp_timer_get_time();
        check(esp_display_present_engine_commit_with_receipt(p, NULL, &inflight[tail]), "handoff commit");
        uint64_t tc1 = esp_timer_get_time();
        uint64_t cw = tc1 - tc0;
        sum_commit += cw;
        if (cw > max_commit) {
            max_commit = cw;
        }
        tail = (tail + 1) % 3;
        queued++;
    }

    /* Drain remaining in-flight frames. */
    while (queued > 0) {
        esp_display_present_frame_status_t st;
        uint64_t tv0 = esp_timer_get_time();
        check(esp_display_present_engine_wait_frame(p, inflight[head], ESP_DISPLAY_PRESENT_COMPLETION_PRESENT_CONFIRMED, 2000, &st), "handoff drain");
        uint64_t tv1 = esp_timer_get_time();
        check(st.result, "handoff drain result");
        check(esp_display_present_engine_release_receipt(p, inflight[head]), "handoff drain release");
        head = (head + 1) % 3;
        queued--;
        uint64_t vsync_w = tv1 - tv0;
        sum_vsync_wait += vsync_w;
        if (vsync_w > max_vsync_wait) {
            max_vsync_wait = vsync_w;
        }
    }

    /* Exercise cancel_frame while async copy is active, then submit one more frame to verify clean handoff. */
    esp_display_present_rect_t cdirty = moving(total_frames);
    esp_display_present_frame_request_t creq = {.full_redraw = cases[test].contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL,
                                                .damage = cases[test].contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL ? NULL :&cdirty,
                                                .damage_count = cases[test].contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL ? 0 : 1
                                               };
    check(esp_display_present_engine_begin_next_frame(p, &creq), "handoff cancel begin");
    {
        esp_display_present_lease_t l;
        check(esp_display_present_engine_acquire_buffer(p, &l), "handoff cancel acquire");
        memset(l.surface.pixels, 0xaa, 16 * WIDTH * 2);
        esp_display_present_engine_area_t sub = {0, 0, WIDTH - 1, cases[test].contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION ? 15 : HEIGHT - 1};
        check(esp_display_present_engine_submit_buffer(p, &l, &sub, WIDTH * 2), "handoff cancel submit");
    }
    uint64_t tcan0 = esp_timer_get_time();
    check(esp_display_present_engine_cancel_frame(p), "handoff cancel_frame");
    uint64_t cancel_drain_us = esp_timer_get_time() - tcan0;

    unsigned expected_mask = (1u << count) - 1u;
    check(tap.used_mask == expected_mask ? ESP_OK : ESP_FAIL, "all physical FBs rotated through scanout");

    ESP_LOGI(TAG, "FB_HANDOFF_PASS cycle=%u mode=%s buffers=%u used_mask=0x%x begin_wait_us=[avg=%"PRIu64", max=%"PRIu64"] acq_wait_us=[avg=%"PRIu64", max=%"PRIu64"] draw_sub_us=[avg=%"PRIu64", max=%"PRIu64"] commit_us=[avg=%"PRIu64", max=%"PRIu64"] vsync_wait_us=[avg=%"PRIu64", max=%"PRIu64"] cancel_drain_us=%"PRIu64,
             cycle, cases[test].name, (unsigned)count, tap.used_mask,
             sum_begin_wait / total_frames, max_begin_wait,
             sum_acq_wait / total_frames, max_acq_wait,
             sum_draw_sub / total_frames, max_draw_sub,
             sum_commit / total_frames, max_commit,
             sum_vsync_wait / total_frames, max_vsync_wait,
             cancel_drain_us);

    check(esp_display_present_engine_stop(p, 2000), "handoff stop");
    check(esp_display_present_engine_destroy(p), "handoff destroy");
    check(esp_display_present_lcd_fb_scanout_destroy(adapter), "handoff adapter destroy");
    check(board_resources_destroy(), "handoff board destroy");
    check(heap_caps_check_integrity_all(true) ? ESP_OK : ESP_FAIL, "handoff heap integrity");
}

static void run_v1_vs_v2_benchmark(unsigned cycle)
{
    static const struct {
        const char *name;
        esp_display_present_mode_t v1_mode;
        esp_display_present_present_mode_t v2_mode;
        size_t buffers;
        esp_display_present_render_contract_t contract;
        bool rotated;
    } bench_cases[] = {
        {"DOUBLE_FULL_0", ESP_DISPLAY_PRESENT_MODE_DOUBLE_FULL, ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_FULL, 2, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL, false},
        {"TRIPLE_FULL_0", ESP_DISPLAY_PRESENT_MODE_TRIPLE_FULL, ESP_DISPLAY_PRESENT_ENGINE_MODE_TRIPLE_FULL, 3, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL, false},
        {"DOUBLE_DIRECT_0", ESP_DISPLAY_PRESENT_MODE_DOUBLE_DIRECT, ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_DIRECT, 2, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT, false},
        {"DOUBLE_PARTIAL_0", ESP_DISPLAY_PRESENT_MODE_DOUBLE_PARTIAL, ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_PARTIAL, 2, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION, false},
        {"TRIPLE_PARTIAL_0", ESP_DISPLAY_PRESENT_MODE_TRIPLE_PARTIAL, ESP_DISPLAY_PRESENT_ENGINE_MODE_TRIPLE_PARTIAL, 3, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION, false},
        {"DOUBLE_FULL_90", ESP_DISPLAY_PRESENT_MODE_DOUBLE_FULL, ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_FULL, 2, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL, true},
        {"TRIPLE_PARTIAL_90", ESP_DISPLAY_PRESENT_MODE_TRIPLE_PARTIAL, ESP_DISPLAY_PRESENT_ENGINE_MODE_TRIPLE_PARTIAL, 3, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION, true},
    };
    const unsigned warmup_frames = 4;
    const unsigned bench_frames = 20;
    const unsigned total_run = warmup_frames + bench_frames;
    uint64_t v1_us_res[7] = {0}, v2_us_res[7] = {0};
    size_t v1_int_res[7] = {0}, v2_int_res[7] = {0};

    for (size_t i = 0; i < sizeof(bench_cases) / sizeof(bench_cases[0]); ++i) {
        uint32_t lw = bench_cases[i].rotated ? HEIGHT : WIDTH;
        uint32_t lh = bench_cases[i].rotated ? WIDTH : HEIGHT;

        /* --- 1. Run V1 --- */
        esp_lcd_panel_handle_t panel_v1;
        check(board_panel_create(&panel_v1, bench_cases[i].buffers), "bench v1 panel create");
        esp_display_presenter_config_t v1_cfg = {
            .width = lw,
            .height = lh,
            .pixel_format = ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565,
            .max_damage_areas = 8,
            .transfer_timeout_ms = 2000,
            .pipeline_acquire_timeout_ms = 2000,
            .target = {
                .hw = {
                    .panel = panel_v1,
                    .panel_type = SCANOUT_KIND == ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_DSI ? ESP_DISPLAY_PRESENT_PANEL_MIPI_DSI : ESP_DISPLAY_PRESENT_PANEL_RGB,
                    .input_pixel_format = ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565,
                    .rotation = bench_cases[i].rotated ? ESP_DISPLAY_PRESENT_ROTATE_90 : ESP_DISPLAY_PRESENT_ROTATE_0,
                    .te_sync = ESP_DISPLAY_PRESENT_TE_SYNC_DISABLED(),
                },
                .fb = {
                    .mode = bench_cases[i].v1_mode,
                },
                .drawbuf = {
                    .lines = 16,
                    .buffers = 2,
                    .in_psram = false,
                },
            },
        };
        esp_display_presenter_t *v1 = NULL;
        check(esp_display_presenter_create(&v1_cfg, &v1), "bench v1 create");
        size_t v1_int_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t v1_psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

        uint64_t v1_t0 = 0;
        for (unsigned f = 0; f < total_run; ++f) {
            if (f == warmup_frames) {
                v1_t0 = esp_timer_get_time();
            }
            bool full = (f == 0) || (bench_cases[i].contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL);
            int bx0 = 8 + ((f ? f - 1 : 0) * 16) % (lw - 80);
            int by0 = 8 + ((f ? f - 1 : 0) * 12) % (lh - 80);
            int bx1 = 8 + (f * 16) % (lw - 80);
            int by1 = 8 + (f * 12) % (lh - 80);
            esp_display_present_area_t dirty_v1[2] = {
                {bx0, by0, bx0 + 63, by0 + 63},
                {bx1, by1, bx1 + 63, by1 + 63},
            };
            esp_display_present_surface_request_t req_v1 = {
                .dirty_areas = full ? NULL : dirty_v1,
                .dirty_area_count = full ? 0 : 2,
            };
            esp_display_present_area_t render_areas[8];
            size_t render_count = 0;
            bool full_cov = false;
            check(esp_display_presenter_begin_next_frame(v1, &req_v1, render_areas, 8, &render_count, &full_cov), "v1 begin");
            if (bench_cases[i].contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION) {
                size_t n_areas = full ? 1 : 2;
                esp_display_present_area_t full_a = {0, 0, (int)lw - 1, (int)lh - 1};
                const esp_display_present_area_t *alist = full ? &full_a : dirty_v1;
                for (size_t a = 0; a < n_areas; ++a) {
                    int h_total = alist[a].y2 - alist[a].y1 + 1;
                    int w_total = alist[a].x2 - alist[a].x1 + 1;
                    for (int off = 0; off < h_total; off += 16) {
                        int rows = (h_total - off > 16) ? 16 : (h_total - off);
                        esp_display_presenter_buffer_t buf;
                        check(esp_display_presenter_acquire_buffer(v1, &buf), "v1 acquire tile");
                        memset(buf.surface.pixels, (int)(f + a), (size_t)w_total * rows * 2);
                        esp_display_present_area_t sub_a = {alist[a].x1, alist[a].y1 + off, alist[a].x2, alist[a].y1 + off + rows - 1};
                        check(esp_display_presenter_submit_buffer(v1, &buf, &sub_a, (size_t)w_total * 2), "v1 submit tile");
                    }
                }
                esp_display_presenter_submit_t sub = {.coverage = ESP_DISPLAY_PRESENT_COVERAGE_FULL};
                check(esp_display_presenter_commit_frame(v1, &sub), "v1 commit part");
            } else {
                esp_display_presenter_buffer_t buf;
                check(esp_display_presenter_acquire_buffer(v1, &buf), "v1 acquire surf");
                if (full) {
                    memset(buf.surface.pixels, (int)f, (size_t)lw * lh * 2);
                } else {
                    for (size_t a = 0; a < 2; ++a) {
                        for (int y = dirty_v1[a].y1; y <= dirty_v1[a].y2; ++y) {
                            memset((uint8_t *)buf.surface.pixels + (size_t)y * buf.surface.stride_bytes + (size_t)dirty_v1[a].x1 * 2,
                                   (int)(f + a), 64 * 2);
                        }
                    }
                }
                esp_display_present_area_t full_a = {0, 0, (int)lw - 1, (int)lh - 1};
                check(esp_display_presenter_submit_buffer(v1, &buf, &full_a, buf.surface.stride_bytes), "v1 submit surf");
                esp_display_presenter_submit_t sub = {
                    .coverage = full ? ESP_DISPLAY_PRESENT_COVERAGE_FULL : ESP_DISPLAY_PRESENT_COVERAGE_AREAS,
                    .areas = full ? NULL : dirty_v1,
                    .area_count = full ? 0 : 2,
                };
                check(esp_display_presenter_commit_frame(v1, &sub), "v1 commit surf");
            }
        }
        uint64_t v1_elapsed_us = esp_timer_get_time() - v1_t0;
        check(esp_display_presenter_quiesce(v1, 5000), "v1 quiesce");
        check(esp_display_presenter_stop(v1), "v1 stop");
        check(esp_display_presenter_delete(v1), "v1 delete");
        check(esp_lcd_panel_del(panel_v1), "v1 panel del");
        check(board_resources_destroy(), "v1 board destroy");

        /* --- 2. Run V2 with identical workload --- */
        esp_lcd_panel_handle_t panel_v2;
        check(board_panel_create(&panel_v2, bench_cases[i].buffers), "bench v2 panel create");
        esp_display_present_lcd_fb_scanout_t *adapter;
        esp_display_present_lcd_fb_scanout_config_t scan = {.panel = panel_v2, .kind = SCANOUT_KIND,
                                                            .width = WIDTH, .height = HEIGHT, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .buffer_count = bench_cases[i].buffers,
                                                            .bounce = SCANOUT_KIND == ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_RGB,
                                                            .delete_panel = true
                                                           };
        check(esp_display_present_lcd_fb_scanout_create(&scan, &adapter), "bench v2 scanout create");
        void *buffers[3] = {0};
        size_t count = 0;
        check(esp_display_present_lcd_fb_scanout_get_buffers(adapter, buffers, 3, &count), "bench v2 get buffers");
        esp_display_present_panel_adapter_t iface;
        check(esp_display_present_lcd_fb_scanout_get_interface(adapter, &iface), "bench v2 iface");
        esp_display_present_executor_t *cpu_exec = NULL, *ppa_exec = NULL;
        esp_display_present_executor_dispatch_t disp = {0};
#ifdef PRESENT_BOARD_EXECUTOR_CHECK
        bool use_async = (bench_cases[i].contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION) || bench_cases[i].rotated;
#else
        bool use_async = (bench_cases[i].contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION);
#endif
        if (use_async) {
            check(esp_display_present_executor_cpu_create(&cpu_exec), "v2 cpu exec");
#ifdef PRESENT_BOARD_EXECUTOR_CHECK
            check(esp_display_present_executor_ppa_create(&ppa_exec), "v2 ppa exec");
#endif
            disp.primary = ppa_exec ? ppa_exec : cpu_exec;
            disp.fallback = ppa_exec ? cpu_exec : NULL;
        }
        esp_display_present_engine_config_t v2_cfg = {
            .backend_kind = ESP_DISPLAY_PRESENT_ENGINE_FB, .mode = bench_cases[i].v2_mode,
            .timeout_ms = 2000, .background_service = use_async,
            .backend = {
                .width = lw, .height = lh, .tile_rows = 16,
                .rotation = bench_cases[i].rotated ? ESP_DISPLAY_PRESENT_ENGINE_ROTATE_90 : ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0,
                .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .buffer_count = count, .max_frames = 3,
                .async_copy = use_async,
                .ext_bufs = buffers, .ext_bytes = WIDTH * HEIGHT * 2,
                .scan_buf = buffers[0], .panel = iface,
            },
        };
        esp_display_present_plan_config_t planned = {
            .presenter = v2_cfg,
            .executor = use_async ? &disp : NULL,
            .resources = {
                .tile_count = bench_cases[i].contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION ? 2 : 0,
                .image_memory = ESP_DISPLAY_PRESENT_MEMORY_EXTERNAL,
                .logical_memory = ESP_DISPLAY_PRESENT_MEMORY_EXTERNAL,
            },
        };
        check(esp_display_present_lcd_fb_scanout_get_output_caps(adapter, &planned.output), "v2 caps");
        esp_display_present_engine_t *v2 = NULL;
        check(esp_display_present_engine_create_planned(&planned, &v2), "bench v2 create");
        size_t v2_int_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t v2_psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

        uint64_t v2_t0 = 0;
        for (unsigned f = 0; f < total_run; ++f) {
            if (f == warmup_frames) {
                v2_t0 = esp_timer_get_time();
            }
            bool full = (f == 0) || (bench_cases[i].contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL);
            uint32_t bx0 = 8 + ((f ? f - 1 : 0) * 16) % (lw - 80);
            uint32_t by0 = 8 + ((f ? f - 1 : 0) * 12) % (lh - 80);
            uint32_t bx1 = 8 + (f * 16) % (lw - 80);
            uint32_t by1 = 8 + (f * 12) % (lh - 80);
            esp_display_present_rect_t dirty_v2[2] = {{bx0, by0, 64, 64}, {bx1, by1, 64, 64}};
            esp_display_present_frame_request_t req_v2 = {.full_redraw = full, .damage = full ? NULL : dirty_v2, .damage_count = full ? 0 : 2};
            check(esp_display_present_engine_begin_next_frame(v2, &req_v2), "v2 begin");
            if (bench_cases[i].contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION) {
                size_t n_areas = full ? 1 : 2;
                esp_display_present_rect_t full_r = {0, 0, lw, lh};
                const esp_display_present_rect_t *rlist = full ? &full_r : dirty_v2;
                for (size_t a = 0; a < n_areas; ++a) {
                    for (uint32_t off = 0; off < rlist[a].height; off += 16) {
                        uint32_t rows = (rlist[a].height - off > 16) ? 16 : (rlist[a].height - off);
                        esp_display_present_lease_t lease;
                        check(esp_display_present_engine_acquire_buffer(v2, &lease), "v2 acquire tile");
                        memset(lease.surface.pixels, (int)(f + a), (size_t)rlist[a].width * rows * 2);
                        esp_display_present_engine_area_t sub_a = {(int32_t)rlist[a].x, (int32_t)(rlist[a].y + off),
                                                                   (int32_t)(rlist[a].x + rlist[a].width - 1), (int32_t)(rlist[a].y + off + rows - 1)
                                                                  };
                        check(esp_display_present_engine_submit_buffer(v2, &lease, &sub_a, (size_t)rlist[a].width * 2), "v2 submit tile");
                    }
                }
            } else {
                esp_display_present_lease_t lease;
                check(esp_display_present_engine_acquire_buffer(v2, &lease), "v2 acquire surf");
                if (full) {
                    memset(lease.surface.pixels, (int)f, (size_t)lw * lh * 2);
                    esp_display_present_engine_area_t full_a = {0, 0, (int32_t)lw - 1, (int32_t)lh - 1};
                    check(esp_display_present_engine_submit_buffer(v2, &lease, &full_a, lease.surface.layout.stride_bytes), "v2 submit full");
                } else {
                    for (size_t a = 0; a < 2; ++a) {
                        for (uint32_t y = 0; y < 64; ++y) {
                            memset((uint8_t *)lease.surface.pixels + (size_t)(dirty_v2[a].y + y) * lease.surface.layout.stride_bytes + (size_t)dirty_v2[a].x * 2,
                                   (int)(f + a), 64 * 2);
                        }
                    }
                    esp_display_present_engine_area_t a0 = {(int32_t)dirty_v2[0].x, (int32_t)dirty_v2[0].y,
                                                            (int32_t)(dirty_v2[0].x + 63), (int32_t)(dirty_v2[0].y + 63)
                                                           };
                    check(esp_display_present_engine_submit_buffer(v2, &lease, &a0, lease.surface.layout.stride_bytes), "v2 submit d0");
                    check(esp_display_present_engine_acquire_buffer(v2, &lease), "v2 acquire surf 2");
                    esp_display_present_engine_area_t a1 = {(int32_t)dirty_v2[1].x, (int32_t)dirty_v2[1].y,
                                                            (int32_t)(dirty_v2[1].x + 63), (int32_t)(dirty_v2[1].y + 63)
                                                           };
                    check(esp_display_present_engine_submit_buffer(v2, &lease, &a1, lease.surface.layout.stride_bytes), "v2 submit d1");
                }
            }
            check(esp_display_present_engine_commit_frame(v2, NULL), "v2 commit");
            if (!use_async) {
                check(esp_display_present_engine_service(v2), "v2 service");
            }
        }
        uint64_t v2_elapsed_us = esp_timer_get_time() - v2_t0;
        check(esp_display_present_engine_drain(v2, 5000), "v2 drain");
        check(esp_display_present_engine_stop(v2, 2000), "v2 stop");
        check(esp_display_present_engine_destroy(v2), "v2 destroy");
        if (ppa_exec) {
            check(esp_display_present_executor_destroy(ppa_exec), "ppa destroy");
        }
        if (cpu_exec) {
            check(esp_display_present_executor_destroy(cpu_exec), "cpu destroy");
        }
        check(esp_display_present_lcd_fb_scanout_destroy(adapter), "v2 scanout destroy");
        check(board_resources_destroy(), "v2 board destroy");

        v1_us_res[i] = v1_elapsed_us / bench_frames;
        v2_us_res[i] = v2_elapsed_us / bench_frames;
        v1_int_res[i] = v1_int_free;
        v2_int_res[i] = v2_int_free;
        uint32_t v1_fps_x10 = (uint32_t)((uint64_t)bench_frames * 10000000ULL / v1_elapsed_us);
        uint32_t v2_fps_x10 = (uint32_t)((uint64_t)bench_frames * 10000000ULL / v2_elapsed_us);
        ESP_LOGI(TAG, "BENCH_V1_V2 cycle=%u case=%s frames=%u v1_us_per_frame=%"PRIu64" v1_fps=%u.%u v2_us_per_frame=%"PRIu64" v2_fps=%u.%u v1_free=(%u,%u) v2_free=(%u,%u)",
                 cycle, bench_cases[i].name, bench_frames,
                 v1_elapsed_us / bench_frames, (unsigned)(v1_fps_x10 / 10), (unsigned)(v1_fps_x10 % 10),
                 v2_elapsed_us / bench_frames, (unsigned)(v2_fps_x10 / 10), (unsigned)(v2_fps_x10 % 10),
                 (unsigned)v1_int_free, (unsigned)v1_psram_free,
                 (unsigned)v2_int_free, (unsigned)v2_psram_free);
    }
    ESP_LOGI(TAG, "=== BENCH_SUMMARY cycle=%u ===", cycle);
    for (size_t i = 0; i < sizeof(bench_cases) / sizeof(bench_cases[0]); ++i) {
        uint32_t f1 = (uint32_t)(10000000ULL / (v1_us_res[i] ? v1_us_res[i] : 1));
        uint32_t f2 = (uint32_t)(10000000ULL / (v2_us_res[i] ? v2_us_res[i] : 1));
        ESP_LOGI(TAG, "SUMMARY case=%-18s V1=%"PRIu64"us(%u.%ufps,int=%u) | V2=%"PRIu64"us(%u.%ufps,int=%u)",
                 bench_cases[i].name,
                 v1_us_res[i], (unsigned)(f1 / 10), (unsigned)(f1 % 10), (unsigned)v1_int_res[i],
                 v2_us_res[i], (unsigned)(f2 / 10), (unsigned)(f2 % 10), (unsigned)v2_int_res[i]);
    }
}

#ifdef PRESENT_BOARD_EXECUTOR_CHECK
void present_board_test_executors(void);
#endif
void app_main(void)
{
#ifdef PRESENT_NATIVE_ROTATION_MATRIX
    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP_LOGI(TAG, "NATIVE_MATRIX_START board=%s rotations=0,90,180,270,0 full_and_dirty=1", BOARD_NAME);
    for (unsigned test = 0; test < CASE_COUNT; ++test) {
        run_combo_case(0, test, true, 0);
        run_fb_handoff_profile(0, test);
    }
    ESP_LOGI(TAG, "MATRIX_PASS board=%s native_modes=%u rotations=5 full_and_dirty=1", BOARD_NAME, (unsigned)CASE_COUNT);
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#else
#ifdef PRESENT_BOARD_EXECUTOR_CHECK
    present_board_test_executors();
#endif
    ESP_LOGI(TAG, "V2 SUITE2 %s %dx%d RGB565 CPU + ROTATION/ASYNC/ACCEL COMBOS", BOARD_NAME, WIDTH, HEIGHT);
    for (unsigned cycle = 0; ; ++cycle) {
        run_v1_vs_v2_benchmark(cycle);
        for (unsigned test = 0; test < CASE_COUNT; ++test) {
            run_fb_handoff_profile(cycle, test);
        }
        for (unsigned test = 0; test < CASE_COUNT; ++test) {
            run_case(cycle, test);
        }
        const unsigned combo_modes[] = {0, 2, 4}; /* DOUBLE_FULL, DOUBLE_DIRECT, TRIPLE_PARTIAL */
        for (size_t i = 0; i < sizeof(combo_modes) / sizeof(combo_modes[0]); ++i) {
            run_combo_case(cycle, combo_modes[i], true, 0);
#ifdef PRESENT_BOARD_EXECUTOR_CHECK
            run_combo_case(cycle, combo_modes[i], true, 1); /* PPA + display + 4 rotations */
            run_combo_case(cycle, combo_modes[i], true, 2); /* DMA2D + CPU fallback + display */
#endif
        }
        ESP_LOGI(TAG, "SUITE_PASS cycle=%u modes=%u confirmed_frames=%u", cycle, (unsigned)CASE_COUNT, (unsigned)CASE_COUNT * FRAME_COUNT);
    }
#endif
}
