/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "emotion_cls.hpp"
#include <cmath>

#if CONFIG_EMOTION_CLS_MODEL_IN_FLASH_RODATA
extern const uint8_t emotion_cls_espdl[] asm("_binary_emotion_cls_espdl_start");
static const char *path = reinterpret_cast<const char *>(emotion_cls_espdl);
#elif CONFIG_EMOTION_CLS_MODEL_IN_FLASH_PARTITION
static const char *path = "emotion_cls";
#endif

namespace emotion_cls {

namespace {
const char *kEmotionCategoryNames[] = {
    "surprise",
    "fear",
    "disgust",
    "happiness",
    "sadness",
    "anger",
    "neutral",
};

template <typename T>
result_t get_top1_result(dl::TensorBase *tensor)
{
    result_t result = {-1, 0.0f, nullptr};
    if (!tensor || tensor->get_size() <= 0) {
        return result;
    }

    const T *data = tensor->get_element_ptr<T>();
    if (!data) {
        return result;
    }

    const int size = tensor->get_size();
    const int exponent = tensor->exponent;

    int best_index = 0;
    T best_quant = data[0];
    for (int i = 1; i < size; ++i) {
        if (data[i] > best_quant) {
            best_quant = data[i];
            best_index = i;
        }
    }

    // Softmax probability of the top-1 class, dequantized first for numerical stability.
    const float max_logit = ldexpf(static_cast<float>(best_quant), exponent);
    float sum_exp = 0.0f;
    for (int i = 0; i < size; ++i) {
        const float logit = ldexpf(static_cast<float>(data[i]), exponent);
        sum_exp += expf(logit - max_logit);
    }

    result.class_id = best_index;
    result.score = (sum_exp > 0.0f) ? (1.0f / sum_exp) : 0.0f;
    result.class_name = EmotionCls::get_class_name(best_index);
    return result;
}
} // namespace

EmotionCls::EmotionCls(const char *model_name) : m_model_name(model_name), m_model(nullptr), m_preprocessor(nullptr)
{
    const char *selected_model_name = (m_model_name && m_model_name[0] != '\0') ? m_model_name : "emotion_cls.espdl";
    m_model = new dl::Model(path,
                            selected_model_name,
                            static_cast<fbs::model_location_type_t>(CONFIG_EMOTION_CLS_MODEL_LOCATION));
    if (m_model) {
        // ImageNet normalization: mean/std * 255 (ImagePreprocessor expects [0,255] range)
        m_preprocessor = new dl::image::ImagePreprocessor(
            m_model, {123.675f, 116.28f, 103.53f}, {58.395f, 57.12f, 57.375f}, false);
    }
}

EmotionCls::~EmotionCls()
{
    delete m_preprocessor;
    delete m_model;
}

const char *EmotionCls::get_class_name(int class_id)
{
    if (class_id < 0 || class_id >= get_class_count()) {
        return nullptr;
    }
    return kEmotionCategoryNames[class_id];
}

result_t EmotionCls::predict(const dl::image::img_t &img)
{
    return predict(img, {});
}

result_t EmotionCls::predict(const dl::image::img_t &img, const std::vector<int> &crop_area)
{
    result_t result = {-1, 0.0f, nullptr};
    if (!m_model || !m_preprocessor) {
        return result;
    }

    m_preprocessor->preprocess(img, crop_area);
    m_model->run();

    dl::TensorBase *output = m_model->get_output();
    if (!output) {
        return result;
    }

    switch (output->dtype) {
    case dl::DATA_TYPE_INT8:
        return get_top1_result<int8_t>(output);
    case dl::DATA_TYPE_INT16:
        return get_top1_result<int16_t>(output);
    default:
        return result;
    }
}

} // namespace emotion_cls
