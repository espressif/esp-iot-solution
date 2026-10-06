/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief ESL AP ECP / PAwR command helpers and 30 s ECP timeout
 */

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"

#include "esp_ble_conn_mgr.h"
#include "esp_eslp_ap.h"
#include "esp_eslp_ap_lifecycle_priv.h"
#include "esp_eslp_ap_cmd_priv.h"

static const char *TAG = "ble_eslp_ap_cmd";

#define ESLP_ECP_TIMEOUT_US     ((uint64_t)BLE_ESL_ECP_PROCEDURE_TIMEOUT_S * 1000000ULL)

#define LED_CTRL_COLOR_BRIGHTNESS(r, g, b, bright) \
    (uint8_t)(((r) & 0x03) | (((g) & 0x03) << 2) | (((b) & 0x03) << 4) | (((bright) & 0x03) << 6))

#define LED_CTRL_REPEAT(type, duration) \
    (uint16_t)(((uint16_t)((type) & 0x01)) | (((uint16_t)((duration) & 0x7FFF)) << 1))

typedef struct {
    bool pending;
    bool blocked; /* After timeout, no new ECP until reconnect */
    uint16_t conn_handle;
    uint8_t esl_id;
    uint8_t group_id;
    esp_timer_handle_t timer;
} eslp_ap_ecp_pending_t;

#ifndef CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS
#define CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS 8
#endif

static bool s_inited;
static eslp_ap_ecp_pending_t s_ecp[CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS];

static void ap_cmd_post(esp_ble_eslp_ap_event_t id, const void *data, size_t len)
{
    esp_err_t ret = esp_event_post(BLE_ESLP_AP_EVENTS, id, data, len, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "post event %d failed: %s", (int)id, esp_err_to_name(ret));
    }
}

static eslp_ap_ecp_pending_t *ecp_find_slot(uint16_t conn_handle)
{
    for (int i = 0; i < CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS; i++) {
        if (s_ecp[i].conn_handle == conn_handle &&
                (s_ecp[i].pending || s_ecp[i].blocked)) {
            return &s_ecp[i];
        }
    }
    return NULL;
}

static eslp_ap_ecp_pending_t *ecp_find_by_conn(uint16_t conn_handle)
{
    for (int i = 0; i < CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS; i++) {
        if (s_ecp[i].pending && s_ecp[i].conn_handle == conn_handle) {
            return &s_ecp[i];
        }
    }
    return NULL;
}

static eslp_ap_ecp_pending_t *ecp_alloc(void)
{
    for (int i = 0; i < CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS; i++) {
        if (!s_ecp[i].pending && !s_ecp[i].blocked) {
            return &s_ecp[i];
        }
    }
    return NULL;
}

static void ecp_clear(eslp_ap_ecp_pending_t *slot)
{
    if (!slot) {
        return;
    }
    if (slot->timer) {
        (void)esp_timer_stop(slot->timer);
        (void)esp_timer_delete(slot->timer);
        slot->timer = NULL;
    }
    slot->pending = false;
    slot->blocked = false;
    slot->conn_handle = BLE_CONN_HANDLE_INVALID;
}

static void ecp_timeout_cb(void *arg)
{
    eslp_ap_ecp_pending_t *slot = arg;
    if (!slot || !slot->pending) {
        return;
    }
    esp_ble_eslp_ap_ecp_timeout_t ev = {
        .conn_handle = slot->conn_handle,
        .esl_id = slot->esl_id,
        .group_id = slot->group_id,
    };
    ESP_LOGW(TAG, "ECP timeout conn=%u esl=0x%02x group=0x%02x",
             (unsigned)ev.conn_handle, ev.esl_id, ev.group_id);
    if (slot->timer) {
        (void)esp_timer_stop(slot->timer);
        (void)esp_timer_delete(slot->timer);
        slot->timer = NULL;
    }
    slot->pending = false;
    /* Block further ECP writes on this link after timeout. */
    slot->blocked = true;
    ap_cmd_post(BLE_ESLP_AP_EVENT_ECP_TIMEOUT, &ev, sizeof(ev));
}

static esp_err_t ecp_start_timer(eslp_ap_ecp_pending_t *slot)
{
    const esp_timer_create_args_t args = {
        .callback = ecp_timeout_cb,
        .arg = slot,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "eslp_ecp",
    };
    esp_err_t ret = esp_timer_create(&args, &slot->timer);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_timer_start_once(slot->timer, ESLP_ECP_TIMEOUT_US);
    if (ret != ESP_OK) {
        (void)esp_timer_delete(slot->timer);
        slot->timer = NULL;
    }
    return ret;
}

esp_err_t eslp_ap_cmd_init(void)
{
    if (s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    memset(s_ecp, 0, sizeof(s_ecp));
    for (int i = 0; i < CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS; i++) {
        s_ecp[i].conn_handle = BLE_CONN_HANDLE_INVALID;
    }
    s_inited = true;
    return ESP_OK;
}

esp_err_t eslp_ap_cmd_deinit(void)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    for (int i = 0; i < CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS; i++) {
        ecp_clear(&s_ecp[i]);
    }
    s_inited = false;
    return ESP_OK;
}

void eslp_ap_cmd_on_ecp_response(uint16_t conn_handle)
{
    eslp_ap_ecp_pending_t *slot = ecp_find_by_conn(conn_handle);
    if (slot) {
        ecp_clear(slot);
    }
}

void eslp_ap_cmd_on_disconnected(uint16_t conn_handle)
{
    eslp_ap_ecp_pending_t *slot = ecp_find_slot(conn_handle);
    if (slot) {
        ecp_clear(slot);
    }
}

esp_err_t eslp_ap_cmd_write_ecp(uint16_t conn_handle, const uint8_t *tlv, uint8_t tlv_len,
                                uint8_t esl_id, uint8_t group_id)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!tlv || tlv_len == 0 || tlv_len > BLE_ESL_ECP_MAX_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }
    eslp_ap_ecp_pending_t *existing = ecp_find_slot(conn_handle);
    if (existing && existing->blocked) {
        ESP_LOGE(TAG, "ECP blocked after timeout until reconnect conn=%u", (unsigned)conn_handle);
        return ESP_ERR_INVALID_STATE;
    }
    if (ecp_find_by_conn(conn_handle)) {
        ESP_LOGE(TAG, "ECP already pending on conn=%u", (unsigned)conn_handle);
        return ESP_ERR_INVALID_STATE;
    }

    /* Factory Reset / Update Complete have no Notification response. */
    bool expect_response = (tlv[0] != BLE_ESL_CMD_FACTORY_RESET &&
                            tlv[0] != BLE_ESL_CMD_UPDATE_COMPLETE);

    eslp_ap_ecp_pending_t *slot = NULL;
    if (expect_response) {
        slot = ecp_alloc();
        if (!slot) {
            return ESP_ERR_NO_MEM;
        }
        /* Arm pending and timer before GATT write. */
        slot->pending = true;
        slot->blocked = false;
        slot->conn_handle = conn_handle;
        slot->esl_id = esl_id;
        slot->group_id = group_id;
        slot->timer = NULL;
        esp_err_t ert = ecp_start_timer(slot);
        if (ert != ESP_OK) {
            ESP_LOGW(TAG, "ECP timer start failed: %s", esp_err_to_name(ert));
            /* Keep pending so a response can still clear the slot. */
        }
    }

    esp_ble_conn_data_t buff = {
        .type = BLE_CONN_UUID_TYPE_16,
        .uuid = { .uuid16 = BLE_ESL_CHR_UUID16_ECP },
        .data = (uint8_t *)tlv,
        .data_len = tlv_len,
    };
    esp_err_t ret = esp_ble_conn_write_by_handle(conn_handle, &buff);
    if (ret != ESP_OK) {
        if (slot) {
            ecp_clear(slot);
        }
        return ret;
    }
    return ESP_OK;
}

static esp_err_t encode_tlv(uint8_t opcode, const uint8_t *params, uint8_t params_len,
                            uint8_t *out, uint8_t *out_len)
{
    if (!params || !out || !out_len) {
        return ESP_ERR_INVALID_ARG;
    }
    if (params_len != BLE_ESL_TLV_PARAMS_LEN(opcode)) {
        ESP_LOGE(TAG, "params_len %u mismatch opcode 0x%02x (expect %u)",
                 params_len, opcode, (unsigned)BLE_ESL_TLV_PARAMS_LEN(opcode));
        return ESP_ERR_INVALID_ARG;
    }
    out[0] = opcode;
    memcpy(&out[1], params, params_len);
    *out_len = (uint8_t)(1 + params_len);
    return ESP_OK;
}

static esp_err_t build_led_params(uint8_t esl_id, uint8_t led_index,
                                  const esp_ble_eslp_ap_led_settings_t *settings,
                                  uint8_t *out, uint8_t *out_len)
{
    if (!settings || !out || !out_len) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t pos = 0;
    out[pos++] = esl_id;
    out[pos++] = led_index;
    out[pos++] = LED_CTRL_COLOR_BRIGHTNESS(settings->color_red, settings->color_green,
                                           settings->color_blue, settings->brightness);
    memcpy(&out[pos], settings->flashing_pattern, 7);
    pos += 7;
    uint16_t repeat = LED_CTRL_REPEAT(settings->repeat_type, settings->repeats_duration);
    out[pos++] = (uint8_t)(repeat & 0xFF);
    out[pos++] = (uint8_t)((repeat >> 8) & 0xFF);
    *out_len = pos;
    return ESP_OK;
}

/**
 * Tag replies in the response slot of the last individually addressed TLV.
 * Broadcast-only payloads need no listen slots.
 */
static void pawr_rsp_listen_slots(const uint8_t *tlv, uint8_t tlv_len,
                                  uint8_t *out_start, uint8_t *out_count)
{
    uint8_t offset = 0;
    uint8_t idx = 0;
    uint8_t last_n = 0;

    while (offset < tlv_len) {
        uint8_t opcode = tlv[offset];
        uint8_t total = (uint8_t)BLE_ESL_TLV_TOTAL_LEN(opcode);
        if (total < 2 || (uint16_t)(offset + total) > tlv_len) {
            break;
        }
        idx++;
        if (tlv[offset + 1] != BLE_ESL_BROADCAST_ADDRESS) {
            last_n = idx;
        }
        offset = (uint8_t)(offset + total);
    }

    if (last_n == 0) {
        *out_start = 0;
        *out_count = 0;
    } else {
        *out_start = (uint8_t)(last_n - 1);
        *out_count = 1;
    }
}

static esp_err_t dispatch_command(uint8_t esl_id, uint8_t group_id, uint8_t opcode,
                                  const uint8_t *params, uint8_t params_len, bool ecp_only)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if ((group_id & 0x7F) != group_id) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t tlv[BLE_ESL_ECP_MAX_SIZE];
    uint8_t tlv_len = 0;
    esp_err_t ret = encode_tlv(opcode, params, params_len, tlv, &tlv_len);
    if (ret != ESP_OK) {
        return ret;
    }

    if (esl_id == BLE_ESL_BROADCAST_ADDRESS) {
        if (ecp_only) {
            return ESP_ERR_INVALID_ARG;
        }
        uint8_t slot = 0;
        uint8_t count = 0;
        pawr_rsp_listen_slots(tlv, tlv_len, &slot, &count);
        return esp_ble_eslp_ap_queue_pawr_command(group_id, tlv, tlv_len, slot, count);
    }

    esp_ble_esl_address_t addr = {
        .esl_id = esl_id,
        .group_id_rfu = (uint8_t)(group_id & 0x7F),
    };

    esp_ble_eslp_state_t state = BLE_ESLP_STATE_UNASSOCIATED;
    (void)esp_ble_eslp_ap_get_tag_state(&addr, &state);

    uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;
    bool have_conn = (eslp_ap_lifecycle_get_conn(&addr, &conn_handle) == ESP_OK);

    if (ecp_only) {
        if (!have_conn) {
            return ESP_ERR_INVALID_STATE;
        }
        return eslp_ap_cmd_write_ecp(conn_handle, tlv, tlv_len, esl_id, group_id);
    }

    switch (state) {
    case BLE_ESLP_STATE_CONFIGURING:
    case BLE_ESLP_STATE_UPDATING:
        if (!have_conn) {
            return ESP_ERR_INVALID_STATE;
        }
        return eslp_ap_cmd_write_ecp(conn_handle, tlv, tlv_len, esl_id, group_id);

    case BLE_ESLP_STATE_SYNCHRONIZED: {
        uint8_t slot = 0;
        uint8_t count = 0;
        pawr_rsp_listen_slots(tlv, tlv_len, &slot, &count);
        return esp_ble_eslp_ap_queue_pawr_command(group_id, tlv, tlv_len, slot, count);
    }

    case BLE_ESLP_STATE_UNSYNCHRONIZED:
        if (have_conn) {
            return eslp_ap_cmd_write_ecp(conn_handle, tlv, tlv_len, esl_id, group_id);
        }
        return ESP_ERR_INVALID_STATE;

    default:
        return ESP_ERR_INVALID_STATE;
    }
}

esp_err_t esp_ble_eslp_ap_ping(uint8_t esl_id, uint8_t group_id)
{
    uint8_t params[1] = { esl_id };
    return dispatch_command(esl_id, group_id, BLE_ESL_CMD_PING, params, sizeof(params), false);
}

esp_err_t esp_ble_eslp_ap_unassociate(uint8_t esl_id, uint8_t group_id)
{
    if (esl_id == BLE_ESL_BROADCAST_ADDRESS) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t params[1] = { esl_id };
    return dispatch_command(esl_id, group_id, BLE_ESL_CMD_UNASSOCIATE, params, sizeof(params), true);
}

esp_err_t esp_ble_eslp_ap_service_reset(uint8_t esl_id, uint8_t group_id)
{
    uint8_t params[1] = { esl_id };
    return dispatch_command(esl_id, group_id, BLE_ESL_CMD_SERVICE_RESET, params, sizeof(params), false);
}

esp_err_t esp_ble_eslp_ap_factory_reset(uint8_t esl_id, uint8_t group_id)
{
    if (esl_id == BLE_ESL_BROADCAST_ADDRESS) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t params[1] = { esl_id };
    return dispatch_command(esl_id, group_id, BLE_ESL_CMD_FACTORY_RESET, params, sizeof(params), true);
}

esp_err_t esp_ble_eslp_ap_update_complete(uint8_t esl_id, uint8_t group_id)
{
    if (esl_id == BLE_ESL_BROADCAST_ADDRESS) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t params[1] = { esl_id };
    return dispatch_command(esl_id, group_id, BLE_ESL_CMD_UPDATE_COMPLETE, params, sizeof(params), true);
}

esp_err_t esp_ble_eslp_ap_read_sensor(uint8_t esl_id, uint8_t group_id, uint8_t sensor_index)
{
    uint8_t params[2] = { esl_id, sensor_index };
    return dispatch_command(esl_id, group_id, BLE_ESL_CMD_READ_SENSOR, params, sizeof(params), false);
}

esp_err_t esp_ble_eslp_ap_refresh_display(uint8_t esl_id, uint8_t group_id, uint8_t display_index)
{
    uint8_t params[2] = { esl_id, display_index };
    return dispatch_command(esl_id, group_id, BLE_ESL_CMD_REFRESH_DISPLAY, params, sizeof(params), false);
}

esp_err_t esp_ble_eslp_ap_display_image(uint8_t esl_id, uint8_t group_id,
                                        uint8_t display_index, uint8_t image_index)
{
    uint8_t params[3] = { esl_id, display_index, image_index };
    return dispatch_command(esl_id, group_id, BLE_ESL_CMD_DISPLAY_IMAGE, params, sizeof(params), false);
}

esp_err_t esp_ble_eslp_ap_display_timed_image(uint8_t esl_id, uint8_t group_id,
                                              uint8_t display_index, uint8_t image_index,
                                              uint32_t absolute_time)
{
    uint8_t params[7] = {
        esl_id,
        display_index,
        image_index,
        (uint8_t)(absolute_time & 0xFF),
        (uint8_t)((absolute_time >> 8) & 0xFF),
        (uint8_t)((absolute_time >> 16) & 0xFF),
        (uint8_t)((absolute_time >> 24) & 0xFF),
    };
    return dispatch_command(esl_id, group_id, BLE_ESL_CMD_DISPLAY_TIMED_IMAGE,
                            params, sizeof(params), false);
}

esp_err_t esp_ble_eslp_ap_led_control(uint8_t esl_id, uint8_t group_id, uint8_t led_index,
                                      const esp_ble_eslp_ap_led_settings_t *settings)
{
    uint8_t params[12];
    uint8_t params_len = 0;
    esp_err_t ret = build_led_params(esl_id, led_index, settings, params, &params_len);
    if (ret != ESP_OK) {
        return ret;
    }
    return dispatch_command(esl_id, group_id, BLE_ESL_CMD_LED_CONTROL, params, params_len, false);
}

esp_err_t esp_ble_eslp_ap_led_timed_control(uint8_t esl_id, uint8_t group_id, uint8_t led_index,
                                            const esp_ble_eslp_ap_led_settings_t *settings,
                                            uint32_t absolute_time)
{
    uint8_t params[16];
    uint8_t base_len = 0;
    esp_err_t ret = build_led_params(esl_id, led_index, settings, params, &base_len);
    if (ret != ESP_OK) {
        return ret;
    }
    params[base_len++] = (uint8_t)(absolute_time & 0xFF);
    params[base_len++] = (uint8_t)((absolute_time >> 8) & 0xFF);
    params[base_len++] = (uint8_t)((absolute_time >> 16) & 0xFF);
    params[base_len++] = (uint8_t)((absolute_time >> 24) & 0xFF);
    return dispatch_command(esl_id, group_id, BLE_ESL_CMD_LED_TIMED_CONTROL,
                            params, base_len, false);
}
