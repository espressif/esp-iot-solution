/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <esp_weaver.h>
#include <esp_weaver_standard_types.h>
#include <esp_weaver_standard_params.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create and configure the light device
 *
 * @param[in] default_power Default power state
 * @param[in] default_brightness Default brightness (0-100)
 * @param[in] default_hue Default hue (0-360)
 * @param[in] default_saturation Default saturation (0-100)
 * @return Pointer to the created device, or NULL on failure
 */
esp_weaver_device_t *app_light_device_create(bool default_power,
                                             int default_brightness, int default_hue, int default_saturation);

/**
 * @brief Get parameter by type from the light device
 *
 * @param[in] type Parameter type (must not be NULL, e.g., ESP_WEAVER_PARAM_POWER)
 * @return Parameter handle on success, or NULL on failure
 */
esp_weaver_param_t *app_light_get_param_by_type(const char *type);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
