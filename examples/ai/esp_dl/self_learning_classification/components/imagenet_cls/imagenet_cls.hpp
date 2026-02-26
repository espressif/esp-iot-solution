/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "dl_image_preprocessor.hpp"
#include "dl_model_base.hpp"
#include "dl_tensor_base.hpp"

namespace imagenet_cls {
class MobileNetV2FeatureExtractor {
public:
    MobileNetV2FeatureExtractor(const char* model_name);
    ~MobileNetV2FeatureExtractor();
    dl::TensorBase *run(const dl::image::img_t &img);
    int get_feat_dim();
private:
    dl::Model *m_model;
    dl::image::ImagePreprocessor *m_image_preprocessor;
    dl::TensorBase *m_model_output;
    dl::TensorBase *m_feat;
};
}
