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
#include "hw_init.h"
#include "esp_lv_adapter.h"
#include "esp_heap_caps.h"

static const char *TAG = "main";

/* FPS monitor task configuration */
#define FPS_MONITOR_TASK_STACK_SIZE    4096
#define FPS_MONITOR_TASK_PRIORITY      3
#define FPS_MONITOR_INTERVAL_MS        1000

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
static esp_lv_adapter_rotation_t get_configured_rotation(void)
{
#if CONFIG_EXAMPLE_DISPLAY_ROTATION_0
    return ESP_LV_ADAPTER_ROTATE_0;
#elif CONFIG_EXAMPLE_DISPLAY_ROTATION_90
    return ESP_LV_ADAPTER_ROTATE_90;
#elif CONFIG_EXAMPLE_DISPLAY_ROTATION_180
    return ESP_LV_ADAPTER_ROTATE_180;
#elif CONFIG_EXAMPLE_DISPLAY_ROTATION_270
    return ESP_LV_ADAPTER_ROTATE_270;
#else
    return ESP_LV_ADAPTER_ROTATE_0;
#endif
}

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FPS_STATS
static void fps_monitor_task(void *arg)
{
    lv_display_t *disp = (lv_display_t *)arg;
    uint32_t fps;
    while (1) {
        if (esp_lv_adapter_get_fps(disp, &fps) == ESP_OK) {
            ESP_LOGI(TAG, "FPS: %lu", fps);
        }
        vTaskDelay(pdMS_TO_TICKS(FPS_MONITOR_INTERVAL_MS));
    }
}
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

void app_main()
{
    esp_lcd_panel_handle_t display_panel = NULL;
    esp_lcd_panel_io_handle_t display_io_handle = NULL;
    esp_lv_adapter_rotation_t rotation = get_configured_rotation();

#if CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI
    esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_MIPI_DSI;
    ESP_LOGI(TAG, "LCD: MIPI DSI");
#elif CONFIG_EXAMPLE_LCD_INTERFACE_RGB
    esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB;
    ESP_LOGI(TAG, "LCD: RGB");
#else
    esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT;
#if CONFIG_EXAMPLE_LCD_INTERFACE_QSPI
    ESP_LOGI(TAG, "LCD: QSPI");
#elif CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITH_PSRAM
    ESP_LOGI(TAG, "LCD: SPI (PSRAM)");
#elif CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITHOUT_PSRAM
    ESP_LOGI(TAG, "LCD: SPI");
#endif
#endif

    ESP_LOGI(TAG, "Init LCD: %dx%d", HW_LCD_H_RES, HW_LCD_V_RES);
    ESP_ERROR_CHECK(hw_lcd_init(&display_panel, &display_io_handle, tear_avoid_mode, rotation));

    esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(esp_lv_adapter_init(&adapter_config));
#if CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI
    esp_lv_adapter_display_config_t display_config = ESP_LV_ADAPTER_DISPLAY_MIPI_DEFAULT_CONFIG(
                                                         display_panel, display_io_handle, HW_LCD_H_RES, HW_LCD_V_RES, rotation);
#elif CONFIG_EXAMPLE_LCD_INTERFACE_RGB
    esp_lv_adapter_display_config_t display_config = ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(
                                                         display_panel, display_io_handle, HW_LCD_H_RES, HW_LCD_V_RES, rotation);
#elif CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITHOUT_PSRAM
    esp_lv_adapter_display_config_t display_config = ESP_LV_ADAPTER_DISPLAY_SPI_WITHOUT_PSRAM_DEFAULT_CONFIG(
                                                         display_panel, display_io_handle, HW_LCD_H_RES, HW_LCD_V_RES, rotation);
#else
    esp_lv_adapter_display_config_t display_config = ESP_LV_ADAPTER_DISPLAY_SPI_WITH_PSRAM_DEFAULT_CONFIG(
                                                         display_panel, display_io_handle, HW_LCD_H_RES, HW_LCD_V_RES, rotation);
#endif

    lv_display_t *disp = esp_lv_adapter_register_display(&display_config);
    if (!disp) {
        ESP_LOGE(TAG, "Display registration failed");
        return;
    }

    lv_indev_t *touch = NULL;
    lv_indev_t *encoder = NULL;

#if HW_USE_TOUCH
    esp_lcd_touch_handle_t touch_handle = NULL;
    ESP_ERROR_CHECK(hw_touch_init(&touch_handle, rotation));
    esp_lv_adapter_touch_config_t touch_config = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, touch_handle);
    touch = esp_lv_adapter_register_touch(&touch_config);
    if (!touch) {
        ESP_LOGE(TAG, "Touch registration failed");
        return;
    }
#elif HW_USE_ENCODER && CONFIG_ESP_LVGL_ADAPTER_ENABLE_KNOB
    esp_lv_adapter_encoder_config_t encoder_config = {
        .disp = disp,
        .encoder_a_b = hw_knob_get_config(),
        .encoder_enter = hw_knob_get_button(),
    };
    encoder = esp_lv_adapter_register_encoder(&encoder_config);
    if (!encoder) {
        ESP_LOGE(TAG, "Encoder registration failed");
        return;
    }
#endif

    ESP_ERROR_CHECK(esp_lv_adapter_start());

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FPS_STATS
    ESP_ERROR_CHECK(esp_lv_adapter_fps_stats_enable(disp, true));
    xTaskCreate(fps_monitor_task, "fps_monitor", FPS_MONITOR_TASK_STACK_SIZE, disp, FPS_MONITOR_TASK_PRIORITY, NULL);
#endif

    s_disp = disp;
    (void)touch;

    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        create_control_ui(encoder);
        esp_lv_adapter_unlock();
    }
}
