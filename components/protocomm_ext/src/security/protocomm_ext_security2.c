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

#include "protocomm_ext_crypto.h"

#ifdef PROTOCOMM_EXT_USE_PSA_CRYPTO
#include "psa/crypto.h"
#else
#include <mbedtls/gcm.h>
#include <mbedtls/error.h>
#endif

#include "protocomm_ext_security.h"
#include "protocomm_ext_security2.h"
#include "protocomm_ext_srp6a_client.h"

#include "session.pb-c.h"
#include "sec2.pb-c.h"
#include "constants.pb-c.h"

static const char *TAG = "security2";

#define PUBLIC_KEY_LEN      PROTOCOMM_EXT_SRP6A_PUBKEY_LEN
#define CLIENT_PROOF_LEN    PROTOCOMM_EXT_SRP6A_PROOF_LEN
#define AES_GCM_KEY_LEN     32
#define AES_GCM_KEY_BITS    256
#define AES_GCM_IV_SIZE     12
#define AES_GCM_TAG_LEN     16

#define SESSION_STATE_RESP0  0
#define SESSION_STATE_RESP1  1
#define SESSION_STATE_DONE   2

typedef struct session {
    uint8_t state;
    uint8_t patch_ver;

    char *username;
    uint16_t username_len;
    char *password;
    uint16_t password_len;

    uint8_t client_pubkey[PUBLIC_KEY_LEN];
    uint8_t client_proof[CLIENT_PROOF_LEN];

    uint8_t session_key[AES_GCM_KEY_LEN];
    uint8_t iv[AES_GCM_IV_SIZE];

#ifdef PROTOCOMM_EXT_USE_PSA_CRYPTO
    psa_key_id_t key_id;
#else
    mbedtls_gcm_context ctx_gcm;
#endif
    bool ctx_gcm_init;

    protocomm_ext_srp6a_client_t *srp;
} session_t;

static void hexdump(const char *msg, const uint8_t *buf, int len)
{
    ESP_LOGD(TAG, "%s:", msg);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, buf, len, ESP_LOG_DEBUG);
}

static void sec2_gcm_iv_counter_increment(uint8_t *iv)
{
    uint32_t counter = ((uint32_t) iv[8] << 24) |
                       ((uint32_t) iv[9] << 16) |
                       ((uint32_t) iv[10] << 8) |
                       ((uint32_t) iv[11]);
    counter++;
    iv[8]  = (uint8_t)((counter >> 24) & 0xff);
    iv[9]  = (uint8_t)((counter >> 16) & 0xff);
    iv[10] = (uint8_t)((counter >> 8) & 0xff);
    iv[11] = (uint8_t)(counter & 0xff);
}

static esp_err_t prepare_command0(session_t *session, SessionData *req)
{
    Sec2Payload *in = (Sec2Payload *) malloc(sizeof(Sec2Payload));
    if (!in) {
        ESP_LOGE(TAG, "Error allocating memory for request");
        return ESP_ERR_NO_MEM;
    }

    S2SessionCmd0 *in_req = (S2SessionCmd0 *) malloc(sizeof(S2SessionCmd0));
    if (!in_req) {
        ESP_LOGE(TAG, "Error allocating memory for request");
        free(in);
        return ESP_ERR_NO_MEM;
    }

    sec2_payload__init(in);
    s2_session_cmd0__init(in_req);

    in_req->client_username.data = (uint8_t *) session->username;
    in_req->client_username.len = session->username_len;
    in_req->client_pubkey.data = session->client_pubkey;
    in_req->client_pubkey.len = PUBLIC_KEY_LEN;

    in->msg = SEC2_MSG_TYPE__S2Session_Command0;
    in->payload_case = SEC2_PAYLOAD__PAYLOAD_SC0;
    in->sc0 = in_req;

    req->proto_case = SESSION_DATA__PROTO_SEC2;
    req->sec_ver = protocomm_ext_security2.ver;
    req->sec2 = in;

    return ESP_OK;
}

static esp_err_t prepare_command1(session_t *session, SessionData *req)
{
    Sec2Payload *out = (Sec2Payload *) malloc(sizeof(Sec2Payload));
    if (!out) {
        ESP_LOGE(TAG, "Error allocating out buffer");
        return ESP_ERR_NO_MEM;
    }
    sec2_payload__init(out);

    S2SessionCmd1 *out_req = (S2SessionCmd1 *) malloc(sizeof(S2SessionCmd1));
    if (!out_req) {
        ESP_LOGE(TAG, "Error allocating out_req buffer");
        free(out);
        return ESP_ERR_NO_MEM;
    }
    s2_session_cmd1__init(out_req);

    out_req->client_proof.data = session->client_proof;
    out_req->client_proof.len = CLIENT_PROOF_LEN;
    hexdump("Client proof", session->client_proof, CLIENT_PROOF_LEN);

    out->msg = SEC2_MSG_TYPE__S2Session_Command1;
    out->payload_case = SEC2_PAYLOAD__PAYLOAD_SC1;
    out->sc1 = out_req;

    req->proto_case = SESSION_DATA__PROTO_SEC2;
    req->sec_ver = protocomm_ext_security2.ver;
    req->sec2 = out;

    return ESP_OK;
}

static void cleanup_command0(SessionData *req)
{
    if (req->sec2) {
        if (req->sec2->sc0) {
            free(req->sec2->sc0);
        }
        free(req->sec2);
        req->sec2 = NULL;
    }
}

static void cleanup_command1(SessionData *req)
{
    if (req->sec2) {
        if (req->sec2->sc1) {
            free(req->sec2->sc1);
        }
        free(req->sec2);
        req->sec2 = NULL;
    }
}

static esp_err_t verify_response0(session_t *session, SessionData *resp)
{
    if ((resp->proto_case != SESSION_DATA__PROTO_SEC2) ||
            (resp->sec2->msg != SEC2_MSG_TYPE__S2Session_Response0)) {
        ESP_LOGE(TAG, "Invalid response type");
        return ESP_ERR_INVALID_ARG;
    }

    Sec2Payload *in = (Sec2Payload *) resp->sec2;
    if (!in->sr0 || in->payload_case != SEC2_PAYLOAD__PAYLOAD_SR0) {
        ESP_LOGE(TAG, "Missing sr0 payload");
        return ESP_ERR_INVALID_ARG;
    }

    if (in->sr0->status != STATUS__Success) {
        ESP_LOGE(TAG, "Device returned non-success status in response0");
        return ESP_FAIL;
    }

    if (in->sr0->device_pubkey.len == 0 || in->sr0->device_pubkey.len > PUBLIC_KEY_LEN) {
        ESP_LOGE(TAG, "Invalid device public key length");
        return ESP_FAIL;
    }

    if (in->sr0->device_salt.len == 0) {
        ESP_LOGE(TAG, "Invalid device salt length");
        return ESP_FAIL;
    }

    hexdump("Device pubkey", in->sr0->device_pubkey.data, in->sr0->device_pubkey.len);
    hexdump("Device salt", in->sr0->device_salt.data, in->sr0->device_salt.len);

    esp_err_t err = protocomm_ext_srp6a_client_process_challenge(
                        session->srp,
                        in->sr0->device_salt.data, in->sr0->device_salt.len,
                        in->sr0->device_pubkey.data, in->sr0->device_pubkey.len,
                        session->client_proof, CLIENT_PROOF_LEN);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SRP process_challenge failed");
        return err;
    }

    return ESP_OK;
}

static esp_err_t verify_response1(session_t *session, SessionData *resp)
{
    if ((resp->proto_case != SESSION_DATA__PROTO_SEC2) ||
            (resp->sec2->msg != SEC2_MSG_TYPE__S2Session_Response1)) {
        ESP_LOGE(TAG, "Invalid response type");
        return ESP_ERR_INVALID_ARG;
    }

    Sec2Payload *in = (Sec2Payload *) resp->sec2;
    if (!in->sr1 || in->payload_case != SEC2_PAYLOAD__PAYLOAD_SR1) {
        ESP_LOGE(TAG, "Missing sr1 payload");
        return ESP_ERR_INVALID_ARG;
    }

    if (in->sr1->status != STATUS__Success) {
        ESP_LOGE(TAG, "Device returned non-success status in response1");
        return ESP_FAIL;
    }

    if (in->sr1->device_proof.len != CLIENT_PROOF_LEN) {
        ESP_LOGE(TAG, "Invalid device proof length");
        return ESP_FAIL;
    }

    if (in->sr1->device_nonce.len != AES_GCM_IV_SIZE) {
        ESP_LOGE(TAG, "Invalid device nonce length");
        return ESP_FAIL;
    }

    hexdump("Device proof", in->sr1->device_proof.data, in->sr1->device_proof.len);

    esp_err_t err = protocomm_ext_srp6a_client_verify_session(
                        session->srp, in->sr1->device_proof.data, in->sr1->device_proof.len);
    if (err != ESP_OK || !protocomm_ext_srp6a_client_authenticated(session->srp)) {
        ESP_LOGE(TAG, "Failed to verify device proof");
        return ESP_FAIL;
    }

    size_t key_len = 0;
    const uint8_t *shared = protocomm_ext_srp6a_client_get_session_key(session->srp, &key_len);
    if (!shared || key_len < AES_GCM_KEY_LEN) {
        ESP_LOGE(TAG, "Invalid session key");
        return ESP_FAIL;
    }

    memcpy(session->session_key, shared, AES_GCM_KEY_LEN);
    hexdump("Session key", session->session_key, AES_GCM_KEY_LEN);

    memcpy(session->iv, in->sr1->device_nonce.data, AES_GCM_IV_SIZE);
    hexdump("Nonce", session->iv, AES_GCM_IV_SIZE);

#ifdef PROTOCOMM_EXT_USE_PSA_CRYPTO
    if (psa_crypto_init() != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_crypto_init failed");
        return ESP_FAIL;
    }
    psa_algorithm_t alg = PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_GCM, AES_GCM_TAG_LEN);
    psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&key_attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&key_attributes, AES_GCM_KEY_BITS);
    psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&key_attributes, alg);
    psa_status_t status = psa_import_key(&key_attributes, session->session_key, AES_GCM_KEY_LEN,
                                         &session->key_id);
    psa_reset_key_attributes(&key_attributes);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_import_key failed with status=%d", (int) status);
        return ESP_FAIL;
    }
    session->ctx_gcm_init = true;
#else
    mbedtls_gcm_init(&session->ctx_gcm);
    session->ctx_gcm_init = true;

    int mbed_err = mbedtls_gcm_setkey(&session->ctx_gcm, MBEDTLS_CIPHER_ID_AES,
                                      session->session_key, AES_GCM_KEY_BITS);
    if (mbed_err != 0) {
        ESP_LOGE(TAG, "Failure at mbedtls_gcm_setkey with error code : -0x%x", -mbed_err);
        mbedtls_gcm_free(&session->ctx_gcm);
        session->ctx_gcm_init = false;
        return ESP_FAIL;
    }
#endif

    return ESP_OK;
}

static esp_err_t sec2_close_session(protocomm_ext_security_handle_t handle)
{
    session_t *cur_session = (session_t *) handle;
    if (!cur_session) {
        return ESP_ERR_INVALID_ARG;
    }

    if (cur_session->ctx_gcm_init) {
#ifdef PROTOCOMM_EXT_USE_PSA_CRYPTO
        if (cur_session->key_id != 0) {
            psa_destroy_key(cur_session->key_id);
            cur_session->key_id = 0;
        }
#else
        mbedtls_gcm_free(&cur_session->ctx_gcm);
#endif
        cur_session->ctx_gcm_init = false;
    }

    if (cur_session->srp) {
        protocomm_ext_srp6a_client_free(cur_session->srp);
        cur_session->srp = NULL;
    }

    return ESP_OK;
}

static esp_err_t sec2_cleanup(protocomm_ext_security_handle_t handle)
{
    session_t *cur_session = (session_t *) handle;
    if (cur_session) {
        sec2_close_session(cur_session);
        free(cur_session->username);
        free(cur_session->password);
        free(cur_session);
    }
    return ESP_OK;
}

static esp_err_t sec2_init(protocomm_ext_security_handle_t *handle, const void *sec_params)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    const protocomm_ext_security2_params_t *params =
        (const protocomm_ext_security2_params_t *) sec_params;
    if (!params || !params->username || params->username_len == 0 ||
            !params->password || params->password_len == 0) {
        ESP_LOGE(TAG, "Invalid security2 params");
        return ESP_ERR_INVALID_ARG;
    }

    session_t *session = (session_t *) calloc(1, sizeof(session_t));
    if (!session) {
        ESP_LOGE(TAG, "Error allocating new session");
        return ESP_ERR_NO_MEM;
    }

    session->patch_ver = 1;

    session->username = malloc(params->username_len);
    session->password = malloc(params->password_len);
    if (!session->username || !session->password) {
        ESP_LOGE(TAG, "Error allocating username/password");
        free(session->username);
        free(session->password);
        free(session);
        return ESP_ERR_NO_MEM;
    }
    memcpy(session->username, params->username, params->username_len);
    memcpy(session->password, params->password, params->password_len);
    session->username_len = params->username_len;
    session->password_len = params->password_len;

    session->srp = protocomm_ext_srp6a_client_new(session->username, session->username_len,
                                                  session->password, session->password_len);
    if (!session->srp) {
        ESP_LOGE(TAG, "Failed to initialize SRP6a client");
        sec2_cleanup(session);
        *handle = NULL;
        return ESP_FAIL;
    }

    size_t pubkey_len = PUBLIC_KEY_LEN;
    if (protocomm_ext_srp6a_client_get_pubkey(session->srp, session->client_pubkey, &pubkey_len) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get client public key");
        sec2_cleanup(session);
        *handle = NULL;
        return ESP_FAIL;
    }
    hexdump("Client pubkey", session->client_pubkey, PUBLIC_KEY_LEN);

    *handle = (protocomm_ext_security_handle_t) session;
    return ESP_OK;
}

static esp_err_t sec2_encrypt(protocomm_ext_security_handle_t handle, const uint8_t *inbuf, ssize_t inlen,
                              uint8_t **outbuf, ssize_t *outlen)
{
    session_t *cur_session = (session_t *) handle;
    if (!cur_session || !inbuf || !outbuf || !outlen || inlen < 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (cur_session->state != SESSION_STATE_DONE || !cur_session->ctx_gcm_init) {
        ESP_LOGE(TAG, "Secure session not established");
        return ESP_ERR_INVALID_STATE;
    }

    hexdump("Encrypt IV", cur_session->iv, AES_GCM_IV_SIZE);

    *outlen = inlen + AES_GCM_TAG_LEN;
    *outbuf = (uint8_t *) malloc(*outlen);
    if (!*outbuf) {
        ESP_LOGE(TAG, "Failed to allocate encrypt buf len %d", (int) *outlen);
        return ESP_ERR_NO_MEM;
    }

#ifdef PROTOCOMM_EXT_USE_PSA_CRYPTO
    psa_algorithm_t alg = PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_GCM, AES_GCM_TAG_LEN);
    size_t out_len = 0;
    psa_status_t status = psa_aead_encrypt(cur_session->key_id, alg,
                                           cur_session->iv, AES_GCM_IV_SIZE,
                                           NULL, 0, inbuf, inlen,
                                           *outbuf, *outlen, &out_len);
    if (status != PSA_SUCCESS || out_len != (size_t) *outlen) {
        ESP_LOGE(TAG, "psa_aead_encrypt failed with status=%d", (int) status);
        free(*outbuf);
        *outbuf = NULL;
        *outlen = 0;
        return ESP_FAIL;
    }
#else
    uint8_t gcm_tag[AES_GCM_TAG_LEN];
    int ret = mbedtls_gcm_crypt_and_tag(&cur_session->ctx_gcm, MBEDTLS_GCM_ENCRYPT, inlen,
                                        cur_session->iv, AES_GCM_IV_SIZE, NULL, 0, inbuf,
                                        *outbuf, AES_GCM_TAG_LEN, gcm_tag);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed at mbedtls_gcm_crypt_and_tag with error code : %d", ret);
        free(*outbuf);
        *outbuf = NULL;
        *outlen = 0;
        return ESP_FAIL;
    }
    memcpy(*outbuf + inlen, gcm_tag, AES_GCM_TAG_LEN);
#endif

    if (cur_session->patch_ver == 1) {
        sec2_gcm_iv_counter_increment(cur_session->iv);
    }

    return ESP_OK;
}

static esp_err_t sec2_decrypt(protocomm_ext_security_handle_t handle, const uint8_t *inbuf, ssize_t inlen,
                              uint8_t **outbuf, ssize_t *outlen)
{
    session_t *cur_session = (session_t *) handle;
    if (!cur_session || !inbuf || !outbuf || !outlen || inlen < AES_GCM_TAG_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    if (cur_session->state != SESSION_STATE_DONE || !cur_session->ctx_gcm_init) {
        ESP_LOGE(TAG, "Secure session not established");
        return ESP_ERR_INVALID_STATE;
    }

    hexdump("Decrypt IV", cur_session->iv, AES_GCM_IV_SIZE);

    *outlen = inlen - AES_GCM_TAG_LEN;
    *outbuf = (uint8_t *) malloc(*outlen);
    if (!*outbuf) {
        ESP_LOGE(TAG, "Failed to allocate decrypt buf len %d", (int) *outlen);
        return ESP_ERR_NO_MEM;
    }

#ifdef PROTOCOMM_EXT_USE_PSA_CRYPTO
    psa_algorithm_t alg = PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_GCM, AES_GCM_TAG_LEN);
    size_t out_len = 0;
    psa_status_t status = psa_aead_decrypt(cur_session->key_id, alg,
                                           cur_session->iv, AES_GCM_IV_SIZE,
                                           NULL, 0, inbuf, inlen,
                                           *outbuf, *outlen, &out_len);
    if (status != PSA_SUCCESS || out_len != (size_t) *outlen) {
        ESP_LOGE(TAG, "psa_aead_decrypt failed with status=%d", (int) status);
        free(*outbuf);
        *outbuf = NULL;
        *outlen = 0;
        return ESP_FAIL;
    }
#else
    int ret = mbedtls_gcm_auth_decrypt(&cur_session->ctx_gcm, inlen - AES_GCM_TAG_LEN,
                                       cur_session->iv, AES_GCM_IV_SIZE, NULL, 0,
                                       inbuf + (inlen - AES_GCM_TAG_LEN), AES_GCM_TAG_LEN,
                                       inbuf, *outbuf);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed at mbedtls_gcm_auth_decrypt : %d", ret);
        free(*outbuf);
        *outbuf = NULL;
        *outlen = 0;
        return ESP_FAIL;
    }
#endif

    if (cur_session->patch_ver == 1) {
        sec2_gcm_iv_counter_increment(cur_session->iv);
    }

    return ESP_OK;
}

static esp_err_t sec2_send_command0(protocomm_ext_security_handle_t handle, uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
    (void) priv_data;
    ESP_LOGD(TAG, "Start to write setup0_command");

    session_t *cur_session = (session_t *) handle;
    if (!cur_session) {
        ESP_LOGE(TAG, "Invalid session context data");
        return ESP_ERR_INVALID_ARG;
    }

    SessionData req;
    session_data__init(&req);
    if (prepare_command0(cur_session, &req) != ESP_OK) {
        ESP_LOGE(TAG, "Failed in prepare_command0");
        return ESP_FAIL;
    }

    *outlen = session_data__get_packed_size(&req);
    *outbuf = (uint8_t *) malloc(*outlen);
    if (!*outbuf) {
        ESP_LOGE(TAG, "Failed to allocate outbuf");
        cleanup_command0(&req);
        return ESP_FAIL;
    }

    session_data__pack(&req, *outbuf);
    cleanup_command0(&req);

    ESP_LOGD(TAG, "Write setup0_command done");
    cur_session->state = SESSION_STATE_RESP0;
    return ESP_OK;
}

static esp_err_t sec2_send_command1(protocomm_ext_security_handle_t handle, uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
    (void) priv_data;
    ESP_LOGD(TAG, "Start to write setup1_command");

    session_t *cur_session = (session_t *) handle;
    if (!cur_session) {
        ESP_LOGE(TAG, "Invalid session context data");
        return ESP_ERR_INVALID_ARG;
    }

    SessionData req;
    session_data__init(&req);
    if (prepare_command1(cur_session, &req) != ESP_OK) {
        ESP_LOGE(TAG, "Failed in prepare_command1");
        return ESP_FAIL;
    }

    *outlen = session_data__get_packed_size(&req);
    *outbuf = (uint8_t *) malloc(*outlen);
    if (!*outbuf) {
        ESP_LOGE(TAG, "Failed to allocate outbuf");
        cleanup_command1(&req);
        return ESP_FAIL;
    }

    session_data__pack(&req, *outbuf);
    cleanup_command1(&req);

    ESP_LOGD(TAG, "Write setup1_command done");
    cur_session->state = SESSION_STATE_RESP1;
    return ESP_OK;
}

static esp_err_t handle_session_response0(session_t *cur_session, SessionData *resp)
{
    ESP_LOGD(TAG, "Request to handle setup0_response");

    if (resp->sec_ver != protocomm_ext_security2.ver) {
        ESP_LOGE(TAG, "Security version mismatch. Closing connection");
        return ESP_ERR_INVALID_ARG;
    }

    if (resp->proto_case != SESSION_DATA__PROTO_SEC2 || !resp->sec2 ||
            resp->sec2->msg != SEC2_MSG_TYPE__S2Session_Response0) {
        ESP_LOGE(TAG, "Invalid response message type");
        return ESP_ERR_INVALID_ARG;
    }

    if (cur_session->state != SESSION_STATE_RESP0) {
        ESP_LOGW(TAG, "Invalid state of session %d (expected %d).",
                 cur_session->state, SESSION_STATE_RESP0);
        return ESP_ERR_INVALID_STATE;
    }

    if (verify_response0(cur_session, resp) != ESP_OK) {
        ESP_LOGE(TAG, "Invalid response 0");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Session setup phase1 done");
    return ESP_OK;
}

static esp_err_t handle_session_response1(session_t *cur_session, SessionData *resp)
{
    ESP_LOGD(TAG, "Request to handle setup1_response");

    if (resp->sec_ver != protocomm_ext_security2.ver) {
        ESP_LOGE(TAG, "Security version mismatch. Closing connection");
        return ESP_ERR_INVALID_ARG;
    }

    if (resp->proto_case != SESSION_DATA__PROTO_SEC2 || !resp->sec2 ||
            resp->sec2->msg != SEC2_MSG_TYPE__S2Session_Response1) {
        ESP_LOGE(TAG, "Invalid response message type");
        return ESP_ERR_INVALID_ARG;
    }

    if (cur_session->state != SESSION_STATE_RESP1) {
        ESP_LOGW(TAG, "Invalid state of session %d (expected %d).",
                 cur_session->state, SESSION_STATE_RESP1);
        return ESP_ERR_INVALID_STATE;
    }

    if (verify_response1(cur_session, resp) != ESP_OK) {
        ESP_LOGE(TAG, "Invalid response 1");
        return ESP_FAIL;
    }

    /* SRP context no longer needed after session key established */
    if (cur_session->srp) {
        protocomm_ext_srp6a_client_free(cur_session->srp);
        cur_session->srp = NULL;
    }
    free(cur_session->password);
    cur_session->password = NULL;
    cur_session->password_len = 0;

    cur_session->state = SESSION_STATE_DONE;
    ESP_LOGD(TAG, "Secure session established successfully");
    return ESP_OK;
}

static esp_err_t sec2_parse_command0(protocomm_ext_security_handle_t handle, const uint8_t *inbuf, ssize_t inlen, void *priv_data)
{
    (void) priv_data;
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

static esp_err_t sec2_parse_command1(protocomm_ext_security_handle_t handle, const uint8_t *inbuf, ssize_t inlen, void *priv_data)
{
    (void) priv_data;
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

const protocomm_ext_security_t protocomm_ext_security2 = {
    .ver = SEC_SCHEME_VERSION__SecScheme2,
    .init = sec2_init,
    .cleanup = sec2_cleanup,
    .security_send_command0 = sec2_send_command0,
    .security_parse_command0 = sec2_parse_command0,
    .security_send_command1 = sec2_send_command1,
    .security_parse_command1 = sec2_parse_command1,
    .encrypt = sec2_encrypt,
    .decrypt = sec2_decrypt,
};
