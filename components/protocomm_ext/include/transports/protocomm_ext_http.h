/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "protocomm_ext_transports.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Protocomm Ext HTTP/HTTPS transport implementation
 *
 * This transport allows protocomm ext to communicate over HTTP/HTTPS
 * as a client. It supports:
 * - Regular IP addresses and hostnames
 * - Secure HTTPS connections
 */
extern const protocomm_ext_transport_t protocomm_ext_transport_http;

#ifdef __cplusplus
}
#endif
