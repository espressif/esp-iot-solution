/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief The blink type with smaller index has the higher priority
 * eg. BLINK_FACTORY_RESET priority is higher than BLINK_UPDATING
 */
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

extern const int DEFAULT_BLINK_LIST_NUM;
extern blink_step_t const *default_led_indicator_blink_lists[];

#ifdef __cplusplus
}
#endif
