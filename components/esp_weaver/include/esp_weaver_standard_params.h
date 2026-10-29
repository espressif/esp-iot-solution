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
#define ESP_WEAVER_DEF_GESTURE_TYPE          "Gesture Type"
#define ESP_WEAVER_DEF_GESTURE_CONFIDENCE    "Gesture Confidence"
#define ESP_WEAVER_DEF_GESTURE_TOSS          "Toss Event"
#define ESP_WEAVER_DEF_GESTURE_FLIP          "Flip Event"
#define ESP_WEAVER_DEF_GESTURE_SHAKE         "Shake Event"
#define ESP_WEAVER_DEF_GESTURE_ROTATION      "Rotation Event"
#define ESP_WEAVER_DEF_GESTURE_PUSH          "Push Event"
#define ESP_WEAVER_DEF_GESTURE_CIRCLE        "Circle Event"
#define ESP_WEAVER_DEF_GESTURE_CLAP_SINGLE   "Clap Single Event"
#define ESP_WEAVER_DEF_GESTURE_CLAP_DOUBLE   "Clap Double Event"
#define ESP_WEAVER_DEF_GESTURE_CLAP_TRIPLE   "Clap Triple Event"
#define ESP_WEAVER_DEF_GESTURE_ORIENTATION   "Orientation Change"
#define ESP_WEAVER_DEF_GESTURE_X_ORIENTATION "X Orientation"
#define ESP_WEAVER_DEF_GESTURE_Y_ORIENTATION "Y Orientation"
#define ESP_WEAVER_DEF_GESTURE_Z_ORIENTATION "Z Orientation"
#define ESP_WEAVER_DEF_GESTURE_SENSITIVITY   "Sensitivity"

#ifdef __cplusplus
}
#endif  /* __cplusplus */
