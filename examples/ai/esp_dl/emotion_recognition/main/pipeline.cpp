/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "pipeline.hpp"
#include "dl_image_define.hpp"
#include "emotion_cls.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "human_face_detect.hpp"
#include <cstring>
#include <vector>

namespace {
constexpr char TAG[] = "emotion_pipeline";

std::vector<int> clamp_crop_area(const EmotionPipeline::FaceBox &box, const dl::image::img_t &img)
{
    const int max_x = static_cast<int>(img.width) - 1;
    const int max_y = static_cast<int>(img.height) - 1;

    int x1 = box.x1 < 0 ? 0 : box.x1;
    int y1 = box.y1 < 0 ? 0 : box.y1;
    int x2 = box.x2 > max_x ? max_x : box.x2;
    int y2 = box.y2 > max_y ? max_y : box.y2;

    if (x2 <= x1) {
        x2 = x1 + 1 <= max_x ? x1 + 1 : max_x;
    }
    if (y2 <= y1) {
        y2 = y1 + 1 <= max_y ? y1 + 1 : max_y;
    }
    return {x1, y1, x2, y2};
}
} // namespace

EmotionPipeline::EmotionPipeline(const Config &config) : m_config(config) {}

EmotionPipeline::~EmotionPipeline()
{
    free_buffers();
    if (m_detect_frame_queue) {
        vQueueDelete(m_detect_frame_queue);
    }
    if (m_detect_result_queue) {
        vQueueDelete(m_detect_result_queue);
    }
    if (m_emotion_request_queue) {
        vQueueDelete(m_emotion_request_queue);
    }
    if (m_emotion_result_queue) {
        vQueueDelete(m_emotion_result_queue);
    }
}

void EmotionPipeline::on_result(result_cb_t cb, void *user_ctx)
{
    m_on_result     = cb;
    m_on_result_ctx = user_ctx;
}

void EmotionPipeline::on_state_change(state_cb_t cb, void *user_ctx)
{
    m_on_state     = cb;
    m_on_state_ctx = user_ctx;
}

void EmotionPipeline::notify_state(State s, esp_err_t err)
{
    if (m_on_state) {
        m_on_state(s, err, m_on_state_ctx);
    }
}

bool EmotionPipeline::init_buffers(size_t frame_size, uint16_t width, uint16_t height)
{
    for (auto &frame : m_detect_frames) {
        frame.data = static_cast<uint8_t *>(heap_caps_malloc(frame_size, MALLOC_CAP_SPIRAM));
        if (frame.data == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate detect buffer");
            return false;
        }
        frame.size = frame_size;
        frame.width = width;
        frame.height = height;
    }
    for (auto &frame : m_emotion_frames) {
        frame.data = static_cast<uint8_t *>(heap_caps_malloc(frame_size, MALLOC_CAP_SPIRAM));
        if (frame.data == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate emotion buffer");
            return false;
        }
        frame.size = frame_size;
        frame.width = width;
        frame.height = height;
    }
    return true;
}

void EmotionPipeline::free_buffers()
{
    for (auto &frame : m_detect_frames) {
        if (frame.data) {
            heap_caps_free(frame.data);
            frame.data = nullptr;
        }
    }
    for (auto &frame : m_emotion_frames) {
        if (frame.data) {
            heap_caps_free(frame.data);
            frame.data = nullptr;
        }
    }
}

esp_err_t EmotionPipeline::start()
{
    if (m_config.camera == nullptr) {
        notify_state(State::Error, ESP_ERR_INVALID_ARG);
        return ESP_ERR_INVALID_ARG;
    }

    notify_state(State::Starting);

    cam_fb_t *init_fb = nullptr;
    while (init_fb == nullptr) {
        init_fb = m_config.camera->cam_fb_get();
        if (init_fb == nullptr) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    const size_t display_frame_size = init_fb->width * init_fb->height * sizeof(uint16_t);
    const uint16_t width = static_cast<uint16_t>(init_fb->width);
    const uint16_t height = static_cast<uint16_t>(init_fb->height);
    m_config.camera->cam_fb_return();

    if (!init_buffers(display_frame_size, width, height)) {
        free_buffers();
        notify_state(State::Error, ESP_ERR_NO_MEM);
        return ESP_ERR_NO_MEM;
    }

    m_detect_frame_queue    = xQueueCreate(1, sizeof(Frame));
    m_detect_result_queue   = xQueueCreate(4, sizeof(Result));
    m_emotion_request_queue = xQueueCreate(1, sizeof(EmotionRequest));
    m_emotion_result_queue  = xQueueCreate(4, sizeof(Result));
    if (m_detect_frame_queue == nullptr || m_detect_result_queue == nullptr ||
            m_emotion_request_queue == nullptr || m_emotion_result_queue == nullptr) {
        ESP_LOGE(TAG, "Failed to create pipeline queues");
        notify_state(State::Error, ESP_ERR_NO_MEM);
        return ESP_ERR_NO_MEM;
    }

    BaseType_t face_task_ok =
        xTaskCreatePinnedToCore(face_detect_task_entry, "face_detect", 12 * 1024, this, 3, nullptr, 1);
    BaseType_t emotion_cls_task_ok =
        xTaskCreatePinnedToCore(emotion_cls_task_entry, "emotion_cls", 12 * 1024, this, 2, nullptr, 0);
    BaseType_t capture_task_ok =
        xTaskCreatePinnedToCore(capture_task_entry, "capture", 6 * 1024, this, 4, nullptr, 0);

    if (face_task_ok != pdPASS || emotion_cls_task_ok != pdPASS || capture_task_ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create pipeline tasks");
        notify_state(State::Error, ESP_FAIL);
        return ESP_FAIL;
    }

    return ESP_OK;
}

void EmotionPipeline::face_detect_task_entry(void *arg)
{
    static_cast<EmotionPipeline *>(arg)->face_detect_task();
}

void EmotionPipeline::emotion_cls_task_entry(void *arg)
{
    static_cast<EmotionPipeline *>(arg)->emotion_cls_task();
}

void EmotionPipeline::capture_task_entry(void *arg)
{
    static_cast<EmotionPipeline *>(arg)->capture_task();
}

void EmotionPipeline::face_detect_task()
{
    auto *detector = new HumanFaceDetect();
    const TickType_t period_ticks = pdMS_TO_TICKS(m_config.face_detect_period_ms);

    while (true) {
        vTaskDelay(period_ticks);

        Frame frame = {};
        if (xQueueReceive(m_detect_frame_queue, &frame, 0) != pdTRUE) {
            continue;
        }

        dl::image::img_t src_img = {
            .data = frame.data,
            .width = frame.width,
            .height = frame.height,
            .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565LE,
        };

        auto &results = detector->run(src_img);
        Result snapshot = {};
        snapshot.timestamp = frame.timestamp;

        for (const auto &res : results) {
            if (snapshot.count >= kMaxFaceBoxes || res.box.size() < 4) {
                break;
            }
            auto &box = snapshot.boxes[snapshot.count++];
            box.x1 = res.box[0];
            box.y1 = res.box[1];
            box.x2 = res.box[2];
            box.y2 = res.box[3];
            box.score = res.score;
            box.emotion_class_id = -1;
            box.emotion_score = 0.0f;
            box.emotion_name[0] = '\0';
        }

        xQueueSendToBack(m_detect_result_queue, &snapshot, 0);
        if (snapshot.count > 0) {
            uint32_t write_index = m_emotion_write_index;
            memcpy(m_emotion_frames[write_index].data, frame.data, frame.size);
            m_emotion_frames[write_index].timestamp = frame.timestamp;

            EmotionRequest request = {
                .frame = m_emotion_frames[write_index],
                .snapshot = snapshot,
            };
            xQueueOverwrite(m_emotion_request_queue, &request);
            m_emotion_write_index = (write_index + 1) % 2;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void EmotionPipeline::emotion_cls_task()
{
    auto *emotion = new emotion_cls::EmotionCls();

    while (true) {
        EmotionRequest request = {};
        if (xQueueReceive(m_emotion_request_queue, &request, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        dl::image::img_t src_img = {
            .data = request.frame.data,
            .width = request.frame.width,
            .height = request.frame.height,
            .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565LE,
        };

        Result snapshot = request.snapshot;
        for (int i = 0; i < snapshot.count; ++i) {
            auto &box = snapshot.boxes[i];
            auto crop_area = clamp_crop_area(box, src_img);
            auto emotion_result = emotion->predict(src_img, crop_area);
            box.emotion_class_id = emotion_result.class_id;
            box.emotion_score = emotion_result.score;
            if (emotion_result.class_name != nullptr) {
                snprintf(box.emotion_name, sizeof(box.emotion_name), "%s", emotion_result.class_name);
                ESP_LOGI(TAG, "Face[%d]: emotion=%s score=%.3f",
                         i, emotion_result.class_name, emotion_result.score);
            }
        }

        xQueueSendToBack(m_emotion_result_queue, &snapshot, 0);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void EmotionPipeline::capture_task()
{
    Result latest_result = {};
    Result queued_face_result = {};
    Result queued_emotion_result = {};
    TickType_t last_detect_feed_tick = 0;
    const TickType_t capture_period_ticks = pdMS_TO_TICKS(m_config.capture_period_ms);
    const TickType_t face_detect_period_ticks = pdMS_TO_TICKS(m_config.face_detect_period_ms);

    notify_state(State::Running);

    while (true) {
        vTaskDelay(capture_period_ticks);

        cam_fb_t *fb = m_config.camera->cam_fb_get();
        if (fb == nullptr) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        TickType_t now = xTaskGetTickCount();
        if (now - last_detect_feed_tick >= face_detect_period_ticks) {
            uint32_t write_index = m_detect_write_index;
            memcpy(m_detect_frames[write_index].data, fb->buf, m_detect_frames[write_index].size);
            m_detect_frames[write_index].timestamp = fb->timestamp;
            xQueueOverwrite(m_detect_frame_queue, &m_detect_frames[write_index]);
            m_detect_write_index = (write_index + 1) % 2;
            last_detect_feed_tick = now;
        }

        while (xQueueReceive(m_detect_result_queue, &queued_face_result, 0) == pdTRUE) {
            latest_result = queued_face_result;
        }
        while (xQueueReceive(m_emotion_result_queue, &queued_emotion_result, 0) == pdTRUE) {
            for (int i = 0; i < queued_emotion_result.count && i < latest_result.count; ++i) {
                latest_result.boxes[i].emotion_class_id = queued_emotion_result.boxes[i].emotion_class_id;
                latest_result.boxes[i].emotion_score    = queued_emotion_result.boxes[i].emotion_score;
                memcpy(latest_result.boxes[i].emotion_name,
                       queued_emotion_result.boxes[i].emotion_name,
                       sizeof(latest_result.boxes[i].emotion_name));
            }
        }

        if (m_on_result) {
            m_on_result(latest_result, fb, m_on_result_ctx);
        }

        if (m_display_fb != nullptr) {
            m_config.camera->cam_fb_return();
        }
        m_display_fb = fb;
    }
}
