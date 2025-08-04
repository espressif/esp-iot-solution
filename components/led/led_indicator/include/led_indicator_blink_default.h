/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include "led_indicator.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief The blink type with smaller index has the higher priority
 * eg. BLINK_FACTORY_RESET priority is higher than BLINK_UPDATING
 */
#ifdef CONFIG_USE_MI_RGB_BLINK_DEFAULT
enum {
    BLINK_MI_WAIT_CONNECT,           /*!< Device waiting for connection */
    BLINK_MI_CONNECTING,             /*!< Connecting to network */
    BLINK_MI_ONLINE,                 /*!< Device online */
    BLINK_MI_FAULT,                  /*!< Device fault (connection failed/network error/disconnected) */
    BLINK_MI_RECONNECTING,           /*!< Reconnecting to network */
    BLINK_MI_OTA_UPDATING,           /*!< OTA updating */
    BLINK_MI_WIFI_PROVISION_OFF,     /*!< Wi-Fi provision function off */
    BLINK_MI_WORKING,                /*!< Device working normally */
    BLINK_MI_WORK_ABNORMAL,          /*!< Device abnormal or high-risk alarm */
    BLINK_MI_ENV_GOOD,               /*!< Environment status good */
    BLINK_MI_ENV_MODERATE,           /*!< Environment moderate pollution or call action */
    BLINK_MI_ENV_SEVERE,             /*!< Environment severe pollution or device fault */
    BLINK_MI_SETTING,                /*!< Device in setting mode */
    BLINK_MI_LOW_BATTERY,            /*!< Low battery */
    BLINK_MI_CHARGING,               /*!< Charging */
    BLINK_MI_CHARGED,                /*!< Fully charged */
    BLINK_MAX,                    /*!< INVALID type */
};
#else
enum {
    BLINK_FACTORY_RESET,           /*!< restoring factory settings */
    BLINK_UPDATING,                /*!< updating software */
    BLINK_CONNECTED,               /*!< connected to AP (or Cloud) succeed */
    BLINK_PROVISIONED,             /*!< provision done */
    BLINK_CONNECTING,              /*!< connecting to AP (or Cloud) */
    BLINK_RECONNECTING,            /*!< reconnecting to AP (or Cloud), if lose connection */
    BLINK_PROVISIONING,            /*!< provisioning */
    BLINK_MAX,                     /*!< INVALID type */
};
#endif

extern const int DEFAULT_BLINK_LIST_NUM;
extern blink_step_t const *default_led_indicator_blink_lists[];

#ifdef __cplusplus
}
#endif
