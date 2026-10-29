/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "iot_knob.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a knob using RTC IO pins
 *
 * @param config Knob configuration with RTC GPIO numbers for encoder A/B
 * @return Knob handle, or NULL on failure
 */
knob_handle_t iot_knob_create_rtc(const knob_config_t *config);

#ifdef __cplusplus
}
#endif
