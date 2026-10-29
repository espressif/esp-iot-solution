/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_eslp_ap.h"

#ifdef __cplusplus
extern "C"
{
#endif

esp_err_t eslp_ap_cmd_init(void);
esp_err_t eslp_ap_cmd_deinit(void);

/**
 * @brief Clear pending ECP procedure after a response
 *
 * @param[in]  conn_handle Connection handle
 */
void eslp_ap_cmd_on_ecp_response(uint16_t conn_handle);

/**
 * @brief Clear pending ECP procedure after disconnect
 *
 * @param[in]  conn_handle Connection handle
 */
void eslp_ap_cmd_on_disconnected(uint16_t conn_handle);

/**
 * @brief Write ECP TLV and start the procedure timer
 *
 * @param[in]  conn_handle Connection handle
 * @param[in]  tlv         The pointer to store the ECP TLV
 * @param[in]  tlv_len     The length of ECP TLV
 * @param[in]  esl_id      ESL ID
 * @param[in]  group_id    ESL Group ID
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t eslp_ap_cmd_write_ecp(uint16_t conn_handle, const uint8_t *tlv, uint8_t tlv_len,
                                uint8_t esl_id, uint8_t group_id);

#ifdef __cplusplus
}
#endif
