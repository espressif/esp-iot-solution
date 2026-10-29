/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_gmp_sha256.h"
#include <string.h>

esp_err_t esp_gmp_sha256_begin(esp_gmp_sha256_ctx_t *ctx)
{
    if (!ctx) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->op = psa_hash_operation_init();
    if (psa_hash_setup(&ctx->op, PSA_ALG_SHA_256) != PSA_SUCCESS) {
        return ESP_FAIL;
    }
    ctx->active = true;
    return ESP_OK;
}

esp_err_t esp_gmp_sha256_update(esp_gmp_sha256_ctx_t *ctx, const uint8_t *data, size_t len)
{
    if (!ctx || !ctx->active) {
        return ESP_ERR_INVALID_STATE;
    }
    if (len == 0) {
        return ESP_OK;
    }
    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }
    if (psa_hash_update(&ctx->op, data, len) != PSA_SUCCESS) {
        esp_gmp_sha256_abort(ctx);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t esp_gmp_sha256_finish(esp_gmp_sha256_ctx_t *ctx, uint8_t out[32])
{
    if (!ctx || !ctx->active || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t hash_len = 0;
    psa_status_t st = psa_hash_finish(&ctx->op, out, 32, &hash_len);
    ctx->active = false;
    if (st != PSA_SUCCESS || hash_len != 32) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

void esp_gmp_sha256_abort(esp_gmp_sha256_ctx_t *ctx)
{
    if (!ctx || !ctx->active) {
        return;
    }
    psa_hash_abort(&ctx->op);
    ctx->active = false;
}

esp_err_t esp_gmp_sha256_digest(const uint8_t *data, size_t len, uint8_t out[32])
{
    if (!data || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t hash_len = 0;
    if (psa_hash_compute(PSA_ALG_SHA_256, data, len, out, 32, &hash_len) != PSA_SUCCESS ||
            hash_len != 32) {
        return ESP_FAIL;
    }
    return ESP_OK;
}
