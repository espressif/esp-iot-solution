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

typedef imu_gesture_inference_model_t streaming_inference_model_t;

const streaming_inference_model_t *streaming_inference_get_desc(void);
const char *streaming_inference_get_label(size_t index);

#ifdef __cplusplus
}
#endif
