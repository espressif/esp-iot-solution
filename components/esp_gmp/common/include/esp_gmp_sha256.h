/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "esp_err.h"
#include "psa/crypto.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    psa_hash_operation_t op;
    bool active;
} esp_gmp_sha256_ctx_t;

esp_err_t esp_gmp_sha256_begin(esp_gmp_sha256_ctx_t *ctx);
esp_err_t esp_gmp_sha256_update(esp_gmp_sha256_ctx_t *ctx, const uint8_t *data, size_t len);
esp_err_t esp_gmp_sha256_finish(esp_gmp_sha256_ctx_t *ctx, uint8_t out[32]);
void esp_gmp_sha256_abort(esp_gmp_sha256_ctx_t *ctx);

esp_err_t esp_gmp_sha256_digest(const uint8_t *data, size_t len, uint8_t out[32]);

#ifdef __cplusplus
}
#endif
