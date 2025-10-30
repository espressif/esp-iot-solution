/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/**
 * @file main.c
 * @brief LVGL demo that drives two SPI LCD panels simultaneously
 *
 * This example demonstrates how to use the LVGL adapter to initialise and render
 * independent content on two SPI displays sharing the same bus.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "sdkconfig.h"
#include "hw_multi.h"
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
    esp_lv_adapter_rotation_t rotation = get_configured_rotation();
    esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT;

    ESP_LOGI(TAG, "Initializing dual SPI LCDs: %dx%d", BSP_LCD_H_RES, BSP_LCD_V_RES);

    hw_multi_panel_t panels[HW_MULTI_DISPLAY_COUNT] = {0};
    size_t panel_count = 0;
    ESP_ERROR_CHECK(hw_multi_lcd_init(panels, HW_MULTI_DISPLAY_COUNT, tear_avoid_mode, rotation, &panel_count));

    esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(esp_lv_adapter_init(&adapter_config));

    lv_display_t *displays[HW_MULTI_DISPLAY_COUNT] = {0};
    size_t display_count = 0;

    for (size_t i = 0; i < panel_count; ++i) {
        esp_lv_adapter_display_config_t display_config = ESP_LV_ADAPTER_DISPLAY_SPI_WITHOUT_PSRAM_DEFAULT_CONFIG(
                                                             panels[i].panel, panels[i].panel_io, BSP_LCD_H_RES, BSP_LCD_V_RES, rotation);
        lv_display_t *disp = esp_lv_adapter_register_display(&display_config);
        if (!disp) {
            ESP_LOGE(TAG, "Failed to register display %d", (int)i);
            continue;
        }
        displays[display_count++] = disp;
        ESP_LOGI(TAG, "Display %d registered", (int)i);
    }

    if (display_count == 0) {
        ESP_LOGE(TAG, "No displays registered, aborting");
        return;
    }

    ESP_ERROR_CHECK(esp_lv_adapter_start());

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FPS_STATS
    ESP_ERROR_CHECK(esp_lv_adapter_fps_stats_enable(displays[0], true));
    xTaskCreate(fps_monitor_task, "fps_monitor", FPS_MONITOR_TASK_STACK_SIZE, displays[0], FPS_MONITOR_TASK_PRIORITY, NULL);
#endif

    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        lv_display_t *original_default = lv_display_get_default();

        if (display_count >= 1 && displays[0]) {
            lv_display_set_default(displays[0]);
            lv_demo_benchmark();
        }

        if (display_count >= 2 && displays[1]) {
            lv_display_set_default(displays[1]);
            lv_obj_t *screen = lv_disp_get_scr_act(displays[1]);
            lv_obj_clean(screen);

            lv_obj_t *label = lv_label_create(screen);
            lv_label_set_text(label, "Second display");
            lv_obj_center(label);
        }

        if (original_default) {
            lv_display_set_default(original_default);
        } else if (displays[0]) {
            lv_display_set_default(displays[0]);
        }

        esp_lv_adapter_unlock();
    }
}
