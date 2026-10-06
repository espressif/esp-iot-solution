/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief Electronic Shelf Label Service
 */

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#include "esp_ble_conn_mgr.h"
#include "esp_esl.h"

/* Display Information and Image Information must be enabled together. */
#if defined(CONFIG_BLE_ESL_DISPLAY_INFO) ^ defined(CONFIG_BLE_ESL_IMAGE_INFO)
#error "BLE_ESL_DISPLAY_INFO and BLE_ESL_IMAGE_INFO must both be enabled or both disabled"
#endif

_Static_assert(sizeof(esp_ble_esl_address_t) == 2, "ESL Address must be 16 bits");
_Static_assert(sizeof(esp_ble_esl_key_material_t) == BLE_ESL_KEY_MATERIAL_SIZE,
               "Key Material must be 24 octets (session key + IV)");
_Static_assert(sizeof(esp_ble_esl_abs_time_t) == 4, "Absolute Time must be uint32");
_Static_assert(sizeof(esp_ble_esl_display_info_t) == BLE_ESL_DISPLAY_INFO_RECORD_SIZE,
               "Display Data Structure must be 5 octets");
_Static_assert(sizeof(esp_ble_esl_image_info_t) == 1, "Image Information must be 1 octet");

static const char *TAG = "ble_esl";

ESP_EVENT_DEFINE_BASE(BLE_ESL_EVENTS);

/* ESL Address Characteristic Value */
static esp_ble_esl_address_t s_address;
/* AP Sync Key Material Characteristic Value */
static esp_ble_esl_key_material_t s_ap_sync_key;
/* ESL Response Key Material Characteristic Value */
static esp_ble_esl_key_material_t s_resp_key;
/* ESL Current Absolute Time: base value + time of last set (for 1 ms tick) */
static esp_ble_esl_abs_time_t s_abs_time_base;
static int64_t s_abs_time_set_us;
static portMUX_TYPE s_abs_time_mux = portMUX_INITIALIZER_UNLOCKED;

#ifdef CONFIG_BLE_ESL_DISPLAY_INFO
/* ESL Display Information Characteristic Value */
static esp_ble_esl_display_info_t s_displays[CONFIG_BLE_ESL_MAX_DISPLAYS];
static uint8_t s_display_count;
#endif

#ifdef CONFIG_BLE_ESL_IMAGE_INFO
/* ESL Image Information Characteristic Value */
static esp_ble_esl_image_info_t s_image_info;
#endif

#ifdef CONFIG_BLE_ESL_SENSOR_INFO
/* ESL Sensor Information Characteristic Value */
static uint8_t s_sensor_info[CONFIG_BLE_ESL_SENSOR_INFO_MAX_LEN];
static uint16_t s_sensor_info_len;
#endif

#ifdef CONFIG_BLE_ESL_LED_INFO
/* ESL LED Information Characteristic Value */
static uint8_t s_leds[CONFIG_BLE_ESL_MAX_LEDS];
static uint8_t s_led_count;
#endif

static esp_ble_esl_abs_time_t esl_abs_time_now(void)
{
    int64_t now_us = esp_timer_get_time();
    portENTER_CRITICAL(&s_abs_time_mux);
    esp_ble_esl_abs_time_t base = s_abs_time_base;
    int64_t set_us = s_abs_time_set_us;
    portEXIT_CRITICAL(&s_abs_time_mux);
    int64_t elapsed_ms = (now_us - set_us) / 1000LL;
    return (esp_ble_esl_abs_time_t)(base + (uint32_t)elapsed_ms);
}

static void esl_abs_time_set(esp_ble_esl_abs_time_t value)
{
    int64_t now_us = esp_timer_get_time();
    portENTER_CRITICAL(&s_abs_time_mux);
    s_abs_time_base = value;
    s_abs_time_set_us = now_us;
    portEXIT_CRITICAL(&s_abs_time_mux);
}

static bool esl_address_valid(const esp_ble_esl_address_t *addr)
{
    return addr && !(addr->group_id_rfu & 0x80) &&
           addr->esl_id != BLE_ESL_BROADCAST_ADDRESS;
}

#ifdef CONFIG_BLE_ESL_DISPLAY_INFO
/* Display_Type 0x00 is reserved */
static bool esl_display_info_valid(const esp_ble_esl_display_info_t *displays, uint8_t count)
{
    if (!displays || count == 0) {
        return false;
    }
    for (uint8_t i = 0; i < count; i++) {
        if (displays[i].display_type == 0x00) {
            return false;
        }
    }
    return true;
}
#endif

#ifdef CONFIG_BLE_ESL_SENSOR_INFO
static bool esl_sensor_info_valid(const uint8_t *data, uint16_t len)
{
    uint16_t off = 0;

    if (!data || len == 0) {
        return false;
    }
    while (off < len) {
        uint8_t size = data[off];
        uint16_t need;

        if (size == BLE_ESL_SENSOR_SIZE_SHORT) {
            need = BLE_ESL_SENSOR_SHORT_LEN;
        } else if (size == BLE_ESL_SENSOR_SIZE_LONG) {
            need = BLE_ESL_SENSOR_LONG_LEN;
        } else {
            return false;
        }
        if ((uint16_t)(off + need) > len) {
            return false;
        }
        off = (uint16_t)(off + need);
    }
    return off == len;
}
#endif

#ifdef CONFIG_BLE_ESL_LED_INFO
static bool esl_led_info_valid(const uint8_t *leds, uint8_t count)
{
    if (!leds || count == 0) {
        return false;
    }
    for (uint8_t i = 0; i < count; i++) {
        uint8_t type = BLE_ESL_LED_TYPE(leds[i]);
        if (type != BLE_ESL_LED_TYPE_SRGB && type != BLE_ESL_LED_TYPE_MONOCHROME) {
            return false;
        }
    }
    return true;
}
#endif

static esp_err_t esl_alloc_out(uint8_t **outbuf, uint16_t *outlen, const void *data, uint16_t len,
                               uint8_t *att_status)
{
    if (!outbuf || !outlen) {
        if (att_status) {
            *att_status = ESP_IOT_ATT_INTERNAL_ERROR;
        }
        return ESP_ERR_INVALID_ARG;
    }

    *outbuf = calloc(1, len ? len : 1);
    if (!(*outbuf)) {
        if (att_status) {
            *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
        }
        return ESP_ERR_NO_MEM;
    }

    if (len && data) {
        memcpy(*outbuf, data, len);
    }
    *outlen = len;
    if (att_status) {
        *att_status = ESP_IOT_ATT_SUCCESS;
    }
    return ESP_OK;
}

static void esl_post_write_event(uint16_t uuid16, const void *data, size_t len)
{
    esp_err_t ret = esp_event_post(BLE_ESL_EVENTS, uuid16, data, len, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to post ESL write event uuid=0x%04x: %s", uuid16, esp_err_to_name(ret));
    }
}

esp_err_t esp_ble_esl_get_address(esp_ble_esl_address_t *out_val)
{
    if (!out_val) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(out_val, &s_address, sizeof(s_address));
    return ESP_OK;
}

esp_err_t esp_ble_esl_set_address(const esp_ble_esl_address_t *in_val)
{
    if (!esl_address_valid(in_val)) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(&s_address, in_val, sizeof(s_address));
    return ESP_OK;
}

esp_err_t esp_ble_esl_get_ap_sync_key(esp_ble_esl_key_material_t *out_val)
{
    if (!out_val) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(out_val, &s_ap_sync_key, sizeof(s_ap_sync_key));
    return ESP_OK;
}

esp_err_t esp_ble_esl_set_ap_sync_key(const esp_ble_esl_key_material_t *in_val)
{
    if (!in_val) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(&s_ap_sync_key, in_val, sizeof(s_ap_sync_key));
    return ESP_OK;
}

esp_err_t esp_ble_esl_get_resp_key(esp_ble_esl_key_material_t *out_val)
{
    if (!out_val) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(out_val, &s_resp_key, sizeof(s_resp_key));
    return ESP_OK;
}

esp_err_t esp_ble_esl_set_resp_key(const esp_ble_esl_key_material_t *in_val)
{
    if (!in_val) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(&s_resp_key, in_val, sizeof(s_resp_key));
    return ESP_OK;
}

esp_err_t esp_ble_esl_get_current_abs_time(esp_ble_esl_abs_time_t *out_val)
{
    if (!out_val) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_val = esl_abs_time_now();
    return ESP_OK;
}

esp_err_t esp_ble_esl_set_current_abs_time(const esp_ble_esl_abs_time_t *in_val)
{
    if (!in_val) {
        return ESP_ERR_INVALID_ARG;
    }
    esl_abs_time_set(*in_val);
    return ESP_OK;
}

esp_err_t esp_ble_esl_set_display_info(const esp_ble_esl_display_info_t *displays, uint8_t count)
{
#ifdef CONFIG_BLE_ESL_DISPLAY_INFO
    if (!esl_display_info_valid(displays, count) || count > CONFIG_BLE_ESL_MAX_DISPLAYS) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(s_displays, displays, count * sizeof(esp_ble_esl_display_info_t));
    s_display_count = count;
    return ESP_OK;
#else
    (void)displays;
    (void)count;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t esp_ble_esl_get_display_info(esp_ble_esl_display_info_t *displays, uint8_t max_count,
                                       uint8_t *out_count)
{
#ifdef CONFIG_BLE_ESL_DISPLAY_INFO
    if (!displays || !out_count || max_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t n = (s_display_count < max_count) ? s_display_count : max_count;
    memcpy(displays, s_displays, n * sizeof(esp_ble_esl_display_info_t));
    *out_count = n;
    return ESP_OK;
#else
    (void)displays;
    (void)max_count;
    (void)out_count;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t esp_ble_esl_set_image_info(const esp_ble_esl_image_info_t *info)
{
#ifdef CONFIG_BLE_ESL_IMAGE_INFO
    if (!info) {
        return ESP_ERR_INVALID_ARG;
    }
    s_image_info = *info;
    return ESP_OK;
#else
    (void)info;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t esp_ble_esl_get_image_info(esp_ble_esl_image_info_t *info)
{
#ifdef CONFIG_BLE_ESL_IMAGE_INFO
    if (!info) {
        return ESP_ERR_INVALID_ARG;
    }
    *info = s_image_info;
    return ESP_OK;
#else
    (void)info;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t esp_ble_esl_set_sensor_info(const uint8_t *data, uint16_t len)
{
#ifdef CONFIG_BLE_ESL_SENSOR_INFO
    if (!esl_sensor_info_valid(data, len) || len > CONFIG_BLE_ESL_SENSOR_INFO_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(s_sensor_info, data, len);
    s_sensor_info_len = len;
    return ESP_OK;
#else
    (void)data;
    (void)len;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t esp_ble_esl_get_sensor_info(uint8_t *data, uint16_t max_len, uint16_t *out_len)
{
#ifdef CONFIG_BLE_ESL_SENSOR_INFO
    if (!data || !out_len || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    uint16_t n = (s_sensor_info_len < max_len) ? s_sensor_info_len : max_len;
    memcpy(data, s_sensor_info, n);
    *out_len = n;
    return ESP_OK;
#else
    (void)data;
    (void)max_len;
    (void)out_len;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t esp_ble_esl_set_led_info(const uint8_t *leds, uint8_t count)
{
#ifdef CONFIG_BLE_ESL_LED_INFO
    if (!esl_led_info_valid(leds, count) || count > CONFIG_BLE_ESL_MAX_LEDS) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(s_leds, leds, count);
    s_led_count = count;
    return ESP_OK;
#else
    (void)leds;
    (void)count;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t esp_ble_esl_get_led_info(uint8_t *leds, uint8_t max_count, uint8_t *out_count)
{
#ifdef CONFIG_BLE_ESL_LED_INFO
    if (!leds || !out_count || max_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t n = (s_led_count < max_count) ? s_led_count : max_count;
    memcpy(leds, s_leds, n);
    *out_count = n;
    return ESP_OK;
#else
    (void)leds;
    (void)max_count;
    (void)out_count;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t esp_ble_esl_notify_ecp_response(const uint8_t *data, uint16_t len)
{
    if (!data || len < 2 || len > BLE_ESL_ECP_MAX_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }
    /* Outbound ECP Notification must be exactly one response TLV. */
    if (len != BLE_ESL_TLV_TOTAL_LEN(data[0])) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_ble_conn_data_t conn_data = {
        .type = BLE_CONN_UUID_TYPE_16,
        .uuid = {
            .uuid16 = BLE_ESL_CHR_UUID16_ECP,
        },
        .data = (uint8_t *)data,
        .data_len = len,
    };

    return esp_ble_conn_notify(&conn_data);
}

static esp_err_t esl_address_cb(const uint8_t *inbuf, uint16_t inlen,
                                uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    (void)outbuf;
    (void)outlen;
    (void)priv_data;

    if (!inbuf) {
        *att_status = ESP_IOT_ATT_READ_NOT_PERMIT;
        return ESP_ERR_INVALID_ARG;
    }
    if (inlen != sizeof(esp_ble_esl_address_t)) {
        *att_status = ESP_IOT_ATT_INVALID_ATTR_LEN;
        return ESP_ERR_INVALID_ARG;
    }
    if ((inbuf[1] & 0x80) || inbuf[0] == BLE_ESL_BROADCAST_ADDRESS) {
        *att_status = ESP_IOT_ATT_VALUE_NOT_ALLOWED;
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&s_address, inbuf, sizeof(s_address));
    esl_post_write_event(BLE_ESL_CHR_UUID16_ADDRESS, &s_address, sizeof(s_address));
    *att_status = ESP_IOT_ATT_SUCCESS;
    return ESP_OK;
}

static esp_err_t esl_ap_sync_key_cb(const uint8_t *inbuf, uint16_t inlen,
                                    uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    (void)outbuf;
    (void)outlen;
    (void)priv_data;

    if (!inbuf) {
        *att_status = ESP_IOT_ATT_READ_NOT_PERMIT;
        return ESP_ERR_INVALID_ARG;
    }
    if (inlen != sizeof(esp_ble_esl_key_material_t)) {
        *att_status = ESP_IOT_ATT_INVALID_ATTR_LEN;
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&s_ap_sync_key, inbuf, sizeof(s_ap_sync_key));
    esl_post_write_event(BLE_ESL_CHR_UUID16_AP_SYNC_KEY, &s_ap_sync_key, sizeof(s_ap_sync_key));
    *att_status = ESP_IOT_ATT_SUCCESS;
    return ESP_OK;
}

static esp_err_t esl_resp_key_cb(const uint8_t *inbuf, uint16_t inlen,
                                 uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    (void)outbuf;
    (void)outlen;
    (void)priv_data;

    if (!inbuf) {
        *att_status = ESP_IOT_ATT_READ_NOT_PERMIT;
        return ESP_ERR_INVALID_ARG;
    }
    if (inlen != sizeof(esp_ble_esl_key_material_t)) {
        *att_status = ESP_IOT_ATT_INVALID_ATTR_LEN;
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&s_resp_key, inbuf, sizeof(s_resp_key));
    esl_post_write_event(BLE_ESL_CHR_UUID16_RESP_KEY, &s_resp_key, sizeof(s_resp_key));
    *att_status = ESP_IOT_ATT_SUCCESS;
    return ESP_OK;
}

static esp_err_t esl_abs_time_cb(const uint8_t *inbuf, uint16_t inlen,
                                 uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    (void)outbuf;
    (void)outlen;
    (void)priv_data;

    if (!inbuf) {
        *att_status = ESP_IOT_ATT_READ_NOT_PERMIT;
        return ESP_ERR_INVALID_ARG;
    }
    if (inlen != sizeof(esp_ble_esl_abs_time_t)) {
        *att_status = ESP_IOT_ATT_INVALID_ATTR_LEN;
        return ESP_ERR_INVALID_ARG;
    }

    esp_ble_esl_abs_time_t value;
    memcpy(&value, inbuf, sizeof(value));
    esl_abs_time_set(value);
    esl_post_write_event(BLE_ESL_CHR_UUID16_CURRENT_ABS_TIME, &value, sizeof(value));
    *att_status = ESP_IOT_ATT_SUCCESS;
    return ESP_OK;
}

#ifdef CONFIG_BLE_ESL_DISPLAY_INFO
static esp_err_t esl_display_info_cb(const uint8_t *inbuf, uint16_t inlen,
                                     uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    (void)inlen;
    (void)priv_data;

    if (inbuf) {
        *att_status = ESP_IOT_ATT_WRITE_NOT_PERMIT;
        return ESP_ERR_INVALID_ARG;
    }
    /* Empty Display Information is not a valid read value. */
    if (s_display_count == 0) {
        *att_status = ESP_IOT_ATT_ERR_UNLIKELY;
        return ESP_ERR_INVALID_STATE;
    }

    uint16_t len = s_display_count * sizeof(esp_ble_esl_display_info_t);
    return esl_alloc_out(outbuf, outlen, s_displays, len, att_status);
}
#endif

#ifdef CONFIG_BLE_ESL_IMAGE_INFO
static esp_err_t esl_image_info_cb(const uint8_t *inbuf, uint16_t inlen,
                                   uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    (void)inlen;
    (void)priv_data;

    if (inbuf) {
        *att_status = ESP_IOT_ATT_WRITE_NOT_PERMIT;
        return ESP_ERR_INVALID_ARG;
    }
    /* Image Info requires Display Info to be configured. */
    if (s_display_count == 0) {
        *att_status = ESP_IOT_ATT_ERR_UNLIKELY;
        return ESP_ERR_INVALID_STATE;
    }

    return esl_alloc_out(outbuf, outlen, &s_image_info, sizeof(s_image_info), att_status);
}
#endif

#ifdef CONFIG_BLE_ESL_SENSOR_INFO
static esp_err_t esl_sensor_info_cb(const uint8_t *inbuf, uint16_t inlen,
                                    uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    (void)inlen;
    (void)priv_data;

    if (inbuf) {
        *att_status = ESP_IOT_ATT_WRITE_NOT_PERMIT;
        return ESP_ERR_INVALID_ARG;
    }
    /* Empty Sensor Information is not a valid read value. */
    if (s_sensor_info_len == 0) {
        *att_status = ESP_IOT_ATT_ERR_UNLIKELY;
        return ESP_ERR_INVALID_STATE;
    }

    return esl_alloc_out(outbuf, outlen, s_sensor_info, s_sensor_info_len, att_status);
}
#endif

#ifdef CONFIG_BLE_ESL_LED_INFO
static esp_err_t esl_led_info_cb(const uint8_t *inbuf, uint16_t inlen,
                                 uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    (void)inlen;
    (void)priv_data;

    if (inbuf) {
        *att_status = ESP_IOT_ATT_WRITE_NOT_PERMIT;
        return ESP_ERR_INVALID_ARG;
    }
    /* Empty LED Information is not a valid read value. */
    if (s_led_count == 0) {
        *att_status = ESP_IOT_ATT_ERR_UNLIKELY;
        return ESP_ERR_INVALID_STATE;
    }

    return esl_alloc_out(outbuf, outlen, s_leds, s_led_count, att_status);
}
#endif

static esp_err_t esl_ecp_cb(const uint8_t *inbuf, uint16_t inlen,
                            uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    (void)outbuf;
    (void)outlen;
    (void)priv_data;

    if (!inbuf) {
        *att_status = ESP_IOT_ATT_READ_NOT_PERMIT;
        return ESP_ERR_INVALID_ARG;
    }
    if (inlen == 0 || inlen > BLE_ESL_ECP_MAX_SIZE) {
        *att_status = ESP_IOT_ATT_INVALID_ATTR_LEN;
        return ESP_ERR_INVALID_ARG;
    }
    /* ECP write must be exactly one TLV */
    if (inlen != BLE_ESL_TLV_TOTAL_LEN(inbuf[0])) {
        *att_status = ESP_IOT_ATT_INVALID_ATTR_LEN;
        return ESP_ERR_INVALID_ARG;
    }

    esl_post_write_event(BLE_ESL_CHR_UUID16_ECP, inbuf, inlen);
    *att_status = ESP_IOT_ATT_SUCCESS;
    return ESP_OK;
}

/* Config chars: Write; Info chars: Read; ECP: Write / Write Without Response / Notify */
static const esp_ble_conn_character_t nu_lookup_table[] = {
    {
        "esl_address", BLE_CONN_UUID_TYPE_16,
        BLE_CONN_GATT_CHR_WRITE | BLE_CONN_GATT_CHR_WRITE_ENC,
        { BLE_ESL_CHR_UUID16_ADDRESS }, esl_address_cb
    },
    {
        "ap_sync_key", BLE_CONN_UUID_TYPE_16,
        BLE_CONN_GATT_CHR_WRITE | BLE_CONN_GATT_CHR_WRITE_ENC,
        { BLE_ESL_CHR_UUID16_AP_SYNC_KEY }, esl_ap_sync_key_cb
    },
    {
        "resp_key", BLE_CONN_UUID_TYPE_16,
        BLE_CONN_GATT_CHR_WRITE | BLE_CONN_GATT_CHR_WRITE_ENC,
        { BLE_ESL_CHR_UUID16_RESP_KEY }, esl_resp_key_cb
    },
    {
        "abs_time", BLE_CONN_UUID_TYPE_16,
        BLE_CONN_GATT_CHR_WRITE | BLE_CONN_GATT_CHR_WRITE_ENC,
        { BLE_ESL_CHR_UUID16_CURRENT_ABS_TIME }, esl_abs_time_cb
    },
#ifdef CONFIG_BLE_ESL_DISPLAY_INFO
    {
        "display_info", BLE_CONN_UUID_TYPE_16,
        BLE_CONN_GATT_CHR_READ | BLE_CONN_GATT_CHR_READ_ENC,
        { BLE_ESL_CHR_UUID16_DISPLAY_INFO }, esl_display_info_cb
    },
#endif
#ifdef CONFIG_BLE_ESL_IMAGE_INFO
    {
        "image_info", BLE_CONN_UUID_TYPE_16,
        BLE_CONN_GATT_CHR_READ | BLE_CONN_GATT_CHR_READ_ENC,
        { BLE_ESL_CHR_UUID16_IMAGE_INFO }, esl_image_info_cb
    },
#endif
#ifdef CONFIG_BLE_ESL_SENSOR_INFO
    {
        "sensor_info", BLE_CONN_UUID_TYPE_16,
        BLE_CONN_GATT_CHR_READ | BLE_CONN_GATT_CHR_READ_ENC,
        { BLE_ESL_CHR_UUID16_SENSOR_INFO }, esl_sensor_info_cb
    },
#endif
#ifdef CONFIG_BLE_ESL_LED_INFO
    {
        "led_info", BLE_CONN_UUID_TYPE_16,
        BLE_CONN_GATT_CHR_READ | BLE_CONN_GATT_CHR_READ_ENC,
        { BLE_ESL_CHR_UUID16_LED_INFO }, esl_led_info_cb
    },
#endif
    {
        "ecp", BLE_CONN_UUID_TYPE_16,
        BLE_CONN_GATT_CHR_WRITE_NO_RSP | BLE_CONN_GATT_CHR_WRITE |
        BLE_CONN_GATT_CHR_NOTIFY | BLE_CONN_GATT_CHR_WRITE_ENC,
        { BLE_ESL_CHR_UUID16_ECP }, esl_ecp_cb
    },
};

static const esp_ble_conn_svc_t svc = {
    .type = BLE_CONN_UUID_TYPE_16,
    .uuid = {
        .uuid16 = BLE_ESL_UUID16,
    },
    .nu_lookup_count = sizeof(nu_lookup_table) / sizeof(nu_lookup_table[0]),
    .nu_lookup = (esp_ble_conn_character_t *)nu_lookup_table
};

esp_err_t esp_ble_esl_init(void)
{
#ifdef CONFIG_BLE_ESL_DISPLAY_INFO
    if (s_display_count == 0) {
        ESP_LOGE(TAG, "call esp_ble_esl_set_display_info() before esp_ble_esl_init()");
        return ESP_ERR_INVALID_STATE;
    }
#endif
    return esp_ble_conn_add_svc(&svc);
}

esp_err_t esp_ble_esl_deinit(void)
{
    return esp_ble_conn_remove_svc(&svc);
}
