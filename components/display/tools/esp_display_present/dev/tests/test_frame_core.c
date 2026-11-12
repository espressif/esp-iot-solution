/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "../../src/backend/present_frame_core.h"
#include <assert.h>
#include "../../src/platform/present_allocation.h"
#include "../../src/backend/present_transaction.h"

int main(void)
{
    present_allocation_t allocation = {0};
    present_allocation_hold(&allocation, PRESENT_REF_SCANOUT);
    assert(!present_allocation_write(&allocation, false, false));
    assert(present_allocation_write(&allocation, false, true));
    present_allocation_written(&allocation);
    present_allocation_drop(&allocation, PRESENT_REF_SCANOUT);
    present_allocation_hold(&allocation, PRESENT_REF_TRANSFER);
    assert(!present_allocation_write(&allocation, true, true));
    present_allocation_drop(&allocation, PRESENT_REF_TRANSFER);
    present_allocation_hold(&allocation, PRESENT_REF_COPY);
    assert(!present_allocation_write(&allocation, true, true));
    present_allocation_drop(&allocation, PRESENT_REF_COPY);
    assert(present_allocation_write(&allocation, false, false));
    present_allocation_written(&allocation);
    present_transaction_t tx;
    present_transaction_prepare(&tx);
    present_transaction_resolution_t decision = present_transaction_resolve(&tx,
    (esp_display_present_tx_result_t) {
        .acceptance = ESP_DISPLAY_PRESENT_TX_REJECTED
    });
    assert(decision.rejected && !decision.complete);
    assert(present_transaction_done(&tx, ESP_OK));
    assert(!present_transaction_done(&tx, ESP_FAIL));
    decision = present_transaction_resolve(&tx,
    (esp_display_present_tx_result_t) {
        .acceptance = ESP_DISPLAY_PRESENT_TX_REJECTED
    });
    assert(!decision.rejected && decision.complete && decision.result == ESP_FAIL);
    present_transaction_prepare(&tx);
    decision = present_transaction_resolve(&tx,
    (esp_display_present_tx_result_t) {
        .acceptance = ESP_DISPLAY_PRESENT_TX_ACCEPTED, .result = ESP_FAIL
    });
    assert(!decision.rejected && !decision.complete && decision.result == ESP_FAIL);
    present_frame_core_t c, other;
    assert(present_frame_core_init(&c, 1) == ESP_OK);
    assert(present_frame_core_init(&other, 1) == ESP_OK);
    esp_display_present_frame_t frame, next, isolated;
    esp_display_present_receipt_t receipt;
    assert(present_frame_core_begin(&c, &frame) == ESP_OK);
    assert(present_frame_core_begin(&other, &isolated) == ESP_OK);
    assert(frame.value != isolated.value);
    present_core_frame_t *f = present_frame_core_find(&c, frame.value);
    /* Streaming output can finish before seal without recycling the frame. */
    f->pending = 1;
    present_frame_core_complete(&c, f);
    assert(f->common.token == frame.value);
    f->leased = 1;
    assert(present_frame_core_seal(&c, f, &receipt) == ESP_ERR_INVALID_STATE);
    f->leased = 0;
    f->pending = 1;
    assert(present_frame_core_seal(&c, f, &receipt) == ESP_OK);
    assert(present_frame_core_begin(&c, &next) == ESP_ERR_NO_MEM);
    assert(present_frame_core_release(f) == ESP_ERR_INVALID_STATE);
    present_frame_core_complete(&c, f);
    assert(f->common.token == receipt.value);
    assert(present_frame_core_release(f) == ESP_OK);
    assert(present_frame_core_begin(&c, &frame) == ESP_OK);
    f = present_frame_core_find(&c, frame.value);
    f->pending = 1;
    assert(present_frame_core_seal(&c, f, NULL) == ESP_OK);
    present_frame_core_complete(&c, f);
    assert(!present_frame_core_find(&c, frame.value));
    /* Cancel remains live while closing until its final access ends. */
    assert(present_frame_core_begin(&c, &frame) == ESP_OK);
    f = present_frame_core_find(&c, frame.value);
    f->pending = 1;
    f->sealed = f->canceled = true;
    c.life = PRESENT_CORE_CLOSING;
    assert(present_frame_core_quiescent(&c) == ESP_ERR_TIMEOUT);
    assert(!present_cancel_matches(&c.canceled, frame.value));
    present_frame_core_fail(&c, f, 123, ESP_FAIL);
    present_frame_core_fail(&c, f, 456, ESP_ERR_INVALID_STATE);
    present_frame_core_complete(&c, f);
    assert(present_cancel_matches(&c.canceled, frame.value));
    assert(c.fault.result == ESP_FAIL && c.fault.transaction_cookie == 123);
    assert(present_frame_core_quiescent(&c) == ESP_OK);
    assert(present_frame_core_begin(&c, &next) == ESP_ERR_INVALID_STATE);
    assert(other.fault.result == ESP_OK);
    assert(present_frame_core_find(&other, isolated.value));
    present_lock_t lock;
    assert(present_lock_init(&lock));
    esp_display_present_frame_request_t req = {0};
    assert(present_frame_core_admit(&c, &lock, 4, 4, &req, &next) == ESP_ERR_INVALID_STATE);
    assert(present_frame_core_admit(&other, &lock, 4, 4, &req, &next) == ESP_OK);
    assert(present_frame_core_can_destroy(&other) == ESP_ERR_INVALID_STATE);
    present_core_frame_t *of = present_frame_core_find(&other, isolated.value);
    of->pending = 1;
    assert(present_frame_core_seal(&other, of, &receipt) == ESP_OK);
    esp_display_present_frame_status_t st;
    assert(present_frame_core_query(&other, &lock, receipt, true, &st) == ESP_OK);
    assert(st.sealed && !st.src_released && !st.terminal && !st.present_confirmed);
    assert(present_frame_core_release_receipt(&other, &lock, receipt) == ESP_ERR_INVALID_STATE);
    present_frame_core_terminal(&other, of);
    assert(present_frame_core_query(&other, &lock, receipt, true, &st) == ESP_OK);
    assert(st.terminal && st.src_released && st.present_confirmed);
    assert(present_frame_core_release_receipt(&other, &lock, receipt) == ESP_OK);
    other.life = PRESENT_CORE_CLOSING;
    assert(present_frame_core_quiescent(&other) == ESP_OK);
    assert(present_frame_core_can_destroy(&other) == ESP_OK);
    present_lock_destroy(&lock);
    present_frame_core_deinit(&c);
    present_frame_core_deinit(&other);
    return 0;
}
