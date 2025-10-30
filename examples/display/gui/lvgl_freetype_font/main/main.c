/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/**
 * @file main.c
 * @brief Unified LVGL demo supporting multiple LCD interface types
 *
 * This example demonstrates how to use LVGL with different LCD interfaces
 * (MIPI DSI, QSPI, RGB, SPI) in a single unified codebase.
 * The interface type and hardware configuration can be selected via menuconfig.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "sdkconfig.h"
#include "hw_init.h"
#include "esp_lv_adapter.h"

#include "mmap_generate_fonts.h"

static const char *TAG = "freetype_demo";

#define FONT_DRIVE_LETTER   'F'
#define FONT_FILE_NAME      "DejaVuSans.ttf"
#define FONT_LABEL_PRESS    "Press me"
#define FONT_LABEL_SMALL    "Sample text size 30"
#define FONT_LABEL_LARGE    "Sample text size 48"
#define ENCODER_THRESHOLD   3

static mmap_assets_handle_t s_font_assets;
static esp_lv_fs_handle_t s_fs_handle;
static esp_lv_adapter_ft_font_handle_t s_font_handle_30;
static esp_lv_adapter_ft_font_handle_t s_font_handle_48;
static const lv_font_t *s_font_30;
static const lv_font_t *s_font_48;
static lv_obj_t *s_label_small;
static lv_obj_t *s_label_large;
static lv_obj_t *s_button;
static char s_font_path[32];
static bool s_compact_mode = false;
static uint32_t s_press_count = 0;

#if HW_USE_ENCODER
static int32_t s_encoder_accumulator = 0;
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

static void button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        s_press_count++;

        if (s_label_small) {
            lv_label_set_text_fmt(s_label_small, "Pressed %u", (unsigned int)s_press_count);
        }

        if (s_label_large && !s_compact_mode) {
            lv_label_set_text_fmt(s_label_large, "Pressed %u", (unsigned int)s_press_count);
        }
    }
#if HW_USE_ENCODER
    else if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        ESP_LOGI(TAG, "Key event: %lu", key);

        if (key == LV_KEY_LEFT || key == LV_KEY_RIGHT) {
            s_encoder_accumulator += (key == LV_KEY_RIGHT) ? 1 : -1;
            ESP_LOGI(TAG, "Encoder count: %ld", s_encoder_accumulator);

            if (s_encoder_accumulator >= ENCODER_THRESHOLD || s_encoder_accumulator <= -ENCODER_THRESHOLD) {
                s_press_count++;

                if (s_label_small) {
                    lv_label_set_text_fmt(s_label_small, "Count %u", (unsigned int)s_press_count);
                }

                if (s_label_large && !s_compact_mode) {
                    lv_label_set_text_fmt(s_label_large, "Count %u", (unsigned int)s_press_count);
                }

                s_encoder_accumulator = 0;
            }
        }
    }
#endif
}

static esp_err_t mount_font_assets(void)
{
    if (s_font_assets) {
        return ESP_OK;
    }

    const mmap_assets_config_t cfg = {
        .partition_label = "fonts",
        .max_files = MMAP_FONTS_FILES,
        .checksum = MMAP_FONTS_CHECKSUM,
        .flags = {
            .mmap_enable = 1,
        },
    };

    ESP_RETURN_ON_ERROR(mmap_assets_new(&cfg, &s_font_assets), TAG, "Failed to map font assets");

    size_t stored_files = mmap_assets_get_stored_files(s_font_assets);
    ESP_RETURN_ON_FALSE(stored_files > 0, ESP_ERR_NOT_FOUND, TAG, "No font files found in partition");

    fs_cfg_t fs_cfg = {
        .fs_letter = FONT_DRIVE_LETTER,
        .fs_nums = (int)stored_files,
        .fs_assets = s_font_assets,
    };

    ESP_RETURN_ON_ERROR(esp_lv_adapter_fs_mount(&fs_cfg, &s_fs_handle), TAG, "Failed to mount font filesystem");

    int written = snprintf(s_font_path, sizeof(s_font_path), "%c:%s", FONT_DRIVE_LETTER, FONT_FILE_NAME);
    ESP_RETURN_ON_FALSE(written > 0 && written < (int)sizeof(s_font_path), ESP_ERR_INVALID_SIZE, TAG, "Failed to build font path");

    return ESP_OK;
}

static esp_err_t init_fonts(void)
{
    if (s_font_handle_30 && s_font_handle_48) {
        return ESP_OK;
    }

    /* Configure 30px font using macro */
    const esp_lv_adapter_ft_font_config_t cfg_font30 = ESP_LV_ADAPTER_FT_FONT_FILE_CONFIG(
                                                           s_font_path, 30, ESP_LV_ADAPTER_FT_FONT_STYLE_NORMAL);

    ESP_RETURN_ON_ERROR(esp_lv_adapter_ft_font_init(&cfg_font30, &s_font_handle_30), TAG, "Failed to init 30px font");
    s_font_30 = esp_lv_adapter_ft_font_get(s_font_handle_30);
    ESP_RETURN_ON_FALSE(s_font_30, ESP_ERR_INVALID_STATE, TAG, "Invalid LVGL font pointer for 30px font");

    /* Configure 48px font using macro */
    const esp_lv_adapter_ft_font_config_t cfg_font48 = ESP_LV_ADAPTER_FT_FONT_FILE_CONFIG(
                                                           s_font_path, 48, ESP_LV_ADAPTER_FT_FONT_STYLE_NORMAL);

    ESP_RETURN_ON_ERROR(esp_lv_adapter_ft_font_init(&cfg_font48, &s_font_handle_48), TAG, "Failed to init 48px font");
    s_font_48 = esp_lv_adapter_ft_font_get(s_font_handle_48);
    ESP_RETURN_ON_FALSE(s_font_48, ESP_ERR_INVALID_STATE, TAG, "Invalid LVGL font pointer for 48px font");

    return ESP_OK;
}

static void create_freetype_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x364F6B), 0);

    lv_obj_t *label_title = lv_label_create(scr);
    lv_obj_set_style_text_font(label_title, s_font_30, 0);
    lv_obj_set_style_text_color(label_title, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text_fmt(label_title, "%s", FONT_FILE_NAME);
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, s_compact_mode ? 4 : 16);

    s_label_small = lv_label_create(scr);
    lv_obj_set_style_text_font(s_label_small, s_font_30, 0);
    lv_obj_set_style_text_color(s_label_small, lv_color_hex(0xFFFFFF), 0);

    if (s_compact_mode) {
        /* Show shorter text in compact mode */
        lv_label_set_text(s_label_small, "Text size 30");
        lv_obj_align(s_label_small, LV_ALIGN_CENTER, 0, -20);
    } else {
        lv_label_set_text(s_label_small, FONT_LABEL_SMALL);
        lv_obj_align(s_label_small, LV_ALIGN_CENTER, 0, -40);
    }

    /* Only show large font label in non-compact mode */
    if (!s_compact_mode) {
        s_label_large = lv_label_create(scr);
        lv_obj_set_style_text_font(s_label_large, s_font_48, 0);
        lv_obj_set_style_text_color(s_label_large, lv_color_hex(0xFFFFFF), 0);
        lv_label_set_text(s_label_large, FONT_LABEL_LARGE);
        lv_obj_align(s_label_large, LV_ALIGN_CENTER, 0, 30);
    } else {
        s_label_large = NULL;
    }

    s_button = lv_btn_create(scr);

    if (s_compact_mode) {
        /* Compact layout for 240x240 displays */
        lv_obj_set_size(s_button, 160, 50);
        lv_obj_set_style_pad_hor(s_button, 8, 0);
        lv_obj_set_style_pad_ver(s_button, 4, 0);
        lv_obj_align(s_button, LV_ALIGN_BOTTOM_MID, 0, -8);
    } else {
        /* Standard layout */
        lv_obj_set_size(s_button, 200, 70);
        lv_obj_align(s_button, LV_ALIGN_BOTTOM_MID, 0, -24);
    }

    lv_obj_set_style_bg_color(s_button, lv_color_hex(0x3FC1C9), 0);
    lv_obj_add_event_cb(s_button, button_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_btn = lv_label_create(s_button);
    lv_obj_set_style_text_font(label_btn, s_font_30, 0);

#if HW_USE_ENCODER
    lv_label_set_text(label_btn, s_compact_mode ? "Count" : FONT_LABEL_PRESS);
    lv_obj_add_event_cb(s_button, button_event_cb, LV_EVENT_KEY, NULL);
#else
    lv_label_set_text(label_btn, FONT_LABEL_PRESS);
#endif

    lv_obj_center(label_btn);

    /* Add background effect in compact mode */
    if (s_compact_mode) {
        lv_obj_set_style_bg_opa(label_title, LV_OPA_60, 0);
        lv_obj_set_style_bg_color(label_title, lv_color_hex(0x303030), 0);
    }
}

void app_main(void)
{
    esp_lcd_panel_handle_t display_panel = NULL;
    esp_lcd_panel_io_handle_t display_io_handle = NULL;
    esp_lv_adapter_rotation_t rotation = get_configured_rotation();

#if CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI
    esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_MIPI_DSI;
    ESP_LOGI(TAG, "Selected LCD interface: MIPI DSI");
#elif CONFIG_EXAMPLE_LCD_INTERFACE_RGB
    esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB;
    ESP_LOGI(TAG, "Selected LCD interface: RGB");
#else
    esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT;
#if CONFIG_EXAMPLE_LCD_INTERFACE_QSPI
    ESP_LOGI(TAG, "Selected LCD interface: QSPI");
#elif CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITH_PSRAM
    ESP_LOGI(TAG, "Selected LCD interface: SPI (with PSRAM)");
#elif CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITHOUT_PSRAM
    ESP_LOGI(TAG, "Selected LCD interface: SPI (without PSRAM)");
#endif
#endif

    ESP_LOGI(TAG, "Initializing LCD: %dx%d", HW_LCD_H_RES, HW_LCD_V_RES);

    /* Detect compact mode for small displays (240x240 or smaller) */
    s_compact_mode = (HW_LCD_H_RES <= 240) || (HW_LCD_V_RES <= 240);
    if (s_compact_mode) {
        ESP_LOGI(TAG, "Compact mode enabled for small display");
    }

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
        ESP_LOGE(TAG, "Failed to register display");
        return;
    }

#if HW_USE_TOUCH
    ESP_LOGI(TAG, "Initializing touch panel");
    esp_lcd_touch_handle_t touch_handle = NULL;
    ESP_ERROR_CHECK(hw_touch_init(&touch_handle, rotation));

    esp_lv_adapter_touch_config_t touch_config = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, touch_handle);

    if (!esp_lv_adapter_register_touch(&touch_config)) {
        ESP_LOGE(TAG, "Failed to register touch input");
        return;
    }
#elif HW_USE_ENCODER && CONFIG_ESP_LVGL_ADAPTER_ENABLE_KNOB
    ESP_LOGI(TAG, "Initializing encoder");
    esp_lv_adapter_encoder_config_t encoder_config = {
        .disp = disp,
        .encoder_a_b = hw_knob_get_config(),
        .encoder_enter = hw_knob_get_button(),
    };

    lv_indev_t *encoder = esp_lv_adapter_register_encoder(&encoder_config);
    if (!encoder) {
        ESP_LOGE(TAG, "Failed to register encoder input");
        return;
    }
#endif

    ESP_ERROR_CHECK(esp_lv_adapter_start());
    ESP_ERROR_CHECK(mount_font_assets());
    ESP_ERROR_CHECK(init_fonts());

    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        create_freetype_screen();

#if HW_USE_ENCODER
        /* Create input group for encoder */
        lv_group_t *group = lv_group_create();
        lv_group_add_obj(group, s_button);
        lv_group_focus_obj(s_button);
        lv_indev_set_group(encoder, group);
        ESP_LOGI(TAG, "Encoder group configured");
#endif

        esp_lv_adapter_unlock();
    } else {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock for UI creation");
    }
}
