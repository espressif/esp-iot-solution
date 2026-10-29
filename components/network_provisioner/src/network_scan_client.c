/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include <esp_log.h>

#include "network_scan.pb-c.h"
#include "network_provisioner_priv.h"

static const char *TAG = "net_prov_scan";

static esp_err_t check_payload_status(Status st, const char *what)
{
    if (st != STATUS__Success) {
        ESP_LOGE(TAG, "%s status=%d", what, (int)st);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t pack_and_send(network_provisioner_t *np, NetworkScanPayload *msg,
                               uint8_t **resp_raw, size_t *resp_len)
{
    size_t packed_len = network_scan_payload__get_packed_size(msg);
    if (packed_len == 0) {
        return ESP_ERR_INVALID_SIZE;
    }
    uint8_t *packed = malloc(packed_len);
    if (!packed) {
        return ESP_ERR_NO_MEM;
    }
    network_scan_payload__pack(msg, packed);

    esp_err_t err = network_provisioner_send_ep(np, NETWORK_PROVISIONER_EP_SCAN,
                                                packed, packed_len, resp_raw, resp_len);
    free(packed);
    return err;
}

esp_err_t network_provisioner_wifi_scan_start(network_provisioner_t *np, bool blocking,
                                              bool passive, uint32_t group_channels,
                                              uint32_t period_ms)
{
    if (!np) {
        return ESP_ERR_INVALID_ARG;
    }

    NetworkScanPayload msg = NETWORK_SCAN_PAYLOAD__INIT;
    CmdScanWifiStart cmd = CMD_SCAN_WIFI_START__INIT;
    msg.msg = NETWORK_SCAN_MSG_TYPE__TypeCmdScanWifiStart;
    msg.payload_case = NETWORK_SCAN_PAYLOAD__PAYLOAD_CMD_SCAN_WIFI_START;
    msg.cmd_scan_wifi_start = &cmd;
    cmd.blocking = blocking;
    cmd.passive = passive;
    cmd.group_channels = group_channels;
    cmd.period_ms = period_ms;

    uint8_t *resp_raw = NULL;
    size_t resp_len = 0;
    esp_err_t err = pack_and_send(np, &msg, &resp_raw, &resp_len);
    if (err != ESP_OK) {
        return err;
    }

    NetworkScanPayload *resp = network_scan_payload__unpack(NULL, resp_len, resp_raw);
    free(resp_raw);
    if (!resp || resp->msg != NETWORK_SCAN_MSG_TYPE__TypeRespScanWifiStart) {
        network_scan_payload__free_unpacked(resp, NULL);
        return ESP_ERR_INVALID_STATE;
    }
    err = check_payload_status(resp->status, "ScanWifiStart");
    network_scan_payload__free_unpacked(resp, NULL);
    return err;
}

esp_err_t network_provisioner_wifi_scan_status(network_provisioner_t *np,
                                               bool *finished, uint32_t *result_count)
{
    if (!np || !finished || !result_count) {
        return ESP_ERR_INVALID_ARG;
    }

    NetworkScanPayload msg = NETWORK_SCAN_PAYLOAD__INIT;
    CmdScanWifiStatus cmd = CMD_SCAN_WIFI_STATUS__INIT;
    msg.msg = NETWORK_SCAN_MSG_TYPE__TypeCmdScanWifiStatus;
    msg.payload_case = NETWORK_SCAN_PAYLOAD__PAYLOAD_CMD_SCAN_WIFI_STATUS;
    msg.cmd_scan_wifi_status = &cmd;

    uint8_t *resp_raw = NULL;
    size_t resp_len = 0;
    esp_err_t err = pack_and_send(np, &msg, &resp_raw, &resp_len);
    if (err != ESP_OK) {
        return err;
    }

    NetworkScanPayload *resp = network_scan_payload__unpack(NULL, resp_len, resp_raw);
    free(resp_raw);
    if (!resp || resp->msg != NETWORK_SCAN_MSG_TYPE__TypeRespScanWifiStatus ||
            !resp->resp_scan_wifi_status) {
        network_scan_payload__free_unpacked(resp, NULL);
        return ESP_ERR_INVALID_STATE;
    }
    err = check_payload_status(resp->status, "ScanWifiStatus");
    if (err == ESP_OK) {
        uint32_t n = resp->resp_scan_wifi_status->result_count;
        if (n > NETWORK_PROVISIONER_MAX_SCAN_RESULTS) {
            ESP_LOGE(TAG, "wifi result_count %u exceeds max %u",
                     (unsigned)n, (unsigned)NETWORK_PROVISIONER_MAX_SCAN_RESULTS);
            err = ESP_ERR_INVALID_SIZE;
        } else {
            *finished = resp->resp_scan_wifi_status->scan_finished;
            *result_count = n;
        }
    }
    network_scan_payload__free_unpacked(resp, NULL);
    return err;
}

esp_err_t network_provisioner_wifi_scan_result(network_provisioner_t *np,
                                               uint32_t start_index, uint32_t count,
                                               network_provisioner_wifi_ap_t *out,
                                               uint32_t *out_count)
{
    if (!np || !out || !out_count || count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (count > NETWORK_PROVISIONER_MAX_SCAN_RESULTS) {
        return ESP_ERR_INVALID_SIZE;
    }

    NetworkScanPayload msg = NETWORK_SCAN_PAYLOAD__INIT;
    CmdScanWifiResult cmd = CMD_SCAN_WIFI_RESULT__INIT;
    msg.msg = NETWORK_SCAN_MSG_TYPE__TypeCmdScanWifiResult;
    msg.payload_case = NETWORK_SCAN_PAYLOAD__PAYLOAD_CMD_SCAN_WIFI_RESULT;
    msg.cmd_scan_wifi_result = &cmd;
    cmd.start_index = start_index;
    cmd.count = count;

    uint8_t *resp_raw = NULL;
    size_t resp_len = 0;
    esp_err_t err = pack_and_send(np, &msg, &resp_raw, &resp_len);
    if (err != ESP_OK) {
        return err;
    }

    NetworkScanPayload *resp = network_scan_payload__unpack(NULL, resp_len, resp_raw);
    free(resp_raw);
    if (!resp || resp->msg != NETWORK_SCAN_MSG_TYPE__TypeRespScanWifiResult ||
            !resp->resp_scan_wifi_result) {
        network_scan_payload__free_unpacked(resp, NULL);
        return ESP_ERR_INVALID_STATE;
    }
    err = check_payload_status(resp->status, "ScanWifiResult");
    if (err != ESP_OK) {
        network_scan_payload__free_unpacked(resp, NULL);
        return err;
    }

    RespScanWifiResult *r = resp->resp_scan_wifi_result;
    uint32_t n = r->n_entries;
    if (n > NETWORK_PROVISIONER_MAX_SCAN_RESULTS) {
        ESP_LOGE(TAG, "wifi n_entries %u exceeds max %u",
                 (unsigned)n, (unsigned)NETWORK_PROVISIONER_MAX_SCAN_RESULTS);
        network_scan_payload__free_unpacked(resp, NULL);
        return ESP_ERR_INVALID_SIZE;
    }
    if (n > count) {
        n = count;
    }
    uint32_t written = 0;
    for (uint32_t i = 0; i < n; i++) {
        WiFiScanResult *e = r->entries[i];
        if (!e) {
            network_scan_payload__free_unpacked(resp, NULL);
            return ESP_ERR_INVALID_RESPONSE;
        }
        memset(&out[written], 0, sizeof(out[written]));
        if (e->ssid.len) {
            size_t sl = e->ssid.len > 32 ? 32 : e->ssid.len;
            memcpy(out[written].ssid, e->ssid.data, sl);
            out[written].ssid_len = (uint8_t)sl;
        }
        if (e->bssid.len >= 6) {
            memcpy(out[written].bssid, e->bssid.data, 6);
        }
        out[written].channel = e->channel;
        out[written].rssi = e->rssi;
        out[written].auth_mode = (int)e->auth;
        written++;
    }
    *out_count = written;
    network_scan_payload__free_unpacked(resp, NULL);
    return ESP_OK;
}

esp_err_t network_provisioner_thread_scan_start(network_provisioner_t *np, bool blocking,
                                                uint32_t channel_mask)
{
    if (!np) {
        return ESP_ERR_INVALID_ARG;
    }

    NetworkScanPayload msg = NETWORK_SCAN_PAYLOAD__INIT;
    CmdScanThreadStart cmd = CMD_SCAN_THREAD_START__INIT;
    msg.msg = NETWORK_SCAN_MSG_TYPE__TypeCmdScanThreadStart;
    msg.payload_case = NETWORK_SCAN_PAYLOAD__PAYLOAD_CMD_SCAN_THREAD_START;
    msg.cmd_scan_thread_start = &cmd;
    cmd.blocking = blocking;
    cmd.channel_mask = channel_mask;

    uint8_t *resp_raw = NULL;
    size_t resp_len = 0;
    esp_err_t err = pack_and_send(np, &msg, &resp_raw, &resp_len);
    if (err != ESP_OK) {
        return err;
    }

    NetworkScanPayload *resp = network_scan_payload__unpack(NULL, resp_len, resp_raw);
    free(resp_raw);
    if (!resp || resp->msg != NETWORK_SCAN_MSG_TYPE__TypeRespScanThreadStart) {
        network_scan_payload__free_unpacked(resp, NULL);
        return ESP_ERR_INVALID_STATE;
    }
    err = check_payload_status(resp->status, "ScanThreadStart");
    network_scan_payload__free_unpacked(resp, NULL);
    return err;
}

esp_err_t network_provisioner_thread_scan_status(network_provisioner_t *np,
                                                 bool *finished, uint32_t *result_count)
{
    if (!np || !finished || !result_count) {
        return ESP_ERR_INVALID_ARG;
    }

    NetworkScanPayload msg = NETWORK_SCAN_PAYLOAD__INIT;
    CmdScanThreadStatus cmd = CMD_SCAN_THREAD_STATUS__INIT;
    msg.msg = NETWORK_SCAN_MSG_TYPE__TypeCmdScanThreadStatus;
    msg.payload_case = NETWORK_SCAN_PAYLOAD__PAYLOAD_CMD_SCAN_THREAD_STATUS;
    msg.cmd_scan_thread_status = &cmd;

    uint8_t *resp_raw = NULL;
    size_t resp_len = 0;
    esp_err_t err = pack_and_send(np, &msg, &resp_raw, &resp_len);
    if (err != ESP_OK) {
        return err;
    }

    NetworkScanPayload *resp = network_scan_payload__unpack(NULL, resp_len, resp_raw);
    free(resp_raw);
    if (!resp || resp->msg != NETWORK_SCAN_MSG_TYPE__TypeRespScanThreadStatus ||
            !resp->resp_scan_thread_status) {
        network_scan_payload__free_unpacked(resp, NULL);
        return ESP_ERR_INVALID_STATE;
    }
    err = check_payload_status(resp->status, "ScanThreadStatus");
    if (err == ESP_OK) {
        uint32_t n = resp->resp_scan_thread_status->result_count;
        if (n > NETWORK_PROVISIONER_MAX_SCAN_RESULTS) {
            ESP_LOGE(TAG, "thread result_count %u exceeds max %u",
                     (unsigned)n, (unsigned)NETWORK_PROVISIONER_MAX_SCAN_RESULTS);
            err = ESP_ERR_INVALID_SIZE;
        } else {
            *finished = resp->resp_scan_thread_status->scan_finished;
            *result_count = n;
        }
    }
    network_scan_payload__free_unpacked(resp, NULL);
    return err;
}

esp_err_t network_provisioner_thread_scan_result(network_provisioner_t *np,
                                                 uint32_t start_index, uint32_t count,
                                                 network_provisioner_thread_network_t *out,
                                                 uint32_t *out_count)
{
    if (!np || !out || !out_count || count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (count > NETWORK_PROVISIONER_MAX_SCAN_RESULTS) {
        return ESP_ERR_INVALID_SIZE;
    }

    NetworkScanPayload msg = NETWORK_SCAN_PAYLOAD__INIT;
    CmdScanThreadResult cmd = CMD_SCAN_THREAD_RESULT__INIT;
    msg.msg = NETWORK_SCAN_MSG_TYPE__TypeCmdScanThreadResult;
    msg.payload_case = NETWORK_SCAN_PAYLOAD__PAYLOAD_CMD_SCAN_THREAD_RESULT;
    msg.cmd_scan_thread_result = &cmd;
    cmd.start_index = start_index;
    cmd.count = count;

    uint8_t *resp_raw = NULL;
    size_t resp_len = 0;
    esp_err_t err = pack_and_send(np, &msg, &resp_raw, &resp_len);
    if (err != ESP_OK) {
        return err;
    }

    NetworkScanPayload *resp = network_scan_payload__unpack(NULL, resp_len, resp_raw);
    free(resp_raw);
    if (!resp || resp->msg != NETWORK_SCAN_MSG_TYPE__TypeRespScanThreadResult ||
            !resp->resp_scan_thread_result) {
        network_scan_payload__free_unpacked(resp, NULL);
        return ESP_ERR_INVALID_STATE;
    }
    err = check_payload_status(resp->status, "ScanThreadResult");
    if (err != ESP_OK) {
        network_scan_payload__free_unpacked(resp, NULL);
        return err;
    }

    RespScanThreadResult *r = resp->resp_scan_thread_result;
    uint32_t n = r->n_entries;
    if (n > NETWORK_PROVISIONER_MAX_SCAN_RESULTS) {
        ESP_LOGE(TAG, "thread n_entries %u exceeds max %u",
                 (unsigned)n, (unsigned)NETWORK_PROVISIONER_MAX_SCAN_RESULTS);
        network_scan_payload__free_unpacked(resp, NULL);
        return ESP_ERR_INVALID_SIZE;
    }
    if (n > count) {
        n = count;
    }
    uint32_t written = 0;
    for (uint32_t i = 0; i < n; i++) {
        ThreadScanResult *e = r->entries[i];
        if (!e) {
            network_scan_payload__free_unpacked(resp, NULL);
            return ESP_ERR_INVALID_RESPONSE;
        }
        memset(&out[written], 0, sizeof(out[written]));
        out[written].pan_id = e->pan_id;
        out[written].channel = e->channel;
        out[written].rssi = e->rssi;
        out[written].lqi = e->lqi;
        if (e->network_name) {
            strncpy(out[written].network_name, e->network_name, sizeof(out[written].network_name) - 1);
        }
        if (e->ext_pan_id.len >= 8) {
            memcpy(out[written].ext_pan_id, e->ext_pan_id.data, 8);
        }
        if (e->ext_addr.len >= 8) {
            memcpy(out[written].ext_addr, e->ext_addr.data, 8);
        }
        written++;
    }
    *out_count = written;
    network_scan_payload__free_unpacked(resp, NULL);
    return ESP_OK;
}
