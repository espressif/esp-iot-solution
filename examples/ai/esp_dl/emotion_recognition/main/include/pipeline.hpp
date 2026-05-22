/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "app_camera.hpp"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <sys/time.h>

class EmotionPipeline {
public:
    static constexpr int kMaxFaceBoxes = 8;

    struct FaceBox {
        int   x1;
        int   y1;
        int   x2;
        int   y2;
        float score;
        int   emotion_class_id;
        float emotion_score;
        char  emotion_name[16];
    };

    struct Result {
        struct timeval timestamp;
        int     count;
        FaceBox boxes[kMaxFaceBoxes];
    };

    enum class State {
        Stopped,
        Starting,
        Running,
        Error,
    };

    struct Config {
        Camera  *camera                = nullptr;
        uint32_t face_detect_period_ms = 100;
        uint32_t capture_period_ms     = 20;
    };

    using result_cb_t = void (*)(const Result &result, cam_fb_t *fb, void *user_ctx);
    using state_cb_t  = void (*)(State state, esp_err_t err, void *user_ctx);

    explicit EmotionPipeline(const Config &config);
    ~EmotionPipeline();

    EmotionPipeline(const EmotionPipeline &) = delete;
    EmotionPipeline &operator=(const EmotionPipeline &) = delete;

    void on_result(result_cb_t cb, void *user_ctx);
    void on_state_change(state_cb_t cb, void *user_ctx);

    esp_err_t start();

private:
    struct Frame {
        uint8_t *data;
        size_t   size;
        uint16_t width;
        uint16_t height;
        struct timeval timestamp;
    };

    struct EmotionRequest {
        Frame  frame;
        Result snapshot;
    };

    static void face_detect_task_entry(void *arg);
    static void emotion_cls_task_entry(void *arg);
    static void capture_task_entry(void *arg);

    void face_detect_task();
    void emotion_cls_task();
    void capture_task();

    void notify_state(State s, esp_err_t err = ESP_OK);
    bool init_buffers(size_t frame_size, uint16_t width, uint16_t height);
    void free_buffers();

    Config      m_config;
    result_cb_t m_on_result     = nullptr;
    void       *m_on_result_ctx = nullptr;
    state_cb_t  m_on_state      = nullptr;
    void       *m_on_state_ctx  = nullptr;

    QueueHandle_t m_detect_frame_queue    = nullptr;
    QueueHandle_t m_detect_result_queue   = nullptr;
    QueueHandle_t m_emotion_request_queue = nullptr;
    QueueHandle_t m_emotion_result_queue  = nullptr;

    Frame    m_detect_frames[2] {};
    Frame    m_emotion_frames[2] {};
    uint32_t m_detect_write_index  = 0;
    uint32_t m_emotion_write_index = 0;
    cam_fb_t *m_display_fb = nullptr;
};
