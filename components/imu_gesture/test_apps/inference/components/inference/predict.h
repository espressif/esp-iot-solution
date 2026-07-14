/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

bool gesture_inference_micro_simple_norm_model_init();
bool gesture_inference_micro_simple_norm_model_predict(
    const float* input,
    size_t input_len,
    float* output,
    size_t output_len);
int gesture_inference_micro_simple_norm_model_predict_label(const float* input, size_t input_len);

#ifdef __cplusplus
}
#endif
