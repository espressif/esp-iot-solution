/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <protocomm_ext_security.h>
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_ESP_PROTOCOMM_EXT_SUPPORT_SECURITY_VERSION_2
/**
 * @brief   Protocomm Ext security version 2 implementation
 *
 * Client-side Security2 using SRP-6a (NG_3072 / SHA512) key exchange
 * and AES-256-GCM encryption
 */
extern const protocomm_ext_security_t protocomm_ext_security2;
#endif

#ifdef __cplusplus
}
#endif
