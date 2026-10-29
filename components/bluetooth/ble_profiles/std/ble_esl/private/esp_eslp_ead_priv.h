/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_esl.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* AD Type: Encrypted Data (CSS) */
#define BLE_ESLP_AD_TYPE_ENCRYPTED_DATA                                     0x31

/* AD Type: ESL (Assigned Numbers) */
#define BLE_ESLP_AD_TYPE_ESL                                                0x34

#define BLE_ESLP_PAYLOAD_MAX_SIZE                                           BLE_ESL_PAYLOAD_MAX_SIZE

/* Encrypted Data AD max size for a full ESL Payload */
#define BLE_ESLP_EAD_OUTER_MAX_SIZE \
    (2 + 5 + (2 + BLE_ESLP_PAYLOAD_MAX_SIZE) + 4)

/**
 * @brief Encrypt an ESL sync command packet
 *
 * @param[in]  key          AP Sync Key Material
 * @param[in]  group_id     Group ID in bits [6:0]
 * @param[in]  tlvs         The pointer to store the command TLV(s)
 * @param[in]  tlvs_len     The length of @p tlvs
 * @param[out] out_ad       The pointer to store the Encrypted Data AD
 * @param[in]  out_ad_cap   The capacity of @p out_ad
 * @param[out] out_ad_len   The pointer to store the written length
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_FAIL on error
 */
esp_err_t eslp_ead_encrypt_sync_command(const esp_ble_esl_key_material_t *key,
                                        uint8_t group_id,
                                        const uint8_t *tlvs, uint8_t tlvs_len,
                                        uint8_t *out_ad, uint8_t out_ad_cap,
                                        uint8_t *out_ad_len);

/**
 * @brief Decrypt an ESL sync command packet
 *
 * @param[in]  key              AP Sync Key Material
 * @param[in]  enc_ad           The pointer to store the Encrypted Data AD
 * @param[in]  enc_ad_len       The length of @p enc_ad
 * @param[out] out_payload      The pointer to store the ESL Payload
 * @param[in]  out_payload_cap  The capacity of @p out_payload
 * @param[out] out_payload_len  The pointer to store the written length
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_FAIL on authentication failure
 */
esp_err_t eslp_ead_decrypt_sync_command(const esp_ble_esl_key_material_t *key,
                                        const uint8_t *enc_ad, uint8_t enc_ad_len,
                                        uint8_t *out_payload, uint8_t out_payload_cap,
                                        uint8_t *out_payload_len);

/**
 * @brief Encrypt an ESL sync response packet
 *
 * @param[in]  key          Response Key Material
 * @param[in]  tlvs         The pointer to store the response TLV(s)
 * @param[in]  tlvs_len     The length of @p tlvs
 * @param[out] out_ad       The pointer to store the Encrypted Data AD
 * @param[in]  out_ad_cap   The capacity of @p out_ad
 * @param[out] out_ad_len   The pointer to store the written length
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_FAIL on error
 */
esp_err_t eslp_ead_encrypt_sync_response(const esp_ble_esl_key_material_t *key,
                                         const uint8_t *tlvs, uint8_t tlvs_len,
                                         uint8_t *out_ad, uint8_t out_ad_cap,
                                         uint8_t *out_ad_len);

/**
 * @brief Decrypt an ESL sync response packet
 *
 * @param[in]  key           Response Key Material
 * @param[in]  enc_ad        The pointer to store the Encrypted Data AD
 * @param[in]  enc_ad_len    The length of @p enc_ad
 * @param[out] out_tlvs      The pointer to store the response TLV(s)
 * @param[in]  out_tlvs_cap  The capacity of @p out_tlvs
 * @param[out] out_tlvs_len  The pointer to store the written length
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_FAIL on authentication failure
 */
esp_err_t eslp_ead_decrypt_sync_response(const esp_ble_esl_key_material_t *key,
                                         const uint8_t *enc_ad, uint8_t enc_ad_len,
                                         uint8_t *out_tlvs, uint8_t out_tlvs_cap,
                                         uint8_t *out_tlvs_len);

/**
 * @brief Reset Tag-side response randomizer
 */
void eslp_ead_reset_response_randomizer(void);

#ifdef __cplusplus
}
#endif
