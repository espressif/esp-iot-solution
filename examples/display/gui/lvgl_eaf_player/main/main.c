/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/**
 * @file main.c
 * @brief LVGL EAF animation player example
 *
 * This example demonstrates how to play EAF (Emote Animation Format) animations
 * using the esp_lv_eaf_player component with LVGL filesystem support.
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
#include "lvgl.h"
#include "lv_eaf.h"
#include "esp_mmap_assets.h"
#include "mmap_generate_eaf.h"

static const char *TAG = "eaf_player";

/* EAF player context */
static mmap_assets_handle_t s_assets;
static esp_lv_fs_handle_t s_fs_handle;
static lv_obj_t *s_eaf_anim;
static lv_obj_t *s_info_label;
static lv_obj_t *s_play_btn;
static bool s_is_playing = true;

/**
 * @brief Play/Pause button click handler
 */
static void play_btn_event_cb(lv_event_t *e)
{
    if (s_eaf_anim == NULL || !lv_eaf_is_loaded(s_eaf_anim)) {
        return;
    }

    s_is_playing = !s_is_playing;

    if (s_is_playing) {
        lv_eaf_resume(s_eaf_anim);
        lv_label_set_text(lv_obj_get_child(s_play_btn, 0), LV_SYMBOL_PAUSE);
    } else {
        lv_eaf_pause(s_eaf_anim);
        lv_label_set_text(lv_obj_get_child(s_play_btn, 0), LV_SYMBOL_PLAY);
    }
}

/**
 * @brief Mount EAF assets partition and register LVGL filesystem
 */
static esp_err_t mount_eaf_assets(void)
{
    mmap_assets_config_t cfg = {
        .partition_label = "eaf",
        .max_files = MMAP_EAF_FILES,
        .checksum = MMAP_EAF_CHECKSUM,
        .flags = { .mmap_enable = 1 },
    };

    ESP_RETURN_ON_ERROR(mmap_assets_new(&cfg, &s_assets), TAG, "mmap init failed");

    size_t file_count = mmap_assets_get_stored_files(s_assets);
    if (file_count == 0) {
        mmap_assets_del(s_assets);
        return ESP_ERR_NOT_FOUND;
    }

    fs_cfg_t fs_cfg = {
        .fs_letter = 'E',
        .fs_nums = file_count,
        .fs_assets = s_assets,
    };

    ESP_RETURN_ON_ERROR(esp_lv_adapter_fs_mount(&fs_cfg, &s_fs_handle), TAG, "fs mount failed");
    ESP_LOGI(TAG, "EAF assets mounted on drive E:");

    return ESP_OK;
}

/**
 * @brief Start EAF animation player
 */
static void start_eaf_player(lv_display_t *disp)
{
    if (esp_lv_adapter_lock(-1) != ESP_OK) {
        return;
    }

    lv_obj_t *screen = lv_disp_get_scr_act(disp);

    /* Create info label */
    s_info_label = lv_label_create(screen);
    lv_obj_set_width(s_info_label, LV_PCT(100));
    lv_obj_set_style_text_align(s_info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_info_label, LV_ALIGN_TOP_MID, 0, 10);

    /* Create EAF animation object */
    s_eaf_anim = lv_eaf_create(screen);
    lv_obj_align(s_eaf_anim, LV_ALIGN_CENTER, 0, 0);

    /* Build file path and load animation */
    const char *file_name = mmap_assets_get_name(s_assets, 0);
    if (file_name) {
        char path[64];
        snprintf(path, sizeof(path), "E:%s", file_name);

        /* Load EAF animation */
        lv_eaf_set_src(s_eaf_anim, path);

        if (lv_eaf_is_loaded(s_eaf_anim)) {
            /* Configure playback: 30ms per frame (~33 FPS), infinite loop */
            lv_eaf_set_frame_delay(s_eaf_anim, 30);
            lv_eaf_set_loop_enabled(s_eaf_anim, true);

            /* Update info label */
            int32_t frames = lv_eaf_get_total_frames(s_eaf_anim);
            char info[48];
            snprintf(info, sizeof(info), "EAF: %ld frames", (long)frames);
            lv_label_set_text(s_info_label, info);

            /* Create play/pause button */
            s_play_btn = lv_btn_create(screen);
            lv_obj_set_size(s_play_btn, 50, 50);
            lv_obj_align(s_play_btn, LV_ALIGN_BOTTOM_MID, 0, -10);
            lv_obj_add_event_cb(s_play_btn, play_btn_event_cb, LV_EVENT_CLICKED, NULL);

            lv_obj_t *btn_label = lv_label_create(s_play_btn);
            lv_label_set_text(btn_label, LV_SYMBOL_PAUSE);
            lv_obj_center(btn_label);

            ESP_LOGI(TAG, "Playing: %s (%ld frames)", path, (long)frames);
        } else {
            lv_label_set_text(s_info_label, "Load failed!");
        }
    } else {
        lv_label_set_text(s_info_label, "No file!");
    }

    esp_lv_adapter_unlock();
}

/**
 * @brief Get configured display rotation
 */
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

void app_main(void)
{
    esp_lcd_panel_handle_t display_panel = NULL;
    esp_lcd_panel_io_handle_t display_io_handle = NULL;
    esp_lv_adapter_rotation_t rotation = get_configured_rotation();

    /* Select tear effect mode based on LCD interface type */
#if CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI
    esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_MIPI_DSI;
#elif CONFIG_EXAMPLE_LCD_INTERFACE_RGB
    esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB;
#else
    esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT;
#endif

    /* Initialize LCD hardware */
    ESP_LOGI(TAG, "Init LCD %dx%d", HW_LCD_H_RES, HW_LCD_V_RES);
    ESP_ERROR_CHECK(hw_lcd_init(&display_panel, &display_io_handle, tear_avoid_mode, rotation));

    /* Initialize LVGL adapter */
    esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(esp_lv_adapter_init(&adapter_config));

    /* Register display */
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
    if (disp == NULL) {
        ESP_LOGE(TAG, "Display register failed");
        return;
    }

    /* Initialize touch if available */
#if HW_USE_TOUCH
    esp_lcd_touch_handle_t touch_handle = NULL;
    ESP_ERROR_CHECK(hw_touch_init(&touch_handle, rotation));
    esp_lv_adapter_touch_config_t touch_config = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, touch_handle);
    esp_lv_adapter_register_touch(&touch_config);
#endif

    /* Start LVGL adapter */
    ESP_ERROR_CHECK(esp_lv_adapter_start());

    /* Mount EAF assets and start player */
    ESP_ERROR_CHECK(mount_eaf_assets());
    start_eaf_player(disp);
}
