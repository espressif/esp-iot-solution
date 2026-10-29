/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

#include <esp_log.h>

#include "network_ctrl.pb-c.h"
#include "network_provisioner_priv.h"

static const char *TAG = "net_prov_ctrl";

static esp_err_t ctrl_roundtrip(network_provisioner_t *np, NetworkCtrlMsgType cmd_type,
                                NetworkCtrlPayload__PayloadCase payload_case,
                                void *cmd_ptr, NetworkCtrlMsgType expect_resp)
{
    if (!np) {
        return ESP_ERR_INVALID_ARG;
    }

    NetworkCtrlPayload msg = NETWORK_CTRL_PAYLOAD__INIT;
    msg.msg = cmd_type;
    msg.payload_case = payload_case;
    switch (payload_case) {
    case NETWORK_CTRL_PAYLOAD__PAYLOAD_CMD_CTRL_WIFI_RESET:
        msg.cmd_ctrl_wifi_reset = cmd_ptr;
        break;
    case NETWORK_CTRL_PAYLOAD__PAYLOAD_CMD_CTRL_WIFI_REPROV:
        msg.cmd_ctrl_wifi_reprov = cmd_ptr;
        break;
    case NETWORK_CTRL_PAYLOAD__PAYLOAD_CMD_CTRL_THREAD_RESET:
        msg.cmd_ctrl_thread_reset = cmd_ptr;
        break;
    case NETWORK_CTRL_PAYLOAD__PAYLOAD_CMD_CTRL_THREAD_REPROV:
        msg.cmd_ctrl_thread_reprov = cmd_ptr;
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }

    size_t packed_len = network_ctrl_payload__get_packed_size(&msg);
    if (packed_len == 0) {
        return ESP_ERR_INVALID_SIZE;
    }
    uint8_t *packed = malloc(packed_len);
    if (!packed) {
        return ESP_ERR_NO_MEM;
    }
    network_ctrl_payload__pack(&msg, packed);

    uint8_t *resp_raw = NULL;
    size_t resp_len = 0;
    esp_err_t err = network_provisioner_send_ep(np, NETWORK_PROVISIONER_EP_CTRL,
                                                packed, packed_len, &resp_raw, &resp_len);
    free(packed);
    if (err != ESP_OK) {
        return err;
    }

    NetworkCtrlPayload *resp = network_ctrl_payload__unpack(NULL, resp_len, resp_raw);
    free(resp_raw);
    if (!resp || resp->msg != expect_resp) {
        network_ctrl_payload__free_unpacked(resp, NULL);
        return ESP_ERR_INVALID_STATE;
    }
    if (resp->status != STATUS__Success) {
        ESP_LOGE(TAG, "ctrl status=%d", (int)resp->status);
        network_ctrl_payload__free_unpacked(resp, NULL);
        return ESP_FAIL;
    }
    network_ctrl_payload__free_unpacked(resp, NULL);
    return ESP_OK;
}

esp_err_t network_provisioner_wifi_reset(network_provisioner_t *np)
{
    CmdCtrlWifiReset cmd = CMD_CTRL_WIFI_RESET__INIT;
    return ctrl_roundtrip(np, NETWORK_CTRL_MSG_TYPE__TypeCmdCtrlWifiReset,
                          NETWORK_CTRL_PAYLOAD__PAYLOAD_CMD_CTRL_WIFI_RESET, &cmd,
                          NETWORK_CTRL_MSG_TYPE__TypeRespCtrlWifiReset);
}

esp_err_t network_provisioner_wifi_reprov(network_provisioner_t *np)
{
    CmdCtrlWifiReprov cmd = CMD_CTRL_WIFI_REPROV__INIT;
    return ctrl_roundtrip(np, NETWORK_CTRL_MSG_TYPE__TypeCmdCtrlWifiReprov,
                          NETWORK_CTRL_PAYLOAD__PAYLOAD_CMD_CTRL_WIFI_REPROV, &cmd,
                          NETWORK_CTRL_MSG_TYPE__TypeRespCtrlWifiReprov);
}

esp_err_t network_provisioner_thread_reset(network_provisioner_t *np)
{
    CmdCtrlThreadReset cmd = CMD_CTRL_THREAD_RESET__INIT;
    return ctrl_roundtrip(np, NETWORK_CTRL_MSG_TYPE__TypeCmdCtrlThreadReset,
                          NETWORK_CTRL_PAYLOAD__PAYLOAD_CMD_CTRL_THREAD_RESET, &cmd,
                          NETWORK_CTRL_MSG_TYPE__TypeRespCtrlThreadReset);
}

esp_err_t network_provisioner_thread_reprov(network_provisioner_t *np)
{
    CmdCtrlThreadReprov cmd = CMD_CTRL_THREAD_REPROV__INIT;
    return ctrl_roundtrip(np, NETWORK_CTRL_MSG_TYPE__TypeCmdCtrlThreadReprov,
                          NETWORK_CTRL_PAYLOAD__PAYLOAD_CMD_CTRL_THREAD_REPROV, &cmd,
                          NETWORK_CTRL_MSG_TYPE__TypeRespCtrlThreadReprov);
}
