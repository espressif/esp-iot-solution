/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/**
 * LVGL Dummy Draw Example
 *
 * Demonstrates bypassing LVGL to write directly to LCD:
 * - dummy_draw_enable/disable()  : Switch between LVGL and direct LCD control
 * - dummy_draw_fill_screen()     : Fill framebuffer
 * - dummy_draw_update_display()  : Blit to LCD hardware
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "sdkconfig.h"
#include <string.h>
#include "example_lvgl_init.h"
#include "esp_heap_caps.h"

static const char *TAG = "main";

/* Dummy draw task configuration */
#define DUMMY_DRAW_TASK_STACK_SIZE     8192
#define DUMMY_DRAW_TASK_PRIORITY       4
#define DUMMY_DRAW_CYCLE_DELAY_MS      100

static lv_display_t *s_disp;
static lv_obj_t *s_info_label;
static lv_obj_t *s_dummy_touch_layer;
static TaskHandle_t s_dummy_task;
static uint16_t *s_dummy_framebuffer;
static size_t s_dummy_pixel_count;
static size_t s_current_color_index;
#if HW_USE_ENCODER
static lv_group_t *s_input_group;
static lv_obj_t *s_toggle_button;
#endif

static const uint16_t s_dummy_palette[] = {
    0xF800,  // Red
    0x07E0,  // Green
    0x001F,  // Blue
};

static const char *s_color_names[] = {"Red", "Green", "Blue"};

static void dummy_draw_enable(void);
static void dummy_draw_disable(void);
static void dummy_draw_fill_screen(uint16_t color_rgb565);
static void dummy_draw_update_display(void);
static void dummy_draw_cycle_colors_task(void *arg);
static void screen_touch_event_cb(lv_event_t *e);
#if HW_USE_ENCODER
static void toggle_button_event_cb(lv_event_t *e);
#endif

/* ========== Dummy Draw Core Functions ========== */

static void dummy_draw_enable(void)
{
    esp_err_t ret = esp_lv_adapter_set_dummy_draw(s_disp, true);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "[Dummy] Enabled");
    } else {
        ESP_LOGE(TAG, "[Dummy] Enable failed");
    }
}

static void dummy_draw_disable(void)
{
    esp_lv_adapter_set_dummy_draw(s_disp, false);
    ESP_LOGI(TAG, "[Dummy] Disabled");
}

static void dummy_draw_fill_screen(uint16_t color_rgb565)
{
    if (!s_dummy_framebuffer) {
        s_dummy_pixel_count = (size_t)HW_LCD_H_RES * HW_LCD_V_RES;
        size_t bytes = s_dummy_pixel_count * sizeof(uint16_t);

        s_dummy_framebuffer = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_dummy_framebuffer) {
            s_dummy_framebuffer = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
        }

        if (!s_dummy_framebuffer) {
            ESP_LOGE(TAG, "[Dummy] Buffer allocation failed");
            return;
        }
        ESP_LOGI(TAG, "[Dummy] Buffer allocated: %dx%d", HW_LCD_H_RES, HW_LCD_V_RES);
    }

    for (size_t i = 0; i < s_dummy_pixel_count; ++i) {
        s_dummy_framebuffer[i] = color_rgb565;
    }
}

static void dummy_draw_update_display(void)
{
    if (!s_dummy_framebuffer) {
        return;
    }

    esp_err_t ret = esp_lv_adapter_dummy_draw_blit(s_disp, 0, 0,
                                                   HW_LCD_H_RES, HW_LCD_V_RES,
                                                   s_dummy_framebuffer, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[Dummy] Blit failed");
    }
}

/* ========== Demo Task ========== */

static void dummy_draw_cycle_colors_task(void *arg)
{
    size_t color_index = 0;
    const size_t palette_size = sizeof(s_dummy_palette) / sizeof(s_dummy_palette[0]);

    ESP_LOGI(TAG, "[Demo] Color cycle started");

    while (esp_lv_adapter_get_dummy_draw_enabled(s_disp)) {
        s_current_color_index = color_index;
        uint16_t current_color = s_dummy_palette[color_index];

        dummy_draw_fill_screen(current_color);
        dummy_draw_update_display();

        ESP_LOGI(TAG, "[Demo] %s (0x%04X)", s_color_names[color_index], current_color);

        color_index = (color_index + 1) % palette_size;
        vTaskDelay(pdMS_TO_TICKS(DUMMY_DRAW_CYCLE_DELAY_MS * 4));
    }

    ESP_LOGI(TAG, "[Demo] Stopped");

    s_dummy_task = NULL;
    dummy_draw_disable();

    vTaskDelete(NULL);
}

/* ========== Mode Switching ========== */

static void enter_dummy_mode(void)
{
    if (esp_lv_adapter_get_dummy_draw_enabled(s_disp)) {
        return;
    }

    dummy_draw_enable();

    if (s_dummy_task == NULL) {
        BaseType_t ret = xTaskCreate(dummy_draw_cycle_colors_task, "dummy_demo",
                                     DUMMY_DRAW_TASK_STACK_SIZE, NULL,
                                     DUMMY_DRAW_TASK_PRIORITY, &s_dummy_task);
        if (ret != pdPASS) {
            ESP_LOGE(TAG, "Task creation failed");
            dummy_draw_disable();
        }
    }
}

static void exit_dummy_mode(void)
{
    if (!esp_lv_adapter_get_dummy_draw_enabled(s_disp)) {
        return;
    }

    dummy_draw_disable();
}

/* ========== Event Handlers ========== */

#if HW_USE_ENCODER
static void toggle_button_event_cb(lv_event_t *e)
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
#endif

static void screen_touch_event_cb(lv_event_t *e)
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

/* ========== UI Creation ========== */

static void create_control_ui(lv_indev_t *encoder)
{
    lv_obj_t *scr = lv_disp_get_scr_act(s_disp);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x202020), 0);

    s_info_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_info_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_info_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(s_info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_info_label, LV_ALIGN_CENTER, 0, 0);

    s_dummy_touch_layer = lv_obj_create(scr);
    lv_obj_remove_style_all(s_dummy_touch_layer);
    lv_obj_set_size(s_dummy_touch_layer, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(s_dummy_touch_layer, 0, 0);
    lv_obj_set_style_bg_opa(s_dummy_touch_layer, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(s_dummy_touch_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_dummy_touch_layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_dummy_touch_layer, screen_touch_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_move_foreground(s_info_label);

#if HW_USE_ENCODER
    s_toggle_button = lv_btn_create(scr);
    lv_obj_set_size(s_toggle_button, 180, 50);
    lv_obj_align(s_toggle_button, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(s_toggle_button, toggle_button_event_cb, LV_EVENT_CLICKED, NULL);

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

    lv_label_set_text_fmt(s_info_label, "Tap to start\n%dx%d", HW_LCD_H_RES, HW_LCD_V_RES);
    ESP_LOGI(TAG, "UI created for %dx%d", HW_LCD_H_RES, HW_LCD_V_RES);
}

void app_main(void)
{
    example_lvgl_ctx_t ctx;
    ESP_ERROR_CHECK(example_lvgl_init(&ctx));

    s_disp = ctx.disp;

    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        create_control_ui(ctx.encoder);
        esp_lv_adapter_unlock();
    }
}
