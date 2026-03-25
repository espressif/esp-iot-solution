/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Suggested default names for the parameters.
 *
 * @note These names are not mandatory. You can use the ESP Weaver Core APIs
 * to create your own parameters with custom names, if required.
 */

#define ESP_WEAVER_DEF_NAME_PARAM            "Name"
#define ESP_WEAVER_DEF_POWER_NAME            "Power"
#define ESP_WEAVER_DEF_BRIGHTNESS_NAME       "Brightness"
#define ESP_WEAVER_DEF_HUE_NAME              "Hue"
#define ESP_WEAVER_DEF_SATURATION_NAME       "Saturation"
#define ESP_WEAVER_DEF_CCT_NAME              "CCT"

#ifdef __cplusplus
}
#endif  /* __cplusplus */
