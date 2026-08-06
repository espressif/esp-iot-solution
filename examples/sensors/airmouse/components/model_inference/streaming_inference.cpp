/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "streaming_inference.h"

#include "predict.h"

static const char *const kLabelStorage[] = {
    "O",
    "S",
    "V",
    "Z",
    "background",
};

static const streaming_inference_model_t s_streaming_inference_model = {
    .input_length = 150,
    .input_channels = 3,
    .output_count = 5,
    .model_preprocess = nullptr,
    .model_init = gesture_inference_micro_simple_norm_model_init,
    .model_predict = gesture_inference_micro_simple_norm_model_predict,
};

extern "C" {

    const streaming_inference_model_t *streaming_inference_get_desc(void)
    {
        return &s_streaming_inference_model;
    }

    const char *streaming_inference_get_label(size_t index)
    {
        if (index >= 5) {
            return nullptr;
        }

        return kLabelStorage[index];
    }

}
