/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bthome_v2.h"
#include "stdio.h"
#include "string.h"
#include "mbedtls/ccm.h"
#include "esp_check.h"

static const char *TAG = "bthome";
static const char *BTHOME_COUNTER_KEY = "counter";

/* BTHome constants */
#define BTHOME_NONCE_LEN    13
#define BTHOME_MAC_LEN      6
#define BTHOME_COUNTER_LEN  4
#define BTHOME_UUID_LEN     2
const static uint16_t BTHOME_UUID = 0xfcd2;

/* Stream operation macros */
#define UINT8_TO_STREAM(p, u8)    {*(p)++ = (uint8_t)(u8);}
#define UINT16_TO_STREAM(p, u16)  {*(p)++ = (uint8_t)(u16); *(p)++ = (uint8_t)((u16) >> 8);}
#define ARRAY_TO_STREAM(p, a, len) {int ijk; for (ijk = 0; ijk < len; ijk++) *(p)++ = (uint8_t) a[ijk];}

/**
 * @brief Object information structure
 */
typedef struct {
    uint8_t object_id;      /* Object ID */
    uint8_t data_length;    /* Data length */
} object_info_t;

/**
 * @brief BTHome object structure
 */
typedef struct bthome_t {
    uint8_t key[16];                /* Encryption key */
    uint8_t local_mac[6];           /* Local MAC address */
    uint8_t peer_mac[6];            /* Peer MAC address */
    uint32_t counter;               /* Counter for encryption */
    bthome_callbacks_t callbacks;  /* Callback functions */
    mbedtls_ccm_context aes_ctx;    /* AES context for encryption */
} bthome_t;

/**
 * @brief Object length mapping table
 *
 * This table maps object IDs to their corresponding data lengths
 */
static const object_info_t object_length[] = {
    {0x51, 2},  /* Acceleration */
    {0x01, 1},  /* Battery */
    {0x12, 2},  /* CO2 */
    {0x09, 1},  /* Count */
    {0x3D, 2},  /* Count2 */
    {0x3E, 4},  /* Count4 */
    {0x43, 2},  /* Current */
    {0x08, 2},  /* Dewpoint */
    {0x40, 2},  /* Distance */
    {0x41, 2},  /* DistanceM */
    {0x42, 3},  /* Duration */
    {0x4D, 4},  /* Energy4 */
    {0x0A, 3},  /* Energy */
    {0x4B, 3},  /* Gas */
    {0x4C, 4},  /* Gas4 */
    {0x52, 2},  /* Gyroscope */
    {0x03, 2},  /* Humidity Precise */
    {0x2E, 1},  /* Humidity */
    {0x05, 3},  /* Illuminance */
    {0x06, 2},  /* Mass */
    {0x07, 2},  /* MassLB */
    {0x14, 2},  /* Moisture Precise */
    {0x2F, 1},  /* Moisture */
    {0x0D, 2},  /* PM2.5 */
    {0x0E, 2},  /* PM10 */
    {0x0B, 3},  /* Power */
    {0x04, 3},  /* Pressure */
    {0x54, 0},  /* Raw */
    {0x3F, 2},  /* Rotation */
    {0x44, 2},  /* Speed */
    {0x45, 2},  /* Temperature */
    {0x02, 2},  /* Temperature Precise */
    {0x53, 0},  /* Text */
    {0x50, 6},  /* Timestamp */
    {0x13, 2},  /* TVOC */
    {0x0C, 2},  /* Voltage */
    {0x4A, 2},  /* Voltage1 */
    {0x4E, 4},  /* Volume */
    {0x47, 2},  /* Volume1 */
    {0x48, 2},  /* Volume2 */
    {0x55, 4},  /* Volume Storage */
    {0x49, 2},  /* VolumeFR */
    {0x46, 1},  /* UV */
    {0x4F, 4},  /* Water */
};

static uint8_t get_data_length(uint8_t object_id)
{
    size_t array_size = sizeof(object_length) / sizeof(object_length[0]);
    for (size_t i = 0; i < array_size; ++i) {
        if (object_length[i].object_id == object_id) {
            return object_length[i].data_length;
        }
    }
    return 0;
}

static bthome_reports_t *bthome_parse_payload(uint8_t *buffer, uint8_t len)
{
    bthome_reports_t* reports = calloc(1, sizeof(bthome_reports_t));
    if (reports == NULL) {
        ESP_LOGE(TAG, "calloc bthome_reports_t failed");
        return NULL;
    }
    uint16_t num_report = 0;
    int i = 0;
    while (i < len) {

        if (reports->num_reports >= BTHOME_REPORTS_MAX) {
            ESP_LOGE(TAG, "bthome_reports_t overflow");
            bthome_free_reports(reports);
            return NULL;
        }

        if ((buffer[i] >= BTHOME_BIN_SENSOR_ID_GENERIC && buffer[i] <= BTHOME_BIN_SENSOR_ID_OPENING) ||
                (buffer[i] >= BTHOME_BIN_SENSOR_ID_BATTERY && buffer[i] <= BTHOME_BIN_SENSOR_ID_WINDOW)) {
            ESP_LOGD(TAG, "bin_sensor id %d val %d\n", buffer[i], buffer[i + 1]);
            reports->report[num_report].id = buffer[i];
            reports->report[num_report].len = 1;
            reports->report[num_report].data = calloc(1, sizeof(uint8_t) * reports->report[num_report].len);
            if (reports->report[num_report].data == NULL) {
                bthome_free_reports(reports);
                return NULL;
            }
            reports->report[num_report].data[0] = buffer[i + 1];
            reports->num_reports = ++num_report;
            i = i + 2;
        } else if (buffer[i] == BTHOME_EVENT_ID_BUTTON) {
            ESP_LOGD(TAG, "event id %d val %d\n", buffer[i], buffer[i + 1]);
            reports->report[num_report].id = buffer[i];
            reports->report[num_report].len = 1;
            reports->report[num_report].data = calloc(1, sizeof(uint8_t) * reports->report[num_report].len);
            if (reports->report[num_report].data == NULL) {
                bthome_free_reports(reports);
                return NULL;
            }
            reports->report[num_report].data[0] = buffer[i + 1];
            reports->num_reports = ++num_report;
            i = i + 2;
        } else if (buffer[i] == BTHOME_EVENT_ID_DIMMER) {
            ESP_LOGD(TAG, "event id %d val %d\n", buffer[i], buffer[i + 1]);
            reports->report[num_report].id = buffer[i];
            reports->report[num_report].len = 2;
            reports->report[num_report].data = calloc(1, sizeof(uint8_t) * reports->report[num_report].len);
            if (reports->report[num_report].data == NULL) {
                bthome_free_reports(reports);
                return NULL;
            }
            reports->report[num_report].data[0] = buffer[i + 1];
            reports->report[num_report].data[1] = buffer[i + 2];
            reports->num_reports = ++num_report;
            i = i + 3;
        } else if (buffer[i] == BTHOME_SENSOR_ID_RAW || buffer[i] == BTHOME_SENSOR_ID_TEXT) {
            int len = buffer[i + 1];
            reports->report[num_report].id = buffer[i];
            reports->report[num_report].len = len;
            reports->report[num_report].data = calloc(1, sizeof(uint8_t) * reports->report[num_report].len);
            if (reports->report[num_report].data == NULL) {
                bthome_free_reports(reports);
                return NULL;
            }
            memcpy(reports->report[num_report].data, buffer + i + 2, len);
            reports->num_reports = ++num_report;
            ESP_LOGD(TAG, "sensor id %d len %d\n", buffer[i], len);
            i = i + 1 + len;
        } else if ((buffer[i] <=  BTHOME_SENSOR_ID_MOISTURE_PRECISE)
                   || (buffer[i] >= BTHOME_SENSOR_ID_COUNT2 && buffer[i] <=  BTHOME_SENSOR_ID_WATER)
                   || (buffer[i] == BTHOME_SENSOR_ID_HUMIDITY) || (buffer[i] == BTHOME_SENSOR_ID_MOISTURE)) {
            int len = get_data_length(buffer[i]);
            reports->report[num_report].id = buffer[i];
            reports->report[num_report].len = len;
            reports->report[num_report].data = calloc(1, sizeof(uint8_t) * reports->report[num_report].len);
            if (reports->report[num_report].data == NULL) {
                bthome_free_reports(reports);
                return NULL;
            }
            memcpy(reports->report[num_report].data, buffer + i + 1, len);
            ESP_LOGD(TAG, "sensor id %d len %d\n", buffer[i], len);
            reports->num_reports = ++num_report;
            i = i + 1 + len;
        } else {
            ESP_LOGW(TAG, "met unknown id 0x%x", buffer[i]);
            bthome_free_reports(reports);
            return NULL;
        }
    }
    if (reports->num_reports == 0) {
        bthome_free_reports(reports);
        return NULL;
    } else {
        return reports;
    }
}

void bthome_free_reports(bthome_reports_t *reports)
{
    if (reports == NULL) {
        return;
    }
    for (int i = 0; i < reports->num_reports; i++) {
        if (reports->report[i].data != NULL) {
            free(reports->report[i].data);
        }
    }
    free(reports);
}

int bthome_decrypt_payload(bthome_handle_t handle, uint8_t *data, uint8_t len, uint8_t *dec_data, uint8_t *dec_data_len)
{
    bthome_t *bthome = (bthome_t *)handle;
    uint8_t nonce[BTHOME_NONCE_LEN];
    uint8_t *nonce_p = nonce;
    memcpy(nonce_p, bthome->peer_mac, BTHOME_MAC_LEN);  //mac
    nonce_p += BTHOME_MAC_LEN;
    memcpy(nonce_p, data, BTHOME_UUID_LEN); //uuid
    nonce_p += BTHOME_UUID_LEN;
    memcpy(nonce_p, data + 2, 1); //device data type
    nonce_p += 1;
    memcpy(nonce_p, &data[len - 8], BTHOME_COUNTER_LEN);
    nonce_p += BTHOME_COUNTER_LEN;
    *dec_data_len = len - 11;
    uint8_t tag[4];
    memcpy(tag, data + len - 4, 4);
    return  mbedtls_ccm_auth_decrypt(&bthome->aes_ctx, *dec_data_len, nonce, BTHOME_NONCE_LEN, NULL, 0, data + 3, dec_data, tag, 4);
}

static bthome_reports_t *bthome_parse_service_data(bthome_handle_t handle, uint8_t *data, uint8_t len)
{
    bthome_t *bthome = (bthome_t *)handle;
    size_t index = 2;
    bthome_device_info_t info;
    info.all = data[index];

    ESP_LOGD(TAG, "device_info: %x\n", info.all);
    ESP_LOGD(TAG, "version: %d\n", info.bit.bthome_version);
    ESP_LOGD(TAG, "encryption_flag: %d\n", info.bit.encryption_flag);
    ESP_LOGD(TAG, "trigger_based_flag:  %d\n", info.bit.trigger_based_flag);

    if (!info.bit.encryption_flag) {
        ESP_LOG_BUFFER_HEX_LEVEL("raw payload", data + 3, len - 3, ESP_LOG_DEBUG);
        return bthome_parse_payload(data + 3, len - 3);
    } else {
        uint8_t payload_len = 0;
        uint8_t payload_dec[31];
        if (bthome_decrypt_payload(bthome, data, len, payload_dec, &payload_len) == 0) {
            ESP_LOG_BUFFER_HEX_LEVEL("payload_dec", payload_dec, payload_len, ESP_LOG_DEBUG);
            return bthome_parse_payload(payload_dec, payload_len);
        } else {
            ESP_LOGE(TAG, "decrypt failed\n");
            return NULL;
        }
    }
    return NULL;
}

bthome_reports_t *bthome_parse_adv_data(bthome_handle_t handle, uint8_t *adv, uint8_t len)
{
    bthome_t *bthome = (bthome_t *)handle;
    size_t index = 0;

    while (index < len) {
        uint8_t length = adv[index];
        if (length == 0) {
            break;
        }

        if (index + length >= len) {
            ESP_LOGW(TAG, "Invalid adv length at index 0x%x length %d\n", index, length);
            break;
        }

        uint8_t type = adv[index + 1];
        uint8_t *data = &adv[index + 2];
        size_t data_length = length - 1;
        if (type == 0x16) {
            uint16_t uuid = (data[1] << 8) | data[0];
            if (uuid == BTHOME_UUID) {
                return bthome_parse_service_data(bthome, data, data_length);
            }
        }

        index += length + 1;
    }
    return NULL;
}

uint8_t bthome_payload_add_sensor_data(uint8_t *buffer, uint8_t offset, bthome_sensor_id_t obj_id, uint8_t *data, uint8_t data_len)
{
    uint8_t* header = buffer + offset;
    UINT8_TO_STREAM(header, obj_id);
    ARRAY_TO_STREAM(header, data, data_len);
    return (offset + data_len + 1);
}

uint8_t bthome_payload_adv_add_bin_sensor_data(uint8_t *buffer, uint8_t offset, bthome_bin_sensor_id_t obj_id, uint8_t data)
{
    uint8_t* header = buffer + offset;
    UINT8_TO_STREAM(header, obj_id);
    UINT8_TO_STREAM(header, data);
    return (offset + 2);
}

uint8_t bthome_payload_adv_add_evt_data(uint8_t *buffer, uint8_t offset, bthome_event_id_t obj_id, uint8_t *evt, uint8_t evt_size)
{
    uint8_t* header = buffer + offset;
    UINT8_TO_STREAM(header, obj_id);
    ARRAY_TO_STREAM(header, evt, evt_size);
    return (offset + evt_size + 1);
}

static esp_err_t bthome_encrypt_payload(bthome_handle_t handle, const uint16_t *uuid, uint8_t *device_info, uint8_t *raw_data, uint8_t data_len, uint8_t *enc_data, uint8_t *tag)
{
    bthome_t *bthome = (bthome_t *)handle;
    uint8_t nonce[BTHOME_NONCE_LEN];
    uint8_t *nonce_p = nonce;
    memcpy(nonce_p, bthome->local_mac, BTHOME_MAC_LEN);
    nonce_p += BTHOME_MAC_LEN;
    memcpy(nonce_p, uuid, BTHOME_UUID_LEN);
    nonce_p += BTHOME_UUID_LEN;
    memcpy(nonce_p, device_info, 1);
    nonce_p += 1;
    memcpy(nonce_p, &bthome->counter, BTHOME_COUNTER_LEN);
    nonce_p += BTHOME_COUNTER_LEN;
    int ret = mbedtls_ccm_encrypt_and_tag(&bthome->aes_ctx, data_len, nonce, BTHOME_NONCE_LEN, NULL, 0, raw_data, enc_data, tag, 4);
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_ccm_encrypt_and_tag failed, ret %d", ret);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "raw_data:");
    for (int i = 0; i < data_len; i++) {
        ESP_LOGD(TAG, " %x", raw_data[i]);
    }
    ESP_LOGD(TAG, "\n");

    ESP_LOGD(TAG, "nonce:");
    for (int i = 0; i < BTHOME_NONCE_LEN; i++) {
        ESP_LOGD(TAG, " %x", nonce[i]);
    }
    ESP_LOGD(TAG, "\n");

    ESP_LOGD(TAG, "ret %d\n", ret);
    ESP_LOGD(TAG, "enc:");
    for (int i = 0; i < data_len; i++) {
        ESP_LOGD(TAG, " %x", enc_data[i]);
    }

    ESP_LOGD(TAG, "tag:");
    for (int i = 0; i < 4; i++) {
        ESP_LOGD(TAG, " %x", tag[i]);
    }
    ESP_LOGD(TAG, "\n");

    return ESP_OK;
}

uint8_t bthome_make_adv_data(bthome_handle_t handle, uint8_t *buffer, uint8_t *name, uint8_t name_len, bthome_device_info_t info, uint8_t *payload, uint8_t payload_len)
{
    bthome_t *bthome = (bthome_t *)handle;
    uint8_t adv_len = 0;
    uint8_t flag_data[] = {0x02, 0x01, 0x06};
    uint8_t bthome_uuid[2];
    bthome_uuid[0] = (uint8_t)(BTHOME_UUID);
    bthome_uuid[1] = (uint8_t)(BTHOME_UUID >> 8);
    ARRAY_TO_STREAM(buffer, flag_data, sizeof(flag_data));
    adv_len += sizeof(flag_data);

    if (name_len != 0) {
        UINT8_TO_STREAM(buffer, name_len + 1);
        UINT8_TO_STREAM(buffer, 0x09);
        ARRAY_TO_STREAM(buffer, name, name_len);
        adv_len += (name_len + 2);
    }

    if (info.bit.encryption_flag) {
        UINT8_TO_STREAM(buffer, payload_len + 12);
    }  else {
        UINT8_TO_STREAM(buffer, payload_len + 4);
    }

    UINT8_TO_STREAM(buffer, 0X16);
    ARRAY_TO_STREAM(buffer, bthome_uuid, sizeof(bthome_uuid));
    UINT8_TO_STREAM(buffer, info.all);
    if (!info.bit.encryption_flag) {
        ARRAY_TO_STREAM(buffer, payload, payload_len);
        adv_len += (payload_len + 5);
        return adv_len;
    } else {
        uint8_t payload_enc[20];
        uint8_t tag[4];
        uint8_t counter[4];
        memcpy(counter, &bthome->counter, 4);
        bthome_encrypt_payload(handle, &BTHOME_UUID, &info.all, payload, payload_len, payload_enc, tag);
        ARRAY_TO_STREAM(buffer, payload_enc, payload_len);
        adv_len += (payload_len + 5);
        ARRAY_TO_STREAM(buffer, counter, 4);
        adv_len += 4;
        ARRAY_TO_STREAM(buffer, tag, 4);
        adv_len += 4;
        bthome->counter++;
        bthome->callbacks.store(handle, BTHOME_COUNTER_KEY, (const uint8_t *)&bthome->counter, sizeof(bthome->counter));
        return adv_len;
    }
}

esp_err_t bthome_set_encrypt_key(bthome_handle_t handle, const uint8_t *key)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(key, ESP_ERR_INVALID_ARG, TAG, "key is null");
    bthome_t* bthome = (bthome_t*)handle;
    memcpy(bthome->key, key, 16);
    return  mbedtls_ccm_setkey(&bthome->aes_ctx, MBEDTLS_CIPHER_ID_AES, key, 128);
}

esp_err_t bthome_set_local_mac_addr(bthome_handle_t handle, uint8_t *mac)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(mac, ESP_ERR_INVALID_ARG, TAG, "mac is null");
    bthome_t* bthome = (bthome_t *)handle;
    memcpy(bthome->local_mac, mac, 6);
    return ESP_OK;
}

esp_err_t bthome_set_peer_mac_addr(bthome_handle_t handle, const uint8_t *mac)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(mac, ESP_ERR_INVALID_ARG, TAG, "mac is null");
    bthome_t* bthome = (bthome_t *)handle;
    memcpy(bthome->peer_mac, mac, 6);
    return ESP_OK;
}

esp_err_t bthome_register_callbacks(bthome_handle_t handle, bthome_callbacks_t *callbacks)
{
    ESP_RETURN_ON_FALSE(callbacks, ESP_ERR_INVALID_ARG, TAG, "callbacks is null");
    ESP_RETURN_ON_FALSE(callbacks->store, ESP_ERR_INVALID_ARG, TAG, "store function is null");
    ESP_RETURN_ON_FALSE(callbacks->load, ESP_ERR_INVALID_ARG, TAG, "load function is null");
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    bthome_t *bthome = (bthome_t *)handle;
    bthome->callbacks.store = callbacks->store;
    bthome->callbacks.load = callbacks->load;
    return ESP_OK;
}

esp_err_t bthome_load_params(bthome_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    bthome_t *bthome = (bthome_t *)handle;
    uint8_t counter[4];
    bthome->callbacks.load(handle, BTHOME_COUNTER_KEY, counter, sizeof(counter));
    memcpy(&bthome->counter, counter, sizeof(bthome->counter));
    ESP_LOGD(TAG, "load counter %lu\n", bthome->counter);
    return ESP_OK;
}

esp_err_t bthome_create(bthome_handle_t *handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    bthome_t *bthome = (bthome_t *)calloc(1, sizeof(bthome_t));
    if (bthome == NULL) {
        return ESP_ERR_NO_MEM;
    }
    mbedtls_ccm_init(&bthome->aes_ctx);
    *handle = (bthome_handle_t)bthome;
    return ESP_OK;
}

esp_err_t bthome_delete(bthome_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    bthome_t *bthome = (bthome_t *)handle;
    mbedtls_ccm_free(&bthome->aes_ctx);
    free(bthome);
    return ESP_OK;
}
