/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "esp_lcd_types.h"
#include "esp_painter.h"
#include "pipeline.hpp"

class EmotionOverlay {
public:
    explicit EmotionOverlay(esp_lcd_panel_handle_t panel);
    ~EmotionOverlay();

    EmotionOverlay(const EmotionOverlay &) = delete;
    EmotionOverlay &operator=(const EmotionOverlay &) = delete;

    esp_err_t draw(cam_fb_t *fb, const EmotionPipeline::Result &result);

private:
    esp_err_t ensure_painter(uint16_t width, uint16_t height);

    esp_lcd_panel_handle_t m_panel;
    esp_painter_handle_t   m_painter = nullptr;
    char                   m_cached_text[32] = {};
};
