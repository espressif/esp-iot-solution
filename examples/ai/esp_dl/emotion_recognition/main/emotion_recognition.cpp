/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "app_camera.hpp"
#include "app_lcd.hpp"
#include "emotion_overlay.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "pipeline.hpp"

static const char *TAG = "emotion_recognition";

static void on_pipeline_state(EmotionPipeline::State state, esp_err_t err, void *user_ctx)
{
    (void)user_ctx;
    ESP_LOGI(TAG, "pipeline state=%d err=%s", static_cast<int>(state), esp_err_to_name(err));
}

static void on_pipeline_result(const EmotionPipeline::Result &result, cam_fb_t *fb, void *user_ctx)
{
    auto *overlay = static_cast<EmotionOverlay *>(user_ctx);
    overlay->draw(fb, result);
}

extern "C" void app_main(void)
{
    auto *panel   = app_lcd_init();
    auto *camera  = new Camera(VIDEO_PIX_FMT_RGB565, 4, V4L2_MEMORY_MMAP, false);
    auto *overlay = new EmotionOverlay(panel);

    EmotionPipeline::Config cfg = {
        .camera                = camera,
        .face_detect_period_ms = 100,
        .capture_period_ms     = 20,
    };
    auto *pipeline = new EmotionPipeline(cfg);

    pipeline->on_state_change(on_pipeline_state, nullptr);
    pipeline->on_result(on_pipeline_result, overlay);

    esp_err_t ret = pipeline->start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start pipeline: %s", esp_err_to_name(ret));
    }
}
