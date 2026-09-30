/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>

#include "esp_attr.h"
#include "esp_display_present.h"
#include "esp_display_present_hw.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_io_interface.h"
#include "soc/soc_caps.h"
#if SOC_LCD_RGB_SUPPORTED
#include "esp_lcd_panel_rgb.h"
#endif
#include "esp_timer.h"
#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"

#define TEST_LCD_H_RES              100
#define TEST_LCD_V_RES              100
#define TEST_FB_W                   7
#define TEST_FB_H                   5
#define TEST_FB_PIXELS              (TEST_FB_W * TEST_FB_H)
#define TEST_MEMORY_LEAK_THRESHOLD  (2000)
#define TEST_QUIESCE_TIMEOUT_MS     (2000)

TEST_CASE("framebuffer requirements validate policy before panel creation",
          "[present][config]")
{
    const struct {
        esp_display_present_panel_t panel;
        esp_display_present_mode_t mode;
        esp_display_present_rotation_t rotation;
        bool te_enabled;
        int te_gpio;
        esp_err_t result;
        uint8_t count;
    } cases[] = {
        {ESP_DISPLAY_PRESENT_PANEL_RGB, ESP_DISPLAY_PRESENT_MODE_NONE, ESP_DISPLAY_PRESENT_ROTATE_0, false, -1, ESP_OK, 1},
        {ESP_DISPLAY_PRESENT_PANEL_RGB, ESP_DISPLAY_PRESENT_MODE_DOUBLE_FULL, ESP_DISPLAY_PRESENT_ROTATE_90, false, -1, ESP_OK, 2},
        {ESP_DISPLAY_PRESENT_PANEL_RGB, ESP_DISPLAY_PRESENT_MODE_TRIPLE_FULL, ESP_DISPLAY_PRESENT_ROTATE_180, false, -1, ESP_OK, 3},
        {ESP_DISPLAY_PRESENT_PANEL_RGB, ESP_DISPLAY_PRESENT_MODE_DOUBLE_DIRECT, ESP_DISPLAY_PRESENT_ROTATE_0, false, -1, ESP_OK, 2},
        {ESP_DISPLAY_PRESENT_PANEL_RGB, ESP_DISPLAY_PRESENT_MODE_DOUBLE_DIRECT, ESP_DISPLAY_PRESENT_ROTATE_180, false, -1, ESP_ERR_NOT_SUPPORTED, 0},
        {ESP_DISPLAY_PRESENT_PANEL_RGB, ESP_DISPLAY_PRESENT_MODE_DOUBLE_PARTIAL, ESP_DISPLAY_PRESENT_ROTATE_270, false, -1, ESP_OK, 2},
        {ESP_DISPLAY_PRESENT_PANEL_RGB, ESP_DISPLAY_PRESENT_MODE_TRIPLE_PARTIAL, ESP_DISPLAY_PRESENT_ROTATE_90, false, -1, ESP_OK, 3},
        {ESP_DISPLAY_PRESENT_PANEL_RGB, ESP_DISPLAY_PRESENT_MODE_AUTO, ESP_DISPLAY_PRESENT_ROTATE_0, false, -1, ESP_OK, 3},
        {ESP_DISPLAY_PRESENT_PANEL_MIPI_DSI, ESP_DISPLAY_PRESENT_MODE_DOUBLE_FULL, ESP_DISPLAY_PRESENT_ROTATE_90, false, -1, ESP_OK, 2},
        {ESP_DISPLAY_PRESENT_PANEL_MIPI_DSI, ESP_DISPLAY_PRESENT_MODE_AUTO, ESP_DISPLAY_PRESENT_ROTATE_0, false, -1, ESP_OK, 3},
        {ESP_DISPLAY_PRESENT_PANEL_MIPI_DSI, ESP_DISPLAY_PRESENT_MODE_TE_SYNC, ESP_DISPLAY_PRESENT_ROTATE_0, true, 4, ESP_ERR_NOT_SUPPORTED, 0},
        {ESP_DISPLAY_PRESENT_PANEL_IO, ESP_DISPLAY_PRESENT_MODE_NONE, ESP_DISPLAY_PRESENT_ROTATE_0, false, -1, ESP_OK, 0},
        {ESP_DISPLAY_PRESENT_PANEL_IO, ESP_DISPLAY_PRESENT_MODE_NONE, ESP_DISPLAY_PRESENT_ROTATE_90, false, -1, ESP_ERR_NOT_SUPPORTED, 0},
        {ESP_DISPLAY_PRESENT_PANEL_IO, ESP_DISPLAY_PRESENT_MODE_TE_SYNC, ESP_DISPLAY_PRESENT_ROTATE_90, true, 0, ESP_OK, 0},
        {ESP_DISPLAY_PRESENT_PANEL_IO, ESP_DISPLAY_PRESENT_MODE_TE_SYNC, ESP_DISPLAY_PRESENT_ROTATE_0, false, 4, ESP_ERR_NOT_SUPPORTED, 0},
        {ESP_DISPLAY_PRESENT_PANEL_IO, ESP_DISPLAY_PRESENT_MODE_TE_SYNC, ESP_DISPLAY_PRESENT_ROTATE_0, true, -1, ESP_ERR_NOT_SUPPORTED, 0},
        {ESP_DISPLAY_PRESENT_PANEL_IO, ESP_DISPLAY_PRESENT_MODE_AUTO, ESP_DISPLAY_PRESENT_ROTATE_90, true, 4, ESP_OK, 0},
        {ESP_DISPLAY_PRESENT_PANEL_IO, ESP_DISPLAY_PRESENT_MODE_AUTO, ESP_DISPLAY_PRESENT_ROTATE_90, true, -1, ESP_ERR_NOT_SUPPORTED, 0},
        {ESP_DISPLAY_PRESENT_PANEL_IO, ESP_DISPLAY_PRESENT_MODE_DOUBLE_FULL, ESP_DISPLAY_PRESENT_ROTATE_0, true, 4, ESP_ERR_NOT_SUPPORTED, 0},
        {(esp_display_present_panel_t)99, ESP_DISPLAY_PRESENT_MODE_AUTO, ESP_DISPLAY_PRESENT_ROTATE_0, false, -1, ESP_ERR_INVALID_ARG, 0},
        {ESP_DISPLAY_PRESENT_PANEL_RGB, (esp_display_present_mode_t)99, ESP_DISPLAY_PRESENT_ROTATE_0, false, -1, ESP_ERR_INVALID_ARG, 0},
        {ESP_DISPLAY_PRESENT_PANEL_RGB, ESP_DISPLAY_PRESENT_MODE_NONE, (esp_display_present_rotation_t)45, false, -1, ESP_ERR_INVALID_ARG, 0},
        {ESP_DISPLAY_PRESENT_PANEL_AUTO, ESP_DISPLAY_PRESENT_MODE_AUTO, ESP_DISPLAY_PRESENT_ROTATE_0, false, -1, ESP_ERR_NOT_SUPPORTED, 0},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const esp_display_present_target_config_t config = {
            .hw = {
                .panel_type = cases[i].panel,
                .rotation = cases[i].rotation,
                .te_enabled = cases[i].te_enabled,
                .te_sync = {.gpio_num = cases[i].te_gpio},
            },
            .fb = {.mode = cases[i].mode},
        };
        uint8_t count = UINT8_MAX;
        TEST_ASSERT_EQUAL(cases[i].result,
                          esp_display_present_get_required_frame_buffer_count(&config, &count));
        TEST_ASSERT_EQUAL_UINT8(cases[i].count, count);
    }
}

TEST_CASE("framebuffer query ignores resources and clears errors",
          "[present][config]")
{
    esp_display_present_target_config_t config = {
        .hw = {
            .panel_type = ESP_DISPLAY_PRESENT_PANEL_RGB,
            .input_pixel_format = (esp_display_present_pixel_format_t)99,
        },
        .fb = {
            .mode = ESP_DISPLAY_PRESENT_MODE_AUTO,
            .frame_buffer_count = UINT8_MAX,
        },
        .drawbuf = {.buffers = UINT8_MAX, .te_compose_buffers = UINT8_MAX},
    };
    uint8_t count = UINT8_MAX;
    TEST_ESP_OK(esp_display_present_get_required_frame_buffer_count(&config, &count));
    TEST_ASSERT_EQUAL_UINT8(3, count);
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG,
                 esp_display_present_get_required_frame_buffer_count(NULL, &count));
    TEST_ASSERT_EQUAL_UINT8(0, count);
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG,
                 esp_display_present_get_required_frame_buffer_count(&config, NULL));
}

typedef struct {
    esp_lcd_panel_io_t base;
    esp_lcd_panel_io_color_trans_done_cb_t on_color_trans_done;
    void *user_ctx;
    esp_timer_handle_t done_timer;
} dummy_io_t;

typedef struct {
    esp_lcd_panel_t base;
    const void *last_pixels;
    int draw_count;
    esp_err_t draw_result;
} dummy_panel_t;

static dummy_io_t s_dummy_io;
static dummy_panel_t s_dummy_panel;
#if SOC_LCD_RGB_SUPPORTED
static esp_lcd_rgb_panel_event_callbacks_t s_rgb_cbs;
static void *s_rgb_user_ctx;
static uint16_t *s_test_fbs[3];

static void fake_rgb_complete_frame(void);

esp_err_t __wrap_esp_lcd_rgb_panel_register_event_callbacks(
    esp_lcd_panel_handle_t panel,
    const esp_lcd_rgb_panel_event_callbacks_t *callbacks,
    void *user_ctx)
{
    (void)panel;
    if (s_dummy_io.done_timer != NULL) {
        (void)esp_timer_stop(s_dummy_io.done_timer);
    }
    s_rgb_cbs = callbacks != NULL
    ? *callbacks : (esp_lcd_rgb_panel_event_callbacks_t) {
        0
    };
    s_rgb_user_ctx = user_ctx;
    if (callbacks != NULL && s_dummy_io.done_timer != NULL) {
        return esp_timer_start_periodic(s_dummy_io.done_timer, 5000);
    }
    return ESP_OK;
}

static void IRAM_ATTR fake_rgb_complete_frame(void)
{
    if (s_rgb_cbs.on_color_trans_done != NULL) {
        (void)s_rgb_cbs.on_color_trans_done(&s_dummy_panel.base, NULL,
                                            s_rgb_user_ctx);
    }
    if (s_rgb_cbs.on_frame_buf_complete != NULL) {
        (void)s_rgb_cbs.on_frame_buf_complete(&s_dummy_panel.base, NULL,
                                              s_rgb_user_ctx);
    }
}
#endif

static void IRAM_ATTR dummy_color_done_timer(void *arg)
{
    dummy_io_t *io = arg;
    if (io->on_color_trans_done != NULL) {
        (void)io->on_color_trans_done(&io->base, NULL, io->user_ctx);
    }
#if SOC_LCD_RGB_SUPPORTED
    fake_rgb_complete_frame();
#endif
}

static esp_err_t dummy_io_register_event_callbacks(
    esp_lcd_panel_io_t *io,
    const esp_lcd_panel_io_callbacks_t *cbs,
    void *user_ctx)
{
    dummy_io_t *dummy = (dummy_io_t *)io;
    dummy->on_color_trans_done = (cbs != NULL) ? cbs->on_color_trans_done : NULL;
    dummy->user_ctx = user_ctx;
    return ESP_OK;
}

static esp_err_t dummy_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start,
                                   int x_end, int y_end, const void *color_data)
{
    dummy_panel_t *dummy = (dummy_panel_t *)panel;
    (void)x_start;
    (void)y_start;
    (void)x_end;
    (void)y_end;
    dummy->last_pixels = color_data;
    dummy->draw_count++;
    if (dummy->draw_result != ESP_OK) {
        return dummy->draw_result;
    }
    if (s_dummy_io.done_timer != NULL &&
            s_dummy_io.on_color_trans_done != NULL) {
        (void)esp_timer_stop(s_dummy_io.done_timer);
        (void)esp_timer_start_once(s_dummy_io.done_timer, 0);
    }
    return ESP_OK;
}

static void dummy_panel_init(void)
{
    memset(&s_dummy_io, 0, sizeof(s_dummy_io));
    memset(&s_dummy_panel, 0, sizeof(s_dummy_panel));
#if SOC_LCD_RGB_SUPPORTED
    memset(&s_rgb_cbs, 0, sizeof(s_rgb_cbs));
    s_rgb_user_ctx = NULL;
#endif
    s_dummy_io.base.register_event_callbacks = dummy_io_register_event_callbacks;
    s_dummy_panel.base.draw_bitmap = dummy_draw_bitmap;

    const esp_timer_create_args_t timer_args = {
        .callback = dummy_color_done_timer,
        .arg = &s_dummy_io,
        .dispatch_method = ESP_TIMER_ISR,
        .name = "dummy_lcd_done",
    };
    TEST_ESP_OK(esp_timer_create(&timer_args, &s_dummy_io.done_timer));
}

#if SOC_LCD_RGB_SUPPORTED
static uint16_t test_pixel(unsigned x, unsigned y, unsigned generation)
{
    return (uint16_t)(0x4000U | ((generation & 0x0fU) << 10) |
                      ((y & 0x1fU) << 5) | (x & 0x1fU));
}

static void fill_surface_pattern(esp_display_presenter_buffer_t *buffer,
                                 unsigned generation)
{
    uint16_t *pixels = buffer->surface.pixels;
    size_t stride = buffer->surface.stride_bytes / sizeof(*pixels);
    for (unsigned y = 0; y < TEST_FB_H; ++y) {
        for (unsigned x = 0; x < TEST_FB_W; ++x) {
            pixels[y * stride + x] = test_pixel(x, y, generation);
        }
    }
}

static void assert_rotated_pattern(const uint16_t *physical,
                                   esp_display_present_rotation_t rotation,
                                   unsigned generation)
{
    const unsigned physical_w =
        (rotation == ESP_DISPLAY_PRESENT_ROTATE_90 ||
         rotation == ESP_DISPLAY_PRESENT_ROTATE_270) ? TEST_FB_H : TEST_FB_W;
    for (unsigned y = 0; y < TEST_FB_H; ++y) {
        for (unsigned x = 0; x < TEST_FB_W; ++x) {
            unsigned px = x;
            unsigned py = y;
            switch (rotation) {
            case ESP_DISPLAY_PRESENT_ROTATE_90:
                px = TEST_FB_H - 1U - y;
                py = x;
                break;
            case ESP_DISPLAY_PRESENT_ROTATE_180:
                px = TEST_FB_W - 1U - x;
                py = TEST_FB_H - 1U - y;
                break;
            case ESP_DISPLAY_PRESENT_ROTATE_270:
                px = y;
                py = TEST_FB_W - 1U - x;
                break;
            default:
                break;
            }
            TEST_ASSERT_EQUAL_HEX16(test_pixel(x, y, generation),
                                    physical[py * physical_w + px]);
        }
    }
}

static void test_fb_presenter_create(esp_display_present_mode_t mode,
                                     esp_display_present_rotation_t rotation,
                                     esp_display_presenter_t **out_presenter)
{
    dummy_panel_init();
    esp_display_present_target_config_t target = {
        .hw = {
            .panel = &s_dummy_panel.base,
            .panel_type = ESP_DISPLAY_PRESENT_PANEL_RGB,
            .input_pixel_format = ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565,
            .rotation = rotation,
        },
        .fb = {
            .mode = mode,
        },
        .drawbuf = {
            .lines = 2,
            .buffers = 1,
        },
    };
    uint8_t count = 0;
    TEST_ESP_OK(esp_display_present_get_required_frame_buffer_count(&target, &count));
    target.fb.frame_buffer_count = count;
    for (uint8_t i = 0; i < count; ++i) {
        s_test_fbs[i] = heap_caps_calloc(
                            TEST_FB_PIXELS, sizeof(uint16_t),
                            MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        TEST_ASSERT_NOT_NULL(s_test_fbs[i]);
        target.fb.frame_buffers[i] = s_test_fbs[i];
    }
    const esp_display_presenter_config_t config = {
        .width = TEST_FB_W,
        .height = TEST_FB_H,
        .pixel_format = ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565,
        .max_damage_areas = 8,
        .pipeline_acquire_timeout_ms = 50,
        .target = target,
    };
    TEST_ESP_OK(esp_display_presenter_create(&config, out_presenter));
}

static void render_full_surface_frame(esp_display_presenter_t *presenter,
                                      unsigned generation)
{
    esp_display_present_area_t render_areas[8];
    size_t render_count = 0;
    bool full = false;
    TEST_ESP_OK(esp_display_presenter_begin_next_frame(
                    presenter, NULL, render_areas, 8, &render_count, &full));
    esp_display_presenter_buffer_t buffer = {0};
    TEST_ESP_OK(esp_display_presenter_acquire_buffer(presenter, &buffer));
    fill_surface_pattern(&buffer, generation);
    const esp_display_present_area_t area = {
        .x1 = 0, .y1 = 0, .x2 = TEST_FB_W - 1, .y2 = TEST_FB_H - 1,
    };
    TEST_ESP_OK(esp_display_presenter_submit_buffer(
                    presenter, &buffer, &area,
                    (size_t)TEST_FB_W * sizeof(uint16_t)));
    const esp_display_presenter_submit_t submit = {
        .coverage = ESP_DISPLAY_PRESENT_COVERAGE_FULL,
    };
    TEST_ESP_OK(esp_display_presenter_commit_frame(presenter, &submit));
}

static void submit_partition_area(esp_display_presenter_t *presenter,
                                  const esp_display_present_area_t *area,
                                  unsigned generation)
{
    esp_display_presenter_buffer_t buffer = {0};
    TEST_ESP_OK(esp_display_presenter_acquire_buffer(presenter, &buffer));
    const unsigned width = (unsigned)(area->x2 - area->x1 + 1);
    const unsigned height = (unsigned)(area->y2 - area->y1 + 1);
    const size_t stride = (size_t)(width + 2U) * sizeof(uint16_t);
    TEST_ASSERT_GREATER_OR_EQUAL(stride * height, buffer.capacity_bytes);
    uint16_t *pixels = buffer.surface.pixels;
    for (unsigned y = 0; y < height; ++y) {
        for (unsigned x = 0; x < width; ++x) {
            pixels[y * (width + 2U) + x] =
                test_pixel((unsigned)area->x1 + x,
                           (unsigned)area->y1 + y, generation);
        }
        pixels[y * (width + 2U) + width] = 0xdead;
        pixels[y * (width + 2U) + width + 1U] = 0xbeef;
    }
    TEST_ESP_OK(esp_display_presenter_submit_buffer(
                    presenter, &buffer, area, stride));
}

static void render_partition_frame(esp_display_presenter_t *presenter,
                                   const esp_display_present_area_t *coverage,
                                   size_t coverage_count,
                                   unsigned generation)
{
    esp_display_present_area_t render_areas[8];
    size_t render_count = 0;
    bool full = false;
    esp_display_present_surface_request_t request = {
        .dirty_areas = coverage,
        .dirty_area_count = coverage_count,
    };
    TEST_ESP_OK(esp_display_presenter_begin_next_frame(
                    presenter, coverage_count != 0 ? &request : NULL,
                    render_areas, 8, &render_count, &full));
    if (coverage_count == 0) {
        for (int y = 0; y < TEST_FB_H; ++y) {
            const esp_display_present_area_t band = {
                .x1 = 0, .y1 = y, .x2 = TEST_FB_W - 1, .y2 = y,
            };
            submit_partition_area(presenter, &band, generation);
        }
    } else {
        for (size_t i = 0; i < coverage_count; ++i) {
            for (int y = coverage[i].y1; y <= coverage[i].y2; ++y) {
                const esp_display_present_area_t band = {
                    .x1 = coverage[i].x1, .y1 = y,
                    .x2 = coverage[i].x2, .y2 = y,
                };
                submit_partition_area(presenter, &band, generation);
            }
        }
    }
    const esp_display_presenter_submit_t submit = coverage_count == 0
    ? (esp_display_presenter_submit_t) {
        .coverage = ESP_DISPLAY_PRESENT_COVERAGE_FULL,
    }
: (esp_display_presenter_submit_t) {
        .coverage = ESP_DISPLAY_PRESENT_COVERAGE_AREAS,
        .areas = coverage,
        .area_count = coverage_count,
    };
    TEST_ESP_OK(esp_display_presenter_commit_frame(presenter, &submit));
}
#endif

static void dummy_panel_deinit(void)
{
#if SOC_LCD_RGB_SUPPORTED
    memset(&s_rgb_cbs, 0, sizeof(s_rgb_cbs));
    s_rgb_user_ctx = NULL;
    for (size_t i = 0; i < sizeof(s_test_fbs) / sizeof(s_test_fbs[0]); ++i) {
        heap_caps_free(s_test_fbs[i]);
        s_test_fbs[i] = NULL;
    }
#endif
    if (s_dummy_io.done_timer != NULL) {
        (void)esp_timer_stop(s_dummy_io.done_timer);
        TEST_ESP_OK(esp_timer_delete(s_dummy_io.done_timer));
        s_dummy_io.done_timer = NULL;
    }
}

static void test_presenter_create(esp_display_presenter_t **out_presenter)
{
    dummy_panel_init();

    const esp_display_present_target_config_t target = {
        .hw = {
            .panel = &s_dummy_panel.base,
            .io = &s_dummy_io.base,
            .panel_type = ESP_DISPLAY_PRESENT_PANEL_IO,
            .input_pixel_format = ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565,
            .rotation = ESP_DISPLAY_PRESENT_ROTATE_0,
            .swap_bytes = false,
        },
        .fb = {
            .mode = ESP_DISPLAY_PRESENT_MODE_NONE,
        },
    };
    const esp_display_presenter_config_t config = {
        .width = TEST_LCD_H_RES,
        .height = TEST_LCD_V_RES,
        .pixel_format = ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565,
        .max_damage_areas = 8,
        .target = target,
    };
    TEST_ESP_OK(esp_display_presenter_create(&config, out_presenter));
    TEST_ASSERT_NOT_NULL(*out_presenter);
}

static void test_presenter_delete(esp_display_presenter_t *presenter)
{
    if (presenter != NULL) {
        TEST_ESP_OK(esp_display_presenter_delete(presenter));
    }
    dummy_panel_deinit();
    TEST_ESP_OK(esp_display_present_hw_resource_release());
}

static size_t before_free_8bit;
static size_t before_free_32bit;

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    unity_utils_check_leak(before_free_8bit, after_free_8bit, "8BIT", TEST_MEMORY_LEAK_THRESHOLD);
    unity_utils_check_leak(before_free_32bit, after_free_32bit, "32BIT", TEST_MEMORY_LEAK_THRESHOLD);
}

TEST_CASE("presenter create/delete arguments", "[present][basic]")
{
    esp_display_presenter_t *presenter = NULL;
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, esp_display_presenter_create(NULL, &presenter));
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, esp_display_presenter_delete(NULL));

    test_presenter_create(&presenter);

    esp_display_presenter_caps_t caps;
    TEST_ESP_OK(esp_display_presenter_get_caps(presenter, &caps));
    TEST_ASSERT_EQUAL(ESP_DISPLAY_PRESENT_CONTRACT_PARTITION, caps.contract);
    TEST_ASSERT_EQUAL_UINT16(TEST_LCD_H_RES, caps.width);
    TEST_ASSERT_EQUAL_UINT16(TEST_LCD_V_RES, caps.height);

    esp_display_presenter_state_t state;
    TEST_ESP_OK(esp_display_presenter_get_state(presenter, &state));
    TEST_ASSERT_EQUAL(ESP_DISPLAY_PRESENTER_STATE_OPEN, state);

    test_presenter_delete(presenter);
}

TEST_CASE("presenter submits a PARTITION frame", "[present][basic]")
{
    esp_display_presenter_t *presenter = NULL;
    test_presenter_create(&presenter);

    esp_display_presenter_caps_t caps;
    TEST_ESP_OK(esp_display_presenter_get_caps(presenter, &caps));

    const esp_display_present_surface_request_t request = {0};
    esp_display_present_area_t render_areas[8];
    size_t render_area_count = 0;
    bool full_coverage = false;
    TEST_ESP_OK(esp_display_presenter_begin_next_frame(
                    presenter, &request, render_areas, 8,
                    &render_area_count, &full_coverage));

    esp_display_present_area_t remaining = {
        .x1 = 0,
        .y1 = 0,
        .x2 = TEST_LCD_H_RES - 1,
        .y2 = TEST_LCD_V_RES - 1,
    };
    while (remaining.y1 <= remaining.y2) {
        esp_display_presenter_buffer_t buffer;
        TEST_ESP_OK(esp_display_presenter_acquire_buffer(presenter, &buffer));
        const size_t stride = (size_t)(remaining.x2 - remaining.x1 + 1) * 2U;
        const size_t remaining_rows = (size_t)(remaining.y2 - remaining.y1 + 1);
        size_t rows = 0;
        TEST_ASSERT_NOT_NULL(buffer.resolve_rows);
        TEST_ESP_OK(buffer.resolve_rows(
                        buffer.resolve_rows_ctx, buffer.lease_id, stride,
                        remaining_rows, &rows));
        TEST_ASSERT_TRUE(rows > 0);
        memset(buffer.surface.pixels, 0xFF, stride * rows);

        const esp_display_present_area_t rendered = {
            .x1 = remaining.x1,
            .y1 = remaining.y1,
            .x2 = remaining.x2,
            .y2 = remaining.y1 + (int)rows - 1,
        };
        TEST_ESP_OK(esp_display_presenter_submit_buffer(
                        presenter, &buffer, &rendered, stride));
        remaining.y1 += (int)rows;
    }

    const esp_display_presenter_submit_t submit = {
        .coverage = ESP_DISPLAY_PRESENT_COVERAGE_FULL,
    };
    TEST_ASSERT_EQUAL(ESP_DISPLAY_PRESENT_CONTRACT_PARTITION, caps.contract);
    TEST_ESP_OK(esp_display_presenter_commit_frame(presenter, &submit));
    TEST_ESP_OK(esp_display_presenter_quiesce(presenter, TEST_QUIESCE_TIMEOUT_MS));
    test_presenter_delete(presenter);
}

#if SOC_LCD_RGB_SUPPORTED
TEST_CASE("FULL presenter produces exact rotated pixels", "[present][pixels]")
{
    const esp_display_present_rotation_t rotations[] = {
        ESP_DISPLAY_PRESENT_ROTATE_0,
        ESP_DISPLAY_PRESENT_ROTATE_90,
        ESP_DISPLAY_PRESENT_ROTATE_180,
        ESP_DISPLAY_PRESENT_ROTATE_270,
    };
    for (size_t i = 0; i < sizeof(rotations) / sizeof(rotations[0]); ++i) {
        esp_display_presenter_t *presenter = NULL;
        test_fb_presenter_create(ESP_DISPLAY_PRESENT_MODE_DOUBLE_FULL,
                                 rotations[i], &presenter);
        esp_display_presenter_caps_t caps;
        TEST_ESP_OK(esp_display_presenter_get_caps(presenter, &caps));
        TEST_ASSERT_EQUAL(ESP_DISPLAY_PRESENT_CONTRACT_FULL, caps.contract);

        render_full_surface_frame(presenter, 1);
        TEST_ASSERT_NOT_NULL(s_dummy_panel.last_pixels);
        assert_rotated_pattern(s_dummy_panel.last_pixels, rotations[i], 1);
        fake_rgb_complete_frame();
        TEST_ESP_OK(esp_display_presenter_quiesce(
                        presenter, TEST_QUIESCE_TIMEOUT_MS));
        test_presenter_delete(presenter);
    }
}

TEST_CASE("DIRECT presenter produces exact pixels at zero rotation",
          "[present][pixels]")
{
    esp_display_presenter_t *presenter = NULL;
    test_fb_presenter_create(ESP_DISPLAY_PRESENT_MODE_DOUBLE_DIRECT,
                             ESP_DISPLAY_PRESENT_ROTATE_0, &presenter);
    esp_display_presenter_caps_t caps;
    TEST_ESP_OK(esp_display_presenter_get_caps(presenter, &caps));
    TEST_ASSERT_EQUAL(ESP_DISPLAY_PRESENT_CONTRACT_DIRECT, caps.contract);

    render_full_surface_frame(presenter, 2);
    TEST_ASSERT_NOT_NULL(s_dummy_panel.last_pixels);
    assert_rotated_pattern(s_dummy_panel.last_pixels,
                           ESP_DISPLAY_PRESENT_ROTATE_0, 2);
    fake_rgb_complete_frame();
    TEST_ESP_OK(esp_display_presenter_quiesce(
                    presenter, TEST_QUIESCE_TIMEOUT_MS));
    test_presenter_delete(presenter);
}

TEST_CASE("DIRECT presenter rejects non-zero rotation",
          "[present][contract]")
{
    const esp_display_present_rotation_t rotations[] = {
        ESP_DISPLAY_PRESENT_ROTATE_90,
        ESP_DISPLAY_PRESENT_ROTATE_180,
        ESP_DISPLAY_PRESENT_ROTATE_270,
    };
    for (size_t i = 0; i < sizeof(rotations) / sizeof(rotations[0]); ++i) {
        esp_display_presenter_t *presenter = NULL;
        test_fb_presenter_create(ESP_DISPLAY_PRESENT_MODE_DOUBLE_DIRECT,
                                 ESP_DISPLAY_PRESENT_ROTATE_0, &presenter);
        TEST_ESP_ERR(ESP_ERR_NOT_SUPPORTED,
                     esp_display_presenter_set_rotation(presenter,
                                                        rotations[i]));
        esp_display_presenter_caps_t caps;
        TEST_ESP_OK(esp_display_presenter_get_caps(presenter, &caps));
        TEST_ASSERT_EQUAL(ESP_DISPLAY_PRESENT_CONTRACT_DIRECT, caps.contract);
        test_presenter_delete(presenter);
    }
}

TEST_CASE("PARTITION multi-band stride and rotation are pixel exact",
          "[present][pixels]")
{
    const esp_display_present_rotation_t rotations[] = {
        ESP_DISPLAY_PRESENT_ROTATE_0,
        ESP_DISPLAY_PRESENT_ROTATE_90,
        ESP_DISPLAY_PRESENT_ROTATE_180,
        ESP_DISPLAY_PRESENT_ROTATE_270,
    };
    for (size_t i = 0; i < sizeof(rotations) / sizeof(rotations[0]); ++i) {
        esp_display_presenter_t *presenter = NULL;
        test_fb_presenter_create(ESP_DISPLAY_PRESENT_MODE_DOUBLE_PARTIAL,
                                 rotations[i], &presenter);
        render_partition_frame(presenter, NULL, 0, 3);
        TEST_ASSERT_NOT_NULL(s_dummy_panel.last_pixels);
        assert_rotated_pattern(s_dummy_panel.last_pixels, rotations[i], 3);
        fake_rgb_complete_frame();
        TEST_ESP_OK(esp_display_presenter_quiesce(
                        presenter, TEST_QUIESCE_TIMEOUT_MS));
        test_presenter_delete(presenter);
    }
}

TEST_CASE("PARTITION partial coverage repairs from displayed frame",
          "[present][pixels][repair]")
{
    esp_display_presenter_t *presenter = NULL;
    test_fb_presenter_create(ESP_DISPLAY_PRESENT_MODE_DOUBLE_PARTIAL,
                             ESP_DISPLAY_PRESENT_ROTATE_90, &presenter);
    render_partition_frame(presenter, NULL, 0, 1);
    fake_rgb_complete_frame();

    const esp_display_present_area_t dirty[] = {
        {.x1 = 1, .y1 = 1, .x2 = 4, .y2 = 2},
        {.x1 = 3, .y1 = 2, .x2 = 5, .y2 = 3},
    };
    render_partition_frame(presenter, dirty, 2, 2);
    const uint16_t *physical = s_dummy_panel.last_pixels;
    const unsigned physical_w = TEST_FB_H;
    for (unsigned y = 0; y < TEST_FB_H; ++y) {
        for (unsigned x = 0; x < TEST_FB_W; ++x) {
            const bool changed =
                (x >= 1 && x <= 4 && y >= 1 && y <= 2) ||
                (x >= 3 && x <= 5 && y >= 2 && y <= 3);
            const unsigned px = TEST_FB_H - 1U - y;
            const unsigned py = x;
            TEST_ASSERT_EQUAL_HEX16(test_pixel(x, y, changed ? 2 : 1),
                                    physical[py * physical_w + px]);
        }
    }
    fake_rgb_complete_frame();
    TEST_ESP_OK(esp_display_presenter_quiesce(
                    presenter, TEST_QUIESCE_TIMEOUT_MS));
    test_presenter_delete(presenter);
}

TEST_CASE("first PARTITION partial request is promoted to full",
          "[present][pixels][repair]")
{
    esp_display_presenter_t *presenter = NULL;
    test_fb_presenter_create(ESP_DISPLAY_PRESENT_MODE_DOUBLE_PARTIAL,
                             ESP_DISPLAY_PRESENT_ROTATE_0, &presenter);
    const esp_display_present_area_t dirty = {
        .x1 = 2, .y1 = 1, .x2 = 4, .y2 = 3,
    };
    const esp_display_present_surface_request_t request = {
        .dirty_areas = &dirty,
        .dirty_area_count = 1,
    };
    esp_display_present_area_t render_areas[8];
    size_t render_count = 99;
    bool full = false;
    TEST_ESP_OK(esp_display_presenter_begin_next_frame(
                    presenter, &request, render_areas, 8,
                    &render_count, &full));
    TEST_ASSERT_TRUE(full);
    TEST_ASSERT_EQUAL(0, render_count);
    esp_display_presenter_cancel_frame(presenter);
    test_presenter_delete(presenter);
}

TEST_CASE("failed framebuffer commit releases lease and next frame recovers",
          "[present][failure]")
{
    esp_display_presenter_t *presenter = NULL;
    test_fb_presenter_create(ESP_DISPLAY_PRESENT_MODE_DOUBLE_FULL,
                             ESP_DISPLAY_PRESENT_ROTATE_0, &presenter);
    s_dummy_panel.draw_result = ESP_FAIL;

    esp_display_present_area_t areas[8];
    size_t count = 0;
    bool full = false;
    TEST_ESP_OK(esp_display_presenter_begin_next_frame(
                    presenter, NULL, areas, 8, &count, &full));
    esp_display_presenter_buffer_t buffer = {0};
    TEST_ESP_OK(esp_display_presenter_acquire_buffer(presenter, &buffer));
    fill_surface_pattern(&buffer, 1);
    const esp_display_present_area_t all = {
        .x1 = 0, .y1 = 0, .x2 = TEST_FB_W - 1, .y2 = TEST_FB_H - 1,
    };
    TEST_ESP_OK(esp_display_presenter_submit_buffer(
                    presenter, &buffer, &all,
                    (size_t)TEST_FB_W * sizeof(uint16_t)));
    const esp_display_presenter_submit_t submit = {
        .coverage = ESP_DISPLAY_PRESENT_COVERAGE_FULL,
    };
    TEST_ESP_ERR(ESP_FAIL,
                 esp_display_presenter_commit_frame(presenter, &submit));
    esp_display_presenter_state_t state;
    TEST_ESP_OK(esp_display_presenter_get_state(presenter, &state));
    TEST_ASSERT_EQUAL(ESP_DISPLAY_PRESENTER_STATE_OPEN, state);

    s_dummy_panel.draw_result = ESP_OK;
    render_full_surface_frame(presenter, 2);
    assert_rotated_pattern(s_dummy_panel.last_pixels,
                           ESP_DISPLAY_PRESENT_ROTATE_0, 2);
    fake_rgb_complete_frame();
    fake_rgb_complete_frame(); /* duplicate completion is ignored */
    TEST_ESP_OK(esp_display_presenter_quiesce(
                    presenter, TEST_QUIESCE_TIMEOUT_MS));
    test_presenter_delete(presenter);
}
#endif

TEST_CASE("presenter rejects invalid frame and lease transitions", "[present][state]")
{
    esp_display_presenter_t *presenter = NULL;
    test_presenter_create(&presenter);

    esp_display_presenter_buffer_t buffer = {0};
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE,
                 esp_display_presenter_acquire_buffer(presenter, &buffer));

    esp_display_present_area_t areas[8];
    size_t count = 0;
    bool full = false;
    TEST_ESP_OK(esp_display_presenter_begin_next_frame(
                    presenter, NULL, areas, 8, &count, &full));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE,
                 esp_display_presenter_begin_next_frame(
                     presenter, NULL, areas, 8, &count, &full));
    TEST_ESP_OK(esp_display_presenter_acquire_buffer(presenter, &buffer));

    esp_display_presenter_buffer_t second = {0};
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE,
                 esp_display_presenter_acquire_buffer(presenter, &second));
    const esp_display_presenter_submit_t submit = {
        .coverage = ESP_DISPLAY_PRESENT_COVERAGE_FULL,
    };
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE,
                 esp_display_presenter_commit_frame(presenter, &submit));

    esp_display_presenter_cancel_frame(presenter);
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE,
                 esp_display_presenter_acquire_buffer(presenter, &second));
    TEST_ESP_OK(esp_display_presenter_begin_next_frame(
                    presenter, NULL, areas, 8, &count, &full));
    esp_display_presenter_cancel_frame(presenter);
    test_presenter_delete(presenter);
}

TEST_CASE("stop with held lease closes without stealing producer storage",
          "[present][state]")
{
    esp_display_presenter_t *presenter = NULL;
    test_presenter_create(&presenter);
    esp_display_present_area_t areas[8];
    size_t count = 0;
    bool full = false;
    TEST_ESP_OK(esp_display_presenter_begin_next_frame(
                    presenter, NULL, areas, 8, &count, &full));
    esp_display_presenter_buffer_t buffer = {0};
    TEST_ESP_OK(esp_display_presenter_acquire_buffer(presenter, &buffer));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE,
                 esp_display_presenter_stop(presenter));

    esp_display_presenter_state_t state;
    TEST_ESP_OK(esp_display_presenter_get_state(presenter, &state));
    TEST_ASSERT_EQUAL(ESP_DISPLAY_PRESENTER_STATE_CLOSING, state);
    esp_display_presenter_cancel_frame(presenter);
    TEST_ESP_OK(esp_display_presenter_stop(presenter));
    TEST_ESP_OK(esp_display_presenter_get_state(presenter, &state));
    TEST_ASSERT_EQUAL(ESP_DISPLAY_PRESENTER_STATE_STOPPED, state);
    test_presenter_delete(presenter);
}

void app_main(void)
{
    printf("ESP Display Present TEST\n");
    unity_run_menu();
}
