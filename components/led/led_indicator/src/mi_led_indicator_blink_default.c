/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "led_indicator.h"
#include "led_indicator_blink_default.h"

/*********************************** Config Blink List in Different Conditions ***********************************/
// LED state definitions
static const blink_step_t default_mi_wait_connect[] = {  // Device waiting for connection (Orange blinking)
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 255, 69, 0), 100},  // Orange, ON for 0.1s
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0, 0, 0), 400},     // OFF for 0.4s
    {LED_BLINK_LOOP, 0, 0},
};

static const blink_step_t default_mi_connecting[] = {    // Device connecting to network (Blue blinking)
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0, 0, 255), 100},   // Blue, ON for 0.1s
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0, 0, 0), 400},     // OFF for 0.4s
    {LED_BLINK_LOOP, 0, 0},
};

static const blink_step_t default_mi_online[] = {        // Device online (Solid blue)
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0, 0, 255), 1000},     // Solid blue
    {LED_BLINK_LOOP, 0, 0},
};

static const blink_step_t default_mi_fault[] = {         // Device fault state (Solid orange)
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 255, 69, 0), 1000},   // Solid orange
    {LED_BLINK_LOOP, 0, 0},
};

static const blink_step_t default_mi_reconnecting[] = {  // Network reconnecting (Blue blinking)
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0, 0, 255), 100},   // Blue, ON for 0.1s
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0, 0, 0), 400},     // OFF for 0.4s
    {LED_BLINK_LOOP, 0, 0},
};

static const blink_step_t default_mi_ota_updating[] = {  // OTA updating (Blue blinking)
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0, 0, 255), 100},   // Blue, ON for 0.1s
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0, 0, 0), 400},     // OFF for 0.4s
    {LED_BLINK_LOOP, 0, 0},
};

static const blink_step_t default_mi_wifi_provision_off[] = {  // Wi-Fi provisioning disabled (LED OFF)
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0, 0, 0), 1000},      // LED OFF
    {LED_BLINK_STOP, 0, 0},
};

static const blink_step_t default_mi_working[] = {       // Normal working state (Solid white)
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 255, 255, 255), 1000}, // Solid white
    {LED_BLINK_LOOP, 0, 0},
};

static const blink_step_t default_mi_work_abnormal[] = { // Working abnormal/High risk alarm (Red blinking)
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 255, 0, 0), 100},   // Red, ON for 0.1s
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0, 0, 0), 400},     // OFF for 0.4s
    {LED_BLINK_LOOP, 0, 0},
};

static const blink_step_t default_mi_env_good[] = {      // Environment good (Solid green)
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0, 255, 0), 1000},     // Solid green
    {LED_BLINK_LOOP, 0, 0},
};

static const blink_step_t default_mi_env_moderate[] = {  // Environment moderate pollution (Solid orange)
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 255, 69, 0), 1000},   // Solid orange
    {LED_BLINK_LOOP, 0, 0},
};

static const blink_step_t default_mi_env_severe[] = {    // Environment severe pollution (Solid red)
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 255, 0, 0), 1000},     // Solid red
    {LED_BLINK_LOOP, 0, 0},
};

static const blink_step_t default_mi_setting[] = {       // Device setting (Solid white)
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 255, 255, 255), 1000}, // Solid white
    {LED_BLINK_LOOP, 0, 0},
};

static const blink_step_t default_mi_low_battery[] = {   // Low battery (Solid red)
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 255, 0, 0), 1000},     // Solid red
    {LED_BLINK_LOOP, 0, 0},
};

static const blink_step_t default_mi_charging[] = {      // Charging (White breathing)
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 255, 255, 255), 250},
    {LED_BLINK_BREATHE, INSERT_INDEX(MAX_INDEX, LED_STATE_OFF), 1500},
    {LED_BLINK_BRIGHTNESS, INSERT_INDEX(MAX_INDEX, LED_STATE_OFF), 500},
    {LED_BLINK_BREATHE, INSERT_INDEX(MAX_INDEX, LED_STATE_ON), 1500},
    {LED_BLINK_BRIGHTNESS, INSERT_INDEX(MAX_INDEX, LED_STATE_ON), 250},
    {LED_BLINK_LOOP, 0, 0},
};

static const blink_step_t default_mi_charged[] = {       // Fully charged (LED OFF)
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0, 0, 0), 0},      // LED OFF
    {LED_BLINK_STOP, 0, 0},
};

/**
 * @brief LED indicator blink lists, the index like BLINK_MI_WAIT_CONNECT defined the priority of the blink
 *
 */
blink_step_t const *default_led_indicator_blink_lists[] = {
    [BLINK_MI_WAIT_CONNECT]        = default_mi_wait_connect,
    [BLINK_MI_CONNECTING]          = default_mi_connecting,
    [BLINK_MI_ONLINE]              = default_mi_online,
    [BLINK_MI_FAULT]               = default_mi_fault,
    [BLINK_MI_RECONNECTING]        = default_mi_reconnecting,
    [BLINK_MI_OTA_UPDATING]        = default_mi_ota_updating,
    [BLINK_MI_WIFI_PROVISION_OFF]  = default_mi_wifi_provision_off,
    [BLINK_MI_WORKING]             = default_mi_working,
    [BLINK_MI_WORK_ABNORMAL]       = default_mi_work_abnormal,
    [BLINK_MI_ENV_GOOD]            = default_mi_env_good,
    [BLINK_MI_ENV_MODERATE]        = default_mi_env_moderate,
    [BLINK_MI_ENV_SEVERE]          = default_mi_env_severe,
    [BLINK_MI_SETTING]             = default_mi_setting,
    [BLINK_MI_LOW_BATTERY]         = default_mi_low_battery,
    [BLINK_MI_CHARGING]            = default_mi_charging,
    [BLINK_MI_CHARGED]             = default_mi_charged,
    [BLINK_MAX]                 = NULL,
};

/* LED blink_steps handling machine implementation */
const int DEFAULT_BLINK_LIST_NUM = (sizeof(default_led_indicator_blink_lists) / sizeof(default_led_indicator_blink_lists[0]) - 1);
