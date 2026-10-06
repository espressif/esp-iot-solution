/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_esl.h"
#include "esp_eslp_state.h"

#ifdef __cplusplus
extern "C"
{
#endif

esp_err_t eslp_ap_lifecycle_init(void);
esp_err_t eslp_ap_lifecycle_deinit(void);
esp_err_t eslp_ap_lifecycle_on_configured(uint16_t conn_handle, const esp_ble_esl_address_t *addr,
                                          const esp_ble_esl_key_material_t *resp_key);
esp_err_t eslp_ap_lifecycle_on_sync_transferred(uint16_t conn_handle);
esp_err_t eslp_ap_lifecycle_on_disconnected(uint16_t conn_handle);
esp_err_t eslp_ap_lifecycle_on_connected(uint16_t conn_handle, const esp_ble_esl_address_t *addr);
esp_err_t eslp_ap_lifecycle_note_sync_activity(uint16_t conn_handle);

esp_err_t eslp_ap_lifecycle_get_conn(const esp_ble_esl_address_t *addr, uint16_t *out_conn);
esp_err_t eslp_ap_lifecycle_get_address(uint16_t conn_handle, esp_ble_esl_address_t *out_addr);

/**
 * @brief Decrypt a PAwR response using tracked Tag Response Keys
 *
 * @param[in]  group_id       ESL Group ID
 * @param[in]  enc_ad         The pointer to store the Encrypted Data AD
 * @param[in]  enc_ad_len     The length of @p enc_ad
 * @param[out] out_tlvs       The pointer to store the response TLV(s)
 * @param[in]  out_tlvs_cap   The capacity of @p out_tlvs
 * @param[out] out_tlvs_len   The pointer to store the written length
 * @param[out] out_addr       The pointer to store the Tag address
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_NOT_FOUND if no Response Key matched
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t eslp_ap_lifecycle_decrypt_pawr_response(uint8_t group_id,
                                                  const uint8_t *enc_ad, uint8_t enc_ad_len,
                                                  uint8_t *out_tlvs, uint8_t out_tlvs_cap,
                                                  uint8_t *out_tlvs_len,
                                                  esp_ble_esl_address_t *out_addr);

#ifdef __cplusplus
}
#endif
