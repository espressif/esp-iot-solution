/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_display_present_types.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t esp_display_present_fetch_panel_frame_buffers(esp_lcd_panel_handle_t panel,
                                                        esp_display_present_panel_interface_t panel_if,
                                                        uint8_t required,
                                                        size_t frame_buffer_size,
                                                        esp_display_present_frame_buffers_t *out_fbs);

esp_err_t esp_display_present_blit_full_frame(esp_lcd_panel_handle_t panel,
                                              uint16_t hor_res,
                                              uint16_t ver_res,
                                              void *frame_buffer);

esp_err_t esp_display_present_blit_area(esp_lcd_panel_handle_t panel,
                                        int x_start,
                                        int y_start,
                                        int x_end,
                                        int y_end,
                                        const void *frame_buffer);

#ifdef __cplusplus
}
#endif
