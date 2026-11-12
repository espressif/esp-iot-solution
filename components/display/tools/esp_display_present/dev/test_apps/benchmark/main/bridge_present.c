/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0 */
#include "bench_api.h"
#include "esp_display_present.h"
#include "esp_lv_present.h"
#include "present_lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static esp_lcd_panel_handle_t panel;
static esp_lcd_panel_io_handle_t io;
static esp_display_presenter_t *presenter;
static lv_display_t *active_display;
static const esp_display_present_mode_t modes[] = {ESP_DISPLAY_PRESENT_MODE_NONE, ESP_DISPLAY_PRESENT_MODE_DOUBLE_FULL, ESP_DISPLAY_PRESENT_MODE_TRIPLE_FULL, ESP_DISPLAY_PRESENT_MODE_DOUBLE_DIRECT, ESP_DISPLAY_PRESENT_MODE_DOUBLE_PARTIAL, ESP_DISPLAY_PRESENT_MODE_TRIPLE_PARTIAL, ESP_DISPLAY_PRESENT_MODE_AUTO, ESP_DISPLAY_PRESENT_MODE_TE_SYNC};
esp_err_t bench_init(void)
{
    return hw_lcd_init(&panel, &io, 3, HW_ROTATE_0);
}
esp_err_t bench_open(const bench_case_t *c, lv_display_t **disp)
{
    esp_display_presenter_config_t cfg = {.width = HW_LCD_H_RES, .height = HW_LCD_V_RES, .max_damage_areas = 32,
                                          .pixel_format = ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565,
                                          .target = {.hw = {.panel = panel, .io = io, .rotation = c->rotation,
#if BENCH_RGB
                                                            .panel_type = ESP_DISPLAY_PRESENT_PANEL_RGB,
#elif BENCH_DSI
                                                            .panel_type = ESP_DISPLAY_PRESENT_PANEL_MIPI_DSI,
#else
                                                            .panel_type = ESP_DISPLAY_PRESENT_PANEL_IO, .swap_bytes = true,
#endif
#if BENCH_QSPI
                                                            .spi_psram_dma_direct = true, .te_enabled = true, .te_sync = {.gpio_num = 43, .bus_freq_hz = 40000000, .data_lines = 4},
#endif
                                                           }, .fb = {.mode = modes[c->mode]}, .drawbuf = {.lines = 50, .buffers = c->tiles, .in_psram = c->psram, .te_compose_buffers = c->compose}
                                                    }
                                         };
    if (c->rotation == 90 || c->rotation == 270) {
        cfg.width = HW_LCD_V_RES;
        cfg.height = HW_LCD_H_RES;
    }
    esp_err_t e = esp_display_presenter_create(&cfg, &presenter);
    if (e != ESP_OK) {
        return e;
    }
    e = esp_lv_present_start(presenter, disp);
    active_display = e == ESP_OK ? *disp : NULL;
    if (e != ESP_OK) {
        esp_display_presenter_delete(presenter);
        presenter = NULL;
    }
    return e;
}
esp_err_t bench_lock(void)
{
    return ESP_OK;
}
void bench_unlock(void) {}
void bench_pump(void)
{
    uint32_t ms = lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(ms == 0 ? 1 : ms > 10 ? 10 : ms));
}
esp_err_t bench_close(void)
{
    esp_err_t e = esp_display_presenter_quiesce(presenter, 10000);
    if (e != ESP_OK) {
        return e;
    }
    /* A binding error can pause drawing while benchmark timers still finish.
     * Check it outside the measured scene interval; cleanup remains mandatory. */
    esp_err_t draw_error = esp_lv_present_native_get_error(lv_display_get_user_data(active_display));
    e = esp_lv_present_stop();
    if (e != ESP_OK) {
        return e;
    }
    active_display = NULL;
    e = esp_display_presenter_delete(presenter);
    return e != ESP_OK ? e : draw_error;
}
uint32_t bench_frame_count(void)
{
    return esp_lv_present_get_frame_count();
}
#if BENCH_QSPI
void meter_reset(void); void meter_report(const char *);
void bench_meter_reset(void)
{
    meter_reset();
}
void bench_meter_report(const char *label)
{
    meter_report(label);
}
#else
void bench_meter_reset(void) {}
void bench_meter_report(const char *label)
{
    (void)label;
}
#endif
