/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "esp_check.h"
#include "esp_display_present_panel.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "soc/soc_caps.h"

#if SOC_LCDCAM_RGB_LCD_SUPPORTED
#include "esp_lcd_panel_rgb.h"
#endif

#if SOC_MIPI_DSI_SUPPORTED
#include "esp_lcd_mipi_dsi.h"
#endif

static const char *TAG = "present_panel";

esp_err_t esp_display_present_fetch_panel_frame_buffers(esp_lcd_panel_handle_t panel,
                                                        esp_display_present_panel_interface_t panel_if,
                                                        uint8_t required,
                                                        size_t frame_buffer_size,
                                                        esp_display_present_frame_buffers_t *out_fbs)
{
    ESP_RETURN_ON_FALSE(panel && required && out_fbs, ESP_ERR_INVALID_ARG, TAG, "invalid framebuffer request");

    memset(out_fbs, 0, sizeof(*out_fbs));

    void *fb0 = NULL;
    void *fb1 = NULL;
    void *fb2 = NULL;
    esp_err_t ret = ESP_ERR_NOT_SUPPORTED;

    switch (panel_if) {
    case ESP_DISPLAY_PRESENT_PANEL_IF_RGB:
#if SOC_LCDCAM_RGB_LCD_SUPPORTED
        if (required == 1) {
            ret = esp_lcd_rgb_panel_get_frame_buffer(panel, 1, &fb0);
        } else if (required == 2) {
            ret = esp_lcd_rgb_panel_get_frame_buffer(panel, 2, &fb0, &fb1);
        } else {
            ret = esp_lcd_rgb_panel_get_frame_buffer(panel, 3, &fb0, &fb1, &fb2);
        }
#endif
        break;
    case ESP_DISPLAY_PRESENT_PANEL_IF_MIPI_DSI:
#if SOC_MIPI_DSI_SUPPORTED
        if (required == 1) {
            ret = esp_lcd_dpi_panel_get_frame_buffer(panel, 1, &fb0);
        } else if (required == 2) {
            ret = esp_lcd_dpi_panel_get_frame_buffer(panel, 2, &fb0, &fb1);
        } else {
            ret = esp_lcd_dpi_panel_get_frame_buffer(panel, 3, &fb0, &fb1, &fb2);
        }
#endif
        break;
    case ESP_DISPLAY_PRESENT_PANEL_IF_OTHER:
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }

    ESP_RETURN_ON_ERROR(ret, TAG, "panel framebuffer request failed");

    out_fbs->buffers[0] = fb0;
    out_fbs->buffers[1] = fb1;
    out_fbs->buffers[2] = fb2;
    out_fbs->count = required;
    out_fbs->size_bytes = frame_buffer_size;
    return ESP_OK;
}

esp_err_t esp_display_present_blit_full_frame(esp_lcd_panel_handle_t panel,
                                              uint16_t hor_res,
                                              uint16_t ver_res,
                                              void *frame_buffer)
{
    ESP_RETURN_ON_FALSE(panel && hor_res && ver_res && frame_buffer,
                        ESP_ERR_INVALID_ARG, TAG, "invalid full frame blit args");

    return esp_lcd_panel_draw_bitmap(panel, 0, 0, hor_res, ver_res, frame_buffer);
}

esp_err_t esp_display_present_blit_area(esp_lcd_panel_handle_t panel,
                                        int x_start,
                                        int y_start,
                                        int x_end,
                                        int y_end,
                                        const void *frame_buffer)
{
    ESP_RETURN_ON_FALSE(panel && frame_buffer, ESP_ERR_INVALID_ARG, TAG, "invalid area blit args");

    return esp_lcd_panel_draw_bitmap(panel, x_start, y_start, x_end, y_end, frame_buffer);
}
