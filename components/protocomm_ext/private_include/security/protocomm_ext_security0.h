/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <protocomm_ext_security.h>

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_ESP_PROTOCOMM_EXT_SUPPORT_SECURITY_VERSION_0
/**
 * @brief   Protocomm Ext security version 0 implementation
 *
 * This is a simple implementation to be used when no
 * security is required for the protocomm ext instance
 */
extern const protocomm_ext_security_t protocomm_ext_security0;
#endif

#ifdef __cplusplus
}
#endif
