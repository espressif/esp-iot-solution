/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
* @brief Ethernet link status
*
*/
typedef enum {
    IOT_ETH_LINK_DOWN, /*!< Ethernet link is down */
    IOT_ETH_LINK_UP,  /*!< Ethernet link is up */
} iot_eth_link_t;

#ifdef __cplusplus
}
#endif
