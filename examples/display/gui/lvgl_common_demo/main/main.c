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
 *
 * For SPI/QSPI interfaces, the example automatically detects and uses GPIO TE
 * (Tearing Effect) synchronization if the panel provides a TE signal.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "hw_init.h"
#include "esp_lv_adapter.h"
#include "lv_demos.h"

static const char *TAG = "main";

/* FPS monitor task configuration */
#define FPS_MONITOR_TASK_STACK_SIZE    4096
#define FPS_MONITOR_TASK_PRIORITY      3
#define FPS_MONITOR_INTERVAL_MS        1000

/**
 * @brief Get the configured display rotation from Kconfig
 *
 * @return esp_lv_adapter_rotation_t Rotation angle
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

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FPS_STATS
/**
 * @brief Task to monitor and log FPS statistics
 *
 * @param arg Pointer to lv_display_t
 */
static void fps_monitor_task(void *arg)
{
    lv_display_t *disp = (lv_display_t *)arg;
    uint32_t fps;

    while (1) {
        if (esp_lv_adapter_get_fps(disp, &fps) == ESP_OK) {
            ESP_LOGI(TAG, "Current FPS: %lu", fps);
        }
        vTaskDelay(pdMS_TO_TICKS(FPS_MONITOR_INTERVAL_MS));
    }
}
#endif

void app_main()
{
    esp_lcd_panel_handle_t display_panel = NULL;
    esp_lcd_panel_io_handle_t display_io_handle = NULL;
    esp_lv_adapter_rotation_t rotation = get_configured_rotation();
    esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode;
    esp_lv_adapter_display_config_t display_config;

    /* Select tear effect mode based on LCD interface type and TE availability */
#if CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI
    tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_MIPI_DSI;
    ESP_LOGI(TAG, "Selected LCD interface: MIPI DSI");
#elif CONFIG_EXAMPLE_LCD_INTERFACE_RGB
    tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB;
    ESP_LOGI(TAG, "Selected LCD interface: RGB");
#else
    /* Check if TE (Tearing Effect) GPIO is available for SPI/QSPI interfaces */
    int te_gpio = hw_lcd_get_te_gpio();
    bool te_supported = (te_gpio != GPIO_NUM_NC);

    /* For SPI/QSPI interfaces, use TE_SYNC if available, otherwise use default */
    if (te_supported) {
        tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TE_SYNC;
        ESP_LOGI(TAG, "TE sync enabled on GPIO %d", te_gpio);
    } else {
        tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT;
        ESP_LOGD(TAG, "TE sync disabled or unsupported (gpio=%d)", te_gpio);
    }
#if CONFIG_EXAMPLE_LCD_INTERFACE_QSPI
    ESP_LOGI(TAG, "Selected LCD interface: QSPI");
#elif CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITH_PSRAM
    ESP_LOGI(TAG, "Selected LCD interface: SPI (with PSRAM)");
#elif CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITHOUT_PSRAM
    ESP_LOGI(TAG, "Selected LCD interface: SPI (without PSRAM)");
#endif
#endif

    /* Initialize the LCD hardware panel */
    ESP_LOGI(TAG, "Initializing LCD: %dx%d", HW_LCD_H_RES, HW_LCD_V_RES);
    ESP_ERROR_CHECK(hw_lcd_init(&display_panel, &display_io_handle, tear_avoid_mode, rotation));

    /* Initialize the LVGL adapter */
    esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(esp_lv_adapter_init(&adapter_config));

    /* Register the display to the LVGL adapter with appropriate configuration */
#if CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI
    display_config = ESP_LV_ADAPTER_DISPLAY_MIPI_DEFAULT_CONFIG(
                         display_panel, display_io_handle, HW_LCD_H_RES, HW_LCD_V_RES, rotation);
#elif CONFIG_EXAMPLE_LCD_INTERFACE_RGB
    display_config = ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(
                         display_panel, display_io_handle, HW_LCD_H_RES, HW_LCD_V_RES, rotation);
#elif CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITHOUT_PSRAM
    display_config = ESP_LV_ADAPTER_DISPLAY_SPI_WITHOUT_PSRAM_DEFAULT_CONFIG(
                         display_panel, display_io_handle, HW_LCD_H_RES, HW_LCD_V_RES, rotation);
#else  /* QSPI or SPI with PSRAM */
    /* Use TE-enabled config if TE GPIO is available, otherwise use default config */
    if (te_supported) {
        display_config = ESP_LV_ADAPTER_DISPLAY_SPI_WITH_PSRAM_TE_DEFAULT_CONFIG(
                             display_panel, display_io_handle,
                             HW_LCD_H_RES, HW_LCD_V_RES, rotation,
                             te_gpio,
                             hw_lcd_get_bus_freq_hz(),
                             hw_lcd_get_bus_data_lines(),
                             hw_lcd_get_bits_per_pixel());
    } else {
        display_config = ESP_LV_ADAPTER_DISPLAY_SPI_WITH_PSRAM_DEFAULT_CONFIG(
                             display_panel, display_io_handle, HW_LCD_H_RES, HW_LCD_V_RES, rotation);
    }
#endif

    lv_display_t *disp = esp_lv_adapter_register_display(&display_config);
    if (disp == NULL) {
        ESP_LOGE(TAG, "Failed to register display");
        return;
    }

    /* Initialize input device based on interface type */
#if HW_USE_TOUCH
    ESP_LOGI(TAG, "Initializing touch panel");
    esp_lcd_touch_handle_t touch_handle = NULL;
    ESP_ERROR_CHECK(hw_touch_init(&touch_handle, rotation));

    /* Use the default config macro for quick setup with 1:1 coordinate scaling */
    esp_lv_adapter_touch_config_t touch_config = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, touch_handle);
    lv_indev_t *touch = esp_lv_adapter_register_touch(&touch_config);
    if (touch == NULL) {
        ESP_LOGE(TAG, "Failed to register touch");
        return;
    }

#elif HW_USE_ENCODER && CONFIG_ESP_LVGL_ADAPTER_ENABLE_KNOB
    ESP_LOGI(TAG, "Initializing encoder/knob");
    esp_lv_adapter_encoder_config_t encoder_config = {
        .disp = disp,
        .encoder_a_b = hw_knob_get_config(),
        .encoder_enter = hw_knob_get_button(),
    };
    lv_indev_t *encoder = esp_lv_adapter_register_encoder(&encoder_config);
    if (encoder == NULL) {
        ESP_LOGE(TAG, "Failed to register encoder");
        return;
    }
#endif

    /* Start the LVGL adapter */
    ESP_ERROR_CHECK(esp_lv_adapter_start());

    /* Optional: Enable FPS statistics for performance monitoring */
#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FPS_STATS
    ESP_ERROR_CHECK(esp_lv_adapter_fps_stats_enable(disp, true));
    xTaskCreate(fps_monitor_task, "fps_monitor", FPS_MONITOR_TASK_STACK_SIZE, disp, FPS_MONITOR_TASK_PRIORITY, NULL);
#endif

    ESP_LOGI(TAG, "Starting LVGL benchmark demo");
    /* Lock the mutex due to the LVGL APIs are not thread-safe */
    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        lv_demo_benchmark();  /* Run the official LVGL benchmark */
        esp_lv_adapter_unlock();
    }
}
