/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "platform/present_os.h"
#include "backend/present_backend_types.h"
/* Acceptance and completion are independent facts. Caller holds its short
 * lock; no callbacks, allocation or driver calls here. Rejection is retryable
 * only when no completion has proved access. Fault alone never proves drain. */
typedef struct {
    bool done;
    esp_err_t result;
} present_transaction_t;
typedef struct {
    bool rejected, complete;
    esp_err_t result;
} present_transaction_resolution_t;
static inline void present_transaction_prepare(present_transaction_t *transaction)
{
    *transaction = (present_transaction_t) {
        0
    };
}
static inline bool PRESENT_ISR_ATTR present_transaction_done(present_transaction_t *transaction, esp_err_t error)
{
    if (transaction->done) {
        return false;
    }
    transaction->done = true;
    if (transaction->result == ESP_OK) {
        transaction->result = error;
    }
    return true;
}
static inline present_transaction_resolution_t present_transaction_resolve(
    const present_transaction_t *transaction, esp_display_present_tx_result_t result)
{
    bool rejected = result.acceptance == ESP_DISPLAY_PRESENT_TX_REJECTED && !transaction->done;
    esp_err_t error = transaction->result != ESP_OK ? transaction->result : result.result;
    if (result.acceptance != ESP_DISPLAY_PRESENT_TX_ACCEPTED && error == ESP_OK) {
        error = ESP_FAIL;
    }
    return (present_transaction_resolution_t) {
        rejected, transaction->done, error
    };
}
