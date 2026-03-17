/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file example_lvgl_init.c
 * @brief Reference implementation — LCD + LVGL adapter initialisation
 *
 * This file is intentionally written in a flat, step-by-step style so that
 * it can serve as a copy-paste starting point for new projects.  Every major
 * step is separated by a comment banner.
 */

#include "example_lvgl_init.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

static const char *TAG = "example_lvgl_init";

/* ---------------------------------------------------------------------------
 * Step 1 helper: read display rotation from Kconfig
 * -------------------------------------------------------------------------*/
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

/* ---------------------------------------------------------------------------
 * Step 8 helper: FPS monitor task (only compiled when Kconfig option is on)
 * -------------------------------------------------------------------------*/
#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FPS_STATS
#define FPS_MONITOR_TASK_STACK_SIZE  4096
#define FPS_MONITOR_TASK_PRIORITY    3
#define FPS_MONITOR_INTERVAL_MS      1000

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
#endif /* CONFIG_ESP_LVGL_ADAPTER_ENABLE_FPS_STATS */

/* ---------------------------------------------------------------------------
 * Main initialisation sequence
 * -------------------------------------------------------------------------*/
esp_err_t example_lvgl_init(example_lvgl_ctx_t *ctx)
{
    ESP_RETURN_ON_FALSE(ctx, ESP_ERR_INVALID_ARG, TAG, "ctx must not be NULL");
    memset(ctx, 0, sizeof(*ctx));

    /* ── Step 1: Rotation ──────────────────────────────────────────────── */
    ctx->rotation = get_configured_rotation();

    /* ── Step 2: Tear-avoidance mode ───────────────────────────────────── */
#if CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI
    ctx->tear_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_MIPI_DSI;
    ESP_LOGI(TAG, "LCD interface: MIPI DSI");
#elif CONFIG_EXAMPLE_LCD_INTERFACE_RGB
    ctx->tear_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB;
    ESP_LOGI(TAG, "LCD interface: RGB");
#else
    int te_gpio = hw_lcd_get_te_gpio();
    bool te_supported = (te_gpio != GPIO_NUM_NC);

    if (te_supported) {
        ctx->tear_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TE_SYNC;
        ESP_LOGI(TAG, "TE sync enabled on GPIO %d", te_gpio);
    } else {
        ctx->tear_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT;
    }
#if CONFIG_EXAMPLE_LCD_INTERFACE_QSPI
    ESP_LOGI(TAG, "LCD interface: QSPI");
#elif CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITH_PSRAM
    ESP_LOGI(TAG, "LCD interface: SPI (with PSRAM)");
#elif CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITHOUT_PSRAM
    ESP_LOGI(TAG, "LCD interface: SPI (without PSRAM)");
#endif
#endif /* interface selection */

    /* ── Step 3: LCD panel init ────────────────────────────────────────── */
    ESP_LOGI(TAG, "Initializing LCD: %dx%d", HW_LCD_H_RES, HW_LCD_V_RES);
    ESP_RETURN_ON_ERROR(hw_lcd_init(&ctx->panel, &ctx->panel_io,
                                    ctx->tear_mode, ctx->rotation),
                        TAG, "hw_lcd_init failed");

    /* ── Step 4: LVGL adapter init ─────────────────────────────────────── */
    esp_lv_adapter_config_t adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    ESP_RETURN_ON_ERROR(esp_lv_adapter_init(&adapter_cfg),
                        TAG, "esp_lv_adapter_init failed");

    /* ── Step 5: Register display ──────────────────────────────────────── */
    esp_lv_adapter_display_config_t display_cfg;

#if CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI
    display_cfg = ESP_LV_ADAPTER_DISPLAY_MIPI_DEFAULT_CONFIG(
                      ctx->panel, ctx->panel_io,
                      HW_LCD_H_RES, HW_LCD_V_RES, ctx->rotation);
#elif CONFIG_EXAMPLE_LCD_INTERFACE_RGB
    display_cfg = ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(
                      ctx->panel, ctx->panel_io,
                      HW_LCD_H_RES, HW_LCD_V_RES, ctx->rotation);
#elif CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITHOUT_PSRAM
    display_cfg = ESP_LV_ADAPTER_DISPLAY_SPI_WITHOUT_PSRAM_DEFAULT_CONFIG(
                      ctx->panel, ctx->panel_io,
                      HW_LCD_H_RES, HW_LCD_V_RES, ctx->rotation);
#else /* QSPI or SPI with PSRAM */
    if (te_supported) {
        display_cfg = ESP_LV_ADAPTER_DISPLAY_SPI_WITH_PSRAM_TE_DEFAULT_CONFIG(
                          ctx->panel, ctx->panel_io,
                          HW_LCD_H_RES, HW_LCD_V_RES, ctx->rotation,
                          te_gpio,
                          hw_lcd_get_bus_freq_hz(),
                          hw_lcd_get_bus_data_lines(),
                          hw_lcd_get_bits_per_pixel());
    } else {
        display_cfg = ESP_LV_ADAPTER_DISPLAY_SPI_WITH_PSRAM_DEFAULT_CONFIG(
                          ctx->panel, ctx->panel_io,
                          HW_LCD_H_RES, HW_LCD_V_RES, ctx->rotation);
    }
#endif

    ctx->disp = esp_lv_adapter_register_display(&display_cfg);
    ESP_RETURN_ON_FALSE(ctx->disp, ESP_FAIL, TAG,
                        "esp_lv_adapter_register_display failed");

    /* ── Step 6: Input device init ─────────────────────────────────────── */
#if HW_USE_TOUCH
    ESP_LOGI(TAG, "Initializing touch panel");
    esp_lcd_touch_handle_t touch_handle = NULL;
    ESP_RETURN_ON_ERROR(hw_touch_init(&touch_handle, ctx->rotation),
                        TAG, "hw_touch_init failed");

    esp_lv_adapter_touch_config_t touch_cfg =
        ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(ctx->disp, touch_handle);
    ctx->touch = esp_lv_adapter_register_touch(&touch_cfg);
    ESP_RETURN_ON_FALSE(ctx->touch, ESP_FAIL, TAG,
                        "esp_lv_adapter_register_touch failed");
#elif HW_USE_ENCODER && CONFIG_ESP_LVGL_ADAPTER_ENABLE_KNOB
    ESP_LOGI(TAG, "Initializing encoder");
    esp_lv_adapter_encoder_config_t encoder_cfg = {
        .disp = ctx->disp,
        .encoder_a_b = hw_knob_get_config(),
        .encoder_enter = hw_knob_get_button(),
    };
    ctx->encoder = esp_lv_adapter_register_encoder(&encoder_cfg);
    ESP_RETURN_ON_FALSE(ctx->encoder, ESP_FAIL, TAG,
                        "esp_lv_adapter_register_encoder failed");
#endif

    /* ── Step 7: Start LVGL worker task ────────────────────────────────── */
    ESP_RETURN_ON_ERROR(esp_lv_adapter_start(), TAG,
                        "esp_lv_adapter_start failed");

    /* ── Step 8: FPS monitor (when enabled via Kconfig) ────────────────── */
#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FPS_STATS
    ESP_RETURN_ON_ERROR(esp_lv_adapter_fps_stats_enable(ctx->disp, true),
                        TAG, "fps_stats_enable failed");
    xTaskCreate(fps_monitor_task, "fps_monitor",
                FPS_MONITOR_TASK_STACK_SIZE, ctx->disp,
                FPS_MONITOR_TASK_PRIORITY, NULL);
#endif

    ESP_LOGI(TAG, "LVGL ready (%dx%d)", HW_LCD_H_RES, HW_LCD_V_RES);
    return ESP_OK;
}
