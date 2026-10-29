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

#include "protocomm_ext_security.h"
#include "protocomm_ext_security0.h"

#include "session.pb-c.h"
#include "sec0.pb-c.h"
#include "constants.pb-c.h"

static const char* TAG = "security0";

/* Lightweight non-NULL sentinel so core can treat Sec0 like other security objects. */
static int s_sec0_handle;

static esp_err_t sec0_init(protocomm_ext_security_handle_t *handle, const void *sec_params)
{
    (void)sec_params;
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    *handle = &s_sec0_handle;
    return ESP_OK;
}

static esp_err_t sec0_cleanup(protocomm_ext_security_handle_t handle)
{
    (void)handle;
    return ESP_OK;
}

static esp_err_t sec0_session_setup(SessionData *req)
{
    Sec0Payload *sec0_payload = (Sec0Payload *) malloc(sizeof(Sec0Payload));
    if (!sec0_payload) {
        ESP_LOGE(TAG, "Error allocating sec0_payload");
        return ESP_ERR_NO_MEM;
    }
    S0SessionCmd *s0cmd = (S0SessionCmd *) malloc(sizeof(S0SessionCmd));
    if (!s0cmd) {
        ESP_LOGE(TAG, "Error allocating s0cmd");
        free(sec0_payload);
        return ESP_ERR_NO_MEM;
    }

    sec0_payload__init(sec0_payload);
    sec0_payload->msg = SEC0_MSG_TYPE__S0_Session_Command;
    sec0_payload->payload_case = SEC0_PAYLOAD__PAYLOAD_SC;
    s0_session_cmd__init(s0cmd);
    sec0_payload->sc = s0cmd;

    req->proto_case = SESSION_DATA__PROTO_SEC0;
    req->sec0 = sec0_payload;
    req->sec_ver = SEC_SCHEME_VERSION__SecScheme0;

    return ESP_OK;
}

static void sec0_session_setup_cleanup(SessionData *req)
{
    if (!req) {
        return;
    }

    free(req->sec0->sc);
    free(req->sec0);
    return;
}

static esp_err_t sec0_send_command0(protocomm_ext_security_handle_t handle, uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
    SessionData req;
    session_data__init(&req);
    esp_err_t ret = sec0_session_setup(&req);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Session setup error %d", ret);
        return ESP_FAIL;
    }

    *outlen = session_data__get_packed_size(&req);
    if (*outlen <= 0) {
        ESP_LOGE(TAG, "Invalid encoding for response");
        sec0_session_setup_cleanup(&req);
        return ESP_FAIL;
    }

    *outbuf = (uint8_t *) malloc(*outlen);
    if (!*outbuf) {
        ESP_LOGE(TAG, "System out of memory");
        sec0_session_setup_cleanup(&req);
        return ESP_ERR_NO_MEM;
    }
    session_data__pack(&req, *outbuf);
    sec0_session_setup_cleanup(&req);
    return ESP_OK;
}

static esp_err_t sec0_parse_command0(protocomm_ext_security_handle_t handle, const uint8_t *inbuf, ssize_t inlen, void *priv_data)
{
    (void)handle;
    (void)priv_data;

    SessionData *resp = session_data__unpack(NULL, inlen, inbuf);
    if (!resp) {
        ESP_LOGE(TAG, "Unable to unpack setup_resp");
        return ESP_ERR_INVALID_ARG;
    }
    if (resp->sec_ver != protocomm_ext_security0.ver) {
        ESP_LOGE(TAG, "Security version mismatch, %d != %d", resp->sec_ver, protocomm_ext_security0.ver);
        session_data__free_unpacked(resp, NULL);
        return ESP_ERR_INVALID_ARG;
    }
    if (resp->proto_case != SESSION_DATA__PROTO_SEC0 || !resp->sec0) {
        ESP_LOGE(TAG, "Invalid Sec0 response proto");
        session_data__free_unpacked(resp, NULL);
        return ESP_ERR_INVALID_ARG;
    }
    if (resp->sec0->payload_case != SEC0_PAYLOAD__PAYLOAD_SR || !resp->sec0->sr) {
        ESP_LOGE(TAG, "Invalid Sec0 response payload");
        session_data__free_unpacked(resp, NULL);
        return ESP_ERR_INVALID_ARG;
    }
    if (resp->sec0->sr->status != STATUS__Success) {
        ESP_LOGE(TAG, "Sec0 session status is not Success (%d)", (int)resp->sec0->sr->status);
        session_data__free_unpacked(resp, NULL);
        return ESP_FAIL;
    }

    session_data__free_unpacked(resp, NULL);
    return ESP_OK;
}

const protocomm_ext_security_t protocomm_ext_security0 = {
    .ver = SEC_SCHEME_VERSION__SecScheme0,
    .init = sec0_init,
    .cleanup = sec0_cleanup,
    .security_send_command0 = sec0_send_command0,
    .security_parse_command0 = sec0_parse_command0,
    .security_send_command1 = NULL,
    .security_parse_command1 = NULL,
    .encrypt = NULL,
    .decrypt = NULL,
};
