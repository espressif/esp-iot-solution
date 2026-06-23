/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/**
 * LVGL Dummy Draw Example
 *
 * Demonstrates bypassing LVGL to write directly to the LCD.
 * Two update paths are selected automatically at runtime:
 *
 *   Path A – Pipeline (RGB, MIPI-DSI with multi-buffer anti-tearing):
 *     get_free_buf() → fill → flush_buf()   [tear-free, self-throttled]
 *
 *   Path B – Direct Blit (SPI, I80, QSPI, or single-buffer modes):
 *     alloc buffer → fill → dummy_draw_blit()
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "lvgl.h"
#include "example_lvgl_init.h"
#include "esp_lv_adapter.h"

static const char *TAG = "main";

#define DUMMY_TASK_STACK   8192
#define DUMMY_TASK_PRI     4
#define COLOR_HOLD_MS      pdMS_TO_TICKS(800)

static lv_display_t *s_disp;
static lv_obj_t     *s_info_label;
static lv_obj_t     *s_touch_layer;
static TaskHandle_t  s_dummy_task;
#if HW_USE_ENCODER
static lv_group_t *s_input_group;
static lv_obj_t   *s_toggle_button;
#endif

static void *s_blit_buf;   /* Path B: heap-allocated framebuffer (lazy) */

static const uint16_t s_palette[] = { 0xF800, 0x07E0, 0x001F };   /* Red, Green, Blue (RGB565) */
static const char    *s_palette_names[] = { "Red", "Green", "Blue" };

/* Fill a display-native framebuffer with a solid RGB565 colour.
 * MIPI-DSI panels commonly use RGB888 (3 B/px); others use RGB565 (2 B/px). */
static void fill_framebuf(void *fb, uint16_t rgb565)
{
    uint8_t bpp = lv_color_format_get_size(lv_display_get_color_format(s_disp));
    size_t  n   = (size_t)HW_LCD_H_RES * HW_LCD_V_RES;

    if (bpp == 2) {
        uint16_t *p = (uint16_t *)fb;
        for (size_t i = 0; i < n; i++) {
            p[i] = rgb565;
        }
    } else if (bpp == 3) {
        uint8_t r = (uint8_t)(((rgb565 >> 11) & 0x1F) << 3);
        uint8_t g = (uint8_t)(((rgb565 >>  5) & 0x3F) << 2);
        uint8_t b = (uint8_t)((rgb565         & 0x1F) << 3);
        uint8_t *p = (uint8_t *)fb;
        for (size_t i = 0; i < n; i++) {
            *p++ = r;
            *p++ = g;
            *p++ = b;
        }
    } else {
        memset(fb, (int)(rgb565 & 0xFF), n * bpp);
    }
}

/* Path A: get the next writable pipeline buffer, fill it, then submit.
 * flush_buf() blocks until the display hardware switches to the new buffer,
 * guaranteeing a tear-free update and naturally throttling to the refresh rate. */
static bool pipeline_update(uint16_t rgb565)
{
    void *fb = esp_lv_adapter_dummy_draw_get_free_buf(s_disp);
    if (!fb) {
        return false;
    }
    fill_framebuf(fb, rgb565);
    return esp_lv_adapter_dummy_draw_flush_buf(s_disp, fb) == ESP_OK;
}

/* Path B: fill a self-allocated buffer and push it to the LCD via blit. */
static void blit_update(uint16_t rgb565)
{
    if (!s_blit_buf) {
        uint8_t bpp  = lv_color_format_get_size(lv_display_get_color_format(s_disp));
        size_t  size = (size_t)HW_LCD_H_RES * HW_LCD_V_RES * bpp;
        s_blit_buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_blit_buf) {
            s_blit_buf = heap_caps_malloc(size, MALLOC_CAP_8BIT);
        }
        if (!s_blit_buf) {
            ESP_LOGE(TAG, "framebuffer alloc failed");
            return;
        }
        ESP_LOGI(TAG, "blit buf: %dx%d @%uB", HW_LCD_H_RES, HW_LCD_V_RES, (unsigned)bpp);
    }
    fill_framebuf(s_blit_buf, rgb565);
    esp_lv_adapter_dummy_draw_blit(s_disp, 0, 0, HW_LCD_H_RES, HW_LCD_V_RES, s_blit_buf, true);
}

static void dummy_draw_cycle_task(void *arg)
{
    /* get_free_buf() returns NULL when the pipeline path is unavailable
     * (e.g. tear avoidance mode NONE / TE_SYNC, or SPI/I80/QSPI).
     * The call itself is non-blocking; the blocking happens inside flush_buf(). */
    bool pipeline = (esp_lv_adapter_dummy_draw_get_free_buf(s_disp) != NULL);
    ESP_LOGI(TAG, "dummy draw: %s", pipeline ? "pipeline (tear-free)" : "direct blit");

    size_t idx = 0;
    while (esp_lv_adapter_get_dummy_draw_enabled(s_disp)) {
        uint16_t color = s_palette[idx];
        ESP_LOGI(TAG, "%s (0x%04X)", s_palette_names[idx], color);
        idx = (idx + 1) % (sizeof(s_palette) / sizeof(s_palette[0]));

        if (pipeline) {
            if (!pipeline_update(color)) {
                ESP_LOGW(TAG, "pipeline failed, switching to blit");
                pipeline = false;
                blit_update(color);
            }
        } else {
            blit_update(color);
        }
        vTaskDelay(COLOR_HOLD_MS);
    }

    s_dummy_task = NULL;
    esp_lv_adapter_set_dummy_draw(s_disp, false);
    vTaskDelete(NULL);
}

static void enter_dummy_mode(void)
{
    if (esp_lv_adapter_get_dummy_draw_enabled(s_disp) || s_dummy_task) {
        return;
    }
    if (esp_lv_adapter_set_dummy_draw(s_disp, true) != ESP_OK) {
        return;
    }
    xTaskCreate(dummy_draw_cycle_task, "dummy", DUMMY_TASK_STACK, NULL, DUMMY_TASK_PRI, &s_dummy_task);
}

static void exit_dummy_mode(void)
{
    esp_lv_adapter_set_dummy_draw(s_disp, false);
}

static void on_click(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (esp_lv_adapter_get_dummy_draw_enabled(s_disp)) {
        exit_dummy_mode();
    } else {
        enter_dummy_mode();
    }
}

static void create_ui(lv_indev_t *encoder)
{
    lv_obj_t *scr = lv_disp_get_scr_act(s_disp);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x202020), 0);

    s_info_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_info_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_info_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(s_info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_info_label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text_fmt(s_info_label, "Tap to start\n%dx%d", HW_LCD_H_RES, HW_LCD_V_RES);

    s_touch_layer = lv_obj_create(scr);
    lv_obj_remove_style_all(s_touch_layer);
    lv_obj_set_size(s_touch_layer, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s_touch_layer, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(s_touch_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_touch_layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_touch_layer, on_click, LV_EVENT_CLICKED, NULL);
    lv_obj_move_foreground(s_info_label);

#if HW_USE_ENCODER
    s_toggle_button = lv_btn_create(scr);
    lv_obj_set_size(s_toggle_button, 180, 50);
    lv_obj_align(s_toggle_button, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(s_toggle_button, on_click, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btn_label = lv_label_create(s_toggle_button);
    lv_label_set_text(btn_label, "Toggle");
    lv_obj_center(btn_label);
    if (encoder) {
        s_input_group = lv_group_create();
        lv_group_add_obj(s_input_group, s_toggle_button);
        lv_indev_set_group(encoder, s_input_group);
    }
#else
    (void)encoder;
#endif
}

void app_main(void)
{
    example_lvgl_ctx_t ctx;
    ESP_ERROR_CHECK(example_lvgl_init(&ctx));
    s_disp = ctx.disp;

    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        create_ui(ctx.encoder);
        esp_lv_adapter_unlock();
    }
}
