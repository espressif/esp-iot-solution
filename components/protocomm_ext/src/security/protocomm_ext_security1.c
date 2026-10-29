/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <esp_err.h>
#include <esp_log.h>
#include "esp_system.h"

#include "protocomm_ext_crypto.h"

#ifdef PROTOCOMM_EXT_USE_PSA_CRYPTO
#include "psa/crypto.h"
#else
#include <mbedtls/aes.h>
#include <mbedtls/sha256.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/error.h>

#ifdef CONFIG_MBEDTLS_ECDH_LEGACY_CONTEXT
#define ACCESS_ECDH(S, var) S->MBEDTLS_PRIVATE(var)
#else
#define ACCESS_ECDH(S, var) S->MBEDTLS_PRIVATE(ctx).MBEDTLS_PRIVATE(mbed_ecdh).MBEDTLS_PRIVATE(var)
#endif
#endif

#include "protocomm_ext_security.h"
#include "protocomm_ext_security1.h"

#include "session.pb-c.h"
#include "sec1.pb-c.h"
#include "constants.pb-c.h"

static const char* TAG = "security1";

#define PUBLIC_KEY_LEN  32
#define SZ_RANDOM       16

#define SESSION_STATE_RESP0  0 /* Waiting for response0 */
#define SESSION_STATE_RESP1  1 /* Waiting for response1 */
#define SESSION_STATE_DONE   2 /* Session setup successful */

typedef struct session {
    /* Session data */
    uint8_t state;
    uint8_t device_pubkey[PUBLIC_KEY_LEN];
    uint8_t client_pubkey[PUBLIC_KEY_LEN];
    uint8_t sym_key[PUBLIC_KEY_LEN];
    uint8_t rand[SZ_RANDOM];

#ifdef PROTOCOMM_EXT_USE_PSA_CRYPTO
    psa_cipher_operation_t ctx_aes;
    psa_key_id_t ecdh_key_id;
    psa_key_id_t aes_key_id;
#else
    /* mbedtls context data for AES */
    mbedtls_aes_context ctx_aes;
    unsigned char stb[16];
    size_t nc_off;

    /* mbedtls context data for Curve25519 */
    mbedtls_ecdh_context     *ctx_server;
    mbedtls_entropy_context  *entropy;
    mbedtls_ctr_drbg_context *ctr_drbg;
#endif
    bool ctx_aes_init;

    uint8_t *sec_params;
    uint16_t sec_params_len;
} session_t;

#ifndef PROTOCOMM_EXT_USE_PSA_CRYPTO
static void flip_endian(uint8_t *data, size_t len)
{
    uint8_t swp_buf;
    for (int i = 0; i < len / 2; i++) {
        swp_buf = data[i];
        data[i] = data[len - i - 1];
        data[len - i - 1] = swp_buf;
    }
}
#endif

static void hexdump(const char *msg, uint8_t *buf, int len)
{
    ESP_LOGD(TAG, "%s:", msg);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, buf, len, ESP_LOG_DEBUG);
}

static esp_err_t prepare_command0(session_t *session, SessionData *req)
{
    Sec1Payload *in = (Sec1Payload *) malloc(sizeof(Sec1Payload));
    if (in == NULL) {
        ESP_LOGE(TAG, "Error allocating memory for request");
        return ESP_ERR_NO_MEM;
    }

    SessionCmd0 *in_req = (SessionCmd0 *) malloc(sizeof(SessionCmd0));
    if (in_req == NULL) {
        ESP_LOGE(TAG, "Error allocating memory for request");
        free(in);
        return ESP_ERR_NO_MEM;
    }

    sec1_payload__init(in);
    session_cmd0__init(in_req);

    in_req->client_pubkey.data = session->client_pubkey;
    in_req->client_pubkey.len = PUBLIC_KEY_LEN;

    in->msg = SEC1_MSG_TYPE__Session_Command0;
    in->payload_case = SEC1_PAYLOAD__PAYLOAD_SC0;
    in->sc0 = in_req;

    req->proto_case = SESSION_DATA__PROTO_SEC1;
    req->sec_ver = protocomm_ext_security1.ver;
    req->sec1 = in;

    return ESP_OK;
}

static esp_err_t prepare_command1(session_t *session, SessionData *req)
{
#ifndef PROTOCOMM_EXT_USE_PSA_CRYPTO
    int ret;
#endif
    uint8_t *outbuf = (uint8_t *) malloc(PUBLIC_KEY_LEN);
    if (!outbuf) {
        ESP_LOGE(TAG, "Error allocating ciphertext buffer");
        return ESP_ERR_NO_MEM;
    }

#ifdef PROTOCOMM_EXT_USE_PSA_CRYPTO
    psa_status_t status;
    psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_algorithm_t alg = PSA_ALG_CTR;
    size_t output_len = 0;

    psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&key_attributes, alg);
    psa_set_key_type(&key_attributes, PSA_KEY_TYPE_AES);
    psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_VOLATILE);
    psa_set_key_bits(&key_attributes, sizeof(session->sym_key) * 8);
    status = psa_import_key(&key_attributes, session->sym_key, sizeof(session->sym_key),
                            &session->aes_key_id);
    psa_reset_key_attributes(&key_attributes);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_import_key failed with status=%d", (int) status);
        free(outbuf);
        return ESP_FAIL;
    }

    session->ctx_aes = psa_cipher_operation_init();
    status = psa_cipher_encrypt_setup(&session->ctx_aes, session->aes_key_id, alg);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_cipher_encrypt_setup failed with status=%d", (int) status);
        psa_destroy_key(session->aes_key_id);
        session->aes_key_id = 0;
        free(outbuf);
        return ESP_FAIL;
    }
    status = psa_cipher_set_iv(&session->ctx_aes, session->rand, sizeof(session->rand));
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_cipher_set_iv failed with status=%d", (int) status);
        psa_cipher_abort(&session->ctx_aes);
        psa_destroy_key(session->aes_key_id);
        session->aes_key_id = 0;
        free(outbuf);
        return ESP_FAIL;
    }
    session->ctx_aes_init = true;

    status = psa_cipher_update(&session->ctx_aes, session->device_pubkey, PUBLIC_KEY_LEN,
                               outbuf, PUBLIC_KEY_LEN, &output_len);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_cipher_update failed with status=%d", (int) status);
        free(outbuf);
        return ESP_FAIL;
    }
#else
    /* Initialise crypto context */
    mbedtls_aes_init(&session->ctx_aes);
    session->ctx_aes_init = true;
    memset(session->stb, 0, sizeof(session->stb));
    session->nc_off = 0;

    ret = mbedtls_aes_setkey_enc(&session->ctx_aes, session->sym_key,
                                 sizeof(session->sym_key) * 8);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed at mbedtls_aes_setkey_enc with error code : %d", ret);
        free(outbuf);
        return ESP_FAIL;
    }

    ret = mbedtls_aes_crypt_ctr(&session->ctx_aes, PUBLIC_KEY_LEN,
                                &session->nc_off, session->rand,
                                session->stb, session->device_pubkey, outbuf);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed at mbedtls_aes_crypt_ctr with error code : %d", ret);
        free(outbuf);
        return ESP_FAIL;
    }
#endif

    Sec1Payload *out = (Sec1Payload *) malloc(sizeof(Sec1Payload));
    if (!out) {
        ESP_LOGE(TAG, "Error allocating out buffer");
        free(outbuf);
        return ESP_ERR_NO_MEM;
    }
    sec1_payload__init(out);

    SessionCmd1 *out_req = (SessionCmd1 *) malloc(sizeof(SessionCmd1));
    if (!out_req) {
        ESP_LOGE(TAG, "Error allocating out_req buffer");
        free(outbuf);
        free(out);
        return ESP_ERR_NO_MEM;
    }
    session_cmd1__init(out_req);

    out_req->client_verify_data.data = outbuf;
    out_req->client_verify_data.len = PUBLIC_KEY_LEN;
    hexdump("Client verify data", outbuf, PUBLIC_KEY_LEN);

    out->msg = SEC1_MSG_TYPE__Session_Command1;
    out->payload_case = SEC1_PAYLOAD__PAYLOAD_SC1;
    out->sc1 = out_req;

    req->proto_case = SESSION_DATA__PROTO_SEC1;
    req->sec_ver = protocomm_ext_security1.ver;
    req->sec1 = out;

    return ESP_OK;
}

static void cleanup_command0(SessionData *req)
{
    if (req->sec1) {
        if (req->sec1->sc0) {
            free(req->sec1->sc0);
        }
        free(req->sec1);
    }
}

static void cleanup_command1(SessionData *req)
{
    if (req->sec1) {
        if (req->sec1->sc1) {
            if (req->sec1->sc1->client_verify_data.data) {
                free(req->sec1->sc1->client_verify_data.data);
            }
            free(req->sec1->sc1);
        }
        free(req->sec1);
    }
}

static esp_err_t verify_response1(session_t *session, SessionData *resp)
{
    if (!resp || !resp->sec1) {
        ESP_LOGE(TAG, "Invalid response type");
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t *cli_pubkey = session->client_pubkey;
    uint8_t *dev_pubkey = session->device_pubkey;

    hexdump("Device pubkey", dev_pubkey, PUBLIC_KEY_LEN);
    hexdump("Client pubkey", cli_pubkey, PUBLIC_KEY_LEN);

    if ((resp->proto_case != SESSION_DATA__PROTO_SEC1) ||
            (resp->sec1->msg != SEC1_MSG_TYPE__Session_Response1) ||
            (resp->sec1->payload_case != SEC1_PAYLOAD__PAYLOAD_SR1) ||
            !resp->sec1->sr1) {
        ESP_LOGE(TAG, "Invalid response type");
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t check_buf[PUBLIC_KEY_LEN] = {0};
    Sec1Payload *in = (Sec1Payload *) resp->sec1;

    if (in->sr1->status != STATUS__Success) {
        ESP_LOGE(TAG, "Session_Response1 status is not Success (%d)", (int)in->sr1->status);
        return ESP_FAIL;
    }

    if (!in->sr1->device_verify_data.data || in->sr1->device_verify_data.len != PUBLIC_KEY_LEN) {
        ESP_LOGE(TAG, "Device verify data length is not as expected");
        return ESP_FAIL;
    }

#ifdef PROTOCOMM_EXT_USE_PSA_CRYPTO
    size_t output_len = 0;
    psa_status_t status = psa_cipher_update(&session->ctx_aes,
                                            in->sr1->device_verify_data.data, PUBLIC_KEY_LEN,
                                            check_buf, sizeof(check_buf), &output_len);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_cipher_update failed with status=%d", (int) status);
        return ESP_FAIL;
    }
#else
    int ret = mbedtls_aes_crypt_ctr(&session->ctx_aes, PUBLIC_KEY_LEN,
                                    &session->nc_off, session->rand, session->stb,
                                    in->sr1->device_verify_data.data, check_buf);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed at mbedtls_aes_crypt_ctr with error code : %d", ret);
        return ESP_FAIL;
    }
#endif
    hexdump("Dec Device verifier", check_buf, sizeof(check_buf));

    if (memcmp(check_buf, session->client_pubkey, sizeof(session->client_pubkey)) != 0) {
        ESP_LOGE(TAG, "Key mismatch. Close connection");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t verify_response0(session_t *session, SessionData *resp)
{
    if (!resp || !resp->sec1) {
        ESP_LOGE(TAG, "Invalid response type");
        return ESP_ERR_INVALID_ARG;
    }

    if ((resp->proto_case != SESSION_DATA__PROTO_SEC1) ||
            (resp->sec1->msg != SEC1_MSG_TYPE__Session_Response0) ||
            (resp->sec1->payload_case != SEC1_PAYLOAD__PAYLOAD_SR0) ||
            !resp->sec1->sr0) {
        ESP_LOGE(TAG, "Invalid response type");
        return ESP_ERR_INVALID_ARG;
    }

#ifndef PROTOCOMM_EXT_USE_PSA_CRYPTO
    int ret;
#endif
    Sec1Payload *in = (Sec1Payload *) resp->sec1;

    if (in->sr0->status != STATUS__Success) {
        ESP_LOGE(TAG, "Session_Response0 status is not Success (%d)", (int)in->sr0->status);
        return ESP_FAIL;
    }

    if (in->sr0->device_pubkey.len != PUBLIC_KEY_LEN) {
        ESP_LOGE(TAG, "Device public key length as not as expected");
        return ESP_FAIL;
    }

    if (in->sr0->device_random.len != SZ_RANDOM) {
        ESP_LOGE(TAG, "Device random data length is not as expected");
        return ESP_FAIL;
    }

    memcpy(session->device_pubkey, in->sr0->device_pubkey.data, PUBLIC_KEY_LEN);

    uint8_t *cli_pubkey = session->client_pubkey;
    uint8_t *dev_pubkey = session->device_pubkey;

    hexdump("Device pubkey", dev_pubkey, PUBLIC_KEY_LEN);
    hexdump("Client pubkey", cli_pubkey, PUBLIC_KEY_LEN);

#ifdef PROTOCOMM_EXT_USE_PSA_CRYPTO
    {
        size_t olen = 0;
        psa_status_t status = psa_raw_key_agreement(PSA_ALG_ECDH, session->ecdh_key_id,
                                                    session->device_pubkey, PUBLIC_KEY_LEN,
                                                    session->sym_key, sizeof(session->sym_key), &olen);
        if (status != PSA_SUCCESS || olen != sizeof(session->sym_key)) {
            ESP_LOGE(TAG, "psa_raw_key_agreement failed with status=%d olen=%u",
                     (int) status, (unsigned) olen);
            return ESP_FAIL;
        }
        psa_destroy_key(session->ecdh_key_id);
        session->ecdh_key_id = 0;
    }

    if (session->sec_params != NULL && session->sec_params_len != 0) {
        uint8_t sha_out[PUBLIC_KEY_LEN] = {0};
        size_t olen = 0;
        psa_hash_operation_t hash_operation = PSA_HASH_OPERATION_INIT;
        psa_status_t status = psa_hash_setup(&hash_operation, PSA_ALG_SHA_256);
        if (status != PSA_SUCCESS) {
            ESP_LOGE(TAG, "psa_hash_setup failed with status=%d", (int) status);
            return ESP_FAIL;
        }
        status = psa_hash_update(&hash_operation, session->sec_params, session->sec_params_len);
        if (status != PSA_SUCCESS) {
            ESP_LOGE(TAG, "psa_hash_update failed with status=%d", (int) status);
            psa_hash_abort(&hash_operation);
            return ESP_FAIL;
        }
        status = psa_hash_finish(&hash_operation, sha_out, sizeof(sha_out), &olen);
        if (status != PSA_SUCCESS) {
            ESP_LOGE(TAG, "psa_hash_finish failed with status=%d", (int) status);
            psa_hash_abort(&hash_operation);
            return ESP_FAIL;
        }
        for (int i = 0; i < PUBLIC_KEY_LEN; i++) {
            session->sym_key[i] ^= sha_out[i];
        }
    }
#else
    ret = mbedtls_mpi_lset(ACCESS_ECDH(&session->ctx_server, Qp).MBEDTLS_PRIVATE(Z), 1);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed at mbedtls_mpi_lset with error code : %d", ret);
        return ESP_FAIL;
    }

    flip_endian(session->device_pubkey, PUBLIC_KEY_LEN);
    ret = mbedtls_mpi_read_binary(ACCESS_ECDH(&session->ctx_server, Qp).MBEDTLS_PRIVATE(X), dev_pubkey, PUBLIC_KEY_LEN);
    flip_endian(session->device_pubkey, PUBLIC_KEY_LEN);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed at mbedtls_mpi_read_binary with error code : %d", ret);
        return ESP_FAIL;
    }

    ret = mbedtls_ecdh_compute_shared(ACCESS_ECDH(&session->ctx_server, grp),
                                      ACCESS_ECDH(&session->ctx_server, z),
                                      ACCESS_ECDH(&session->ctx_server, Qp),
                                      ACCESS_ECDH(&session->ctx_server, d),
                                      mbedtls_ctr_drbg_random,
                                      session->ctr_drbg);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed at mbedtls_ecdh_compute_shared with error code : %d", ret);
        return ESP_FAIL;
    }

    ret = mbedtls_mpi_write_binary(ACCESS_ECDH(&session->ctx_server, z), session->sym_key, PUBLIC_KEY_LEN);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed at mbedtls_mpi_write_binary with error code : %d", ret);
        return ESP_FAIL;
    }
    flip_endian(session->sym_key, PUBLIC_KEY_LEN);

    if (session->sec_params != NULL && session->sec_params_len != 0) {
        uint8_t sha_out[PUBLIC_KEY_LEN] = {0};

        ret = mbedtls_sha256((const uint8_t *) session->sec_params, session->sec_params_len, sha_out, 0);
        if (ret != 0) {
            ESP_LOGE(TAG, "Failed at mbedtls_sha256_ret with error code : %d", ret);
            return ESP_FAIL;
        }

        for (int i = 0; i < PUBLIC_KEY_LEN; i++) {
            session->sym_key[i] ^= sha_out[i];
        }
    }
#endif

    hexdump("Shared key", session->sym_key, PUBLIC_KEY_LEN);

    memcpy(session->rand, in->sr0->device_random.data, SZ_RANDOM);
    hexdump("Dev random", session->rand, sizeof(session->rand));
    return ESP_OK;
}

static esp_err_t sec1_close_session(protocomm_ext_security_handle_t handle)
{
    session_t *cur_session = (session_t *) handle;
    if (!cur_session) {
        return ESP_ERR_INVALID_ARG;
    }

    if (cur_session->ctx_aes_init) {
#ifdef PROTOCOMM_EXT_USE_PSA_CRYPTO
        psa_cipher_abort(&cur_session->ctx_aes);
        if (cur_session->aes_key_id != 0) {
            psa_destroy_key(cur_session->aes_key_id);
            cur_session->aes_key_id = 0;
        }
#else
        mbedtls_aes_free(&cur_session->ctx_aes);
#endif
        cur_session->ctx_aes_init = false;
    }

    return ESP_OK;
}

static esp_err_t sec1_cleanup(protocomm_ext_security_handle_t handle)
{
    session_t *cur_session = (session_t *) handle;
    if (!cur_session) {
        return ESP_OK;
    }

    sec1_close_session(cur_session);

#ifdef PROTOCOMM_EXT_USE_PSA_CRYPTO
    if (cur_session->ecdh_key_id != 0) {
        psa_destroy_key(cur_session->ecdh_key_id);
        cur_session->ecdh_key_id = 0;
    }
#else
    if (cur_session->ctx_server) {
        mbedtls_ecdh_free(cur_session->ctx_server);
        free(cur_session->ctx_server);
        cur_session->ctx_server = NULL;
    }
    if (cur_session->ctr_drbg) {
        mbedtls_ctr_drbg_free(cur_session->ctr_drbg);
        free(cur_session->ctr_drbg);
        cur_session->ctr_drbg = NULL;
    }
    if (cur_session->entropy) {
        mbedtls_entropy_free(cur_session->entropy);
        free(cur_session->entropy);
        cur_session->entropy = NULL;
    }
#endif

    free(cur_session->sec_params);
    cur_session->sec_params = NULL;
    free(cur_session);
    return ESP_OK;
}

static esp_err_t sec1_init(protocomm_ext_security_handle_t *handle, const void *sec_params)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    session_t *session = (session_t *) calloc(1, sizeof(session_t));
    if (!session) {
        ESP_LOGE(TAG, "Error allocating new session");
        *handle = NULL;
        return ESP_ERR_NO_MEM;
    }

    const protocomm_ext_security1_params_t *sec_params_struct = (const protocomm_ext_security1_params_t *) sec_params;
    if (sec_params_struct && sec_params_struct->data && sec_params_struct->len != 0) {
        session->sec_params = (uint8_t *) calloc(1, sec_params_struct->len);
        if (!session->sec_params) {
            ESP_LOGE(TAG, "Error allocating memory for sec_params");
            free(session);
            *handle = NULL;
            return ESP_ERR_NO_MEM;
        }
        memcpy(session->sec_params, sec_params_struct->data, sec_params_struct->len);
        session->sec_params_len = sec_params_struct->len;
    }

#ifdef PROTOCOMM_EXT_USE_PSA_CRYPTO
    if (psa_crypto_init() != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_crypto_init failed");
        free(session->sec_params);
        free(session);
        *handle = NULL;
        return ESP_FAIL;
    }

    {
        psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;
        size_t olen = 0;
        psa_set_key_type(&key_attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_MONTGOMERY));
        psa_set_key_bits(&key_attributes, 255);
        psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_VOLATILE);
        psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_DERIVE | PSA_KEY_USAGE_EXPORT);
        psa_set_key_algorithm(&key_attributes, PSA_ALG_ECDH);
        psa_status_t status = psa_generate_key(&key_attributes, &session->ecdh_key_id);
        psa_reset_key_attributes(&key_attributes);
        if (status != PSA_SUCCESS) {
            ESP_LOGE(TAG, "psa_generate_key failed with status=%d", (int) status);
            goto exit;
        }
        status = psa_export_public_key(session->ecdh_key_id, session->client_pubkey,
                                       PUBLIC_KEY_LEN, &olen);
        if (status != PSA_SUCCESS || olen != PUBLIC_KEY_LEN) {
            ESP_LOGE(TAG, "psa_export_public_key failed with status=%d", (int) status);
            goto exit;
        }
    }
#else
    session->ctx_server = malloc(sizeof(mbedtls_ecdh_context));
    session->entropy    = malloc(sizeof(mbedtls_entropy_context));
    session->ctr_drbg   = malloc(sizeof(mbedtls_ctr_drbg_context));
    if (!session->ctx_server || !session->entropy || !session->ctr_drbg) {
        ESP_LOGE(TAG, "Failed to allocate memory for mbedtls context");
        free(session->ctx_server);
        free(session->entropy);
        free(session->ctr_drbg);
        free(session->sec_params);
        free(session);
        *handle = NULL;
        return ESP_ERR_NO_MEM;
    }

    mbedtls_ecdh_init(session->ctx_server);
    mbedtls_ecdh_setup(session->ctx_server, MBEDTLS_ECP_DP_CURVE25519);
    mbedtls_ctr_drbg_init(session->ctr_drbg);
    mbedtls_entropy_init(session->entropy);

    esp_err_t ret = mbedtls_ctr_drbg_seed(session->ctr_drbg, mbedtls_entropy_func, session->entropy, NULL, 0);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed at mbedtls_ctr_drbg_seed with error code : %d", ret);
        goto exit;
    }

    ret = mbedtls_ecp_group_load(ACCESS_ECDH(&session->ctx_server, grp), MBEDTLS_ECP_DP_CURVE25519);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed at mbedtls_ecp_group_load with error code : %d", ret);
        goto exit;
    }

    ret = mbedtls_ecdh_gen_public(ACCESS_ECDH(&session->ctx_server, grp),
                                  ACCESS_ECDH(&session->ctx_server, d),
                                  ACCESS_ECDH(&session->ctx_server, Q),
                                  mbedtls_ctr_drbg_random,
                                  session->ctr_drbg);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed at mbedtls_ecdh_gen_public with error code : %d", ret);
        goto exit;
    }

    ret = mbedtls_mpi_write_binary(ACCESS_ECDH(&session->ctx_server, Q).MBEDTLS_PRIVATE(X),
                                   session->client_pubkey,
                                   PUBLIC_KEY_LEN);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed at mbedtls_mpi_write_binary with error code : %d", ret);
        goto exit;
    }
    flip_endian(session->client_pubkey, PUBLIC_KEY_LEN);
#endif

    *handle = (protocomm_ext_security_handle_t) session;

    return ESP_OK;
exit:
    /* Crypto contexts were initialized; sec1_cleanup frees them and the session. */
    sec1_cleanup(session);
    *handle = NULL;
    /* mbedtls returns negative codes; the init() contract is esp_err_t. */
    return ESP_FAIL;
}

static esp_err_t sec1_decrypt(protocomm_ext_security_handle_t handle, const uint8_t *inbuf, ssize_t inlen, uint8_t **outbuf, ssize_t *outlen)
{
    session_t *cur_session = (session_t *) handle;
    if (!cur_session || !inbuf || !outbuf || !outlen || inlen < 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (cur_session->state != SESSION_STATE_DONE) {
        ESP_LOGE(TAG, "Secure session not established");
        return ESP_ERR_INVALID_STATE;
    }

    if (inlen == 0) {
        *outbuf = NULL;
        *outlen = 0;
        return ESP_OK;
    }

    *outlen = inlen;
    *outbuf = (uint8_t *) malloc(*outlen);
    if (!*outbuf) {
        ESP_LOGE(TAG, "Failed to allocate encrypt/decrypt buf len %d", *outlen);
        return ESP_ERR_NO_MEM;
    }

#ifdef PROTOCOMM_EXT_USE_PSA_CRYPTO
    size_t out_len = 0;
    psa_status_t status = psa_cipher_update(&cur_session->ctx_aes, inbuf, inlen,
                                            *outbuf, *outlen, &out_len);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_cipher_update failed with status=%d", (int) status);
        free(*outbuf);
        *outbuf = NULL;
        *outlen = 0;
        return ESP_FAIL;
    }
#else
    int ret = mbedtls_aes_crypt_ctr(&cur_session->ctx_aes, inlen, &cur_session->nc_off, cur_session->rand, cur_session->stb, inbuf, *outbuf);

    if (ret != 0) {
        ESP_LOGE(TAG, "Failed at mbedtls_aes_crypt_ctr with error code : %d", ret);
        free(*outbuf);
        *outbuf = NULL;
        *outlen = 0;
        return ESP_FAIL;
    }
#endif
    return ESP_OK;
}

static esp_err_t sec1_send_command1(protocomm_ext_security_handle_t handle, uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
    ESP_LOGD(TAG, "Start to write setup1_command");

    session_t *cur_session = (session_t *) handle;
    if (!cur_session) {
        ESP_LOGE(TAG, "Invalid session context data");
        return ESP_ERR_INVALID_ARG;
    }

    SessionData req;

    /*********** Transaction1 - SessionCmd1 ****************/
    session_data__init(&req);
    if (prepare_command1(cur_session, &req) != ESP_OK) {
        ESP_LOGE(TAG, "Failed in prepare_command1");
        goto exit_cmd1;
    }

    *outlen = session_data__get_packed_size(&req);
    *outbuf = (uint8_t *) malloc(*outlen);
    if (!*outbuf) {
        ESP_LOGE(TAG, "Failed to allocate outbuf");
        cleanup_command1(&req);
        goto exit_cmd1;
    }

    session_data__pack(&req, *outbuf);
    cleanup_command1(&req);

    ESP_LOGD(TAG, "Write setup1_command done");
    cur_session->state = SESSION_STATE_RESP1;
    return ESP_OK;

exit_cmd1:
    ESP_LOGE(TAG, "Write setup1_command failed");

    return ESP_FAIL;
}

static esp_err_t sec1_send_command0(protocomm_ext_security_handle_t handle, uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
    ESP_LOGD(TAG, "Start to write setup0_command");

    session_t *cur_session = (session_t *) handle;
    if (!cur_session) {
        ESP_LOGE(TAG, "Invalid session context data");
        return ESP_ERR_INVALID_ARG;
    }

    SessionData req;

    /*********** Transaction0 - SessionCmd0 ****************/
    session_data__init(&req);
    if (prepare_command0(cur_session, &req) != ESP_OK) {
        ESP_LOGE(TAG, "Failed in prepare_command0");
        goto exit_cmd0;
    }

    *outlen = session_data__get_packed_size(&req);
    *outbuf = (uint8_t *) malloc(*outlen);
    if (!*outbuf) {
        ESP_LOGE(TAG, "Failed to allocate outbuf");
        cleanup_command0(&req);
        goto exit_cmd0;
    }

    session_data__pack(&req, *outbuf);
    cleanup_command0(&req);

    ESP_LOGD(TAG, "Write setup0_command done");
    cur_session->state = SESSION_STATE_RESP0;
    return ESP_OK;

exit_cmd0:
    ESP_LOGE(TAG, "Write setup0_command failed");

    return ESP_FAIL;
}

static esp_err_t handle_session_response0(session_t *cur_session, SessionData *resp)
{
    ESP_LOGD(TAG, "Request to handle setup0_response");
    esp_err_t ret;

    if (resp->sec_ver != protocomm_ext_security1.ver) {
        ESP_LOGE(TAG, "Security version mismatch. Closing connection");
        return ESP_ERR_INVALID_ARG;
    }

    if (!resp->sec1 || resp->sec1->msg != SEC1_MSG_TYPE__Session_Response0) {
        ESP_LOGE(TAG, "Invalid response message type");
        return ESP_ERR_INVALID_ARG;
    }

    if (cur_session->state != SESSION_STATE_RESP0) {
        ESP_LOGW(TAG, "Invalid state of session %d (expected %d).",
                 cur_session->state, SESSION_STATE_RESP0);
        return ESP_ERR_INVALID_STATE;
    }

    /*********** Transaction0 - SessionResp0 ****************/
    if (verify_response0(cur_session, resp) != ESP_OK) {
        ESP_LOGE(TAG, "Invalid response 0");
        ret = ESP_FAIL;
        goto exit_resp0;
    }

    ESP_LOGD(TAG, "Session setup phase1 done");
    ret = ESP_OK;

exit_resp0:
#ifndef PROTOCOMM_EXT_USE_PSA_CRYPTO
    mbedtls_ecdh_free(cur_session->ctx_server);
    free(cur_session->ctx_server);
    cur_session->ctx_server = NULL;

    mbedtls_ctr_drbg_free(cur_session->ctr_drbg);
    free(cur_session->ctr_drbg);
    cur_session->ctr_drbg = NULL;

    mbedtls_entropy_free(cur_session->entropy);
    free(cur_session->entropy);
    cur_session->entropy = NULL;
#endif

    return ret;
}

static esp_err_t handle_session_response1(session_t *cur_session, SessionData *resp)
{
    ESP_LOGD(TAG, "Request to handle setup1_response");
    esp_err_t ret;

    if (resp->sec_ver != protocomm_ext_security1.ver) {
        ESP_LOGE(TAG, "Security version mismatch. Closing connection");
        return ESP_ERR_INVALID_ARG;
    }

    if (!resp->sec1 || resp->sec1->msg != SEC1_MSG_TYPE__Session_Response1) {
        ESP_LOGE(TAG, "Invalid response message type");
        return ESP_ERR_INVALID_ARG;
    }

    if (cur_session->state != SESSION_STATE_RESP1) {
        ESP_LOGW(TAG, "Invalid state of session %d (expected %d).",
                 cur_session->state, SESSION_STATE_RESP1);
        return ESP_ERR_INVALID_STATE;
    }

    /*********** Transaction1 - SessionResp1 ****************/
    if (verify_response1(cur_session, resp) != ESP_OK) {
        ESP_LOGE(TAG, "Invalid response 1");
        ret = ESP_FAIL;
        goto exit_resp1;
    }

    cur_session->state = SESSION_STATE_DONE;
    ESP_LOGD(TAG, "Secure session established successfully");
    ret = ESP_OK;

exit_resp1:

    return ret;
}

static esp_err_t sec1_parse_command0(protocomm_ext_security_handle_t handle, const uint8_t *inbuf, ssize_t inlen, void *priv_data)
{
    session_t *cur_session = (session_t *) handle;
    if (!cur_session) {
        ESP_LOGE(TAG, "Invalid session context data");
        return ESP_ERR_INVALID_ARG;
    }

    SessionData *resp = session_data__unpack(NULL, inlen, inbuf);
    if (!resp) {
        ESP_LOGE(TAG, "Unable to unpack command0 response");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = handle_session_response0(cur_session, resp);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Session setup error %d", ret);
        session_data__free_unpacked(resp, NULL);
        return ESP_FAIL;
    }

    session_data__free_unpacked(resp, NULL);
    return ESP_OK;
}

static esp_err_t sec1_parse_command1(protocomm_ext_security_handle_t handle, const uint8_t *inbuf, ssize_t inlen, void *priv_data)
{
    session_t *cur_session = (session_t *) handle;
    if (!cur_session) {
        ESP_LOGE(TAG, "Invalid session context data");
        return ESP_ERR_INVALID_ARG;
    }

    SessionData *resp = session_data__unpack(NULL, inlen, inbuf);
    if (!resp) {
        ESP_LOGE(TAG, "Unable to unpack command1 response");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = handle_session_response1(cur_session, resp);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Session setup error %d", ret);
        session_data__free_unpacked(resp, NULL);
        return ESP_FAIL;
    }

    session_data__free_unpacked(resp, NULL);
    return ESP_OK;
}

const protocomm_ext_security_t protocomm_ext_security1 = {
    .ver = SEC_SCHEME_VERSION__SecScheme1,
    .init = sec1_init,
    .cleanup = sec1_cleanup,
    .security_send_command0 = sec1_send_command0,
    .security_parse_command0 = sec1_parse_command0,
    .security_send_command1 = sec1_send_command1,
    .security_parse_command1 = sec1_parse_command1,
    .encrypt = sec1_decrypt, /* Encrypt == decrypt for AES-CTR */
    .decrypt = sec1_decrypt,
};
