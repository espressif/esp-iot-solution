/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <esp_log.h>
#include <string.h>
#include <esp_err.h>

#include "ota_send_file.pb-c.h"

#include "ota.h"

static const char *TAG = "proto_ota";

typedef struct ota_cmd {
    int cmd_id;
    esp_err_t (*command_handler)(SendFilePayload *req,
                                 SendFilePayload *resp, void *priv_data);
} ota_cmd_t;

static esp_err_t ota_send_file_handler(SendFilePayload *req,
                                       SendFilePayload *resp,
                                       void *priv_data);

static esp_err_t ota_start_cmd_handler(SendFilePayload *req,
                                       SendFilePayload *resp, void *priv_data);

static esp_err_t ota_finish_cmd_handler(SendFilePayload *req,
                                        SendFilePayload *resp, void *priv_data);

static ota_cmd_t cmd_table[] = {
    {
        .cmd_id = SEND_FILE_MSG_TYPE__TypeCmdSendFile,
        .command_handler = ota_send_file_handler
    },
    {
        .cmd_id = SEND_FILE_MSG_TYPE__TypeCmdStartOTA,
        .command_handler = ota_start_cmd_handler
    },
    {
        .cmd_id = SEND_FILE_MSG_TYPE__TypeCmdFinishOTA,
        .command_handler = ota_finish_cmd_handler
    }
};

static esp_err_t
ota_send_file_handler(SendFilePayload *req,
                      SendFilePayload *resp, void *priv_data)
{
    ota_handlers_t *h = (ota_handlers_t *) priv_data;
    if (!h) {
        ESP_LOGE(TAG, "Command invoked without handlers");
        return ESP_ERR_INVALID_STATE;
    }

    RespSendFile *resp_payload = (RespSendFile *) malloc(sizeof(RespSendFile));
    if (!resp_payload) {
        ESP_LOGE(TAG, "Error allocating memory");
        return ESP_ERR_NO_MEM;
    }

    resp_send_file__init(resp_payload);
    resp->status = (h->ota_send_file(req->cmd_send_file->data.data,
                                     req->cmd_send_file->data.len) == ESP_OK ?
                    STATUS__Success : STATUS__InternalError);
    resp->payload_case = SEND_FILE_PAYLOAD__PAYLOAD_RESP_SEND_FILE;
    resp->resp_send_file = resp_payload;
    return ESP_OK;
}

static esp_err_t
ota_start_cmd_handler(SendFilePayload *req,
                      SendFilePayload *resp, void *priv_data)
{
    ESP_LOGI(TAG, "%s", __func__);
    ota_handlers_t *h = (ota_handlers_t *) priv_data;
    if (!h) {
        ESP_LOGE(TAG, "Command invoked without handlers");
        return ESP_ERR_INVALID_STATE;
    }

    RespStartOTA *resp_payload = (RespStartOTA *) malloc(sizeof(RespStartOTA));
    if (!resp_payload) {
        ESP_LOGE(TAG, "Error allocating memory");
        return ESP_ERR_NO_MEM;
    }

    resp_start_ota__init(resp_payload);
    resp->status = (h->ota_start_cmd(req->cmd_start_ota->file_size,
                                     req->cmd_start_ota->block_size,
                                     req->cmd_start_ota->partition_name) == ESP_OK ?
                    STATUS__Success : STATUS__InternalError);
    resp->payload_case = SEND_FILE_PAYLOAD__PAYLOAD_CMD_START_OTA;
    resp->resp_start_ota = resp_payload;
    return ESP_OK;
}

static esp_err_t
ota_finish_cmd_handler(SendFilePayload *req,
                       SendFilePayload *resp, void *priv_data)
{
    ESP_LOGI(TAG, "%s", __func__);
    ota_handlers_t *h = (ota_handlers_t *) priv_data;
    if (!h) {
        ESP_LOGE(TAG, "Command invoked without handlers");
        return ESP_ERR_INVALID_STATE;
    }

    RespFinishOTA *resp_payload = (RespFinishOTA *) malloc(sizeof(RespFinishOTA));
    if (!resp_payload) {
        ESP_LOGE(TAG, "Error allocating memory");
        return ESP_ERR_NO_MEM;
    }

    resp_finish_ota__init(resp_payload);
    resp->status = (h->ota_finish_cmd() == ESP_OK ?
                    STATUS__Success : STATUS__InternalError);
    resp->payload_case = SEND_FILE_PAYLOAD__PAYLOAD_CMD_FINISH_OTA;
    resp->resp_finish_ota = resp_payload;
    return ESP_OK;
}

static int
lookup_cmd_handler(int cmd_id)
{
    for (size_t i = 0; i < sizeof(cmd_table) / sizeof(ota_cmd_t); i++) {
        if (cmd_table[i].cmd_id == cmd_id) {
            return i;
        }
    }

    return -1;
}

static void
ota_cmd_cleanup(SendFilePayload *resp, void *priv_data)
{
    switch (resp->msg) {
    case SEND_FILE_MSG_TYPE__TypeRespSendFile: {
        free(resp->resp_send_file);
    }
    break;
    case SEND_FILE_MSG_TYPE__TypeRespFinishOTA: {
        free(resp->resp_finish_ota);
    }
    break;
    case SEND_FILE_MSG_TYPE__TypeRespStartOTA: {
        free(resp->resp_start_ota);
    }
    break;
    default:
        ESP_LOGE(TAG, "Unsupported response type in cleanup_handler");
        break;
    }
    return;
}

static esp_err_t
ota_cmd_dispatcher(SendFilePayload *req,
                   SendFilePayload *resp, void *priv_data)
{
    esp_err_t ret;

    ESP_LOGD(TAG, "In ota_cmd_dispatcher Cmd=%d", req->msg);

    int cmd_index = lookup_cmd_handler(req->msg);
    if (cmd_index < 0) {
        ESP_LOGE(TAG, "Failed to find cmd with ID = %d in the command table", req->msg);
        return ESP_FAIL;
    }

    ret = cmd_table[cmd_index].command_handler(req, resp, priv_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error executing command handler");
    }

    return ret;
}

esp_err_t
ota_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen,
            uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
    SendFilePayload *req;
    SendFilePayload resp;
    esp_err_t ret = ESP_OK;

    req = send_file_payload__unpack(NULL, inlen, inbuf);
    if (!req) {
        ESP_LOGE(TAG, "Unable to unpack ota message");
        return ESP_ERR_INVALID_ARG;
    }

    send_file_payload__init(&resp);
    ret = ota_cmd_dispatcher(req, &resp, priv_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Command dispatcher error %02X", ret);
        ret = ESP_FAIL;
        goto exit;
    }

    resp.msg = req->msg + 1; /* Response is request + 1 */
    *outlen = send_file_payload__get_packed_size(&resp);
    if (*outlen <= 0) {
        ESP_LOGE(TAG, "Invalid encoding for response");
        ret = ESP_FAIL;
        goto exit;
    }

    *outbuf = (uint8_t *) malloc(*outlen);
    if (!*outbuf) {
        ESP_LOGE(TAG, "Failed to allocate memory for the output buffer");
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    send_file_payload__pack(&resp, *outbuf);
    ESP_LOGD(TAG, "Response packet size : %d", *outlen);
exit:

    send_file_payload__free_unpacked(req, NULL);
    ota_cmd_cleanup(&resp, priv_data);
    return ret;
}
