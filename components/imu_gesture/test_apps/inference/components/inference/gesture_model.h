/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>

#include "imu_gesture_inference.h"

#ifdef __cplusplus
extern "C" {
#endif

const imu_gesture_inference_model_t *gesture_inference_micro_simple_norm_gesture_model_get_desc(void);
const char *gesture_inference_micro_simple_norm_gesture_model_get_label(size_t index);

#ifdef __cplusplus
}
#endif
