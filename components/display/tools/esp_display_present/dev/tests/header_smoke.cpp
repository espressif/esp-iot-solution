/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "backend/present_backend.h"
#include "pixel/present_pixel_engine.h"
#include "platform/lcd/present_lcd_io.h"
#include "core/present_engine.h"
#include "pixel/present_executor.h"
#include "config/present_plan.h"
#include "config/present_mode.h"
#include "platform/lcd/present_lcd_fb_scanout.h"
#include "core/present_transfer_tracker.h"
#include "core/present_completion_tracker.h"
#include "core/present_session.h"
#include "core/present_control.h"
#include "core/present_producer.h"
#include "resources/present_memory.h"
int main()
{
    if (!present_event_memory()) {
        return 3;
    }
    esp_display_present_pixel_engine_t *engine = nullptr;
    esp_display_present_pixel_config_t config{};
    config.max_jobs = 1;
    if (esp_display_present_pixel_create(&config, &engine) != ESP_OK) {
        return 1;
    }
    if (esp_display_present_pixel_stop(engine, 0) != ESP_OK) {
        return 2;
    }
    return esp_display_present_pixel_destroy(engine) != ESP_OK;
}
