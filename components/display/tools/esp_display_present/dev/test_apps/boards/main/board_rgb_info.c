/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "board_rgb_info.h"
#include "esp_log.h"
#include "platform/lcd/present_lcd_rgb_info.h"
static struct {
    esp_lcd_panel_handle_t panel;
    present_lcd_rgb_info_t info;
} binding;

void board_rgb_created(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_config_t *config)
{
    size_t buffers = config->flags.no_fb ? 0 : config->num_fbs ? config->num_fbs : config->flags.double_fb ? 2 : 1;
    unsigned bpp = config->in_color_format == LCD_COLOR_FMT_RGB888 ? 24 :
                   config->in_color_format == LCD_COLOR_FMT_RGB565 ? 16 : 0;
    binding.info = (present_lcd_rgb_info_t) {
        .width = config->timings.h_res, .height = config->timings.v_res,
        .frame_buffer_count = buffers,
        .frame_buffer_bytes = (size_t)config->timings.h_res * config->timings.v_res * (bpp / 8),
        .bounce_buffer_bytes = config->bounce_buffer_size_px * (bpp / 8),
        .bits_per_pixel = bpp,
        .continuous_refresh = !config->flags.refresh_on_demand,
        .custom_bounce_callback = config->flags.no_fb,
        /* These maintained BSPs apply no driver-side memory transforms. */
        .transformed = false,
    };
    binding.panel = panel;
    ESP_LOGI("rgb_facts", "RGB_FACTS width=%u height=%u fbs=%u bounce_bytes=%u continuous=%u bpp=%u",
             (unsigned)binding.info.width, (unsigned)binding.info.height,
             (unsigned)buffers, (unsigned)binding.info.bounce_buffer_bytes,
             (unsigned)binding.info.continuous_refresh, bpp);
}
void board_rgb_forget(void)
{
    binding.panel = NULL;
}
esp_err_t present_lcd_rgb_get_board_info(esp_lcd_panel_handle_t panel, present_lcd_rgb_info_t *info)
{
    if (panel != binding.panel || !panel) {
        return ESP_ERR_NOT_FOUND;
    }
    if (!binding.info.bits_per_pixel) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    *info = binding.info;
    return ESP_OK;
}
