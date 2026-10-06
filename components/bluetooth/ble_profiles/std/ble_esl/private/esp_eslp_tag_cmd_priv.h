/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_eslp_tag.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief   Tag command handler result
 */
typedef struct {
    bool has_response;                              /*!< True if @p rsp should be sent */
    uint8_t rsp_len;                                /*!< Response TLV length */
    uint8_t rsp[BLE_ESL_ECP_MAX_SIZE];              /*!< Response TLV */
} eslp_tag_cmd_result_t;

esp_err_t eslp_tag_cmd_init(void);
esp_err_t eslp_tag_cmd_deinit(void);

/**
 * @brief Cancel pending timed Display/LED commands
 */
void eslp_tag_cmd_cancel_pending(void);

/**
 * @brief Recompute ACTIVE_LED / PENDING_* bits in the Tag Basic State
 *
 * @param[in,out] basic_state The pointer to store the Basic State bitmap
 */
void eslp_tag_cmd_sync_basic_state(uint16_t *basic_state);

/**
 * @brief Handle Display / LED / Sensor commands
 *
 * @param[in]  cmd    The pointer to store the ECP command
 * @param[out] result The pointer to store the response
 * @param[in]  pawr   True when invoked from PAwR
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_NOT_SUPPORTED on unsupported opcode
 */
esp_err_t eslp_tag_cmd_handle(const esp_ble_eslp_ecp_command_t *cmd,
                              eslp_tag_cmd_result_t *result, bool pawr);

#ifdef __cplusplus
}
#endif
