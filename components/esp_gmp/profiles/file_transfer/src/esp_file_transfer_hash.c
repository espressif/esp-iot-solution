/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_file_transfer_internal.h"

#include <stdlib.h>

#include "psa/crypto.h"
struct ft_hash_context {
    psa_hash_operation_t operation;
    bool active;
};

esp_err_t ft_hash_create(ft_hash_context_t **context)
{
    if (!context) {
        return ESP_ERR_INVALID_ARG;
    }
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        return ESP_FAIL;
    }
    ft_hash_context_t *created = calloc(1, sizeof(*created));
    if (!created) {
        return ESP_ERR_NO_MEM;
    }
    created->operation = psa_hash_operation_init();
    status = psa_hash_setup(&created->operation, PSA_ALG_SHA_256);
    if (status != PSA_SUCCESS) {
        free(created);
        return status == PSA_ERROR_NOT_SUPPORTED ? ESP_ERR_NOT_SUPPORTED : ESP_FAIL;
    }
    created->active = true;
    *context = created;
    return ESP_OK;
}

esp_err_t ft_hash_update(ft_hash_context_t *context, const void *data, size_t len)
{
    if (!context || !context->active || (!data && len != 0)) {
        return ESP_ERR_INVALID_ARG;
    }
    return psa_hash_update(&context->operation, data, len) == PSA_SUCCESS ? ESP_OK : ESP_FAIL;
}

esp_err_t ft_hash_finish(ft_hash_context_t *context, uint8_t digest[32])
{
    if (!context || !context->active || !digest) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t digest_len = 0;
    psa_status_t status = psa_hash_finish(&context->operation, digest, 32, &digest_len);
    context->active = false;
    return status == PSA_SUCCESS && digest_len == 32 ? ESP_OK : ESP_FAIL;
}

void ft_hash_destroy(ft_hash_context_t *context)
{
    if (!context) {
        return;
    }
    if (context->active) {
        psa_hash_abort(&context->operation);
    }
    free(context);
}
