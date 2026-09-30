/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdint.h>

#include "esp_display_present.h"
#include "esp_gsp_esp_lcd.h"
#include "esp_log.h"
#include "hw_init.h"
#include "showcase_ui.h"
#include "bundle_gsp.h"

static const char *TAG = "gsp_showcase";
extern const uint8_t runtime_badge_start[] asm("_binary_runtime_badge_png_start");
extern const uint8_t runtime_badge_end[] asm("_binary_runtime_badge_png_end");

#if HW_USE_ENCODER
static void knob_left(void *knob, void *ctx)
{
    (void)knob;
    showcase_ui_knob_turn(ctx, -1);
}

static void knob_right(void *knob, void *ctx)
{
    (void)knob;
    showcase_ui_knob_turn(ctx, 1);
}

static void knob_click(void *button, void *ctx)
{
    (void)button;
    showcase_ui_knob_click(ctx);
}

static void knob_long(void *button, void *ctx)
{
    (void)button;
    showcase_ui_knob_long(ctx);
}
#endif

void app_main(void)
{
    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_io_handle_t io = NULL;
#if CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI
    const esp_display_present_panel_t panel_type = ESP_DISPLAY_PRESENT_PANEL_MIPI_DSI;
    const bool swap_bytes = false;
#elif CONFIG_EXAMPLE_LCD_INTERFACE_RGB
    const esp_display_present_panel_t panel_type = ESP_DISPLAY_PRESENT_PANEL_RGB;
    const bool swap_bytes = false;
#else
    const esp_display_present_panel_t panel_type = ESP_DISPLAY_PRESENT_PANEL_IO;
    const bool swap_bytes = true;
#endif
    if (hw_lcd_get_bits_per_pixel() != 16) {
        ESP_LOGE(TAG, "The selected panel must use RGB565");
        ESP_ERROR_CHECK(ESP_ERR_NOT_SUPPORTED);
    }
    const int te_gpio = hw_lcd_get_te_gpio();
    esp_display_present_target_config_t display = {
        .hw = {
            .panel_type = panel_type,
            .input_pixel_format = ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565,
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

    uint8_t frame_buffer_count = 0;
    ESP_ERROR_CHECK(esp_display_present_get_required_frame_buffer_count(
                        &display, &frame_buffer_count));
    ESP_ERROR_CHECK(hw_lcd_init(&panel, &io, frame_buffer_count, HW_ROTATE_0));
    display.hw.panel = panel;
    display.hw.io = io;

    esp_lcd_touch_handle_t touch = NULL;
#if HW_USE_TOUCH
    ESP_ERROR_CHECK(hw_touch_init(&touch, HW_ROTATE_0));
#endif

    esp_gsp_config_t app_config = gsp_bundle_config();
    esp_gsp_esp_lcd_config_t lcd_config = ESP_GSP_ESP_LCD_CONFIG_INIT();
    lcd_config.display = display;
    lcd_config.touch = touch;

    esp_gsp_handle_t ui = NULL;
    ESP_ERROR_CHECK(esp_gsp_esp_lcd_start(&app_config, &lcd_config, &ui));
    if (showcase_ui_init(ui, runtime_badge_start, runtime_badge_end - runtime_badge_start) != ESP_GSP_OK) {
        ESP_LOGE(TAG, "Failed to initialize showcase UI");
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }
#if HW_USE_ENCODER
    knob_handle_t knob = iot_knob_create(hw_knob_get_config());
    if (knob == NULL) {
        ESP_LOGE(TAG, "Failed to create navigation knob");
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }
    ESP_ERROR_CHECK(iot_knob_register_cb(knob, KNOB_LEFT, knob_left, ui));
    ESP_ERROR_CHECK(iot_knob_register_cb(knob, KNOB_RIGHT, knob_right, ui));
    button_handle_t button = hw_knob_get_button();
    if (button == NULL) {
        ESP_LOGE(TAG, "Failed to create navigation button");
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }
    ESP_ERROR_CHECK(iot_button_register_cb(button, BUTTON_SINGLE_CLICK, NULL, knob_click, ui));
    ESP_ERROR_CHECK(iot_button_register_cb(button, BUTTON_LONG_PRESS_START, NULL, knob_long, ui));
#endif
    ESP_LOGI(TAG, "ESP-GSP showcase started: %dx%d RGB565, four scenes", HW_LCD_H_RES, HW_LCD_V_RES);
}
