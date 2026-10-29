/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include <esp_log.h>

#include "network_config.pb-c.h"
#include "network_constants.pb-c.h"
#include "network_provisioner_priv.h"

static const char *TAG = "net_prov_cfg";

static esp_err_t check_status(Status st, const char *what)
{
    if (st != STATUS__Success) {
        ESP_LOGE(TAG, "%s status=%d", what, (int)st);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t pack_and_send(network_provisioner_t *np, NetworkConfigPayload *msg,
                               uint8_t **resp_raw, size_t *resp_len, bool wipe_packed)
{
    size_t packed_len = network_config_payload__get_packed_size(msg);
    if (packed_len == 0) {
        return ESP_ERR_INVALID_SIZE;
    }
    uint8_t *packed = malloc(packed_len);
    if (!packed) {
        return ESP_ERR_NO_MEM;
    }
    network_config_payload__pack(msg, packed);

    esp_err_t err = network_provisioner_send_ep(np, NETWORK_PROVISIONER_EP_CONFIG,
                                                packed, packed_len, resp_raw, resp_len);
    if (wipe_packed) {
        network_provisioner_wipe_free(packed, packed_len);
    } else {
        free(packed);
    }
    return err;
}

esp_err_t network_provisioner_wifi_set_config(network_provisioner_t *np,
                                              const uint8_t *ssid, size_t ssid_len,
                                              const uint8_t *passphrase, size_t passphrase_len,
                                              const uint8_t *bssid, int32_t channel)
{
    if (!np || !ssid || ssid_len == 0 || ssid_len > 32) {
        return ESP_ERR_INVALID_ARG;
    }
    if (passphrase_len > 64) {
        return ESP_ERR_INVALID_ARG;
    }
    if (passphrase_len > 0 && !passphrase) {
        return ESP_ERR_INVALID_ARG;
    }

    NetworkConfigPayload msg = NETWORK_CONFIG_PAYLOAD__INIT;
    CmdSetWifiConfig cmd = CMD_SET_WIFI_CONFIG__INIT;
    msg.msg = NETWORK_CONFIG_MSG_TYPE__TypeCmdSetWifiConfig;
    msg.payload_case = NETWORK_CONFIG_PAYLOAD__PAYLOAD_CMD_SET_WIFI_CONFIG;
    msg.cmd_set_wifi_config = &cmd;

    cmd.ssid.data = (uint8_t *)ssid;
    cmd.ssid.len = ssid_len;
    if (passphrase && passphrase_len) {
        cmd.passphrase.data = (uint8_t *)passphrase;
        cmd.passphrase.len = passphrase_len;
    }
    if (bssid) {
        cmd.bssid.data = (uint8_t *)bssid;
        cmd.bssid.len = 6;
    }
    cmd.channel = channel;

    uint8_t *resp_raw = NULL;
    size_t resp_len = 0;
    esp_err_t err = pack_and_send(np, &msg, &resp_raw, &resp_len, true);
    if (err != ESP_OK) {
        return err;
    }

    NetworkConfigPayload *resp = network_config_payload__unpack(NULL, resp_len, resp_raw);
    free(resp_raw);
    if (!resp || resp->msg != NETWORK_CONFIG_MSG_TYPE__TypeRespSetWifiConfig ||
            !resp->resp_set_wifi_config) {
        network_config_payload__free_unpacked(resp, NULL);
        return ESP_ERR_INVALID_STATE;
    }
    err = check_status(resp->resp_set_wifi_config->status, "SetWifiConfig");
    network_config_payload__free_unpacked(resp, NULL);
    return err;
}

esp_err_t network_provisioner_wifi_apply_config(network_provisioner_t *np)
{
    if (!np) {
        return ESP_ERR_INVALID_ARG;
    }

    NetworkConfigPayload msg = NETWORK_CONFIG_PAYLOAD__INIT;
    CmdApplyWifiConfig cmd = CMD_APPLY_WIFI_CONFIG__INIT;
    msg.msg = NETWORK_CONFIG_MSG_TYPE__TypeCmdApplyWifiConfig;
    msg.payload_case = NETWORK_CONFIG_PAYLOAD__PAYLOAD_CMD_APPLY_WIFI_CONFIG;
    msg.cmd_apply_wifi_config = &cmd;

    uint8_t *resp_raw = NULL;
    size_t resp_len = 0;
    esp_err_t err = pack_and_send(np, &msg, &resp_raw, &resp_len, false);
    if (err != ESP_OK) {
        return err;
    }

    NetworkConfigPayload *resp = network_config_payload__unpack(NULL, resp_len, resp_raw);
    free(resp_raw);
    if (!resp || resp->msg != NETWORK_CONFIG_MSG_TYPE__TypeRespApplyWifiConfig ||
            !resp->resp_apply_wifi_config) {
        network_config_payload__free_unpacked(resp, NULL);
        return ESP_ERR_INVALID_STATE;
    }
    err = check_status(resp->resp_apply_wifi_config->status, "ApplyWifiConfig");
    network_config_payload__free_unpacked(resp, NULL);
    return err;
}

esp_err_t network_provisioner_wifi_get_status(network_provisioner_t *np,
                                              network_provisioner_wifi_status_t *status)
{
    if (!np || !status) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(status, 0, sizeof(*status));
    status->fail_reason = NETWORK_PROVISIONER_WIFI_FAIL_UNKNOWN;

    NetworkConfigPayload msg = NETWORK_CONFIG_PAYLOAD__INIT;
    CmdGetWifiStatus cmd = CMD_GET_WIFI_STATUS__INIT;
    msg.msg = NETWORK_CONFIG_MSG_TYPE__TypeCmdGetWifiStatus;
    msg.payload_case = NETWORK_CONFIG_PAYLOAD__PAYLOAD_CMD_GET_WIFI_STATUS;
    msg.cmd_get_wifi_status = &cmd;

    uint8_t *resp_raw = NULL;
    size_t resp_len = 0;
    esp_err_t err = pack_and_send(np, &msg, &resp_raw, &resp_len, false);
    if (err != ESP_OK) {
        return err;
    }

    NetworkConfigPayload *resp = network_config_payload__unpack(NULL, resp_len, resp_raw);
    free(resp_raw);
    if (!resp || resp->msg != NETWORK_CONFIG_MSG_TYPE__TypeRespGetWifiStatus ||
            !resp->resp_get_wifi_status) {
        network_config_payload__free_unpacked(resp, NULL);
        return ESP_ERR_INVALID_STATE;
    }

    RespGetWifiStatus *r = resp->resp_get_wifi_status;
    err = check_status(r->status, "GetWifiStatus");
    if (err != ESP_OK) {
        network_config_payload__free_unpacked(resp, NULL);
        return err;
    }

    if (r->wifi_sta_state < WIFI_STATION_STATE__Connected ||
            r->wifi_sta_state > WIFI_STATION_STATE__ConnectionFailed) {
        network_config_payload__free_unpacked(resp, NULL);
        return ESP_ERR_INVALID_RESPONSE;
    }
    status->state = (network_provisioner_wifi_state_t)r->wifi_sta_state;
    if (r->state_case == RESP_GET_WIFI_STATUS__STATE_WIFI_FAIL_REASON) {
        if (r->wifi_fail_reason != WIFI_CONNECT_FAILED_REASON__AuthError &&
                r->wifi_fail_reason != WIFI_CONNECT_FAILED_REASON__WifiNetworkNotFound) {
            network_config_payload__free_unpacked(resp, NULL);
            return ESP_ERR_INVALID_RESPONSE;
        }
        status->fail_reason = (network_provisioner_wifi_fail_reason_t)r->wifi_fail_reason;
    } else if (r->state_case == RESP_GET_WIFI_STATUS__STATE_WIFI_CONNECTED && r->wifi_connected) {
        if (r->wifi_connected->ip4_addr) {
            strncpy(status->ip4_addr, r->wifi_connected->ip4_addr, sizeof(status->ip4_addr) - 1);
        }
        if (r->wifi_connected->ssid.len) {
            size_t n = r->wifi_connected->ssid.len;
            if (n > sizeof(status->ssid) - 1) {
                n = sizeof(status->ssid) - 1;
            }
            memcpy(status->ssid, r->wifi_connected->ssid.data, n);
        }
        status->channel = r->wifi_connected->channel;
    }

    network_config_payload__free_unpacked(resp, NULL);
    return ESP_OK;
}

esp_err_t network_provisioner_thread_set_config(network_provisioner_t *np,
                                                const uint8_t *dataset, size_t dataset_len)
{
    if (!np || !dataset || dataset_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    NetworkConfigPayload msg = NETWORK_CONFIG_PAYLOAD__INIT;
    CmdSetThreadConfig cmd = CMD_SET_THREAD_CONFIG__INIT;
    msg.msg = NETWORK_CONFIG_MSG_TYPE__TypeCmdSetThreadConfig;
    msg.payload_case = NETWORK_CONFIG_PAYLOAD__PAYLOAD_CMD_SET_THREAD_CONFIG;
    msg.cmd_set_thread_config = &cmd;
    cmd.dataset.data = (uint8_t *)dataset;
    cmd.dataset.len = dataset_len;

    uint8_t *resp_raw = NULL;
    size_t resp_len = 0;
    esp_err_t err = pack_and_send(np, &msg, &resp_raw, &resp_len, true);
    if (err != ESP_OK) {
        return err;
    }

    NetworkConfigPayload *resp = network_config_payload__unpack(NULL, resp_len, resp_raw);
    free(resp_raw);
    if (!resp || resp->msg != NETWORK_CONFIG_MSG_TYPE__TypeRespSetThreadConfig ||
            !resp->resp_set_thread_config) {
        network_config_payload__free_unpacked(resp, NULL);
        return ESP_ERR_INVALID_STATE;
    }
    err = check_status(resp->resp_set_thread_config->status, "SetThreadConfig");
    network_config_payload__free_unpacked(resp, NULL);
    return err;
}

esp_err_t network_provisioner_thread_apply_config(network_provisioner_t *np)
{
    if (!np) {
        return ESP_ERR_INVALID_ARG;
    }

    NetworkConfigPayload msg = NETWORK_CONFIG_PAYLOAD__INIT;
    CmdApplyThreadConfig cmd = CMD_APPLY_THREAD_CONFIG__INIT;
    msg.msg = NETWORK_CONFIG_MSG_TYPE__TypeCmdApplyThreadConfig;
    msg.payload_case = NETWORK_CONFIG_PAYLOAD__PAYLOAD_CMD_APPLY_THREAD_CONFIG;
    msg.cmd_apply_thread_config = &cmd;

    uint8_t *resp_raw = NULL;
    size_t resp_len = 0;
    esp_err_t err = pack_and_send(np, &msg, &resp_raw, &resp_len, false);
    if (err != ESP_OK) {
        return err;
    }

    NetworkConfigPayload *resp = network_config_payload__unpack(NULL, resp_len, resp_raw);
    free(resp_raw);
    if (!resp || resp->msg != NETWORK_CONFIG_MSG_TYPE__TypeRespApplyThreadConfig ||
            !resp->resp_apply_thread_config) {
        network_config_payload__free_unpacked(resp, NULL);
        return ESP_ERR_INVALID_STATE;
    }
    err = check_status(resp->resp_apply_thread_config->status, "ApplyThreadConfig");
    network_config_payload__free_unpacked(resp, NULL);
    return err;
}

esp_err_t network_provisioner_thread_get_status(network_provisioner_t *np,
                                                network_provisioner_thread_status_t *status)
{
    if (!np || !status) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(status, 0, sizeof(*status));
    status->fail_reason = NETWORK_PROVISIONER_THREAD_FAIL_UNKNOWN;

    NetworkConfigPayload msg = NETWORK_CONFIG_PAYLOAD__INIT;
    CmdGetThreadStatus cmd = CMD_GET_THREAD_STATUS__INIT;
    msg.msg = NETWORK_CONFIG_MSG_TYPE__TypeCmdGetThreadStatus;
    msg.payload_case = NETWORK_CONFIG_PAYLOAD__PAYLOAD_CMD_GET_THREAD_STATUS;
    msg.cmd_get_thread_status = &cmd;

    uint8_t *resp_raw = NULL;
    size_t resp_len = 0;
    esp_err_t err = pack_and_send(np, &msg, &resp_raw, &resp_len, false);
    if (err != ESP_OK) {
        return err;
    }

    NetworkConfigPayload *resp = network_config_payload__unpack(NULL, resp_len, resp_raw);
    free(resp_raw);
    if (!resp || resp->msg != NETWORK_CONFIG_MSG_TYPE__TypeRespGetThreadStatus ||
            !resp->resp_get_thread_status) {
        network_config_payload__free_unpacked(resp, NULL);
        return ESP_ERR_INVALID_STATE;
    }

    RespGetThreadStatus *r = resp->resp_get_thread_status;
    err = check_status(r->status, "GetThreadStatus");
    if (err != ESP_OK) {
        network_config_payload__free_unpacked(resp, NULL);
        return err;
    }

    if (r->thread_state < THREAD_NETWORK_STATE__Attached ||
            r->thread_state > THREAD_NETWORK_STATE__AttachingFailed) {
        network_config_payload__free_unpacked(resp, NULL);
        return ESP_ERR_INVALID_RESPONSE;
    }
    status->state = (network_provisioner_thread_state_t)r->thread_state;
    if (r->state_case == RESP_GET_THREAD_STATUS__STATE_THREAD_FAIL_REASON) {
        if (r->thread_fail_reason != THREAD_ATTACH_FAILED_REASON__DatasetInvalid &&
                r->thread_fail_reason != THREAD_ATTACH_FAILED_REASON__ThreadNetworkNotFound) {
            network_config_payload__free_unpacked(resp, NULL);
            return ESP_ERR_INVALID_RESPONSE;
        }
        status->fail_reason = (network_provisioner_thread_fail_reason_t)r->thread_fail_reason;
    } else if (r->state_case == RESP_GET_THREAD_STATUS__STATE_THREAD_ATTACHED &&
               r->thread_attached) {
        status->pan_id = r->thread_attached->pan_id;
        status->channel = r->thread_attached->channel;
        if (r->thread_attached->name) {
            strncpy(status->name, r->thread_attached->name, sizeof(status->name) - 1);
        }
    }

    network_config_payload__free_unpacked(resp, NULL);
    return ESP_OK;
}
