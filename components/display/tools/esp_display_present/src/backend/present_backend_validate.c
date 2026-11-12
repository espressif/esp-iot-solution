/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "backend/present_backend_validate.h"
#include "platform/present_limits.h"

esp_err_t present_backend_config_validate_shared(const esp_display_present_backend_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!config->width || !config->height ||
            config->rotation < ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0 || config->rotation > ESP_DISPLAY_PRESENT_ENGINE_ROTATE_270 ||
            (config->format != ESP_DISPLAY_PRESENT_FORMAT_RGB565 && config->format != ESP_DISPLAY_PRESENT_FORMAT_RGB888) ||
            (config->draw_order != ESP_DISPLAY_PRESENT_ORDER_NATIVE && config->draw_order != ESP_DISPLAY_PRESENT_ORDER_SWAPPED) ||
            (config->panel_order != ESP_DISPLAY_PRESENT_ORDER_NATIVE && config->panel_order != ESP_DISPLAY_PRESENT_ORDER_SWAPPED) ||
            (config->format == ESP_DISPLAY_PRESENT_FORMAT_RGB888 &&
             (config->draw_order != ESP_DISPLAY_PRESENT_ORDER_NATIVE || config->panel_order != ESP_DISPLAY_PRESENT_ORDER_NATIVE)) ||
            config->render_contract < ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION ||
            config->render_contract > ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT ||
            !config->max_frames || config->max_frames > PRESENT_MAX_FRAMES) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}
