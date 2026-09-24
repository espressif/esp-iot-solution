/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "greencube_detect.hpp"

extern const uint8_t greencube_detect_espdl[] asm("_binary_greencube_detect_espdl_start");

namespace greencube_detect {

ESPDet::ESPDet(const char *model_name, float score_thr, float nms_thr)
{
    m_model = new dl::Model((const char *)greencube_detect_espdl, model_name, fbs::MODEL_LOCATION_IN_FLASH_RODATA);
    m_model->minimize();
    m_image_preprocessor = new dl::image::ImagePreprocessor(m_model, {0, 0, 0}, {255, 255, 255});
    m_image_preprocessor->enable_letterbox({114, 114, 114});
    m_postprocessor = new dl::detect::ESPDetPostProcessor(
    m_model, m_image_preprocessor, score_thr, nms_thr, 10, {{8, 8, 4, 4}, {16, 16, 8, 8}, {32, 32, 16, 16}});
}

std::list<dl::detect::result_t> &ESPDet::run(const dl::image::img_t &img)
{
    m_image_preprocessor->preprocess(img);

    // Single-core on purpose. RUNTIME_MODE_AUTO puts an operator worker on the
    // other core, and that core then takes load/store access faults while the
    // USB camera streams (cross-core cache-sync race, see usb_camera.cpp).
    // Measured: AUTO 3 panics / 120 s, SINGLE_CORE 0 / 180 s.
    m_model->run(dl::RUNTIME_MODE_SINGLE_CORE);

    m_postprocessor->clear_result();
    m_postprocessor->postprocess();
    return m_postprocessor->get_result(img.width, img.height);
}

} // namespace greencube_detect

GreenCubeDetect::GreenCubeDetect(bool lazy_load)
{
    m_score_thr[0] = greencube_detect::ESPDet::default_score_thr;
    m_nms_thr[0] = greencube_detect::ESPDet::default_nms_thr;

    if (lazy_load) {
        m_model = nullptr;
    } else {
        load_model();
    }
}

void GreenCubeDetect::load_model()
{
    m_model = new greencube_detect::ESPDet(
        "greencube_v2_224_esp32p4.espdl", m_score_thr[0], m_nms_thr[0]);
}
