/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief Minimal OLED initialization demo using esp_lvgl_adapter
 */

#include "esp_err.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "lvgl.h"
#include "hw_oled.h"

static const char *TAG = "mono_demo";

LV_FONT_DECLARE(font_puhui_14_1);

void app_main(void)
{
    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_io_handle_t panel_io = NULL;

    ESP_LOGI(TAG, "Initialize OLED panel");
    ESP_ERROR_CHECK(hw_oled_init(&panel, &panel_io));

    ESP_LOGI(TAG, "Initialize LVGL adapter");
    const esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(esp_lv_adapter_init(&adapter_config));

    ESP_LOGI(TAG, "Register OLED display");
    const esp_lv_adapter_display_config_t display_config =
        ESP_LV_ADAPTER_DISPLAY_SPI_MONO_DEFAULT_CONFIG(
            panel,
            panel_io,
            HW_OLED_H_RES,
            HW_OLED_V_RES,
            ESP_LV_ADAPTER_ROTATE_0,
            ESP_LV_ADAPTER_MONO_LAYOUT_VTILED);

    lv_display_t *disp = esp_lv_adapter_register_display(&display_config);
    if (!disp) {
        ESP_LOGE(TAG, "Display registration failed");
        return;
    }

    ESP_LOGI(TAG, "Start LVGL adapter task");
    ESP_ERROR_CHECK(esp_lv_adapter_start());

    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        lv_obj_t *scr = lv_scr_act();

        lv_obj_t *label_top = lv_label_create(scr);
        lv_obj_set_style_text_font(label_top, &font_puhui_14_1, 0);
        lv_label_set_text(label_top, "你好");
        lv_obj_align(label_top, LV_ALIGN_TOP_MID, 0, 0);

        lv_obj_t *label_bottom = lv_label_create(scr);
        lv_obj_set_style_text_font(label_bottom, &font_puhui_14_1, 0);
        lv_label_set_text(label_bottom, "ESP LVGL Adapter");
        lv_obj_align(label_bottom, LV_ALIGN_CENTER, 0, 0);

        esp_lv_adapter_unlock();
    } else {
        ESP_LOGW(TAG, "Failed to lock LVGL adapter");
    }

    ESP_LOGI(TAG, "OLED initialization complete");
}
