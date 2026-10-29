/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * IDF 6.0+ ships Mbed TLS 4 / TF-PSA-Crypto and drops the legacy
 * mbedtls/{aes,gcm,sha256,sha512,ecdh}.h public headers. Use PSA there.
 */
#pragma once

#include "esp_idf_version.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
#define PROTOCOMM_EXT_USE_PSA_CRYPTO 1
#endif
