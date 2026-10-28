/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * No-op gateway stubs for standalone AP mode (no phone / JSONL).
 */

#include "app_gateway.h"

esp_err_t app_gateway_init(void)
{
    return ESP_OK;
}

esp_err_t app_gateway_start(void)
{
    return ESP_OK;
}

esp_err_t app_gateway_start_phone_first(void (*on_phone_ready)(void))
{
    (void)on_phone_ready;
    return ESP_OK;
}

void app_gateway_emit_event_line(const char *json_object)
{
    (void)json_object;
}

void app_gateway_on_esl_scan_result(const uint8_t addr[6], uint8_t addr_type, int8_t rssi, bool associated)
{
    (void)addr;
    (void)addr_type;
    (void)rssi;
    (void)associated;
}

void app_gateway_on_esl_response(uint8_t esl_id, uint8_t group_id, int resp_type)
{
    (void)esl_id;
    (void)group_id;
    (void)resp_type;
}

void app_gateway_on_esl_cmd_timeout(uint8_t esl_id, uint8_t group_id)
{
    (void)esl_id;
    (void)group_id;
}

void app_gateway_on_arbiter_complete(const esl_ap_arbiter_request_t *request, esp_err_t status)
{
    (void)request;
    (void)status;
}

void app_gateway_on_esl_state_changed(uint16_t esl_addr, int old_state, int new_state)
{
    (void)esl_addr;
    (void)old_state;
    (void)new_state;
}

void app_gateway_on_image_transferred(uint16_t conn_handle, uint8_t image_index, int status)
{
    (void)conn_handle;
    (void)image_index;
    (void)status;
}

void app_gateway_finish_command(const char *cmd, uint8_t esl_id, uint8_t group_id,
                                const char *status)
{
    (void)cmd;
    (void)esl_id;
    (void)group_id;
    (void)status;
}

void app_gateway_on_recovery_summary_changed(void)
{
}

void app_gateway_on_tag_recovery_changed(uint8_t slot)
{
    (void)slot;
}

bool app_gateway_is_phone_connected(void)
{
    return false;
}

bool app_gateway_is_started(void)
{
    return false;
}
