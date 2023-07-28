/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "led_indicator.h"
#include "led_indicator_blink_default.h"

/*********************************** Config Blink List in Different Conditions ***********************************/
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

/* LED blink_steps handling machine implementation */
const int DEFAULT_BLINK_LIST_NUM = (sizeof(default_led_indicator_blink_lists) / sizeof(default_led_indicator_blink_lists[0]) - 1);
