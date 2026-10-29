/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "led_indicator.h"
#include "led_indicator_blink_default.h"

/*********************************** Config Blink List in Different Conditions ***********************************/
#ifdef CONFIG_USE_MI_RGB_BLINK_DEFAULT
/**
 * @brief Device waiting for connection (Orange blinking)
 *
 */
static const blink_step_t default_mi_wait_connect[] = {
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 255, 69, 0), 100},     // Orange, ON for 0.1s
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0, 0, 0), 400},        // OFF for 0.4s
    {LED_BLINK_LOOP, 0, 0},
};

/**
 * @brief Device connecting to network (Blue blinking)
 *
 */
static const blink_step_t default_mi_connecting[] = {
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0, 0, 255), 100},      // Blue, ON for 0.1s
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0, 0, 0), 400},        // OFF for 0.4s
    {LED_BLINK_LOOP, 0, 0},
};

/**
 * @brief Device online (Solid blue)
 *
 */
static const blink_step_t default_mi_online[] = {
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0, 0, 255), 1000},     // Solid blue
    {LED_BLINK_LOOP, 0, 0},
};

/**
 * @brief Device fault state (Solid orange)
 *
 */
static const blink_step_t default_mi_fault[] = {
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 255, 69, 0), 1000},    // Solid orange
    {LED_BLINK_LOOP, 0, 0},
};

/**
 * @brief Network reconnecting (Blue blinking)
 *
 */
static const blink_step_t default_mi_reconnecting[] = {
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0, 0, 255), 100},      // Blue, ON for 0.1s
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0, 0, 0), 400},        // OFF for 0.4s
    {LED_BLINK_LOOP, 0, 0},
};

/**
 * @brief OTA updating (Blue blinking)
 *
 */
static const blink_step_t default_mi_ota_updating[] = {
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0, 0, 255), 100},      // Blue, ON for 0.1s
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0, 0, 0), 400},        // OFF for 0.4s
    {LED_BLINK_LOOP, 0, 0},
};

/**
 * @brief Wi-Fi provisioning disabled (LED OFF)
 *
 */
static const blink_step_t default_mi_wifi_provision_off[] = {
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0, 0, 0), 1000},       // LED OFF
    {LED_BLINK_STOP, 0, 0},
};

/**
 * @brief Normal working state (Solid white)
 *
 */
static const blink_step_t default_mi_working[] = {
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 255, 255, 255), 1000}, // Solid white
    {LED_BLINK_LOOP, 0, 0},
};

/**
 * @brief Working abnormal/High risk alarm (Red blinking)
 *
 */
static const blink_step_t default_mi_work_abnormal[] = {
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 255, 0, 0), 100},      // Red, ON for 0.1s
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0, 0, 0), 400},        // OFF for 0.4s
    {LED_BLINK_LOOP, 0, 0},
};

/**
 * @brief Environment good (Solid green)
 *
 */
static const blink_step_t default_mi_env_good[] = {
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 0, 255, 0), 1000},     // Solid green
    {LED_BLINK_LOOP, 0, 0},
};

/**
 * @brief Environment moderate pollution (Solid orange)
 *
 */
static const blink_step_t default_mi_env_moderate[] = {
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 255, 69, 0), 1000},    // Solid orange
    {LED_BLINK_LOOP, 0, 0},
};

/**
 * @brief Environment severe pollution (Solid red)
 *
 */
static const blink_step_t default_mi_env_severe[] = {
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 255, 0, 0), 1000},     // Solid red
    {LED_BLINK_LOOP, 0, 0},
};

/**
 * @brief Device setting (Solid white)
 *
 */
static const blink_step_t default_mi_setting[] = {
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 255, 255, 255), 1000}, // Solid white
    {LED_BLINK_LOOP, 0, 0},
};

/**
 * @brief Low battery (Solid red)
 *
 */
static const blink_step_t default_mi_low_battery[] = {
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 255, 0, 0), 1000},     // Solid red
    {LED_BLINK_LOOP, 0, 0},
};

/**
 * @brief Charging (White breathing)
 *
 */
static const blink_step_t default_mi_charging[] = {
    {LED_BLINK_RGB, SET_IRGB(MAX_INDEX, 255, 255, 255), 250},
    {LED_BLINK_BREATHE, INSERT_INDEX(MAX_INDEX, LED_STATE_OFF), 1500},
    {LED_BLINK_BRIGHTNESS, INSERT_INDEX(MAX_INDEX, LED_STATE_OFF), 500},
    {LED_BLINK_BREATHE, INSERT_INDEX(MAX_INDEX, LED_STATE_ON), 1500},
    {LED_BLINK_BRIGHTNESS, INSERT_INDEX(MAX_INDEX, LED_STATE_ON), 250},
    {LED_BLINK_LOOP, 0, 0},
};

/**
 * @brief Fully charged (LED OFF)
 *
 */
static const blink_step_t default_mi_charged[] = {
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

#else
/**
 * @brief connecting to AP (or Cloud)
 *
 */
static const blink_step_t default_connecting[] = {
    {LED_BLINK_HOLD, LED_STATE_ON, 200},
    {LED_BLINK_HOLD, LED_STATE_OFF, 800},
    {LED_BLINK_LOOP, 0, 0},
};

/**
 * @brief connected to AP (or Cloud) succeed
 *
 */
static const blink_step_t default_connected[] = {
    {LED_BLINK_HOLD, LED_STATE_ON, 1000},
    {LED_BLINK_LOOP, 0, 0},
};

/**
 * @brief reconnecting to AP (or Cloud), if lose connection
 *
 */
static const blink_step_t default_reconnecting[] = {
    {LED_BLINK_HOLD, LED_STATE_ON, 100},
    {LED_BLINK_HOLD, LED_STATE_OFF, 200},
    {LED_BLINK_LOOP, 0, 0},
}; //offline

/**
 * @brief updating software
 *
 */
static const blink_step_t default_updating[] = {
    {LED_BLINK_HOLD, LED_STATE_ON, 50},
    {LED_BLINK_HOLD, LED_STATE_OFF, 100},
    {LED_BLINK_HOLD, LED_STATE_ON, 50},
    {LED_BLINK_HOLD, LED_STATE_OFF, 800},
    {LED_BLINK_LOOP, 0, 0},
};

/**
 * @brief restoring factory settings
 *
 */
static const blink_step_t default_factory_reset[] = {
    {LED_BLINK_HOLD, LED_STATE_ON, 200},
    {LED_BLINK_HOLD, LED_STATE_OFF, 200},
    {LED_BLINK_LOOP, 0, 0},
};

/**
 * @brief provision done
 *
 */
static const blink_step_t default_provisioned[] = {
    {LED_BLINK_HOLD, LED_STATE_OFF, 1000},
    {LED_BLINK_STOP, 0, 0},
};

/**
 * @brief provisioning
 *
 */
static const blink_step_t default_provisioning[] = {
    {LED_BLINK_HOLD, LED_STATE_ON, 500},
    {LED_BLINK_HOLD, LED_STATE_OFF, 500},
    {LED_BLINK_LOOP, 0, 0},
};

/**
 * @brief LED indicator blink lists, the index like BLINK_FACTORY_RESET defined the priority of the blink
 *
 */
blink_step_t const *default_led_indicator_blink_lists[] = {
    [BLINK_FACTORY_RESET] = default_factory_reset,
    [BLINK_UPDATING] = default_updating,
    [BLINK_CONNECTED] = default_connected,
    [BLINK_PROVISIONED] = default_provisioned,
    [BLINK_RECONNECTING] = default_reconnecting,
    [BLINK_CONNECTING] = default_connecting,
    [BLINK_PROVISIONING] = default_provisioning,
    [BLINK_MAX] = NULL,
};
#endif

/* LED blink_steps handling machine implementation */
const int DEFAULT_BLINK_LIST_NUM = (sizeof(default_led_indicator_blink_lists) / sizeof(default_led_indicator_blink_lists[0]) - 1);
