/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0 */
#include "backend/present_backend.h"
#include "core/present_completion_tracker.h"
#include <assert.h>

typedef struct {
    esp_display_present_frame_status_t status[2];
    unsigned queries, releases;
    esp_err_t query_error, release_error;
} fixture_t;
static esp_err_t query(present_backend_t *backend, esp_display_present_receipt_t receipt, esp_display_present_frame_status_t *out)
{
    fixture_t *fixture = (fixture_t *)backend;
    assert(receipt.value >= 1 && receipt.value <= 2);
    ++fixture->queries;
    *out = fixture->status[receipt.value - 1];
    return fixture->query_error;
}
static esp_err_t release(present_backend_t *backend, esp_display_present_receipt_t receipt)
{
    fixture_t *fixture = (fixture_t *)backend;
    assert(receipt.value == fixture->releases + 1);
    if (fixture->release_error == ESP_OK) {
        ++fixture->releases;
    }
    return fixture->release_error;
}
int main(void)
{
    const present_backend_ops_t ops = {.query = query, .release_receipt = release};
    fixture_t fixture = {.status = {{.sealed = true}, {.sealed = true, .terminal = true}}};
    present_backend_t *backend = (present_backend_t *)&fixture;
    present_completion_tracker_t tracker = {.limit = 2, .count = 2, .committed = 2,
    .receipts = {{1}, {2}}, .observers = {{.token = 55, .backend = {1}}}
                                           };
    assert(present_completion_reap(&tracker, &ops, backend) == ESP_OK);
    assert(tracker.count == 2 && !fixture.releases); /* Preserve FIFO when a later frame is already terminal. */
    fixture.status[0].terminal = true;
    fixture.status[0].result = ESP_ERR_TIMEOUT;
    fixture.query_error = ESP_FAIL;
    assert(present_completion_reap(&tracker, &ops, backend) == ESP_FAIL);
    assert(tracker.count == 2 && !tracker.observers[0].status.terminal);
    fixture.query_error = ESP_OK;
    fixture.release_error = ESP_FAIL;
    assert(present_completion_reap(&tracker, &ops, backend) == ESP_FAIL);
    assert(tracker.count == 2 && !fixture.releases && !tracker.observers[0].status.terminal);
    fixture.release_error = ESP_OK;
    assert(present_completion_reap(&tracker, &ops, backend) == ESP_OK);
    assert(!tracker.count && tracker.head == 0 && tracker.completed == 2 && fixture.releases == 2);
    assert(tracker.first_error == ESP_ERR_TIMEOUT);
    present_completion_record_error(&tracker, ESP_FAIL);
    assert(tracker.first_error == ESP_ERR_TIMEOUT);
    present_completion_observer_t *observer = present_completion_find(&tracker, (esp_display_present_engine_receipt_t) {
        55
    });
    assert(observer);
    unsigned queries = fixture.queries;
    esp_display_present_frame_status_t status;
    assert(present_completion_query(observer, &ops, backend, &status) == ESP_OK);
    assert(status.terminal && status.result == ESP_ERR_TIMEOUT && fixture.queries == queries); /* Cache survives backend receipt release. */
    assert(!present_completion_find(&tracker, (esp_display_present_engine_receipt_t) {
        56
    }));
    assert(present_completion_query(NULL, &ops, backend, &status) == ESP_ERR_INVALID_STATE);
    return 0;
}
