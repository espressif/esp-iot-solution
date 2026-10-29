/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "dl_image_preprocessor.hpp"
#include "dl_model_base.hpp"
#include <vector>

namespace emotion_cls {

struct result_t {
    int class_id;
    float score;
    const char *class_name;
};

class EmotionCls {
public:
    explicit EmotionCls(const char *model_name = nullptr);
    ~EmotionCls();

    EmotionCls(const EmotionCls &) = delete;
    EmotionCls &operator=(const EmotionCls &) = delete;

    static constexpr int get_class_count()
    {
        return 7;
    }
    static const char *get_class_name(int class_id);
    result_t predict(const dl::image::img_t &img);
    result_t predict(const dl::image::img_t &img, const std::vector<int> &crop_area);
    dl::Model *get_raw_model() const
    {
        return m_model;
    }

private:
    const char *m_model_name;
    dl::Model *m_model;
    dl::image::ImagePreprocessor *m_preprocessor;
};

} // namespace emotion_cls
