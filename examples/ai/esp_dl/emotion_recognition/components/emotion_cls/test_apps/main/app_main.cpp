/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "dl_image_jpeg.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "emotion_cls.hpp"

extern const uint8_t test_jpg_start[] asm("_binary_test_jpg_start");
extern const uint8_t test_jpg_end[] asm("_binary_test_jpg_end");
const char *TAG = "emotion_cls";

extern "C" void app_main(void)
{
    dl::image::jpeg_img_t jpeg_img = {.data = (void *)test_jpg_start, .data_len = (size_t)(test_jpg_end - test_jpg_start)};
    auto img = dl::image::sw_decode_jpeg(jpeg_img, dl::image::DL_IMAGE_PIX_TYPE_RGB888);
    if (img.data == nullptr) {
        ESP_LOGE(TAG, "Failed to decode test image");
        return;
    }

    emotion_cls::EmotionCls cls;

    int64_t start_us = esp_timer_get_time();
    emotion_cls::result_t result = cls.predict(img);
    int64_t elapsed_us = esp_timer_get_time() - start_us;

    if (result.class_id >= 0 && result.class_name != nullptr) {
        ESP_LOGI(TAG,
                 "Inference result: class_id=%d, class_name=%s, score=%.3f",
                 result.class_id,
                 result.class_name,
                 result.score);
    } else {
        ESP_LOGW(TAG, "Inference result is invalid");
    }

    ESP_LOGI(TAG, "Inference time: %lld us (%.2f ms)", elapsed_us, elapsed_us / 1000.0f);
}
