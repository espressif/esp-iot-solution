/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "esl_ap_arbiter.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t app_gateway_init(void);
esp_err_t app_gateway_start(void);

/**
 * Start NUS advertising for phone observability.
 * ESL/PAwR recovery is started independently by app_main; hello only reports status.
 * @deprecated Prefer app_gateway_start(); on_phone_ready is ignored.
 */
esp_err_t app_gateway_start_phone_first(void (*on_phone_ready)(void));

/** Push a JSONL event line (without trailing newline) to the phone if connected. */
void app_gateway_emit_event_line(const char *json_object);

/** Notify gateway about ESL protocol events for async JSONL events. */
void app_gateway_on_esl_scan_result(const uint8_t addr[6], uint8_t addr_type, int8_t rssi, bool associated);
void app_gateway_on_esl_response(uint8_t esl_id, uint8_t group_id, int resp_type);
void app_gateway_on_esl_cmd_timeout(uint8_t esl_id, uint8_t group_id);
void app_gateway_on_esl_state_changed(uint16_t esl_addr, int old_state, int new_state);
void app_gateway_on_image_transferred(uint16_t conn_handle, uint8_t image_index, int status);
void app_gateway_finish_command(const char *cmd, uint8_t esl_id, uint8_t group_id, const char *status);

/** Arbiter completion for APP-priority PAwR commands (opcode-matched). */
void app_gateway_on_arbiter_complete(const esl_ap_arbiter_request_t *request, esp_err_t status);

/** Recovery observability hooks (recovery-v1). */
void app_gateway_on_recovery_summary_changed(void);
void app_gateway_on_tag_recovery_changed(uint8_t slot);

bool app_gateway_is_phone_connected(void);
bool app_gateway_is_started(void);

/**
 * Drop the phone ACL and stop NUS advertising so a PAwR response slot
 * is not starved. Resume restarts advertising if the gateway is already up.
 */
void app_gateway_set_pawr_radio_hold(bool hold);

#ifdef __cplusplus
}
#endif
