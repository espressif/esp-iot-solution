/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "gesture_model.h"

#include "predict.h"

static const char *const kLabelStorage[] = {
    "O",
    "S",
    "V",
    "Z",
    "background",
};

static const imu_gesture_inference_model_t s_model = {
    .input_length = 150,
    .input_channels = 3,
    .output_count = 5,
    .model_init = gesture_inference_micro_simple_norm_model_init,
    .model_predict = gesture_inference_micro_simple_norm_model_predict,
};

extern "C" {

    const imu_gesture_inference_model_t *gesture_inference_micro_simple_norm_gesture_model_get_desc(void)
    {
        return &s_model;
    }

    const char *gesture_inference_micro_simple_norm_gesture_model_get_label(size_t index)
    {
        if (index >= 5) {
            return nullptr;
        }

        return kLabelStorage[index];
    }

}
