/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief BLE-MIDI GATT Service
 */

#include "esp_err.h"
#include "esp_ble_conn_mgr.h"
#include "esp_ble_midi_svc.h"

#define BLE_MIDI_MAX_PACKET_SIZE 256

/* Weak default implementation; profile can override with strong symbol */
__attribute__((weak)) void esp_ble_midi_on_bep_received(const uint8_t *data, uint16_t len)
{
    /* Default no-op implementation */
    (void)data;
    (void)len;
}

static esp_err_t esp_midi_io_cb(const uint8_t *inbuf, uint16_t inlen,
                                uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    (void)outbuf;
    (void)outlen;
    (void)priv_data;

    if (!att_status) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!inbuf && inlen) {
        *att_status = ESP_IOT_ATT_INVALID_PDU;
        return ESP_ERR_INVALID_ARG;
    }

    if (inlen > BLE_MIDI_MAX_PACKET_SIZE) {
        *att_status = ESP_IOT_ATT_INVALID_PDU;
        return ESP_ERR_INVALID_SIZE;
    }

    if (inbuf && inlen) {
        esp_ble_midi_on_bep_received(inbuf, inlen);
    }

    *att_status = ESP_IOT_ATT_SUCCESS;
    return ESP_OK;
}

static const esp_ble_conn_character_t nu_lookup_table[] = {
    {"midi_io", BLE_CONN_UUID_TYPE_128, (BLE_CONN_GATT_CHR_WRITE_NO_RSP | BLE_CONN_GATT_CHR_NOTIFY), { .uuid128 = BLE_MIDI_CHAR_UUID128 }, esp_midi_io_cb},
};

static const esp_ble_conn_svc_t svc = {
    .type = BLE_CONN_UUID_TYPE_128,
    .uuid = { .uuid128 = BLE_MIDI_SERVICE_UUID128 },
    .nu_lookup_count = sizeof(nu_lookup_table) / sizeof(nu_lookup_table[0]),
    /* Cast is safe: esp_ble_conn_add_svc() copies the data and does not modify the original */
    // NOLINT
    .nu_lookup = (esp_ble_conn_character_t *)nu_lookup_table
};

esp_err_t esp_ble_midi_svc_init(void)
{
    return esp_ble_conn_add_svc(&svc);
}

esp_err_t esp_ble_midi_svc_deinit(void)
{
    return esp_ble_conn_remove_svc(&svc);
}
