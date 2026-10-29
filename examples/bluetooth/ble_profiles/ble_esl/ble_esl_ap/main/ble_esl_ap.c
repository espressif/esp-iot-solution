/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * IDF-like ESL AP façade over esp_ble_eslp_ap + ble_conn_mgr.
 */

#include "ble_esl_ap.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_esl.h"
#include "host/ble_gap.h"

static const char *TAG = "ble_esl_ap_facade";

#ifndef CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS
#define CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS 8
#endif

/* ATT header (3) + ESL key material (24). Default MTU 23 cannot carry AP Sync Key. */
#define FACADE_MIN_ATT_MTU  27
#define FACADE_PREF_ATT_MTU 247
#define FACADE_PAST_RETRY_INTERVAL_US 200000ULL
#define FACADE_PAST_RETRY_MAX_ATTEMPTS 15
#define FACADE_PAST_CONFIRM_GRACE_US 1600000ULL
/* Cap one-shot requeues when esp_event_post(ticks_to_wait=0) fails (queue full). */
#define FACADE_PAST_POST_RETRY_MAX 30
#define FACADE_INTERNAL_PAST_RETRY_EVENT 0x7f000001

typedef struct {
    bool used;
    bool announced;
    bool disc_done;
    bool enc_done;
    bool mtu_done;
    uint16_t conn_handle;
    uint8_t ble_addr[6];
    uint8_t ble_addr_type;
    ble_esl_address_t esl_addr;
    bool has_esl_addr;
} facade_tag_t;

static ble_esl_ap_callback_t s_cb;
static ble_esl_ap_pawr_params_t s_pawr;
static bool s_inited;
static bool s_sync_in_progress;
static uint16_t s_sync_conn = BLE_CONN_HANDLE_INVALID;
static bool s_sync_has_addr;
static ble_esl_address_t s_sync_esl_addr;
static esp_timer_handle_t s_sync_retry_timer;
static uint8_t s_sync_retry_attempts;
static uint8_t s_sync_post_failures;
static bool s_pending_connect;
static bool s_pawr_paused_for_acl;
static uint8_t s_pending_addr[6];
static uint8_t s_pending_addr_type;
static facade_tag_t s_tags[CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS];

static void pause_pawr_for_acl_connect(void);
static void resume_pawr_after_acl_connect(void);
static esp_err_t sync_retry_schedule(uint64_t delay_us);
static void sync_wait_clear(void);
static void emit_synchronized(uint16_t conn_handle, ble_esl_address_t esl_addr, esp_err_t status);

static void sync_fail_from_timer(esp_err_t status)
{
    uint16_t conn_handle = s_sync_conn;
    ble_esl_address_t esl_addr = s_sync_esl_addr;
    sync_wait_clear();
    emit_synchronized(conn_handle, esl_addr, status);
    if (conn_handle != BLE_CONN_HANDLE_INVALID) {
        (void)ble_esl_ap_disconnect(conn_handle);
    }
}

static void sync_retry_timer_cb(void *arg)
{
    (void)arg;
    if (!s_inited || !s_sync_in_progress) {
        return;
    }

    /* ticks_to_wait=0: never block ESP_TIMER_TASK on a full event queue. */
    esp_err_t ret = esp_event_post(BLE_ESLP_AP_EVENTS, FACADE_INTERNAL_PAST_RETRY_EVENT,
                                   NULL, 0, 0);
    if (ret == ESP_OK) {
        s_sync_post_failures = 0;
        return;
    }

    ESP_LOGW(TAG, "PAST retry event post failed: %s (miss %u/%u)",
             esp_err_to_name(ret),
             (unsigned)(s_sync_post_failures + 1),
             (unsigned)FACADE_PAST_POST_RETRY_MAX);
    if (++s_sync_post_failures > FACADE_PAST_POST_RETRY_MAX) {
        ESP_LOGW(TAG, "PAST retry event post exhausted; clearing sync pipeline");
        sync_fail_from_timer(ESP_ERR_TIMEOUT);
        return;
    }
    if (sync_retry_schedule(FACADE_PAST_RETRY_INTERVAL_US) != ESP_OK) {
        ESP_LOGW(TAG, "Unable to re-arm PAST retry after event post failure");
        sync_fail_from_timer(ESP_FAIL);
    }
}

static esp_err_t sync_retry_schedule(uint64_t delay_us)
{
    if (!s_sync_in_progress) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_sync_retry_timer) {
        const esp_timer_create_args_t args = {
            .callback = sync_retry_timer_cb,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "eslp_past_retry",
        };
        esp_err_t ret = esp_timer_create(&args, &s_sync_retry_timer);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    (void)esp_timer_stop(s_sync_retry_timer);
    return esp_timer_start_once(s_sync_retry_timer, delay_us);
}

static void emit(ble_esl_ap_evt_t event, void *param)
{
    if (s_cb) {
        s_cb(event, param);
    }
}

static void sync_wait_clear(void)
{
    if (s_sync_retry_timer) {
        (void)esp_timer_stop(s_sync_retry_timer);
    }
    s_sync_retry_attempts = 0;
    s_sync_post_failures = 0;
    s_sync_in_progress = false;
    s_sync_conn = BLE_CONN_HANDLE_INVALID;
    s_sync_has_addr = false;
    memset(&s_sync_esl_addr, 0, sizeof(s_sync_esl_addr));
}

static void sync_wait_arm(uint16_t conn_handle, const facade_tag_t *tag)
{
    if (s_sync_retry_timer) {
        (void)esp_timer_stop(s_sync_retry_timer);
    }
    s_sync_retry_attempts = 0;
    s_sync_post_failures = 0;
    s_sync_in_progress = true;
    s_sync_conn = conn_handle;
    if (tag && tag->has_esl_addr) {
        s_sync_has_addr = true;
        s_sync_esl_addr = tag->esl_addr;
    } else {
        s_sync_has_addr = false;
        memset(&s_sync_esl_addr, 0, sizeof(s_sync_esl_addr));
    }
}

static bool sync_wait_matches_addr(const ble_esl_address_t *addr)
{
    return s_sync_in_progress && s_sync_has_addr && addr &&
           s_sync_esl_addr.esl_id == addr->esl_id &&
           s_sync_esl_addr.group_id_rfu == addr->group_id_rfu;
}

static void emit_synchronized(uint16_t conn_handle, ble_esl_address_t esl_addr, esp_err_t status)
{
    ble_esl_ap_synchronized_t sync = {
        .conn_handle = conn_handle,
        .esl_addr = esl_addr,
        .status = status,
    };
    emit(BLE_ESL_AP_EVT_SYNCHRONIZED, &sync);
}

static bool addr_eq(const uint8_t a[6], const uint8_t b[6])
{
    return memcmp(a, b, 6) == 0;
}

static facade_tag_t *tag_by_conn(uint16_t conn)
{
    for (int i = 0; i < CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS; i++) {
        if (s_tags[i].used && s_tags[i].conn_handle == conn) {
            return &s_tags[i];
        }
    }
    return NULL;
}

static facade_tag_t *tag_by_ble(const uint8_t addr[6], uint8_t addr_type)
{
    for (int i = 0; i < CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS; i++) {
        if (s_tags[i].used && s_tags[i].ble_addr_type == addr_type &&
                addr_eq(s_tags[i].ble_addr, addr)) {
            return &s_tags[i];
        }
    }
    return NULL;
}

static facade_tag_t *tag_by_esl(ble_esl_address_t addr)
{
    for (int i = 0; i < CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS; i++) {
        if (s_tags[i].used && s_tags[i].has_esl_addr &&
                s_tags[i].esl_addr.esl_id == addr.esl_id &&
                s_tags[i].esl_addr.group_id_rfu == addr.group_id_rfu) {
            return &s_tags[i];
        }
    }
    return NULL;
}

static facade_tag_t *tag_alloc(void)
{
    for (int i = 0; i < CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS; i++) {
        if (!s_tags[i].used) {
            memset(&s_tags[i], 0, sizeof(s_tags[i]));
            s_tags[i].conn_handle = BLE_CONN_HANDLE_INVALID;
            s_tags[i].used = true;
            return &s_tags[i];
        }
    }
    return NULL;
}

static void tag_release(facade_tag_t *tag)
{
    if (tag == NULL) {
        return;
    }
    memset(tag, 0, sizeof(*tag));
    tag->conn_handle = BLE_CONN_HANDLE_INVALID;
}

static bool adv_has_uuid16(const uint8_t *data, uint16_t len, uint16_t uuid)
{
    size_t i = 0;
    while (i + 1 < len) {
        uint8_t elen = data[i];
        if (elen == 0 || (i + 1 + elen) > len) {
            break;
        }
        uint8_t type = data[i + 1];
        if (type == 0x02 || type == 0x03) {
            for (size_t j = i + 2; j + 1 < i + 1 + elen; j += 2) {
                uint16_t u = (uint16_t)(data[j] | (data[j + 1] << 8));
                if (u == uuid) {
                    return true;
                }
            }
        }
        i += (size_t)(1 + elen);
    }
    return false;
}

static void maybe_note_mtu(facade_tag_t *tag)
{
    uint16_t mtu = 0;
    if (!tag || tag->mtu_done) {
        return;
    }
    if (esp_ble_conn_get_mtu_by_handle(tag->conn_handle, &mtu) == ESP_OK &&
            mtu >= FACADE_MIN_ATT_MTU) {
        tag->mtu_done = true;
    }
}

static void maybe_emit_connected(facade_tag_t *tag)
{
    if (!tag || tag->announced) {
        return;
    }
    maybe_note_mtu(tag);
    if (!tag->disc_done || !tag->enc_done || !tag->mtu_done) {
        ESP_LOGI(TAG, "Tag conn=%u waiting disc=%d enc=%d mtu=%d",
                 tag->conn_handle, (int)tag->disc_done, (int)tag->enc_done,
                 (int)tag->mtu_done);
        if (tag->disc_done && tag->enc_done && !tag->mtu_done) {
            (void)esp_ble_conn_mtu_update(tag->conn_handle, FACADE_PREF_ATT_MTU);
        }
        return;
    }
    ble_esl_ap_conn_info_t info = {
        .conn_handle = tag->conn_handle,
        .addr_type = tag->ble_addr_type,
        .has_display_info = true,
        .has_led_info = true,
        .has_sensor_info = true,
    };
    memcpy(info.addr, tag->ble_addr, 6);
    tag->announced = true;
    emit(BLE_ESL_AP_EVT_CONNECTED, &info);
}

static void on_conn_event(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;
    if (base != BLE_CONN_MGR_EVENTS || !s_inited) {
        return;
    }

    switch (id) {
    case ESP_BLE_CONN_EVENT_SCAN_RESULT: {
        const esp_ble_conn_scan_result_t *res = event_data;
        if (!res) {
            break;
        }
        ble_esl_ap_scan_result_t scan = {0};
        memcpy(scan.addr, res->addr, 6);
        scan.addr_type = res->addr_type;
        scan.rssi = res->rssi;
        scan.adv_data_len = res->adv_data_len;
        if (scan.adv_data_len > sizeof(scan.adv_data_storage)) {
            scan.adv_data_len = sizeof(scan.adv_data_storage);
        }
        memcpy(scan.adv_data_storage, res->adv_data, scan.adv_data_len);
        scan.adv_data = scan.adv_data_storage;
        scan.advertises_esl_service = adv_has_uuid16(scan.adv_data, scan.adv_data_len, BLE_ESL_UUID16);
        /* ESL UUID => Unassociated; flags-only / no ESL UUID => associated Unsynchronized. */
        scan.is_associated = !scan.advertises_esl_service;
        scan.data_complete = (res->ext_data_status == 0) ||
                             (res->ext_data_status == ESP_BLE_CONN_SCAN_EXT_STATUS_LEGACY);
        scan.is_connectable = true;
        if (scan.advertises_esl_service) {
            ESP_LOGI(TAG, "GAP scan ESL %02x:%02x:%02x:%02x:%02x:%02x rssi=%d len=%u",
                     scan.addr[5], scan.addr[4], scan.addr[3],
                     scan.addr[2], scan.addr[1], scan.addr[0],
                     scan.rssi, (unsigned)scan.adv_data_len);
        } else {
            static uint8_t s_no_uuid_logs;
            if (s_no_uuid_logs < 8) {
                ESP_LOGI(TAG, "GAP scan no-ESL-UUID %02x:%02x:%02x:%02x:%02x:%02x rssi=%d len=%u (not forwarded unless in inventory)",
                         scan.addr[5], scan.addr[4], scan.addr[3],
                         scan.addr[2], scan.addr[1], scan.addr[0],
                         scan.rssi, (unsigned)scan.adv_data_len);
                s_no_uuid_logs++;
            }
        }
        emit(BLE_ESL_AP_EVT_SCAN_RESULT, &scan);
        break;
    }
    case ESP_BLE_CONN_EVENT_CONNECTED: {
        const esp_ble_conn_event_data_t *ev = event_data;
        if (!ev) {
            break;
        }
        bool expect_tag = s_pending_connect &&
                          addr_eq(ev->connected.peer_addr, s_pending_addr);
        facade_tag_t *known = tag_by_ble(ev->connected.peer_addr, ev->connected.peer_addr_type);
        ESP_LOGI(TAG, "GAP connected conn=%u type=%u %02x:%02x:%02x:%02x:%02x:%02x pending=%d expect=%d known=%d",
                 ev->connected.conn_handle, ev->connected.peer_addr_type,
                 ev->connected.peer_addr[5], ev->connected.peer_addr[4],
                 ev->connected.peer_addr[3], ev->connected.peer_addr[2],
                 ev->connected.peer_addr[1], ev->connected.peer_addr[0],
                 (int)s_pending_connect, (int)expect_tag, known ? 1 : 0);
        if (!expect_tag && !known) {
            ESP_LOGI(TAG, "Ignore non-Tag connection conn=%u", ev->connected.conn_handle);
            break;
        }
        s_pending_connect = false;
        resume_pawr_after_acl_connect();
        facade_tag_t *tag = known ? known : tag_alloc();
        if (!tag) {
            ESP_LOGW(TAG, "tag table full, drop conn=%u", ev->connected.conn_handle);
            (void)esp_ble_conn_disconnect_by_handle(ev->connected.conn_handle);
            break;
        }
        tag->conn_handle = ev->connected.conn_handle;
        memcpy(tag->ble_addr, ev->connected.peer_addr, 6);
        tag->ble_addr_type = ev->connected.peer_addr_type;
        tag->announced = false;
        tag->disc_done = false;
        tag->enc_done = false;
        tag->mtu_done = false;
        (void)esp_ble_conn_mtu_update(tag->conn_handle, FACADE_PREF_ATT_MTU);
        break;
    }
    case ESP_BLE_CONN_EVENT_CONNECT_FAILED: {
        const esp_ble_conn_event_data_t *ev = event_data;
        if (!ev) {
            break;
        }
        bool expect_tag = s_pending_connect &&
                          s_pending_addr_type == ev->connect_failed.peer_addr_type &&
                          addr_eq(ev->connect_failed.peer_addr, s_pending_addr);
        facade_tag_t *known = tag_by_ble(ev->connect_failed.peer_addr,
                                         ev->connect_failed.peer_addr_type);
        ESP_LOGW(TAG, "GAP connect failed status=%d type=%u %02x:%02x:%02x:%02x:%02x:%02x pending=%d expect=%d known=%d",
                 ev->connect_failed.status, ev->connect_failed.peer_addr_type,
                 ev->connect_failed.peer_addr[5], ev->connect_failed.peer_addr[4],
                 ev->connect_failed.peer_addr[3], ev->connect_failed.peer_addr[2],
                 ev->connect_failed.peer_addr[1], ev->connect_failed.peer_addr[0],
                 (int)s_pending_connect, (int)expect_tag, known ? 1 : 0);
        if (!expect_tag && !known) {
            break;
        }
        if (expect_tag) {
            s_pending_connect = false;
            resume_pawr_after_acl_connect();
        }
        ble_esl_ap_connect_failed_t failed = {
            .status = ev->connect_failed.status,
            .addr_type = ev->connect_failed.peer_addr_type,
        };
        memcpy(failed.addr, ev->connect_failed.peer_addr, sizeof(failed.addr));
        emit(BLE_ESL_AP_EVT_CONNECT_FAILED, &failed);
        break;
    }
    case ESP_BLE_CONN_EVENT_MTU: {
        const esp_ble_conn_event_data_t *ev = event_data;
        if (!ev) {
            break;
        }
        ESP_LOGI(TAG, "MTU updated conn=%u mtu=%u",
                 ev->mtu_update.conn_handle, ev->mtu_update.mtu);
        facade_tag_t *tag = tag_by_conn(ev->mtu_update.conn_handle);
        if (!tag) {
            break;
        }
        tag->mtu_done = ev->mtu_update.mtu >= FACADE_MIN_ATT_MTU;
        maybe_emit_connected(tag);
        break;
    }
    case ESP_BLE_CONN_EVENT_DISC_COMPLETE: {
        uint16_t conn = BLE_CONN_HANDLE_INVALID;
        if (event_data) {
            conn = *(const uint16_t *)event_data;
        }
        ESP_LOGI(TAG, "GATT discovery complete conn=%u", conn);
        if (conn != BLE_CONN_HANDLE_INVALID) {
            facade_tag_t *tag = tag_by_conn(conn);
            if (tag) {
                tag->disc_done = true;
                maybe_emit_connected(tag);
                break;
            }
        }
        /* Older conn_mgr posted a NULL payload; mark unannounced Tag links. */
        for (int i = 0; i < CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS; i++) {
            if (s_tags[i].used && s_tags[i].conn_handle != BLE_CONN_HANDLE_INVALID &&
                    !s_tags[i].announced) {
                s_tags[i].disc_done = true;
                maybe_emit_connected(&s_tags[i]);
            }
        }
        break;
    }
    case ESP_BLE_CONN_EVENT_ENC_CHANGE: {
        const esp_ble_conn_event_data_t *ev = event_data;
        if (!ev || ev->enc_change.status != 0 || !ev->enc_change.encrypted) {
            break;
        }
        facade_tag_t *tag = tag_by_conn(ev->enc_change.conn_handle);
        if (!tag) {
            break;
        }
        tag->enc_done = true;
        ESP_LOGI(TAG, "Link encrypted conn=%u disc_done=%d", tag->conn_handle, (int)tag->disc_done);
        maybe_emit_connected(tag);
        break;
    }
    case ESP_BLE_CONN_EVENT_DISCONNECTED: {
        const esp_ble_conn_event_data_t *ev = event_data;
        if (!ev) {
            break;
        }
        facade_tag_t *tag = tag_by_conn(ev->disconnected.conn_handle);
        if (!tag) {
            break;
        }
        if (s_sync_in_progress && s_sync_conn == ev->disconnected.conn_handle) {
            /* Lifecycle normally posts TAG_STATE_CHANGED from this same GAP
             * event. Keep a short fallback so a missing lifecycle event cannot
             * leave the facade permanently busy; a normal state event clears it. */
            s_sync_retry_attempts = FACADE_PAST_RETRY_MAX_ATTEMPTS;
            (void)sync_retry_schedule(FACADE_PAST_RETRY_INTERVAL_US);
        }
        /* Do not clear s_sync_in_progress here: lifecycle decides Synchronized
         * vs Unsynchronized on this disconnect and posts TAG_STATE_CHANGED. */
        if (tag->announced) {
            ble_esl_ap_disconn_info_t info = {
                .conn_handle = ev->disconnected.conn_handle,
                .reason = ev->disconnected.reason,
            };
            memcpy(info.addr, ev->disconnected.peer_addr, 6);
            emit(BLE_ESL_AP_EVT_DISCONNECTED, &info);
        }
        tag->conn_handle = BLE_CONN_HANDLE_INVALID;
        tag->announced = false;
        tag->disc_done = false;
        tag->enc_done = false;
        tag->mtu_done = false;
        break;
    }
    default:
        break;
    }
}

static void emit_response_from_tlvs(uint8_t group_id, const esp_ble_esl_address_t *addr,
                                    const uint8_t *data, uint8_t data_len)
{
    if (!data || data_len == 0) {
        return;
    }
    esl_ap_response_t resp = {
        .esl_id = addr ? addr->esl_id : data[1],
        .group_id = group_id,
        .type = data[0],
    };
    emit(BLE_ESL_AP_EVT_RESPONSE, &resp);
}

static void on_ap_event(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;
    if (base != BLE_ESLP_AP_EVENTS || !s_inited) {
        return;
    }

    switch (id) {
    case FACADE_INTERNAL_PAST_RETRY_EVENT: {
        if (!s_sync_in_progress || s_sync_conn == BLE_CONN_HANDLE_INVALID) {
            break;
        }
        uint16_t conn_handle = s_sync_conn;
        ble_esl_address_t esl_addr = s_sync_esl_addr;
        if (s_sync_retry_attempts >= FACADE_PAST_RETRY_MAX_ATTEMPTS) {
            ESP_LOGW(TAG, "PAST confirmation timeout conn=%u after %u attempts",
                     (unsigned)conn_handle, (unsigned)s_sync_retry_attempts);
            sync_wait_clear();
            emit_synchronized(conn_handle, esl_addr, ESP_ERR_TIMEOUT);
            (void)ble_esl_ap_disconnect(conn_handle);
            break;
        }

        s_sync_retry_attempts++;
        esp_err_t ret = esp_ble_eslp_ap_sync_transfer(conn_handle, 0);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "PAST retry %u/%u failed conn=%u: %s",
                     (unsigned)s_sync_retry_attempts,
                     (unsigned)FACADE_PAST_RETRY_MAX_ATTEMPTS,
                     (unsigned)conn_handle, esp_err_to_name(ret));
        } else {
            ESP_LOGD(TAG, "PAST retry %u/%u accepted conn=%u",
                     (unsigned)s_sync_retry_attempts,
                     (unsigned)FACADE_PAST_RETRY_MAX_ATTEMPTS,
                     (unsigned)conn_handle);
        }

        uint64_t delay_us = s_sync_retry_attempts >= FACADE_PAST_RETRY_MAX_ATTEMPTS
                            ? FACADE_PAST_CONFIRM_GRACE_US
                            : FACADE_PAST_RETRY_INTERVAL_US;
        if (sync_retry_schedule(delay_us) != ESP_OK) {
            ESP_LOGW(TAG, "Unable to schedule next PAST retry");
            sync_wait_clear();
            emit_synchronized(conn_handle, esl_addr, ESP_FAIL);
            (void)ble_esl_ap_disconnect(conn_handle);
        }
        break;
    }
    case BLE_ESLP_AP_EVENT_TAG_CONFIGURED: {
        const esp_ble_eslp_ap_tag_configured_t *ev = event_data;
        if (!ev) {
            break;
        }
        facade_tag_t *tag = tag_by_conn(ev->conn_handle);
        if (tag) {
            tag->esl_addr = ev->address;
            tag->has_esl_addr = true;
        }
        ble_esl_ap_configured_t cfg = {
            .conn_handle = ev->conn_handle,
            .status = 0,
        };
        emit(BLE_ESL_AP_EVT_CONFIGURED, &cfg);
        break;
    }
    case BLE_ESLP_AP_EVENT_IMAGE_TRANSFERRED: {
        const esp_ble_eslp_ap_image_transferred_t *ev = event_data;
        if (!ev) {
            break;
        }
        ble_esl_ap_image_transferred_t xfer = {
            .conn_handle = ev->conn_handle,
            .image_index = ev->image_index,
            .status = ev->status,
        };
        emit(BLE_ESL_AP_EVT_IMAGE_TRANSFERRED, &xfer);
        break;
    }
    case BLE_ESLP_AP_EVENT_SYNC_TRANSFERRED: {
        /* PAST set_info accepted only — not Tag sync confirmation.
         * Wait for TAG_STATE_CHANGED -> SYNCHRONIZED (Tag ACL teardown or
         * decryptable PAwR response). */
        uint16_t conn = BLE_CONN_HANDLE_INVALID;
        if (event_data) {
            conn = *(const uint16_t *)event_data;
        }
        ESP_LOGI(TAG, "PAST set_info accepted conn=%u; waiting for Tag sync confirm",
                 (unsigned)conn);
        break;
    }
    case BLE_ESLP_AP_EVENT_ECP_RESPONSE: {
        const esp_ble_eslp_ap_ecp_response_t *ev = event_data;
        if (!ev || ev->data_len == 0) {
            break;
        }
        facade_tag_t *tag = tag_by_conn(ev->conn_handle);
        uint8_t group = tag && tag->has_esl_addr ? BLE_ESL_ADDR_GROUP_ID(tag->esl_addr) : 0;
        esp_ble_esl_address_t addr = {0};
        if (tag && tag->has_esl_addr) {
            addr = tag->esl_addr;
        }
        emit_response_from_tlvs(group, &addr, ev->data, ev->data_len);
        break;
    }
    case BLE_ESLP_AP_EVENT_ECP_TIMEOUT: {
        const esp_ble_eslp_ap_ecp_timeout_t *ev = event_data;
        if (!ev) {
            break;
        }
        esl_ap_cmd_timeout_t to = {
            .esl_id = ev->esl_id,
            .group_id = ev->group_id,
        };
        emit(BLE_ESL_AP_EVT_CMD_TIMEOUT, &to);
        break;
    }
    case BLE_ESLP_AP_EVENT_SUBEV_RESP: {
        const esp_ble_eslp_ap_subev_resp_t *ev = event_data;
        if (!ev || !ev->address_valid || ev->data_len == 0) {
            break;
        }
        emit_response_from_tlvs(BLE_ESL_ADDR_GROUP_ID(ev->address), &ev->address,
                                ev->data, ev->data_len);
        break;
    }
    case BLE_ESLP_AP_EVENT_TAG_STATE_CHANGED: {
        const esp_ble_eslp_ap_tag_state_changed_t *ev = event_data;
        if (!ev) {
            break;
        }
        ble_esl_ap_state_changed_t ch = {
            .esl_addr = ev->address,
            .old_state = ev->prev_state,
            .new_state = ev->new_state,
        };
        emit(BLE_ESL_AP_EVT_STATE_CHANGED, &ch);

        /* Only complete an armed PAST wait here. Idle PAwR promotions stay as
         * STATE_CHANGED so inventory ONLINE still requires Ping / provision. */
        if (!s_sync_in_progress) {
            break;
        }
        bool addr_match = sync_wait_matches_addr(&ev->address);
        bool conn_match = (s_sync_conn != BLE_CONN_HANDLE_INVALID &&
                           s_sync_conn == ev->conn_handle);
        if (!addr_match && !conn_match) {
            break;
        }
        if (ev->new_state == BLE_ESLP_STATE_SYNCHRONIZED) {
            uint16_t conn = (ev->conn_handle != BLE_CONN_HANDLE_INVALID)
                            ? ev->conn_handle : s_sync_conn;
            sync_wait_clear();
            emit_synchronized(conn, ev->address, ESP_OK);
        } else if (ev->new_state == BLE_ESLP_STATE_UNSYNCHRONIZED ||
                   ev->new_state == BLE_ESLP_STATE_UNASSOCIATED) {
            ESP_LOGW(TAG, "PAST wait failed: state -> %d", (int)ev->new_state);
            sync_wait_clear();
            emit_synchronized(ev->conn_handle, ev->address, ESP_FAIL);
        }
        break;
    }
    default:
        break;
    }
}

esp_err_t ble_esl_ap_init(const ble_esl_ap_config_t *config)
{
    if (config == NULL || config->callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    s_cb = config->callback;
    s_pawr = config->pawr_config;
    memset(s_tags, 0, sizeof(s_tags));
    for (int i = 0; i < CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS; i++) {
        s_tags[i].conn_handle = BLE_CONN_HANDLE_INVALID;
    }

    esp_err_t ret = esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID,
                                               on_conn_event, NULL);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_event_handler_register(BLE_ESLP_AP_EVENTS, ESP_EVENT_ANY_ID, on_ap_event, NULL);
    if (ret != ESP_OK) {
        (void)esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, on_conn_event);
        return ret;
    }

    ret = esp_ble_eslp_ap_init();
    if (ret != ESP_OK) {
        (void)esp_event_handler_unregister(BLE_ESLP_AP_EVENTS, ESP_EVENT_ANY_ID, on_ap_event);
        (void)esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, on_conn_event);
        return ret;
    }

    s_inited = true;
    return ESP_OK;
}

esp_err_t ble_esl_ap_start_scan(void)
{
    /* Disable HCI duplicate filter so a stable public Tag keeps emitting
     * scan_result. Do not stop/restart discovery: that races PAwR on
     * instance 0 and trips pawrBcast_hal_sched_subevent on ESP32-C6. */
    esp_ble_conn_scan_params_t params = {
        .passive = true,
        .filter_duplicates = false,
    };
    (void)esp_ble_conn_scan_params_set(&params);

    if (ble_gap_disc_active()) {
        return ESP_OK;
    }
    return esp_ble_conn_scan_start();
}

esp_err_t ble_esl_ap_stop_scan(void)
{
    return esp_ble_conn_scan_stop();
}

esp_err_t ble_esl_ap_start_pawr(void)
{
    esp_ble_eslp_ap_pawr_config_t cfg = {
        .itvl_min = s_pawr.periodic_adv_interval_min,
        .itvl_max = s_pawr.periodic_adv_interval_max,
        .num_subevents = s_pawr.num_subevents,
        .subevent_interval = s_pawr.subevent_interval,
        .response_slot_delay = s_pawr.response_slot_delay,
        .response_slot_spacing = s_pawr.response_slot_spacing,
        .num_response_slots = s_pawr.num_response_slots,
        .include_tx_power = false,
    };
    return esp_ble_eslp_ap_pawr_start(&cfg);
}

esp_err_t ble_esl_ap_stop_pawr(void)
{
    return esp_ble_eslp_ap_pawr_stop();
}

static void pause_pawr_for_acl_connect(void)
{
    if (s_pawr_paused_for_acl) {
        return;
    }
    ESP_LOGI(TAG, "Pause PAwR before ACL connect");
    (void)ble_esl_ap_stop_pawr();
    s_pawr_paused_for_acl = true;
}

static void resume_pawr_after_acl_connect(void)
{
    if (!s_pawr_paused_for_acl) {
        return;
    }
    s_pawr_paused_for_acl = false;
    ESP_LOGI(TAG, "Resume PAwR after ACL");
    if (ble_esl_ap_start_pawr() != ESP_OK) {
        ESP_LOGW(TAG, "Resume PAwR failed");
    }
}

esp_err_t ble_esl_ap_connect(const uint8_t addr[6], uint8_t addr_type)
{
    if (addr == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    /* Unassociated connect uses AUX_CONNECT_REQ. Running PAwR on instance 0
     * at the same time asserts the ESP32-C6 scheduler. */
    pause_pawr_for_acl_connect();
    s_pending_connect = true;
    memcpy(s_pending_addr, addr, 6);
    s_pending_addr_type = addr_type;
    ESP_LOGI(TAG, "ACL connect type=%u %02x:%02x:%02x:%02x:%02x:%02x",
             addr_type, addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
    esp_err_t ret = esp_ble_conn_connect_to_addr(addr, addr_type);
    if (ret == ESP_ERR_INVALID_STATE) {
        (void)esp_ble_conn_connect_cancel();
        ret = esp_ble_conn_connect_to_addr(addr, addr_type);
    }
    if (ret != ESP_OK) {
        s_pending_connect = false;
        resume_pawr_after_acl_connect();
    }
    return ret;
}

esp_err_t ble_esl_ap_connect_synced(ble_esl_address_t addr)
{
    facade_tag_t *tag = tag_by_esl(addr);
    if (tag == NULL) {
        return ESP_ERR_NOT_FOUND;
    }
    if (tag->conn_handle != BLE_CONN_HANDLE_INVALID) {
        /* Provision ACL is still up (Tag waiting for PAST). Re-announce so
         * the app starts OTS on this link instead of waiting for a new CONNECTED. */
        ESP_LOGI(TAG, "connect_synced: reuse ACL conn=%u", tag->conn_handle);
        tag->announced = false;
        maybe_emit_connected(tag);
        return ESP_OK;
    }

    uint8_t subevent = BLE_ESL_ADDR_GROUP_ID(addr);
    s_pending_connect = true;
    memcpy(s_pending_addr, tag->ble_addr, 6);
    s_pending_addr_type = tag->ble_addr_type;

    esp_err_t ret = esp_ble_conn_connect_with_synced(tag->ble_addr, tag->ble_addr_type, subevent);
    if (ret == ESP_ERR_INVALID_STATE) {
        /* Recovery's 10 s timer expires while NimBLE's GAP connect still runs (30 s). */
        ESP_LOGW(TAG, "PAwR connect_synced busy, cancel leftover GAP then retry");
        (void)esp_ble_conn_connect_cancel();
        ret = esp_ble_conn_connect_with_synced(tag->ble_addr, tag->ble_addr_type, subevent);
    }
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "PAwR connect_synced failed (%s), fallback ACL", esp_err_to_name(ret));
        ret = esp_ble_conn_connect_to_addr(tag->ble_addr, tag->ble_addr_type);
        if (ret == ESP_ERR_INVALID_STATE) {
            (void)esp_ble_conn_connect_cancel();
            ret = esp_ble_conn_connect_to_addr(tag->ble_addr, tag->ble_addr_type);
        }
    }
    if (ret != ESP_OK) {
        s_pending_connect = false;
    }
    return ret;
}

esp_err_t ble_esl_ap_disconnect(uint16_t conn_handle)
{
    return esp_ble_conn_disconnect_by_handle(conn_handle);
}

void ble_esl_ap_abort_tag_connections(void)
{
    uint8_t pending_addr[6];
    uint8_t pending_type = s_pending_addr_type;
    bool had_pending = s_pending_connect;
    memcpy(pending_addr, s_pending_addr, 6);

    s_pending_connect = false;
    (void)esp_ble_conn_connect_cancel();

    /* Tag may already hold ACL while host still thinks connect is in progress
     * (missed AUX_CONNECT_RSP). Cancel does not drop that link. */
    if (had_pending) {
        struct ble_gap_conn_desc desc;
        ble_addr_t a;
        memcpy(a.val, pending_addr, 6);
        static const uint8_t k_types[] = {
            BLE_ADDR_PUBLIC, BLE_ADDR_RANDOM, BLE_ADDR_PUBLIC_ID, BLE_ADDR_RANDOM_ID
        };
        a.type = pending_type;
        bool found = (ble_gap_conn_find_by_addr(&a, &desc) == 0);
        for (size_t i = 0; !found && i < sizeof(k_types); i++) {
            a.type = k_types[i];
            found = (ble_gap_conn_find_by_addr(&a, &desc) == 0);
        }
        if (found) {
            ESP_LOGW(TAG, "Abort: terminate leftover ACL conn=%u", desc.conn_handle);
            (void)ble_gap_terminate(desc.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        }
    }

    resume_pawr_after_acl_connect();
    for (int i = 0; i < CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS; i++) {
        if (s_tags[i].used && s_tags[i].conn_handle != BLE_CONN_HANDLE_INVALID) {
            (void)esp_ble_conn_disconnect_by_handle(s_tags[i].conn_handle);
        }
    }
}

esp_err_t ble_esl_ap_read_info(uint16_t conn_handle)
{
    esp_ble_eslp_ap_tag_info_t info = {0};
    esp_err_t ret = esp_ble_eslp_ap_read_info(conn_handle, &info);
    ble_esl_ap_esl_info_t out = {
        .conn_handle = conn_handle,
        .displays = NULL,
        .sensors = NULL,
        .leds = NULL,
    };
    /* Always deliver ESL_INFO so provision can continue even if optional chars are missing. */
    emit(BLE_ESL_AP_EVT_ESL_INFO, &out);
    return ret == ESP_ERR_NOT_FOUND ? ESP_OK : ret;
}

esp_err_t ble_esl_ap_configure(uint16_t conn_handle, const ble_esl_ap_esl_config_t *cfg)
{
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_ble_eslp_ap_tag_config_t tcfg;
    (void)esp_ble_eslp_ap_tag_config_default(&tcfg);
    tcfg.address.esl_id = cfg->esl_id;
    BLE_ESL_ADDR_SET_GROUP_ID(tcfg.address, cfg->group_id);
    tcfg.ap_sync_key = cfg->ap_sync_key;
    tcfg.resp_key = cfg->resp_key;
    tcfg.subscribe_ecp = true;
    tcfg.send_update_complete = false;

    facade_tag_t *tag = tag_by_conn(conn_handle);
    if (tag) {
        tag->esl_addr = tcfg.address;
        tag->has_esl_addr = true;
    }
    return esp_ble_eslp_ap_configure_tag(conn_handle, &tcfg);
}

esp_err_t ble_esl_ap_transfer_image(const ble_esl_ap_image_transfer_params_t *params)
{
    if (params == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)params->truncate;
    esp_ble_eslp_ap_image_transfer_t xfer = {
        .conn_handle = params->conn_handle,
        .image_index = params->image_index,
        .data = params->data,
        .data_len = params->data_len,
    };
    return esp_ble_eslp_ap_transfer_image(&xfer);
}

esp_err_t ble_esl_ap_synchronize(uint16_t conn_handle)
{
    facade_tag_t *tag = tag_by_conn(conn_handle);
    uint8_t esl_id = tag && tag->has_esl_addr ? tag->esl_addr.esl_id : 0;
    uint8_t group_id = tag && tag->has_esl_addr ? BLE_ESL_ADDR_GROUP_ID(tag->esl_addr) : 0;
    const bool keep_sync = esp_ble_eslp_ap_keeps_sync_without_past(conn_handle);
    ble_esl_address_t esl_addr = {0};
    if (tag && tag->has_esl_addr) {
        esl_addr = tag->esl_addr;
    }

    /* Arm before Update Complete: Tag may drop ACL synchronously on UC. */
    sync_wait_arm(conn_handle, tag);

    esp_err_t ret = esp_ble_eslp_ap_update_complete(esl_id, group_id);
    if (ret != ESP_OK) {
        sync_wait_clear();
        return ret;
    }

    /* UC write can complete the wait inline (Tag 0x13 teardown → SYNCHRONIZED). */
    if (!s_sync_in_progress) {
        esp_ble_eslp_state_t st = BLE_ESLP_STATE_UNASSOCIATED;
        if (tag && tag->has_esl_addr &&
                esp_ble_eslp_ap_get_tag_state(&tag->esl_addr, &st) == ESP_OK &&
                st == BLE_ESLP_STATE_SYNCHRONIZED) {
            return ESP_OK;
        }
        return ESP_FAIL;
    }

    esp_ble_eslp_state_t st = BLE_ESLP_STATE_UNASSOCIATED;
    if (tag && tag->has_esl_addr) {
        (void)esp_ble_eslp_ap_get_tag_state(&tag->esl_addr, &st);
    }
    /* Tag already tore down on UC (true keep-sync): wait for confirm only. */
    if (tag_by_conn(conn_handle) == NULL) {
        ESP_LOGI(TAG, "Update Complete conn=%u; ACL already gone, waiting for sync confirm",
                 (unsigned)conn_handle);
        return ESP_OK;
    }

    /* Always PAST while ACL remains. Skipping PAST when keep_sync=1 races a
     * stale AP SYNCHRONIZED bit against a Tag that already lost PAwR (e.g.
     * ONLINE advertising unsync → recovery). Tag waits forever for PAST. */
    if (keep_sync) {
        ESP_LOGI(TAG, "Update Complete conn=%u keep_sync=1; ACL up — send PAST",
                 (unsigned)conn_handle);
    }

    ret = esp_ble_eslp_ap_sync_transfer(conn_handle, 0);
    if (ret == ESP_OK) {
        /* Update Complete has no readiness response. Retry set_info while the
         * ACL is alive so the Tag can first release its old sync slot and arm
         * PAST reception. TAG_STATE_CHANGED / disconnect cancels this timer. */
        s_sync_retry_attempts = 1;
        ret = sync_retry_schedule(FACADE_PAST_RETRY_INTERVAL_US);
        if (ret == ESP_OK) {
            return ESP_OK;
        }
        ESP_LOGW(TAG, "PAST retry timer start failed: %s", esp_err_to_name(ret));
    }

    sync_wait_clear();

    if (ret == ESP_ERR_NOT_FOUND && tag && tag->has_esl_addr) {
        st = BLE_ESLP_STATE_UNASSOCIATED;
        if (esp_ble_eslp_ap_get_tag_state(&tag->esl_addr, &st) == ESP_OK &&
                st == BLE_ESLP_STATE_SYNCHRONIZED) {
            ESP_LOGI(TAG, "Tag closed conn=%u on Update Complete; profile already SYNCHRONIZED",
                     (unsigned)conn_handle);
            emit_synchronized(conn_handle, esl_addr, ESP_OK);
            return ESP_OK;
        }
        ESP_LOGW(TAG, "sync_transfer NOT_FOUND conn=%u but Tag not SYNCHRONIZED (st=%s)",
                 (unsigned)conn_handle, esp_ble_eslp_state_str(st));
    }

    emit_synchronized(conn_handle, esl_addr, ret);
    return ret;
}

bool ble_esl_ap_synchronize_in_progress(void)
{
    return s_sync_in_progress;
}

esp_err_t ble_esl_ap_write_absolute_time(uint16_t conn_handle)
{
    esp_ble_esl_abs_time_t abs_time = (esp_ble_esl_abs_time_t)esp_log_timestamp();
    esp_ble_conn_data_t buff = {
        .type = BLE_CONN_UUID_TYPE_16,
        .uuid = {
            .uuid16 = BLE_ESL_CHR_UUID16_CURRENT_ABS_TIME,
        },
        .data = (uint8_t *) &abs_time,
        .data_len = sizeof(abs_time),
    };
    esp_err_t ret = esp_ble_conn_write_by_handle(conn_handle, &buff);
    ble_esl_ap_abs_time_written_t ev = {
        .conn_handle = conn_handle,
        .status = ret,
    };
    emit(BLE_ESL_AP_EVT_ABS_TIME_WRITTEN, &ev);
    return ret;
}

esp_err_t ble_esl_ap_unassociate(uint8_t esl_id, uint8_t group_id)
{
    return esp_ble_eslp_ap_unassociate(esl_id, group_id);
}

esp_err_t ble_esl_ap_forget_tag(uint8_t esl_id, uint8_t group_id)
{
    ble_esl_address_t addr = {
        .esl_id = esl_id,
    };
    BLE_ESL_ADDR_SET_GROUP_ID(addr, group_id);

    facade_tag_t *tag = tag_by_esl(addr);
    esp_err_t profile_ret = esp_ble_eslp_ap_forget_tag(esl_id, group_id);
    if (profile_ret != ESP_OK && profile_ret != ESP_ERR_NOT_FOUND) {
        return profile_ret;
    }

    if (sync_wait_matches_addr(&addr)) {
        sync_wait_clear();
    }
    if (tag != NULL) {
        if (s_pending_connect && tag->ble_addr_type == s_pending_addr_type &&
                addr_eq(tag->ble_addr, s_pending_addr)) {
            s_pending_connect = false;
            (void)esp_ble_conn_connect_cancel();
            resume_pawr_after_acl_connect();
        }
        tag_release(tag);
        ESP_LOGI(TAG, "Released Tag slot esl_id=%u group=%u", esl_id, group_id);
        return ESP_OK;
    }

    return profile_ret;
}

esp_err_t ble_esl_ap_led_control(uint8_t esl_id, uint8_t group_id, uint8_t led_index,
                                 const esl_ap_led_settings_t *led)
{
    return esp_ble_eslp_ap_led_control(esl_id, group_id, led_index, led);
}

esp_err_t ble_esl_ap_pawr_command(uint8_t esl_id, uint8_t group_id, uint8_t opcode,
                                  const uint8_t *params, uint8_t params_len)
{
    if (params_len > 0 && params == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t tlv[BLE_ESL_ECP_MAX_SIZE];
    if ((uint16_t)(1 + params_len) > sizeof(tlv)) {
        return ESP_ERR_INVALID_ARG;
    }
    tlv[0] = opcode;
    if (params_len > 0) {
        memcpy(&tlv[1], params, params_len);
    }
    uint8_t tlv_len = (uint8_t)(1 + params_len);
    uint8_t count = (esl_id == BLE_ESL_BROADCAST_ADDRESS) ? 0 : 1;
    return esp_ble_eslp_ap_queue_pawr_command(group_id, tlv, tlv_len, 0, count);
}

ble_esl_state_t ble_esl_ap_get_esl_state(ble_esl_address_t addr)
{
    esp_ble_eslp_state_t st = BLE_ESLP_STATE_UNASSOCIATED;
    if (esp_ble_eslp_ap_get_tag_state(&addr, &st) != ESP_OK) {
        return BLE_ESL_STATE_UNASSOCIATED;
    }
    return st;
}

esp_err_t ble_esl_ap_restore_persisted_esl(const ble_esl_ap_persisted_esl_t *info)
{
    if (info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = esp_ble_eslp_ap_set_ap_sync_key(&info->ap_sync_key);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_ble_eslp_ap_restore_persisted(&info->esl_address, info->ble_addr,
                                            info->ble_addr_type, &info->resp_key);
    if (ret != ESP_OK) {
        return ret;
    }
    facade_tag_t *tag = tag_by_esl(info->esl_address);
    if (!tag) {
        tag = tag_alloc();
    }
    if (tag) {
        tag->esl_addr = info->esl_address;
        tag->has_esl_addr = true;
        memcpy(tag->ble_addr, info->ble_addr, 6);
        tag->ble_addr_type = info->ble_addr_type;
        tag->conn_handle = BLE_CONN_HANDLE_INVALID;
    }
    return ESP_OK;
}
