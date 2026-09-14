/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include "dl_detect_base.hpp"
#include "dl_detect_espdet_postprocessor.hpp"

namespace greencube_detect {

class ESPDet : public dl::detect::DetectImpl {
public:
    // UVC input scores lower than the validation set. Keep enough margin for
    // quantization/camera-domain shift while NMS still removes duplicates.
    static constexpr float default_score_thr = 0.4f;
    static constexpr float default_nms_thr = 0.7f;
    ESPDet(const char *model_name, float score_thr, float nms_thr);
    std::list<dl::detect::result_t> &run(const dl::image::img_t &img) override;
};

} // namespace greencube_detect

class GreenCubeDetect : public dl::detect::DetectWrapper {
public:
    GreenCubeDetect(bool lazy_load = true);

private:
    void load_model() override;
};
