/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_memory_utils.h"
#include "esp_timer.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "lvgl_ppa_blend.h"
#include "lcd_ppa_blend.h"
#include "ppa_blend.h"
#include "camera_ppa_blend.h"
#include "app_lcd.h"
#include "lvgl.h"
#include "ui.h"

static void periodic_timer_callback(void* arg);

void app_main(void)
{
    // Initialize lcd panel
    esp_lcd_panel_handle_t lcd = NULL;           // LCD panel handle
    esp_lcd_touch_handle_t tp = NULL;            // LCD touch panel handle

    ESP_ERROR_CHECK(app_lcd_init(&lcd));
    ESP_ERROR_CHECK(app_touch_init(&tp));

    // Initialize PPA blend
    ppa_blend_config_t ppa_config = {
        .task_priority = 5,
        .display_panel = lcd,
    };
    ESP_ERROR_CHECK(ppa_blend_new(&ppa_config));

    // Initialize Camera PPA blend
    ESP_ERROR_CHECK(camera_ppa_blend_init());
    // Register the user callback function for the PPA blend operation done
    ppa_blend_register_notify_done_callback(camera_ppa_blend_notify_done);

    // Register the user data flow start callback functions for the PPA blend operation
    lvgl_ppa_blend_register_switch_on_callback(camera_stream_switch_on);

    // Register the user data flow stop callback functions for the PPA blend operation
    lvgl_ppa_blend_register_switch_off_callback(camera_stream_switch_off);

    // Initialize LCD PPA blend
    ESP_ERROR_CHECK(lcd_ppa_blend_init(lcd));

    // Initialize LVGL PPA blend
    lv_display_t *disp = NULL;
    lv_indev_t *disp_indev = NULL;
    ESP_ERROR_CHECK(lvgl_ppa_blend_init(lcd, tp, &disp, &disp_indev));

    lvgl_ppa_blend_lock(0);

    ui_init();

    lvgl_ppa_blend_unlock();

    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &periodic_timer_callback,
        /* name is optional, but may help identify the timer when debugging */
        .name = "periodic"
    };

    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 2200000));
}

static void periodic_timer_callback(void* arg)
{
    static int index = 0;
    index++;
    if (index % 3 == 0) {
        lvgl_mode_change(LVGL_PPA_BLEND_COLOR_MODE_RGB565);

        lv_disp_load_scr(ui_ScreenRGB565);
    } else if (index % 3 == 1) {
        lvgl_mode_change(LVGL_PPA_BLEND_COLOR_MODE_ARGB8888);

        lv_disp_load_scr(ui_ScreenRGB8888);
    } else {
        lvgl_mode_change(LVGL_PPA_BLEND_USER_DATA_MODE);
    }
}
