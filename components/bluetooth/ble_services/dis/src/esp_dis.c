/*
 * SPDX-FileCopyrightText: 2019-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief Device Information Service
 */

#include <string.h>

#include "esp_ble_conn_mgr.h"
#include "esp_dis.h"

/* Device information */
static esp_ble_dis_data_t s_ble_dis_data = {
#ifdef CONFIG_BLE_DIS_MODEL
    .model_number      = CONFIG_BLE_DIS_MODEL,
#endif
#ifdef CONFIG_BLE_DIS_SERIAL_NUMBER
    .serial_number     = CONFIG_BLE_DIS_SERIAL_NUMBER_STR,
#endif
#ifdef CONFIG_BLE_DIS_FW_REV
    .firmware_revision = CONFIG_BLE_DIS_FW_REV_STR,
#endif
#ifdef CONFIG_BLE_DIS_HW_REV
    .hardware_revision = CONFIG_BLE_DIS_HW_REV_STR,
#endif
#ifdef CONFIG_BLE_DIS_SW_REV
    .software_revision = CONFIG_BLE_DIS_SW_REV_STR,
#endif
#ifdef CONFIG_BLE_DIS_MANUF
    .manufacturer_name = CONFIG_BLE_DIS_MANUF,
#endif
#ifdef CONFIG_BLE_DIS_SYSTEM_ID
    .system_id         = CONFIG_BLE_DIS_SYSTEM_ID,
#endif
#ifdef CONFIG_BLE_DIS_PNP
    .pnp_id            = {
        .src = CONFIG_BLE_DIS_PNP_VID_SRC,
        .vid = CONFIG_BLE_DIS_PNP_VID,
        .pid = CONFIG_BLE_DIS_PNP_PID,
        .ver = CONFIG_BLE_DIS_PNP_VER
    },
#endif
};

#ifdef CONFIG_BLE_DIS_SYSTEM_ID
static esp_err_t esp_dis_system_id_cb(const uint8_t *inbuf, uint16_t inlen,
                                      uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    *outlen = strlen(s_ble_dis_data.system_id);
    *outbuf = (uint8_t *)strndup(s_ble_dis_data.system_id, *outlen);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }

    *att_status = ESP_IOT_ATT_SUCCESS;

    return ESP_OK;
}
#endif

#ifdef CONFIG_BLE_DIS_MODEL
static esp_err_t esp_dis_model_number_cb(const uint8_t *inbuf, uint16_t inlen,
                                         uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    *outlen = strlen(s_ble_dis_data.model_number);
    *outbuf = (uint8_t *)strndup(s_ble_dis_data.model_number, *outlen);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }

    *att_status = ESP_IOT_ATT_SUCCESS;

    return ESP_OK;
}
#endif

#ifdef CONFIG_BLE_DIS_SERIAL_NUMBER
static esp_err_t esp_dis_serial_number_cb(const uint8_t *inbuf, uint16_t inlen,
                                          uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    *outlen = strlen(s_ble_dis_data.serial_number);
    *outbuf = (uint8_t *)strndup(s_ble_dis_data.serial_number, *outlen);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }

    *att_status = ESP_IOT_ATT_SUCCESS;

    return ESP_OK;
}
#endif

#ifdef CONFIG_BLE_DIS_FW_REV
static esp_err_t esp_dis_firmware_revision_cb(const uint8_t *inbuf, uint16_t inlen,
                                              uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    *outlen = strlen(s_ble_dis_data.firmware_revision);
    *outbuf = (uint8_t *)strndup(s_ble_dis_data.firmware_revision, *outlen);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }

    *att_status = ESP_IOT_ATT_SUCCESS;

    return ESP_OK;
}
#endif

#ifdef CONFIG_BLE_DIS_HW_REV
static esp_err_t esp_dis_hardware_revision_cb(const uint8_t *inbuf, uint16_t inlen,
                                              uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    *outlen = strlen(s_ble_dis_data.hardware_revision);
    *outbuf = (uint8_t *)strndup(s_ble_dis_data.hardware_revision, *outlen);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }

    *att_status = ESP_IOT_ATT_SUCCESS;

    return ESP_OK;
}
#endif

#ifdef CONFIG_BLE_DIS_SW_REV
static esp_err_t esp_dis_software_revision_cb(const uint8_t *inbuf, uint16_t inlen,
                                              uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    *outlen = strlen(s_ble_dis_data.software_revision);
    *outbuf = (uint8_t *)strndup(s_ble_dis_data.software_revision, *outlen);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }

    *att_status = ESP_IOT_ATT_SUCCESS;

    return ESP_OK;
}
#endif

#ifdef CONFIG_BLE_DIS_MANUF
static esp_err_t esp_dis_manufacturer_name_chr_cb(const uint8_t *inbuf, uint16_t inlen,
                                                  uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    *outlen = strlen(s_ble_dis_data.manufacturer_name);
    *outbuf = (uint8_t *)strndup(s_ble_dis_data.manufacturer_name, *outlen);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }

    *att_status = ESP_IOT_ATT_SUCCESS;

    return ESP_OK;
}
#endif

#ifdef CONFIG_BLE_DIS_PNP
static esp_err_t esp_dis_pnp_id_chr_cb(const uint8_t *inbuf, uint16_t inlen,
                                       uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    if (inbuf || !outbuf || !outlen) {
        *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        return ESP_ERR_INVALID_ARG;
    }

    *outlen = sizeof(esp_ble_dis_pnp_t);
    *outbuf = calloc(1, *outlen);
    if (!(*outbuf)) {
        *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        return ESP_ERR_NO_MEM;
    }
    memcpy(*outbuf, &s_ble_dis_data.pnp_id, *outlen);

    *att_status = ESP_IOT_ATT_SUCCESS;

    return ESP_OK;
}
#endif

esp_err_t esp_ble_dis_get_model_number(const char **value)
{
    if (!value) {
        return ESP_ERR_INVALID_ARG;
    }

    *value = s_ble_dis_data.model_number;
    return ESP_OK;
}

esp_err_t esp_ble_dis_set_model_number(const char *value)
{
    if (!value) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(s_ble_dis_data.model_number, 0, CONFIG_BLE_DIS_STR_MAX);
    memcpy(s_ble_dis_data.model_number, value, MIN(CONFIG_BLE_DIS_STR_MAX, strlen(value)));
    return ESP_OK;
}

esp_err_t esp_ble_dis_get_serial_number(const char **value)
{
    if (!value) {
        return ESP_ERR_INVALID_ARG;
    }

    *value = s_ble_dis_data.serial_number;
    return ESP_OK;
}

esp_err_t esp_ble_dis_set_serial_number(const char *value)
{
    if (!value) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(s_ble_dis_data.serial_number, 0, CONFIG_BLE_DIS_STR_MAX);
    memcpy(s_ble_dis_data.serial_number, value, MIN(CONFIG_BLE_DIS_STR_MAX, strlen(value)));
    return ESP_OK;
}

esp_err_t esp_ble_dis_get_firmware_revision(const char **value)
{
    if (!value) {
        return ESP_ERR_INVALID_ARG;
    }

    *value = s_ble_dis_data.firmware_revision;
    return ESP_OK;
}

esp_err_t esp_ble_dis_set_firmware_revision(const char *value)
{
    if (!value) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(s_ble_dis_data.firmware_revision, 0, CONFIG_BLE_DIS_STR_MAX);
    memcpy(s_ble_dis_data.firmware_revision, value, MIN(CONFIG_BLE_DIS_STR_MAX, strlen(value)));
    return ESP_OK;
}

esp_err_t esp_ble_dis_get_hardware_revision(const char **value)
{
    if (!value) {
        return ESP_ERR_INVALID_ARG;
    }

    *value = s_ble_dis_data.hardware_revision;
    return ESP_OK;
}

esp_err_t esp_ble_dis_set_hardware_revision(const char *value)
{
    if (!value) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(s_ble_dis_data.hardware_revision, 0, CONFIG_BLE_DIS_STR_MAX);
    memcpy(s_ble_dis_data.hardware_revision, value, MIN(CONFIG_BLE_DIS_STR_MAX, strlen(value)));
    return ESP_OK;
}

esp_err_t esp_ble_dis_get_software_revision(const char **value)
{
    if (!value) {
        return ESP_ERR_INVALID_ARG;
    }

    *value = s_ble_dis_data.software_revision;
    return ESP_OK;
}

esp_err_t esp_ble_dis_set_software_revision(const char *value)
{
    if (!value) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(s_ble_dis_data.software_revision, 0, CONFIG_BLE_DIS_STR_MAX);
    memcpy(s_ble_dis_data.software_revision, value, MIN(CONFIG_BLE_DIS_STR_MAX, strlen(value)));
    return ESP_OK;
}

esp_err_t esp_ble_dis_get_manufacturer_name(const char **value)
{
    if (!value) {
        return ESP_ERR_INVALID_ARG;
    }

    *value = s_ble_dis_data.manufacturer_name;
    return ESP_OK;
}

esp_err_t esp_ble_dis_set_manufacturer_name(const char *value)
{
    if (!value) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(s_ble_dis_data.manufacturer_name, 0, CONFIG_BLE_DIS_STR_MAX);
    memcpy(s_ble_dis_data.manufacturer_name, value, MIN(CONFIG_BLE_DIS_STR_MAX, strlen(value)));
    return ESP_OK;
}

esp_err_t esp_ble_dis_get_system_id(const char **value)
{
    if (!value) {
        return ESP_ERR_INVALID_ARG;
    }

    *value = s_ble_dis_data.system_id;
    return ESP_OK;
}

esp_err_t esp_ble_dis_set_system_id(const char *value)
{
    if (!value) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(s_ble_dis_data.system_id, 0, CONFIG_BLE_DIS_STR_MAX);
    memcpy(s_ble_dis_data.system_id, value, MIN(CONFIG_BLE_DIS_STR_MAX, strlen(value)));
    return ESP_OK;
}

esp_err_t esp_ble_dis_get_pnp_id(esp_ble_dis_pnp_t *pnp_id)
{
    if (!pnp_id) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(pnp_id, &s_ble_dis_data.pnp_id, sizeof(esp_ble_dis_pnp_t));
    return ESP_OK;
}

esp_err_t esp_ble_dis_set_pnp_id(esp_ble_dis_pnp_t *pnp_id)
{
    if (!pnp_id) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&s_ble_dis_data.pnp_id, pnp_id, sizeof(esp_ble_dis_pnp_t));
    return ESP_OK;
}

static const esp_ble_conn_character_t nu_lookup_table[] = {
#ifdef CONFIG_BLE_DIS_SYSTEM_ID
    {"system_id",           BLE_CONN_UUID_TYPE_16,  BLE_CONN_GATT_CHR_READ, { BLE_DIS_CHR_UUID16_SYSTEM_ID },          esp_dis_system_id_cb},
#endif
#ifdef CONFIG_BLE_DIS_MODEL
    {"model_number",        BLE_CONN_UUID_TYPE_16,  BLE_CONN_GATT_CHR_READ, { BLE_DIS_CHR_UUID16_MODEL_NUMBER },       esp_dis_model_number_cb},
#endif
#ifdef CONFIG_BLE_DIS_SERIAL_NUMBER
    {"serial_number",       BLE_CONN_UUID_TYPE_16,  BLE_CONN_GATT_CHR_READ, { BLE_DIS_CHR_UUID16_SERIAL_NUMBER },      esp_dis_serial_number_cb},
#endif
#ifdef CONFIG_BLE_DIS_FW_REV
    {"firmware_revision",   BLE_CONN_UUID_TYPE_16,  BLE_CONN_GATT_CHR_READ, { BLE_DIS_CHR_UUID16_FIRMWARE_REVISION },  esp_dis_firmware_revision_cb},
#endif
#ifdef CONFIG_BLE_DIS_HW_REV
    {"hardware_revision",   BLE_CONN_UUID_TYPE_16,  BLE_CONN_GATT_CHR_READ, { BLE_DIS_CHR_UUID16_HARDWARE_REVISION },  esp_dis_hardware_revision_cb},
#endif
#ifdef CONFIG_BLE_DIS_SW_REV
    {"software_revision",   BLE_CONN_UUID_TYPE_16,  BLE_CONN_GATT_CHR_READ, { BLE_DIS_CHR_UUID16_SOFTWARE_REVISION },  esp_dis_software_revision_cb},
#endif
#ifdef CONFIG_BLE_DIS_MANUF
    {"manufacturer_name",   BLE_CONN_UUID_TYPE_16,  BLE_CONN_GATT_CHR_READ, { BLE_DIS_CHR_UUID16_MANUFACTURER_NAME },  esp_dis_manufacturer_name_chr_cb},
#endif
#ifdef CONFIG_BLE_DIS_PNP
    {"pnp_id",              BLE_CONN_UUID_TYPE_16,  BLE_CONN_GATT_CHR_READ, { BLE_DIS_CHR_UUID16_PNP_ID },  esp_dis_pnp_id_chr_cb},
#endif
};

static const esp_ble_conn_svc_t svc = {
    .type = BLE_CONN_UUID_TYPE_16,
    .uuid = {
        .uuid16 = BLE_DIS_UUID16,
    },
    .nu_lookup_count = sizeof(nu_lookup_table) / sizeof(nu_lookup_table[0]),
    .nu_lookup = (esp_ble_conn_character_t *)nu_lookup_table
};

esp_err_t esp_ble_dis_init(void)
{
    return esp_ble_conn_add_svc(&svc);
}
