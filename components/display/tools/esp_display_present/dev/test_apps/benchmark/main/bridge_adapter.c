/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0 */
#include "bench_api.h"
#include "esp_lv_adapter.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static esp_lcd_panel_handle_t panel;
static esp_lcd_panel_io_handle_t io;
static lv_display_t *display;
static const esp_lv_adapter_tear_avoid_mode_t modes[] = {ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE, ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_FULL, ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_FULL, ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_DIRECT, ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_PARTIAL, ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL, ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE, ESP_LV_ADAPTER_TEAR_AVOID_MODE_TE_SYNC};
esp_err_t bench_init(void)
{
    esp_err_t e = hw_lcd_init(&panel, &io, 3, HW_ROTATE_0); if (e != ESP_OK)return e;
    esp_lv_adapter_config_t cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(); cfg.task_stack_size = 24576;
    e = esp_lv_adapter_init(&cfg); return e == ESP_OK ? esp_lv_adapter_start() : e;
}
esp_err_t bench_open(const bench_case_t *c, lv_display_t **disp)
{
    if (!c->adapter_supported || c->mode == B_AUTO) {
        return ESP_ERR_NOT_SUPPORTED;
    }
#if BENCH_RGB
    esp_lv_adapter_display_config_t cfg = ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(panel, io, HW_LCD_H_RES, HW_LCD_V_RES, c->rotation);
#elif BENCH_DSI
    esp_lv_adapter_display_config_t cfg = ESP_LV_ADAPTER_DISPLAY_MIPI_DEFAULT_CONFIG(panel, io, HW_LCD_H_RES, HW_LCD_V_RES, c->rotation);
#else
    esp_lv_adapter_display_config_t cfg = ESP_LV_ADAPTER_DISPLAY_SPI_WITH_PSRAM_DEFAULT_CONFIG(panel, io, HW_LCD_H_RES, HW_LCD_V_RES, c->rotation);
#endif
#if BENCH_QSPI
    if (c->mode == B_TE) {
        cfg.te_sync = (esp_lv_adapter_te_sync_config_t) {
            .gpio_num = 43, .bus_freq_hz = 40000000, .data_lines = 4,
            .bits_per_pixel = 16,
            .pipeline = c->adapter_te_pipeline == 0 ? ESP_LV_ADAPTER_TE_PIPELINE_SYNC_FULL : ESP_LV_ADAPTER_TE_PIPELINE_ASYNC_FULL,
            .bounce_buffer_enabled = c->adapter_te_pipeline == 2, .bounce_buffer_rows = 16,
        };
    }
#endif
    cfg.tear_avoid_mode = modes[c->mode]; cfg.profile.buffer_height = 50; cfg.profile.use_psram = c->psram; cfg.profile.require_double_buffer = c->tiles == 2;
    esp_err_t e = esp_lv_adapter_lock(-1); if (e != ESP_OK)return e;
    display = esp_lv_adapter_register_display(&cfg); esp_lv_adapter_unlock(); *disp = display;
    return display ? ESP_OK : ESP_FAIL;
}
esp_err_t bench_lock(void)
{
    return esp_lv_adapter_lock(-1);
}
void bench_unlock(void)
{
    esp_lv_adapter_unlock();
}
void bench_pump(void)
{
    vTaskDelay(pdMS_TO_TICKS(10));
}
esp_err_t bench_close(void)
{
    return esp_lv_adapter_unregister_display(display);
}
uint32_t bench_frame_count(void)
{
    return 0;   /* Unavailable: report must not compare adapter transport FPS. */
}
void bench_meter_reset(void) {}
void bench_meter_report(const char *label)
{
    (void)label;
}
