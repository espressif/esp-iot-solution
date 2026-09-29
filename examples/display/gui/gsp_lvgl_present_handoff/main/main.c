/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "driver/gpio.h"
#include "esp_display_present.h"
#include "esp_gsp_esp_lcd.h"
#include "esp_lv_adapter_display.h"
#include "handoff_stress_runner.h"
#include "hw_init.h"

#include "bundle_gsp.h"

void app_main(void)
{
    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_io_handle_t io = NULL;

#if CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI
    const esp_lv_adapter_tear_avoid_mode_t tear_mode =
        ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_MIPI_DSI;
    const esp_display_present_panel_t panel_type = ESP_DISPLAY_PRESENT_PANEL_MIPI_DSI;
    const bool swap_bytes = false;
#elif CONFIG_EXAMPLE_LCD_INTERFACE_RGB
    const esp_lv_adapter_tear_avoid_mode_t tear_mode =
        ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB;
    const esp_display_present_panel_t panel_type = ESP_DISPLAY_PRESENT_PANEL_RGB;
    const bool swap_bytes = false;
#else
    const int te_probe = hw_lcd_get_te_gpio();
    const esp_lv_adapter_tear_avoid_mode_t tear_mode =
        (te_probe != GPIO_NUM_NC) ? ESP_LV_ADAPTER_TEAR_AVOID_MODE_TE_SYNC
        : ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT;
    const esp_display_present_panel_t panel_type = ESP_DISPLAY_PRESENT_PANEL_IO;
    const bool swap_bytes = true;
#endif

    ESP_ERROR_CHECK(hw_lcd_init(&panel, &io,
                                esp_lv_adapter_get_required_frame_buffer_count(tear_mode, ESP_LV_ADAPTER_ROTATE_0),
                                HW_ROTATE_0));

    const uint8_t bpp = hw_lcd_get_bits_per_pixel();
    const int te_gpio = hw_lcd_get_te_gpio();
    const esp_display_present_target_config_t display_cfg = {
        .hw = {
            .panel = panel,
            .io = io,
            .panel_type = panel_type,
            .input_pixel_format = (bpp == 24)
            ? ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB888
            : ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565,
            .rotation = ESP_DISPLAY_PRESENT_ROTATE_0,
            .swap_bytes = swap_bytes,
            .te_enabled = (te_gpio != GPIO_NUM_NC),
            .te_sync = {
                .gpio_num = te_gpio,
                .bus_freq_hz = hw_lcd_get_bus_freq_hz(),
                .data_lines = hw_lcd_get_bus_data_lines(),
            },
        },
        .fb = {
            .mode = ESP_DISPLAY_PRESENT_MODE_AUTO,
        },
    };

    esp_lcd_touch_handle_t touch = NULL;
#if HW_USE_TOUCH
    (void)hw_touch_init(&touch, HW_ROTATE_0);
#endif

    /* The app owns the presenter. GSP and LVGL borrow it in turn. */
    const esp_display_presenter_config_t presenter_config = {
        .width = HW_LCD_H_RES,
        .height = HW_LCD_V_RES,
        .pixel_format = display_cfg.hw.input_pixel_format,
        .max_damage_areas = 16,
        .target = display_cfg,
    };
    esp_display_presenter_t *presenter = NULL;
    ESP_ERROR_CHECK(
        esp_display_presenter_create(&presenter_config, &presenter));

    esp_gsp_handle_t gsp = NULL;
    esp_gsp_config_t app_config = gsp_bundle_config();
    esp_gsp_esp_lcd_config_t esp_config = ESP_GSP_ESP_LCD_CONFIG_INIT();
    esp_config.presenter = presenter;
    esp_config.touch = touch;
    esp_config.perf_log = true;
    ESP_ERROR_CHECK(esp_gsp_esp_lcd_start(&app_config, &esp_config, &gsp));

    handoff_stress_run(presenter, gsp);
}
