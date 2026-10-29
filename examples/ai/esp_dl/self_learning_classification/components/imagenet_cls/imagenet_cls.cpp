/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "imagenet_cls.hpp"
#include <filesystem>

#if CONFIG_IMAGENET_CLS_MODEL_IN_FLASH_RODATA
extern const uint8_t imagenet_cls_espdl[] asm("_binary_imagenet_cls_espdl_start");
static const char *path = (const char *)imagenet_cls_espdl;
#elif CONFIG_IMAGENET_CLS_MODEL_IN_FLASH_PARTITION
static const char *path = "imagenet_cls";
#endif

namespace imagenet_cls {
MobileNetV2FeatureExtractor::MobileNetV2FeatureExtractor(const char *model_name)
{
    m_model = new dl::Model(path, model_name, static_cast<fbs::model_location_type_t>(CONFIG_IMAGENET_CLS_MODEL_LOCATION));
    m_model->minimize();
    m_image_preprocessor = new dl::image::ImagePreprocessor(m_model, {123.675, 116.28, 103.53}, {58.395, 57.12, 57.375});
    m_model_output = m_model->get_output();
    m_feat = new dl::TensorBase(m_model_output->shape, nullptr, 0, dl::DATA_TYPE_FLOAT);
}

MobileNetV2FeatureExtractor::~MobileNetV2FeatureExtractor()
{
    delete m_feat;
    delete m_model;
    delete m_image_preprocessor;
}

dl::TensorBase *MobileNetV2FeatureExtractor::run(const dl::image::img_t &img)
{
    DL_LOG_INFER_LATENCY_INIT();
    DL_LOG_INFER_LATENCY_START();
    m_image_preprocessor->preprocess(img);
    DL_LOG_INFER_LATENCY_END_PRINT("feat", "pre");

    DL_LOG_INFER_LATENCY_START();
    m_model->run();
    DL_LOG_INFER_LATENCY_END_PRINT("feat", "model");

    DL_LOG_INFER_LATENCY_START();
    m_feat->assign(m_model_output);
    DL_LOG_INFER_LATENCY_END_PRINT("feat", "post");

    return m_feat;
}

int MobileNetV2FeatureExtractor::get_feat_dim()
{
    return m_model_output->get_size();
}
}
