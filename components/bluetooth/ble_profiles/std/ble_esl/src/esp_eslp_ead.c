/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief ESL Encrypted Advertising Data helpers for PAwR sync packets
 */

#include <string.h>

#include "esp_log.h"
#include "esp_random.h"
#include "sdkconfig.h"

#include "esp_eslp_ead_priv.h"

#if defined(CONFIG_BT_NIMBLE_ENC_ADV_DATA)
#include "host/ble_ead.h"
#include "host/ble_aes_ccm.h"
#endif

static const char *TAG = "ble_eslp_ead";

#if defined(CONFIG_BT_NIMBLE_ENC_ADV_DATA)

static uint8_t s_response_randomizer[BLE_EAD_RANDOMIZER_SIZE];
static bool s_response_randomizer_inited;

static uint8_t s_cmd_randomizer[BLE_EAD_RANDOMIZER_SIZE];
static bool s_cmd_randomizer_inited;

static void randomizer_seed(uint8_t *randomizer, bool response)
{
    esp_fill_random(randomizer, BLE_EAD_RANDOMIZER_SIZE);
    if (response) {
        randomizer[BLE_EAD_RANDOMIZER_SIZE - 1] |=
            (uint8_t)(1U << BLE_EAD_RANDOMIZER_DIRECTION_BIT);
    } else {
        randomizer[BLE_EAD_RANDOMIZER_SIZE - 1] &=
            (uint8_t)~(1U << BLE_EAD_RANDOMIZER_DIRECTION_BIT);
    }
}

static void response_randomizer_init_if_needed(void)
{
    if (s_response_randomizer_inited) {
        return;
    }
    randomizer_seed(s_response_randomizer, true);
    s_response_randomizer_inited = true;
}

static void response_randomizer_increment(void)
{
    for (uint8_t i = 0; i < BLE_EAD_RANDOMIZER_SIZE; i++) {
        if (i == BLE_EAD_RANDOMIZER_SIZE - 1) {
            uint8_t counter = (uint8_t)((s_response_randomizer[i] & 0x7F) + 1);
            s_response_randomizer[i] =
                (uint8_t)(counter | (1U << BLE_EAD_RANDOMIZER_DIRECTION_BIT));
            break;
        }
        s_response_randomizer[i]++;
        if (s_response_randomizer[i] != 0) {
            break;
        }
    }
}

static void cmd_randomizer_init_if_needed(void)
{
    if (s_cmd_randomizer_inited) {
        return;
    }
    /* Direction bit clear for AP to ESL */
    randomizer_seed(s_cmd_randomizer, false);
    s_cmd_randomizer_inited = true;
}

static void cmd_randomizer_increment(void)
{
    for (uint8_t i = 0; i < BLE_EAD_RANDOMIZER_SIZE; i++) {
        if (i == BLE_EAD_RANDOMIZER_SIZE - 1) {
            s_cmd_randomizer[i] = (uint8_t)((s_cmd_randomizer[i] + 1) & 0x7F);
            break;
        }
        s_cmd_randomizer[i]++;
        if (s_cmd_randomizer[i] != 0) {
            break;
        }
    }
}

static int ead_encrypt_with_randomizer(const uint8_t session_key[BLE_EAD_KEY_SIZE],
                                       const uint8_t iv[BLE_EAD_IV_SIZE],
                                       const uint8_t randomizer[BLE_EAD_RANDOMIZER_SIZE],
                                       const uint8_t *payload, size_t payload_size,
                                       uint8_t *encrypted_payload)
{
    static const uint8_t ead_aad[] = { 0xEA };
    uint8_t nonce[BLE_EAD_NONCE_SIZE];

    memcpy(nonce, randomizer, BLE_EAD_RANDOMIZER_SIZE);
    memcpy(nonce + BLE_EAD_RANDOMIZER_SIZE, iv, BLE_EAD_IV_SIZE);
    memcpy(encrypted_payload, randomizer, BLE_EAD_RANDOMIZER_SIZE);

    return ble_aes_ccm_encrypt(session_key, nonce, payload, payload_size,
                               ead_aad, sizeof(ead_aad),
                               encrypted_payload + BLE_EAD_RANDOMIZER_SIZE,
                               BLE_EAD_MIC_SIZE);
}

static esp_err_t build_inner_esl_ad(const uint8_t *esl_payload, uint8_t payload_len,
                                    uint8_t *out, uint8_t out_cap, uint8_t *out_len)
{
    if (!esl_payload || !out || !out_len || payload_len == 0 ||
            payload_len > BLE_ESLP_PAYLOAD_MAX_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t inner_len = (uint8_t)(1 + payload_len); /* type + payload */
    uint8_t total = (uint8_t)(1 + inner_len);       /* len + type + payload */
    if (total > out_cap) {
        return ESP_ERR_INVALID_SIZE;
    }
    out[0] = inner_len;
    out[1] = BLE_ESLP_AD_TYPE_ESL;
    memcpy(&out[2], esl_payload, payload_len);
    *out_len = total;
    return ESP_OK;
}

static esp_err_t strip_inner_esl_ad(const uint8_t *plain, uint8_t plain_len,
                                    uint8_t *out_payload, uint8_t out_cap,
                                    uint8_t *out_payload_len)
{
    if (!plain || !out_payload || !out_payload_len || plain_len < 2) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t inner_len = plain[0];
    uint8_t inner_type = plain[1];
    if (inner_type != BLE_ESLP_AD_TYPE_ESL ||
            inner_len < 1 ||
            (uint16_t)(1 + inner_len) > plain_len) {
        return ESP_ERR_INVALID_SIZE;
    }
    uint8_t payload_len = (uint8_t)(inner_len - 1);
    if (payload_len > out_cap) {
        return ESP_ERR_INVALID_SIZE;
    }
    memcpy(out_payload, &plain[2], payload_len);
    *out_payload_len = payload_len;
    return ESP_OK;
}

static esp_err_t parse_outer_ead(const uint8_t *enc_ad, uint8_t enc_ad_len,
                                 const uint8_t **enc_payload, uint8_t *enc_payload_len)
{
    if (!enc_ad || !enc_payload || !enc_payload_len) {
        return ESP_ERR_INVALID_ARG;
    }
    /* Minimum: [len][type][randomizer][1 ciphertext][MIC] */
    if (enc_ad_len < 2 + BLE_EAD_RANDOMIZER_SIZE + BLE_EAD_MIC_SIZE + 1) {
        return ESP_ERR_INVALID_SIZE;
    }
    uint8_t ad_len = enc_ad[0];
    uint8_t ad_type = enc_ad[1];
    if (ad_type != BLE_ESLP_AD_TYPE_ENCRYPTED_DATA ||
            ad_len < 1 + BLE_EAD_RANDOMIZER_SIZE + BLE_EAD_MIC_SIZE ||
            (uint16_t)(ad_len + 1) > enc_ad_len) {
        return ESP_ERR_INVALID_SIZE;
    }
    *enc_payload = &enc_ad[2];
    *enc_payload_len = (uint8_t)(ad_len - 1);
    return ESP_OK;
}

static esp_err_t wrap_outer_ead(const uint8_t *enc_payload, uint8_t enc_payload_len,
                                uint8_t *out_ad, uint8_t out_ad_cap, uint8_t *out_ad_len)
{
    uint16_t ad_len = (uint16_t)(1 + enc_payload_len); /* type + payload */
    uint16_t total = (uint16_t)(1 + ad_len);
    if (total > out_ad_cap || ad_len > 0xFF) {
        return ESP_ERR_INVALID_SIZE;
    }
    out_ad[0] = (uint8_t)ad_len;
    out_ad[1] = BLE_ESLP_AD_TYPE_ENCRYPTED_DATA;
    memcpy(&out_ad[2], enc_payload, enc_payload_len);
    *out_ad_len = (uint8_t)total;
    return ESP_OK;
}

static int ead_encrypt_response_local(const uint8_t session_key[BLE_EAD_KEY_SIZE],
                                      const uint8_t iv[BLE_EAD_IV_SIZE],
                                      const uint8_t *payload, size_t payload_size,
                                      uint8_t *encrypted_payload)
{
    response_randomizer_init_if_needed();
    int ret = ead_encrypt_with_randomizer(session_key, iv, s_response_randomizer,
                                          payload, payload_size, encrypted_payload);
    if (ret == 0) {
        response_randomizer_increment();
    }
    return ret;
}

static int ead_encrypt_command_local(const uint8_t session_key[BLE_EAD_KEY_SIZE],
                                     const uint8_t iv[BLE_EAD_IV_SIZE],
                                     const uint8_t *payload, size_t payload_size,
                                     uint8_t *encrypted_payload)
{
    cmd_randomizer_init_if_needed();
    int ret = ead_encrypt_with_randomizer(session_key, iv, s_cmd_randomizer,
                                          payload, payload_size, encrypted_payload);
    if (ret == 0) {
        cmd_randomizer_increment();
    }
    return ret;
}

void eslp_ead_reset_response_randomizer(void)
{
    randomizer_seed(s_response_randomizer, true);
    s_response_randomizer_inited = true;
}

esp_err_t eslp_ead_encrypt_sync_command(const esp_ble_esl_key_material_t *key,
                                        uint8_t group_id,
                                        const uint8_t *tlvs, uint8_t tlvs_len,
                                        uint8_t *out_ad, uint8_t out_ad_cap,
                                        uint8_t *out_ad_len)
{
    if (!key || !tlvs || !out_ad || !out_ad_len || tlvs_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if ((uint16_t)(1 + tlvs_len) > BLE_ESLP_PAYLOAD_MAX_SIZE) {
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t esl_payload[BLE_ESLP_PAYLOAD_MAX_SIZE];
    esl_payload[0] = (uint8_t)(group_id & 0x7F);
    memcpy(&esl_payload[1], tlvs, tlvs_len);
    uint8_t esl_payload_len = (uint8_t)(1 + tlvs_len);

    uint8_t plaintext[2 + BLE_ESLP_PAYLOAD_MAX_SIZE];
    uint8_t plaintext_len = 0;
    esp_err_t ret = build_inner_esl_ad(esl_payload, esl_payload_len,
                                       plaintext, sizeof(plaintext), &plaintext_len);
    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t enc_payload[BLE_EAD_ENCRYPTED_PAYLOAD_SIZE(2 + BLE_ESLP_PAYLOAD_MAX_SIZE)];
    int rc = ead_encrypt_command_local(key->session_key, key->iv,
                                       plaintext, plaintext_len, enc_payload);
    if (rc != 0) {
        ESP_LOGE(TAG, "command EAD encrypt failed: %d", rc);
        return ESP_FAIL;
    }

    return wrap_outer_ead(enc_payload, (uint8_t)BLE_EAD_ENCRYPTED_PAYLOAD_SIZE(plaintext_len),
                          out_ad, out_ad_cap, out_ad_len);
}

esp_err_t eslp_ead_decrypt_sync_command(const esp_ble_esl_key_material_t *key,
                                        const uint8_t *enc_ad, uint8_t enc_ad_len,
                                        uint8_t *out_payload, uint8_t out_payload_cap,
                                        uint8_t *out_payload_len)
{
    if (!key || !enc_ad || !out_payload || !out_payload_len) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t *enc_payload = NULL;
    uint8_t enc_payload_len = 0;
    esp_err_t ret = parse_outer_ead(enc_ad, enc_ad_len, &enc_payload, &enc_payload_len);
    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t plain[2 + BLE_ESLP_PAYLOAD_MAX_SIZE];
    uint8_t plain_len = (uint8_t)BLE_EAD_DECRYPTED_PAYLOAD_SIZE(enc_payload_len);
    if (plain_len > sizeof(plain)) {
        return ESP_ERR_INVALID_SIZE;
    }

    int rc = ble_ead_decrypt(key->session_key, key->iv, enc_payload, enc_payload_len, plain);
    if (rc != 0) {
        ESP_LOGW(TAG, "ble_ead_decrypt (sync cmd) failed: %d", rc);
        return ESP_ERR_INVALID_CRC;
    }

    return strip_inner_esl_ad(plain, plain_len, out_payload, out_payload_cap, out_payload_len);
}

esp_err_t eslp_ead_encrypt_sync_response(const esp_ble_esl_key_material_t *key,
                                         const uint8_t *tlvs, uint8_t tlvs_len,
                                         uint8_t *out_ad, uint8_t out_ad_cap,
                                         uint8_t *out_ad_len)
{
    if (!key || !tlvs || !out_ad || !out_ad_len || tlvs_len == 0 ||
            tlvs_len > BLE_ESLP_PAYLOAD_MAX_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t plaintext[2 + BLE_ESLP_PAYLOAD_MAX_SIZE];
    uint8_t plaintext_len = 0;
    esp_err_t ret = build_inner_esl_ad(tlvs, tlvs_len, plaintext, sizeof(plaintext), &plaintext_len);
    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t enc_payload[BLE_EAD_ENCRYPTED_PAYLOAD_SIZE(2 + BLE_ESLP_PAYLOAD_MAX_SIZE)];
    int rc = ead_encrypt_response_local(key->session_key, key->iv,
                                        plaintext, plaintext_len, enc_payload);
    if (rc != 0) {
        ESP_LOGE(TAG, "response EAD encrypt failed: %d", rc);
        return ESP_FAIL;
    }

    return wrap_outer_ead(enc_payload, (uint8_t)BLE_EAD_ENCRYPTED_PAYLOAD_SIZE(plaintext_len),
                          out_ad, out_ad_cap, out_ad_len);
}

esp_err_t eslp_ead_decrypt_sync_response(const esp_ble_esl_key_material_t *key,
                                         const uint8_t *enc_ad, uint8_t enc_ad_len,
                                         uint8_t *out_tlvs, uint8_t out_tlvs_cap,
                                         uint8_t *out_tlvs_len)
{
    if (!key || !enc_ad || !out_tlvs || !out_tlvs_len) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t *enc_payload = NULL;
    uint8_t enc_payload_len = 0;
    esp_err_t ret = parse_outer_ead(enc_ad, enc_ad_len, &enc_payload, &enc_payload_len);
    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t plain[2 + BLE_ESLP_PAYLOAD_MAX_SIZE];
    uint8_t plain_len = (uint8_t)BLE_EAD_DECRYPTED_PAYLOAD_SIZE(enc_payload_len);
    if (plain_len > sizeof(plain)) {
        return ESP_ERR_INVALID_SIZE;
    }

    int rc = ble_ead_decrypt(key->session_key, key->iv, enc_payload, enc_payload_len, plain);
    if (rc != 0) {
        return ESP_ERR_INVALID_CRC;
    }

    return strip_inner_esl_ad(plain, plain_len, out_tlvs, out_tlvs_cap, out_tlvs_len);
}

#else /* !CONFIG_BT_NIMBLE_ENC_ADV_DATA */

void eslp_ead_reset_response_randomizer(void)
{
}

esp_err_t eslp_ead_encrypt_sync_command(const esp_ble_esl_key_material_t *key,
                                        uint8_t group_id,
                                        const uint8_t *tlvs, uint8_t tlvs_len,
                                        uint8_t *out_ad, uint8_t out_ad_cap,
                                        uint8_t *out_ad_len)
{
    (void)key;
    (void)group_id;
    (void)tlvs;
    (void)tlvs_len;
    (void)out_ad;
    (void)out_ad_cap;
    (void)out_ad_len;
    ESP_LOGE(TAG, "BT_NIMBLE_ENC_ADV_DATA is required for ESL EAD");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t eslp_ead_decrypt_sync_command(const esp_ble_esl_key_material_t *key,
                                        const uint8_t *enc_ad, uint8_t enc_ad_len,
                                        uint8_t *out_payload, uint8_t out_payload_cap,
                                        uint8_t *out_payload_len)
{
    (void)key;
    (void)enc_ad;
    (void)enc_ad_len;
    (void)out_payload;
    (void)out_payload_cap;
    (void)out_payload_len;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t eslp_ead_encrypt_sync_response(const esp_ble_esl_key_material_t *key,
                                         const uint8_t *tlvs, uint8_t tlvs_len,
                                         uint8_t *out_ad, uint8_t out_ad_cap,
                                         uint8_t *out_ad_len)
{
    (void)key;
    (void)tlvs;
    (void)tlvs_len;
    (void)out_ad;
    (void)out_ad_cap;
    (void)out_ad_len;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t eslp_ead_decrypt_sync_response(const esp_ble_esl_key_material_t *key,
                                         const uint8_t *enc_ad, uint8_t enc_ad_len,
                                         uint8_t *out_tlvs, uint8_t out_tlvs_cap,
                                         uint8_t *out_tlvs_len)
{
    (void)key;
    (void)enc_ad;
    (void)enc_ad_len;
    (void)out_tlvs;
    (void)out_tlvs_cap;
    (void)out_tlvs_len;
    return ESP_ERR_NOT_SUPPORTED;
}

#endif /* CONFIG_BT_NIMBLE_ENC_ADV_DATA */
