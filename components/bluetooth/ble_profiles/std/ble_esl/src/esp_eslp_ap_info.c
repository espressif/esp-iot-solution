/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief ESL AP read of Tag information characteristics
 */

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"

#include "esp_ble_conn_mgr.h"
#include "esp_eslp_ap.h"

#ifndef BLE_DIS_CHR_UUID16_PNP_ID
#define BLE_DIS_CHR_UUID16_PNP_ID   0x2A50
#endif

static const char *TAG = "ble_eslp_ap_info";

static void ap_info_post(const esp_ble_eslp_ap_tag_info_t *info)
{
    esp_err_t ret = esp_event_post(BLE_ESLP_AP_EVENTS, BLE_ESLP_AP_EVENT_TAG_INFO,
                                   info, sizeof(*info), portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "post TAG_INFO failed: %s", esp_err_to_name(ret));
    }
}

/**
 * Read into a caller buffer. Oversized values truncate to @p buf_cap.
 */
static esp_err_t ap_read_uuid16(uint16_t conn_handle, uint16_t uuid16,
                                uint8_t *buf, uint16_t buf_cap, uint16_t *out_len)
{
    if (!buf || buf_cap == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_ble_conn_data_t rd = {0};
    rd.type = BLE_CONN_UUID_TYPE_16;
    rd.uuid.uuid16 = uuid16;
    rd.data = buf;
    rd.data_len = buf_cap;
    esp_err_t ret = esp_ble_conn_read_by_handle(conn_handle, &rd);
    if (ret == ESP_OK && out_len) {
        *out_len = rd.data_len;
    }
    return ret;
}

esp_err_t esp_ble_eslp_ap_read_info(uint16_t conn_handle, esp_ble_eslp_ap_tag_info_t *out_info)
{
    if (conn_handle == BLE_CONN_HANDLE_INVALID) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_ble_eslp_ap_tag_info_t info = {0};
    info.conn_handle = conn_handle;
    info.status = ESP_ERR_NOT_FOUND;
    bool any = false;
    uint16_t n = 0;

    /* Display Information (optional) */
    uint8_t display_buf[BLE_ESLP_AP_MAX_DISPLAYS * sizeof(esp_ble_esl_display_info_t)];
    if (ap_read_uuid16(conn_handle, BLE_ESL_CHR_UUID16_DISPLAY_INFO,
                       display_buf, sizeof(display_buf), &n) == ESP_OK &&
            n >= sizeof(esp_ble_esl_display_info_t)) {
        uint8_t count = (uint8_t)(n / sizeof(esp_ble_esl_display_info_t));
        if (count > BLE_ESLP_AP_MAX_DISPLAYS) {
            count = BLE_ESLP_AP_MAX_DISPLAYS;
        }
        memcpy(info.displays, display_buf, count * sizeof(esp_ble_esl_display_info_t));
        info.num_displays = count;
        info.has_display_info = true;
        any = true;
    }

    /* Image Information (optional) */
    uint8_t image_buf[sizeof(esp_ble_esl_image_info_t)];
    if (ap_read_uuid16(conn_handle, BLE_ESL_CHR_UUID16_IMAGE_INFO,
                       image_buf, sizeof(image_buf), &n) == ESP_OK &&
            n >= sizeof(esp_ble_esl_image_info_t)) {
        memcpy(&info.image_info, image_buf, sizeof(esp_ble_esl_image_info_t));
        info.has_image_info = true;
        any = true;
    }

    /* Sensor Information (optional) */
    uint8_t sensor_buf[BLE_ESLP_AP_SENSOR_INFO_MAX_LEN];
    if (ap_read_uuid16(conn_handle, BLE_ESL_CHR_UUID16_SENSOR_INFO,
                       sensor_buf, sizeof(sensor_buf), &n) == ESP_OK && n > 0) {
        memcpy(info.sensor_info, sensor_buf, n);
        info.sensor_info_len = n;
        info.has_sensor_info = true;
        any = true;
    }

    /* LED Information (optional) */
    uint8_t led_buf[BLE_ESLP_AP_MAX_LEDS];
    if (ap_read_uuid16(conn_handle, BLE_ESL_CHR_UUID16_LED_INFO,
                       led_buf, sizeof(led_buf), &n) == ESP_OK && n > 0) {
        uint8_t count = (uint8_t)n;
        if (count > BLE_ESLP_AP_MAX_LEDS) {
            count = BLE_ESLP_AP_MAX_LEDS;
        }
        memcpy(info.leds, led_buf, count);
        info.num_leds = count;
        info.has_led_info = true;
        any = true;
    }

    /* Optional DIS PnP ID */
    uint8_t pnp_buf[sizeof(info.pnp_id)];
    if (ap_read_uuid16(conn_handle, BLE_DIS_CHR_UUID16_PNP_ID,
                       pnp_buf, sizeof(pnp_buf), &n) == ESP_OK &&
            n >= sizeof(info.pnp_id)) {
        memcpy(info.pnp_id, pnp_buf, sizeof(info.pnp_id));
        info.has_pnp_id = true;
        any = true;
    }

    info.status = any ? ESP_OK : ESP_ERR_NOT_FOUND;
    if (out_info) {
        *out_info = info;
    }
    ap_info_post(&info);
    return info.status;
}
