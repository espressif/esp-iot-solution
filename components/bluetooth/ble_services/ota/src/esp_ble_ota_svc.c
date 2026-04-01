/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief Implementation of ESP BLE OTA GATT service (0x8018 + 0x8020–0x8023) for esp_ble_conn_mgr.
 */

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_ble_conn_mgr.h"
#include "esp_ble_ota_svc.h"

static const char *TAG = "esp_ble_ota_svc";

__attribute__((weak)) void esp_ble_ota_recv_fw_data(const uint8_t *data, uint16_t len)
{
    (void)data;
    (void)len;
}

__attribute__((weak)) void esp_ble_ota_recv_cmd_data(const uint8_t *data, uint16_t len)
{
    (void)data;
    (void)len;
}

__attribute__((weak)) void esp_ble_ota_recv_customer_data(const uint8_t *data, uint16_t len)
{
    (void)data;
    (void)len;
}

/**
 * @brief PROGRESS_BAR characteristic value length (octets).
 *
 * Over-the-air order: first octet (low) = integer part 0–100; second octet (high) = hundredths 0–99.
 * Value = first + second / 100.0. When integer is 100, hundredths shall be 0.
 * Matches GATT Read value and @ref esp_ble_ota_notify_progress_bar_raw payload.
 */
#define BLE_OTA_PROGRESS_BAR_LEN                    2
static uint8_t s_progress_bar[BLE_OTA_PROGRESS_BAR_LEN];

/* Mutex: protects s_progress_bar only (same pattern as esp_ots.c). */
static SemaphoreHandle_t s_ble_ota_svc_mutex = NULL;
#define BLE_OTA_MUTEX_TIMEOUT_MS 2000
#define OTA_PROGRESS_BAR_MUTEX_TIMEOUT_TICKS pdMS_TO_TICKS(BLE_OTA_MUTEX_TIMEOUT_MS)

static bool ble_ota_mutex_lock(void)
{
    if (s_ble_ota_svc_mutex == NULL) {
        return false;
    }

    if (xSemaphoreTake(s_ble_ota_svc_mutex, OTA_PROGRESS_BAR_MUTEX_TIMEOUT_TICKS) != pdTRUE) {
        ESP_LOGW(TAG, "progress_bar mutex timeout after %d ms", BLE_OTA_MUTEX_TIMEOUT_MS);
        return false;
    }

    return true;
}

static void ble_ota_mutex_unlock(bool locked)
{
    if (locked && s_ble_ota_svc_mutex != NULL) {
        xSemaphoreGive(s_ble_ota_svc_mutex);
    }
}

static esp_err_t ota_notify_chr(uint16_t uuid16, const uint8_t *data, uint16_t len)
{
    if (len == 0 || len > BLE_OTA_ATT_PAYLOAD_MAX_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_ble_conn_data_t conn_data = {
        .type = BLE_CONN_UUID_TYPE_16,
        .uuid = { .uuid16 = uuid16 },
        .data = (uint8_t *)data,
        .data_len = len,
    };
    return esp_ble_conn_notify(&conn_data);
}

uint16_t esp_ble_ota_progress_bar_local_get(void)
{
    uint16_t v;
    const bool locked = ble_ota_mutex_lock();
    if (!locked) {
        return UINT16_MAX;
    }
    memcpy(&v, s_progress_bar, sizeof(v));
    ble_ota_mutex_unlock(locked);
    return v;
}

esp_err_t esp_ble_ota_notify_recv_fw_raw(const uint8_t *data, uint16_t len)
{
    return ota_notify_chr(BLE_OTA_CHR_UUID16_RECV_FW, data, len);
}

esp_err_t esp_ble_ota_notify_progress_bar_raw(uint8_t low_octet, uint8_t high_octet)
{
    uint8_t payload[BLE_OTA_PROGRESS_BAR_LEN];

    if (low_octet > 100 || high_octet > 99) {
        return ESP_ERR_INVALID_ARG;
    }
    if (low_octet == 100 && high_octet != 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const bool locked = ble_ota_mutex_lock();
    if (!locked) {
        return ESP_ERR_TIMEOUT;
    }
    s_progress_bar[0] = low_octet;
    s_progress_bar[1] = high_octet;
    memcpy(payload, s_progress_bar, sizeof(payload));
    ble_ota_mutex_unlock(locked);

    return ota_notify_chr(BLE_OTA_CHR_UUID16_PROGRESS_BAR, payload, BLE_OTA_PROGRESS_BAR_LEN);
}

esp_err_t esp_ble_ota_notify_command_raw(const uint8_t *data, uint16_t len)
{
    return ota_notify_chr(BLE_OTA_CHR_UUID16_COMMAND, data, len);
}

esp_err_t esp_ble_ota_notify_customer_raw(const uint8_t *data, uint16_t len)
{
    return ota_notify_chr(BLE_OTA_CHR_UUID16_CUSTOMER, data, len);
}

static esp_err_t validate_write_wo_rsp(const uint8_t *inbuf, uint16_t inlen,
                                       uint8_t **outbuf, uint16_t *outlen,
                                       uint8_t *att_status, bool *processable)
{
    if (att_status == NULL || processable == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *processable = false;

    if (outbuf == NULL || outlen == NULL) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    if (inbuf == NULL || inlen == 0 || inlen > BLE_OTA_ATT_PAYLOAD_MAX_LEN) {
        *att_status = ESP_IOT_ATT_INVALID_ATTR_LEN;
        return ESP_OK;
    }

    *att_status = ESP_IOT_ATT_SUCCESS;
    *processable = true;
    return ESP_OK;
}

static esp_err_t esp_ota_recv_fw_cb(const uint8_t *inbuf, uint16_t inlen,
                                    uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    (void)priv_data;
    bool processable = false;
    esp_err_t ret = validate_write_wo_rsp(inbuf, inlen, outbuf, outlen, att_status, &processable);

    if (ret != ESP_OK || !processable) {
        return ret;
    }

    esp_ble_ota_recv_fw_data(inbuf, inlen);
    return ESP_OK;
}

static esp_err_t esp_ota_command_cb(const uint8_t *inbuf, uint16_t inlen,
                                    uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    (void)priv_data;
    bool processable = false;
    esp_err_t ret = validate_write_wo_rsp(inbuf, inlen, outbuf, outlen, att_status, &processable);

    if (ret != ESP_OK || !processable) {
        return ret;
    }

    esp_ble_ota_recv_cmd_data(inbuf, inlen);
    return ESP_OK;
}

static esp_err_t esp_ota_customer_cb(const uint8_t *inbuf, uint16_t inlen,
                                     uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    (void)priv_data;
    bool processable = false;
    esp_err_t ret = validate_write_wo_rsp(inbuf, inlen, outbuf, outlen, att_status, &processable);

    if (ret != ESP_OK || !processable) {
        return ret;
    }

    esp_ble_ota_recv_customer_data(inbuf, inlen);
    return ESP_OK;
}

/** 0x8021 Read: heap copy of 2-byte progress; see esp_ble_ota_svc.h for encoding. */
static esp_err_t esp_ota_progress_cb(const uint8_t *inbuf, uint16_t inlen,
                                     uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    (void)priv_data;

    if (att_status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (inbuf != NULL || inlen > 0) {
        *att_status = ESP_IOT_ATT_WRITE_NOT_PERMIT;
        return ESP_ERR_INVALID_ARG;
    }

    if (outbuf == NULL || outlen == NULL) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    *outlen = BLE_OTA_PROGRESS_BAR_LEN;
    *outbuf = (uint8_t *)calloc(1, *outlen);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }
    const bool locked = ble_ota_mutex_lock();
    if (!locked) {
        free(*outbuf);
        *outbuf = NULL;
        *outlen = 0;
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_TIMEOUT;
    }
    memcpy(*outbuf, s_progress_bar, *outlen);
    ble_ota_mutex_unlock(locked);
    *att_status = ESP_IOT_ATT_SUCCESS;
    return ESP_OK;
}

static const esp_ble_conn_character_t nu_lookup_table[] = {
    {
        "recv_fw",
        BLE_CONN_UUID_TYPE_16,
        (BLE_CONN_GATT_CHR_WRITE | BLE_CONN_GATT_CHR_WRITE_NO_RSP | BLE_CONN_GATT_CHR_NOTIFY | BLE_CONN_GATT_CHR_INDICATE),
        { .uuid16 = BLE_OTA_CHR_UUID16_RECV_FW },
        esp_ota_recv_fw_cb
    },
    {
        "progress_bar",
        BLE_CONN_UUID_TYPE_16,
        (BLE_CONN_GATT_CHR_WRITE | BLE_CONN_GATT_CHR_READ | BLE_CONN_GATT_CHR_NOTIFY | BLE_CONN_GATT_CHR_INDICATE),
        { .uuid16 = BLE_OTA_CHR_UUID16_PROGRESS_BAR },
        esp_ota_progress_cb
    },
    {
        "command",
        BLE_CONN_UUID_TYPE_16,
        (BLE_CONN_GATT_CHR_WRITE | BLE_CONN_GATT_CHR_WRITE_NO_RSP | BLE_CONN_GATT_CHR_NOTIFY | BLE_CONN_GATT_CHR_INDICATE),
        { .uuid16 = BLE_OTA_CHR_UUID16_COMMAND },
        esp_ota_command_cb
    },
    {
        "customer",
        BLE_CONN_UUID_TYPE_16,
        (BLE_CONN_GATT_CHR_WRITE | BLE_CONN_GATT_CHR_WRITE_NO_RSP | BLE_CONN_GATT_CHR_NOTIFY | BLE_CONN_GATT_CHR_INDICATE),
        { .uuid16 = BLE_OTA_CHR_UUID16_CUSTOMER },
        esp_ota_customer_cb
    },
};

static const esp_ble_conn_svc_t svc = {
    .type = BLE_CONN_UUID_TYPE_16,
    .uuid = { .uuid16 = BLE_OTA_SERVICE_UUID16 },
    .nu_lookup_count = sizeof(nu_lookup_table) / sizeof(nu_lookup_table[0]),
    /* NOLINTNEXTLINE: esp_ble_conn_add_svc copies the table */
    .nu_lookup = (esp_ble_conn_character_t *)nu_lookup_table
};

esp_err_t esp_ble_ota_svc_init(void)
{
    if (s_ble_ota_svc_mutex == NULL) {
        s_ble_ota_svc_mutex = xSemaphoreCreateMutex();
        if (s_ble_ota_svc_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create progress_bar mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    const bool locked = ble_ota_mutex_lock();
    if (!locked) {
        return ESP_ERR_TIMEOUT;
    }
    memset(s_progress_bar, 0, sizeof(s_progress_bar));
    ble_ota_mutex_unlock(locked);

    return esp_ble_conn_add_svc(&svc);
}

esp_err_t esp_ble_ota_svc_deinit(void)
{
    esp_err_t ret = esp_ble_conn_remove_svc(&svc);

    if (s_ble_ota_svc_mutex != NULL) {
        vSemaphoreDelete(s_ble_ota_svc_mutex);
        s_ble_ota_svc_mutex = NULL;
    }

    return ret;
}
