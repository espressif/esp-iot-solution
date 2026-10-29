/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef ESL_AP_ARBITER_HOST_TEST
#include "esp_err.h"
#include "ble_esl_common.h"
#else
#ifndef ESP_OK
typedef int esp_err_t;
#define ESP_OK                 0
#define ESP_ERR_INVALID_ARG    0x102
#define ESP_ERR_INVALID_STATE  0x103
#define ESP_ERR_NO_MEM         0x101
#define ESP_ERR_TIMEOUT        0x107
#define ESP_FAIL               -1
#define ESP_ERR_INVALID_RESPONSE 0x108
#endif
#ifndef BLE_ESL_TLV_MAX_SIZE
#define BLE_ESL_TLV_MAX_SIZE   17
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ESL_AP_ARBITER_MAX_GROUPS
#define ESL_AP_ARBITER_MAX_GROUPS      8
#endif

#ifndef ESL_AP_ARBITER_QUEUE_DEPTH
#define ESL_AP_ARBITER_QUEUE_DEPTH     8
#endif

#ifndef ESL_AP_ARBITER_AGING_MS
#define ESL_AP_ARBITER_AGING_MS        1000
#endif

typedef enum {
    ESL_AP_ARBITER_PRIO_HEALTH   = 0,   /* weight 1 */
    ESL_AP_ARBITER_PRIO_APP      = 1,   /* weight 2 */
    ESL_AP_ARBITER_PRIO_RECOVERY = 2,   /* weight 4; aging ceiling */
} esl_ap_arbiter_priority_t;

typedef enum {
    ESL_AP_EXPECT_BASIC_STATE = 1,
    ESL_AP_EXPECT_ERROR_OR_BASIC_STATE,
    ESL_AP_EXPECT_COMMAND_SPECIFIC,
} esl_ap_expected_response_t;

typedef struct {
    uint32_t  request_id;
    uint64_t  boot_id;
    uint32_t  recovery_generation;
    uint32_t  record_generation;
    uint8_t   record_slot;
    uint16_t  esl_addr;
    uint8_t   group_id;
    uint8_t   command_opcode;
    esl_ap_expected_response_t expected_response;
    esl_ap_arbiter_priority_t priority;
    uint32_t  guard_timeout_ms;
    uint8_t   params_len;
    uint8_t   params[BLE_ESL_TLV_MAX_SIZE];
} esl_ap_arbiter_request_t;

typedef void (*esl_ap_arbiter_completion_cb_t)(const esl_ap_arbiter_request_t *request,
                                               const uint8_t *data, uint8_t len,
                                               esp_err_t status);

/** Called once when a queued request becomes the group active context (PAwR send). */
typedef void (*esl_ap_arbiter_send_cb_t)(const esl_ap_arbiter_request_t *request);

esp_err_t esl_ap_arbiter_init(esl_ap_arbiter_completion_cb_t completion_cb);
void esl_ap_arbiter_set_send_cb(esl_ap_arbiter_send_cb_t send_cb);
esp_err_t esl_ap_arbiter_submit(const esl_ap_arbiter_request_t *request);
esp_err_t esl_ap_arbiter_invalidate_generation(uint8_t group_id, uint16_t esl_addr,
                                               uint32_t before_generation);
bool esl_ap_arbiter_group_busy(uint8_t group_id, esl_ap_arbiter_priority_t *out_active_priority);

/**
 * Inject a response for the active context of group_id (PAwR path / host test).
 * Matches esl_addr + command_opcode + request_id.
 */
esp_err_t esl_ap_arbiter_on_response(uint8_t group_id, uint16_t esl_addr,
                                     uint8_t command_opcode, uint32_t request_id,
                                     const uint8_t *data, uint8_t len);

/**
 * Complete the active request when esl_addr + command_opcode + request_id match.
 * Use non-ESP_OK status for Error responses / send failures.
 */
esp_err_t esl_ap_arbiter_complete(uint8_t group_id, uint16_t esl_addr,
                                  uint8_t command_opcode, uint32_t request_id,
                                  esp_err_t status);

/** Copy the group's active request if any; returns false when idle. */
bool esl_ap_arbiter_peek_active(uint8_t group_id, esl_ap_arbiter_request_t *out);

/** Advance time-based aging and guard timeouts; schedule next if idle. */
void esl_ap_arbiter_tick(uint64_t now_ms);

#ifdef ESL_AP_ARBITER_HOST_TEST
void esl_ap_arbiter_host_reset(void);
uint32_t esl_ap_arbiter_host_queue_depth(uint8_t group_id);
bool esl_ap_arbiter_host_has_active(uint8_t group_id);
esl_ap_arbiter_priority_t esl_ap_arbiter_host_active_priority(uint8_t group_id);
uint32_t esl_ap_arbiter_host_completed_count(esl_ap_arbiter_priority_t prio);
uint32_t esl_ap_arbiter_host_cancelled_count(void);
#endif

#ifdef __cplusplus
}
#endif
