/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "hw_init.h"
#include "esp_lv_adapter.h"
#include "lvgl.h"

static const char *TAG = "main";

static lv_display_t *s_disp;
static lv_obj_t *s_status_label;
static lv_obj_t *s_sleep_button;
static lv_indev_t *s_encoder;
static esp_lcd_panel_handle_t s_panel_handle;
static esp_lcd_panel_io_handle_t s_panel_io_handle;
static esp_lv_adapter_rotation_t s_rotation;
static esp_lv_adapter_tear_avoid_mode_t s_tear_mode;

static void create_demo_ui(void);
static void sleep_button_event_cb(lv_event_t *e);
static void enter_light_sleep(void);
static void bind_encoder_to_ui(void);
static esp_lv_adapter_rotation_t get_configured_rotation(void);
static esp_lv_adapter_tear_avoid_mode_t get_default_tear_mode(void);

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

static esp_lv_adapter_tear_avoid_mode_t get_default_tear_mode(void)
{
#if CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI
    return ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_MIPI_DSI;
#elif CONFIG_EXAMPLE_LCD_INTERFACE_RGB
    return ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB;
#else
    return ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT;
#endif
}

static void sleep_button_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ESP_LOGI(TAG, "Sleep button pressed, entering light sleep...");
    enter_light_sleep();
}

static void create_demo_ui(void)
{
    if (esp_lv_adapter_lock(-1) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to lock LVGL for UI creation");
        return;
    }

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101820), 0);

    s_status_label = lv_label_create(scr);
    lv_obj_set_width(s_status_label, LV_PCT(90));
    lv_label_set_long_mode(s_status_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text_fmt(s_status_label,
                          "Press button to enter light sleep.\nWake timer: %d ms",
                          CONFIG_EXAMPLE_LIGHT_SLEEP_WAKE_TIMER_MS);
    lv_obj_align(s_status_label, LV_ALIGN_TOP_MID, 0, 20);

    s_sleep_button = lv_btn_create(scr);
    lv_obj_set_size(s_sleep_button, LV_PCT(60), 60);
    lv_obj_align(s_sleep_button, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(s_sleep_button, sleep_button_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(s_sleep_button);
    lv_label_set_text(btn_label, "Sleep Now");
    lv_obj_center(btn_label);

    esp_lv_adapter_unlock();

    bind_encoder_to_ui();
}

static void enter_light_sleep(void)
{
    // Step 1: Prepare adapter for sleep - pauses worker and waits for flush completion
    ESP_LOGI(TAG, "Step 1: Preparing adapter for sleep");
    ESP_ERROR_CHECK(esp_lv_adapter_sleep_prepare());

    // Step 2: Deinitialize LCD hardware to free resources
    ESP_LOGI(TAG, "Step 2: Deinitializing LCD hardware");
    ESP_ERROR_CHECK(hw_lcd_deinit());

    // Step 3: Configure wake-up timer
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup((uint64_t)CONFIG_EXAMPLE_LIGHT_SLEEP_WAKE_TIMER_MS * 1000ULL));

    // Step 4: Enter light sleep
    ESP_LOGI(TAG, "Step 4: Entering light sleep for %d ms", CONFIG_EXAMPLE_LIGHT_SLEEP_WAKE_TIMER_MS);
    ESP_ERROR_CHECK(esp_light_sleep_start());

    // Step 5: Reinitialize LCD hardware after wake-up
    ESP_LOGI(TAG, "Step 5: Reinitializing LCD hardware after wake-up");
    ESP_ERROR_CHECK(hw_lcd_init(&s_panel_handle, &s_panel_io_handle, s_tear_mode, s_rotation));

    // Step 6: Recover adapter - rebinds panel and resumes worker
    ESP_LOGI(TAG, "Step 6: Recovering adapter");
    ESP_ERROR_CHECK(esp_lv_adapter_sleep_recover(s_disp, s_panel_handle, s_panel_io_handle));
}

void app_main(void)
{
    ESP_LOGI(TAG, "LVGL Light Sleep Demo");
    ESP_LOGI(TAG, "LCD resolution %dx%d", HW_LCD_H_RES, HW_LCD_V_RES);

    // Get configuration
    s_rotation = get_configured_rotation();
    s_tear_mode = get_default_tear_mode();

    // Initialize LCD hardware
    ESP_ERROR_CHECK(hw_lcd_init(&s_panel_handle, &s_panel_io_handle, s_tear_mode, s_rotation));

    // Initialize adapter
    esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(esp_lv_adapter_init(&adapter_config));

    // Register display
#if CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI
    esp_lv_adapter_display_config_t display_cfg = ESP_LV_ADAPTER_DISPLAY_MIPI_DEFAULT_CONFIG(
                                                      s_panel_handle, s_panel_io_handle,
                                                      HW_LCD_H_RES, HW_LCD_V_RES, s_rotation);
#elif CONFIG_EXAMPLE_LCD_INTERFACE_RGB
    esp_lv_adapter_display_config_t display_cfg = ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(
                                                      s_panel_handle, s_panel_io_handle,
                                                      HW_LCD_H_RES, HW_LCD_V_RES, s_rotation);
#elif CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITHOUT_PSRAM
    esp_lv_adapter_display_config_t display_cfg = ESP_LV_ADAPTER_DISPLAY_SPI_WITHOUT_PSRAM_DEFAULT_CONFIG(
                                                      s_panel_handle, s_panel_io_handle,
                                                      HW_LCD_H_RES, HW_LCD_V_RES, s_rotation);
#else
    esp_lv_adapter_display_config_t display_cfg = ESP_LV_ADAPTER_DISPLAY_SPI_WITH_PSRAM_DEFAULT_CONFIG(
                                                      s_panel_handle, s_panel_io_handle,
                                                      HW_LCD_H_RES, HW_LCD_V_RES, s_rotation);
#endif

    s_disp = esp_lv_adapter_register_display(&display_cfg);
    if (s_disp == NULL) {
        ESP_LOGE(TAG, "Display registration failed");
        return;
    }

    // Register input device (optional)
#if HW_USE_TOUCH
    esp_lcd_touch_handle_t touch_handle = NULL;
    ESP_ERROR_CHECK(hw_touch_init(&touch_handle, s_rotation));
    esp_lv_adapter_touch_config_t touch_cfg = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(s_disp, touch_handle);
    esp_lv_adapter_register_touch(&touch_cfg);
#elif HW_USE_ENCODER && CONFIG_ESP_LVGL_ADAPTER_ENABLE_KNOB
    esp_lv_adapter_encoder_config_t encoder_cfg = {
        .disp = s_disp,
        .encoder_a_b = hw_knob_get_config(),
        .encoder_enter = hw_knob_get_button(),
    };
    s_encoder = esp_lv_adapter_register_encoder(&encoder_cfg);
    if (s_encoder == NULL) {
        ESP_LOGE(TAG, "Encoder registration failed");
    }
#endif

    // Start adapter
    ESP_ERROR_CHECK(esp_lv_adapter_start());

    // Create UI
    create_demo_ui();
}

static void bind_encoder_to_ui(void)
{
    if (!s_encoder || !s_sleep_button) {
        return;
    }

    if (esp_lv_adapter_lock(-1) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to lock LVGL for encoder binding");
        return;
    }

    lv_group_t *group = lv_group_get_default();
    if (!group) {
        group = lv_group_create();
        lv_group_set_default(group);
    }

    if (!group) {
        ESP_LOGE(TAG, "Failed to create default input group");
        esp_lv_adapter_unlock();
        return;
    }

    lv_group_remove_all_objs(group);
    lv_group_add_obj(group, s_sleep_button);
    lv_group_focus_obj(s_sleep_button);
    lv_indev_set_group(s_encoder, group);

    esp_lv_adapter_unlock();
}
