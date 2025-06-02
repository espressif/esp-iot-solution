/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "iot_eth.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Handle of netif glue - an intermediate layer between netif and Ethernet driver
 *
 */
typedef struct iot_eth_netif_glue_t* iot_eth_netif_glue_handle_t;

/**
 * @brief Create a netif glue
 *
 * @param eth_hdl Ethernet handle
 *
 * @return iot_eth_netif_glue_handle_t
 */
iot_eth_netif_glue_handle_t iot_eth_new_netif_glue(iot_eth_handle_t eth_hdl);

/**
 * @brief Destroy a netif glue
 *
 * @param netif_glue_hdl Netif glue handle
 */
esp_err_t iot_eth_del_netif_glue(iot_eth_netif_glue_handle_t netif_glue_hdl);

#ifdef __cplusplus
}
#endif
