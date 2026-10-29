/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/********** STANDARD UI TYPES **********/

#define ESP_WEAVER_UI_TEXT              "esp.ui.text"
#define ESP_WEAVER_UI_TOGGLE            "esp.ui.toggle"
#define ESP_WEAVER_UI_SLIDER            "esp.ui.slider"
#define ESP_WEAVER_UI_HUE_SLIDER        "esp.ui.hue-slider"

/********** STANDARD PARAM TYPES **********/

#define ESP_WEAVER_PARAM_NAME           "esp.param.name"
#define ESP_WEAVER_PARAM_POWER          "esp.param.power"
#define ESP_WEAVER_PARAM_BRIGHTNESS     "esp.param.brightness"
#define ESP_WEAVER_PARAM_HUE            "esp.param.hue"
#define ESP_WEAVER_PARAM_SATURATION     "esp.param.saturation"
#define ESP_WEAVER_PARAM_CCT            "esp.param.cct"
#define ESP_WEAVER_PARAM_GESTURE_TYPE        "esp.param.gesture-type"
#define ESP_WEAVER_PARAM_GESTURE_CONFIDENCE  "esp.param.gesture-confidence"
#define ESP_WEAVER_PARAM_GESTURE_TOSS        "esp.param.gesture-toss"
#define ESP_WEAVER_PARAM_GESTURE_FLIP        "esp.param.gesture-flip"
#define ESP_WEAVER_PARAM_GESTURE_SHAKE       "esp.param.gesture-shake"
#define ESP_WEAVER_PARAM_GESTURE_ROTATION    "esp.param.gesture-rotation"
#define ESP_WEAVER_PARAM_GESTURE_PUSH        "esp.param.gesture-push"
#define ESP_WEAVER_PARAM_GESTURE_CIRCLE      "esp.param.gesture-circle"
#define ESP_WEAVER_PARAM_GESTURE_CLAP_SINGLE "esp.param.gesture-clap-single"
#define ESP_WEAVER_PARAM_GESTURE_CLAP_DOUBLE "esp.param.gesture-clap-double"
#define ESP_WEAVER_PARAM_GESTURE_CLAP_TRIPLE "esp.param.gesture-clap-triple"
#define ESP_WEAVER_PARAM_ORIENTATION_CHANGE  "esp.param.orientation-change"
#define ESP_WEAVER_PARAM_ORIENTATION_X       "esp.param.orientation-x"
#define ESP_WEAVER_PARAM_ORIENTATION_Y       "esp.param.orientation-y"
#define ESP_WEAVER_PARAM_ORIENTATION_Z       "esp.param.orientation-z"
#define ESP_WEAVER_PARAM_SENSITIVITY         "esp.param.sensitivity"

/********** STANDARD DEVICE TYPES **********/

#define ESP_WEAVER_DEVICE_LIGHTBULB     "esp.device.lightbulb"
#define ESP_WEAVER_DEVICE_IMU_GESTURE   "esp.device.imu-gesture"

#ifdef __cplusplus
}
#endif  /* __cplusplus */
