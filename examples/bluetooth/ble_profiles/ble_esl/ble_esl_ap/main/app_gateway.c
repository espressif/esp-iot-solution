/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 *
 * Lightweight NUS GATT (coexists with ESL AP NimBLE host) + JSONL gateway.
 */

#include "app_gateway.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "os/os_mbuf.h"
#include "host/ble_att.h"
#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "host/ble_hs_mbuf.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "nimble/ble.h"

#include "ble_esl_ap.h"
#include "ble_esl_common.h"
#include "app_inventory.h"
#include "app_esl_ap.h"
#include "app_recovery.h"
#include "esl_ap_arbiter.h"
#include "esl_image_manager.h"
#include "sdkconfig.h"

/* Phone mode registers NUS after conn_mgr has started GATT. Without dynamic
 * services, ble_gatts_add_svcs() returns BLE_HS_EBUSY and the ATT DB never
 * gets the NUS RX/TX characteristics. */
#ifndef CONFIG_BT_NIMBLE_DYNAMIC_SERVICE
#error "ESL_AP_MODE_WITH_APP requires CONFIG_BT_NIMBLE_DYNAMIC_SERVICE"
#endif

static const char *TAG = "app_gateway";

#define NUS_SVC_BYTES   0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, \
                        0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e
#define NUS_RX_BYTES    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, \
                        0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e
#define NUS_TX_BYTES    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, \
                        0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e

#define GW_RX_SCRATCH       1024
#define GW_LINE_MAX         1024
#define GW_TX_LINE_MAX      1024
/* Leave room for {"v":1,"id":"<48>","ok":true,"data":...} envelope. */
#define GW_DATA_MAX         (GW_TX_LINE_MAX - 80)
/* One tag object after recovery-v1 + quarantine/ops_hint fields (~520B typical). */
#define GW_TAG_JSON_MAX     768
#define GW_RX_QUEUE_DEPTH   8
#define GW_WORK_QUEUE_DEPTH 16
/* Full SSD1680 128x296 1bpp frame. NUS JSONL+Base64 remains a temporary APP->AP
 * data path; see README "Temporary image upload transport". */
#define GW_UPLOAD_MAX       4736

enum {
    GW_WORK_NUS_RX = 0,
    GW_WORK_TAG_RECOVERY,
    GW_WORK_RECOVERY_SUMMARY,
};

/* PAwR uses ext-adv instance 0; phone NUS uses a separate instance. */
#define NUS_EXT_ADV_INSTANCE    1
#define NUS_ADV_NAME            "ESP-ESL-AP"

static const ble_uuid128_t s_svc_uuid = BLE_UUID128_INIT(NUS_SVC_BYTES);
static const ble_uuid128_t s_rx_uuid = BLE_UUID128_INIT(NUS_RX_BYTES);
static const ble_uuid128_t s_tx_uuid = BLE_UUID128_INIT(NUS_TX_BYTES);

static uint16_t s_tx_val_handle;
static volatile uint16_t s_phone_conn = BLE_HS_CONN_HANDLE_NONE;
static bool s_tx_subscribed;
static bool s_nus_registered;
static bool s_started;
static bool s_nus_radio_hold;
static bool s_busy;
static uint8_t s_own_addr_type;

static QueueHandle_t s_work_queue;
static SemaphoreHandle_t s_emit_mutex;
static char s_line_acc[GW_LINE_MAX];
static size_t s_line_len;
static bool s_line_discard;

typedef struct {
    uint8_t type;
    uint8_t slot;
    uint16_t len;
    uint8_t data[GW_RX_SCRATCH];
} gw_work_msg_t;

/* Upload session (v0.2) */
static bool s_up_active;
static uint32_t s_up_image_id;
static uint32_t s_up_version;
static uint32_t s_up_expected;
static uint32_t s_up_received;
static uint32_t s_up_crc_expect;
static uint8_t s_up_buf[GW_UPLOAD_MAX];
static char s_pending_cmd[32];
static uint8_t s_pending_esl_id;
static uint8_t s_pending_group_id;
static uint8_t s_pending_opcode;
static uint32_t s_next_app_request_id;

#define APP_GATEWAY_PAWR_GUARD_MS  3000u

static struct ble_gatt_chr_def s_chr_defs[3];
static struct ble_gatt_svc_def s_svc_defs[2];

static void gw_worker(void *arg);
static int nus_gap_event(struct ble_gap_event *event, void *arg);
static int start_nus_advertising(void);
static void handle_line(const char *line);
static void reply_ok(const char *id, const char *data_json);
static void reply_err(const char *id, const char *code, const char *message);
static void emit_raw(const char *json_object);
static void emit_raw_ex(const char *json_object, TickType_t wait_ticks);
static void emit_recovery_summary_on_worker(void);
static void emit_tag_recovery_on_worker(uint8_t slot);
static bool json_get_string(const char *json, const char *key, char *out, size_t out_len);
static bool json_get_int(const char *json, const char *key, int *out);
static void parse_tag_selector(const char *json, int *esl_id, int *group_id);
static bool json_get_tag_ble_addr(const char *json, char *out, size_t out_len, int *addr_type);
static bool parse_ble_addr(const char *str, uint8_t out[6]);
static const char *find_cmd(const char *json);
static uint32_t crc32_calc(const uint8_t *data, size_t len);
static int b64_decode(const char *in, uint8_t *out, size_t out_cap, size_t *out_len);
static void format_addr(const uint8_t addr[6], char *out, size_t out_len);
static const char *state_to_str(ble_esl_state_t st);
static const char *product_state_str(esl_product_state_t st);
static const char *desired_state_str(uint8_t desired);
static const char *provision_phase_str(uint8_t phase);
static const char *failure_reason_str(esl_ap_failure_reason_t reason);
static const char *lifecycle_str(esl_ap_lifecycle_t life);
static const char *error_code_str(esl_ap_error_code_t err);
static const char *recommended_action_str(esl_ap_error_code_t err, bool retryable);
static const char *legacy_sync_state(const esl_product_tag_record_t *rec);
static bool find_tag_view(uint8_t esl_id, uint8_t group_id, esl_product_tag_record_t *out);
static bool require_tag_online(const char *id, uint8_t esl_id, uint8_t group_id);
static bool is_group_broadcast(uint8_t esl_id);
static bool require_group_online(const char *id, uint8_t group_id);
static bool require_pawr_target_online(const char *id, uint8_t esl_id, uint8_t group_id);
static bool accept_pawr_cmd(const char *id, const char *cmd, uint8_t esl_id, uint8_t group_id);
static bool prepare_pawr_dispatch(const char *id, const char *cmd, uint8_t esl_id, uint8_t group_id);
static void finish_pawr_dispatch(const char *cmd, uint8_t esl_id, uint8_t group_id, esp_err_t ret);
static int format_tag_json(char *out, size_t out_len, const esl_product_tag_record_t *view);
static int format_boot_id(char *out, size_t out_len);

static int nus_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)attr_handle;
    (void)arg;
    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_WRITE_CHR: {
        static uint8_t rx_buf[GW_RX_SCRATCH];
        uint16_t total = OS_MBUF_PKTLEN(ctxt->om);
        if (total > sizeof(rx_buf)) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        uint16_t copied = 0;
        if (ble_hs_mbuf_to_flat(ctxt->om, rx_buf, total, &copied) != 0) {
            return BLE_ATT_ERR_UNLIKELY;
        }
        if (copied == 0 || s_work_queue == NULL) {
            return 0;
        }
        gw_work_msg_t msg = { .type = GW_WORK_NUS_RX, .len = copied };
        memcpy(msg.data, rx_buf, copied);
        if (xQueueSend(s_work_queue, &msg, 0) != pdTRUE) {
            ESP_LOGW(TAG, "work queue full, drop %u bytes", copied);
        } else {
            ESP_LOGD(TAG, "NUS RX %u bytes on conn=%u", copied, conn_handle);
        }
        s_phone_conn = conn_handle;
        return 0;
    }
    case BLE_GATT_ACCESS_OP_READ_CHR:
        return BLE_ATT_ERR_READ_NOT_PERMITTED;
    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
}

static void build_nus_table(void)
{
    s_chr_defs[0] = (struct ble_gatt_chr_def) {
        .uuid = &s_rx_uuid.u,
        .access_cb = nus_chr_access,
        .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
    };
    s_chr_defs[1] = (struct ble_gatt_chr_def) {
        .uuid = &s_tx_uuid.u,
        .access_cb = nus_chr_access,
        .flags = BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &s_tx_val_handle,
    };
    s_chr_defs[2] = (struct ble_gatt_chr_def) {
        0
    };
    s_svc_defs[0] = (struct ble_gatt_svc_def) {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_svc_uuid.u,
        .characteristics = s_chr_defs,
    };
    s_svc_defs[1] = (struct ble_gatt_svc_def) {
        0
    };
}

esp_err_t app_gateway_init(void)
{
    if (s_work_queue == NULL) {
        s_work_queue = xQueueCreate(GW_WORK_QUEUE_DEPTH, sizeof(gw_work_msg_t));
        if (s_work_queue == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (s_emit_mutex == NULL) {
        s_emit_mutex = xSemaphoreCreateMutex();
        if (s_emit_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (!s_nus_registered) {
        build_nus_table();
        int rc;
#if MYNEWT_VAL(BLE_DYNAMIC_SERVICE)
        /* conn_mgr starts GATT inside esp_ble_conn_start(); static add_svcs then
         * returns BLE_HS_EBUSY. Dynamic registration is the supported path. */
        if (ble_hs_is_enabled()) {
            rc = ble_gatts_add_dynamic_svcs(s_svc_defs);
            if (rc != 0) {
                ESP_LOGE(TAG, "ble_gatts_add_dynamic_svcs rc=%d", rc);
                return ESP_FAIL;
            }
            ESP_LOGI(TAG, "NUS service registered dynamically");
        } else
#endif
        {
            rc = ble_gatts_count_cfg(s_svc_defs);
            if (rc != 0) {
                ESP_LOGE(TAG, "ble_gatts_count_cfg rc=%d", rc);
                return ESP_FAIL;
            }
            rc = ble_gatts_add_svcs(s_svc_defs);
            if (rc != 0) {
                ESP_LOGE(TAG, "ble_gatts_add_svcs rc=%d", rc);
                return ESP_FAIL;
            }
            ESP_LOGI(TAG, "NUS service queued for GATT registration");
        }
        s_nus_registered = true;
    }

    ble_svc_gap_device_name_set("ESP-ESL-AP");
    /* Phone GAP events come from the instance-1 ext-adv callback only.
     * A global ble_gap_event_listener would deliver CONNECT/DISCONNECT/MTU
     * twice and restart NUS advertising while PAwR is scheduling on
     * instance 0 — ESP32-C6 asserts in pawrBcast_hal_sched_subevent. */

    return ESP_OK;
}

esp_err_t app_gateway_start(void)
{
    static TaskHandle_t task;
    if (task == NULL) {
        if (xTaskCreate(gw_worker, "gw_worker", 8192, NULL, 5, &task) != pdPASS) {
            return ESP_ERR_NO_MEM;
        }
    }

    s_started = true;

    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "ensure_addr rc=%d", rc);
        return ESP_FAIL;
    }
    rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGW(TAG, "id_infer_auto rc=%d, fallback public", rc);
        s_own_addr_type = BLE_OWN_ADDR_PUBLIC;
    }

    rc = start_nus_advertising();
    if (rc != 0) {
        ESP_LOGE(TAG, "NUS advertising failed rc=%d", rc);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "gateway started (ESL AP started=%d)", app_esl_ap_is_started());
    return ESP_OK;
}

esp_err_t app_gateway_start_phone_first(void (*on_phone_ready)(void))
{
    /* Legacy API: ESL/PAwR no longer waits for phone hello. Ignore callback. */
    (void)on_phone_ready;
    return app_gateway_start();
}

static int start_nus_advertising(void)
{
#if !MYNEWT_VAL(BLE_EXT_ADV)
#error "NUS advertising requires CONFIG_BT_NIMBLE_EXT_ADV (legacy adv is ENOTSUP)"
#endif

    if (NUS_EXT_ADV_INSTANCE >= BLE_ADV_INSTANCES) {
        ESP_LOGE(TAG, "NUS instance %d not available (BLE_ADV_INSTANCES=%d); "
                 "raise CONFIG_BT_NIMBLE_MAX_EXT_ADV_INSTANCES",
                 NUS_EXT_ADV_INSTANCE, BLE_ADV_INSTANCES);
        return BLE_HS_ENOMEM;
    }

    if (ble_gap_ext_adv_active(NUS_EXT_ADV_INSTANCE)) {
        (void)ble_gap_ext_adv_stop(NUS_EXT_ADV_INSTANCE);
    }

    struct ble_gap_ext_adv_params params;
    memset(&params, 0, sizeof(params));
    /* Legacy PDU keeps Android/nRF Connect discovery simple (flags+name / UUID rsp). */
    params.connectable = 1;
    params.scannable = 1;
    params.legacy_pdu = 1;
    params.own_addr_type = s_own_addr_type;
    params.primary_phy = BLE_HCI_LE_PHY_1M;
    params.secondary_phy = BLE_HCI_LE_PHY_1M;
    params.sid = 1;
    params.itvl_min = BLE_GAP_ADV_FAST_INTERVAL1_MIN;
    params.itvl_max = BLE_GAP_ADV_FAST_INTERVAL1_MIN;

    int rc = ble_gap_ext_adv_configure(NUS_EXT_ADV_INSTANCE, &params, NULL,
                                       nus_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ext_adv_configure rc=%d", rc);
        return rc;
    }

    struct ble_hs_adv_fields adv = {
        .flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP,
        .name = (const uint8_t *)NUS_ADV_NAME,
        .name_len = (uint8_t)strlen(NUS_ADV_NAME),
        .name_is_complete = 1,
    };
    struct os_mbuf *adv_data = os_msys_get_pkthdr(BLE_HS_ADV_MAX_SZ, 0);
    if (adv_data == NULL) {
        ESP_LOGE(TAG, "no mbuf for adv data");
        return BLE_HS_ENOMEM;
    }
    rc = ble_hs_adv_set_fields_mbuf(&adv, adv_data);
    if (rc != 0) {
        os_mbuf_free_chain(adv_data);
        ESP_LOGE(TAG, "adv fields rc=%d", rc);
        return rc;
    }
    rc = ble_gap_ext_adv_set_data(NUS_EXT_ADV_INSTANCE, adv_data);
    if (rc != 0) {
        ESP_LOGE(TAG, "ext_adv_set_data rc=%d", rc);
        return rc;
    }

    struct ble_hs_adv_fields rsp = {
        .uuids128 = &s_svc_uuid,
        .num_uuids128 = 1,
        .uuids128_is_complete = 1,
    };
    struct os_mbuf *rsp_data = os_msys_get_pkthdr(BLE_HS_ADV_MAX_SZ, 0);
    if (rsp_data == NULL) {
        ESP_LOGE(TAG, "no mbuf for scan rsp");
        return BLE_HS_ENOMEM;
    }
    rc = ble_hs_adv_set_fields_mbuf(&rsp, rsp_data);
    if (rc != 0) {
        os_mbuf_free_chain(rsp_data);
        ESP_LOGE(TAG, "rsp fields rc=%d", rc);
        return rc;
    }
    rc = ble_gap_ext_adv_rsp_set_data(NUS_EXT_ADV_INSTANCE, rsp_data);
    if (rc != 0) {
        ESP_LOGE(TAG, "ext_adv_rsp_set_data rc=%d", rc);
        return rc;
    }

    rc = ble_gap_ext_adv_start(NUS_EXT_ADV_INSTANCE, 0, 0);
    if (rc != 0) {
        ESP_LOGE(TAG, "ext_adv_start rc=%d", rc);
        return rc;
    }

    ESP_LOGI(TAG, "NUS advertising as %s (ext instance %d)",
             NUS_ADV_NAME, NUS_EXT_ADV_INSTANCE);
    return 0;
}

static bool gap_conn_is_phone_slave(uint16_t conn_handle)
{
    struct ble_gap_conn_desc desc;
    if (ble_gap_conn_find(conn_handle, &desc) != 0) {
        return false;
    }
    return desc.role == BLE_GAP_ROLE_SLAVE;
}

static int nus_gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0 &&
                !gap_conn_is_phone_slave(event->connect.conn_handle)) {
            ESP_LOGD(TAG, "ignore central connect handle=%d (ESL tag)",
                     event->connect.conn_handle);
            return 0;
        }
        ESP_LOGI(TAG, "phone connect %s status=%d handle=%d",
                 event->connect.status == 0 ? "ok" : "failed",
                 event->connect.status, event->connect.conn_handle);
        if (event->connect.status == 0) {
            s_phone_conn = event->connect.conn_handle;
            s_tx_subscribed = false;
            /* Instance 1 is already disabled by the controller when the
             * connection forms. A second stop races PAwR on instance 0. */
            if (s_nus_radio_hold) {
                (void)ble_gap_terminate(event->connect.conn_handle,
                                        BLE_ERR_REM_USER_CONN_TERM);
            }
        } else if (s_phone_conn == BLE_HS_CONN_HANDLE_NONE && !s_nus_radio_hold) {
            start_nus_advertising();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGW(TAG, "GAP disconnect reason=%d handle=%d",
                 event->disconnect.reason, event->disconnect.conn.conn_handle);
        if (event->disconnect.conn.conn_handle == s_phone_conn) {
            ESP_LOGW(TAG, "phone disconnected reason=%d handle=%d",
                     event->disconnect.reason, event->disconnect.conn.conn_handle);
            s_phone_conn = BLE_HS_CONN_HANDLE_NONE;
            s_tx_subscribed = false;
            if (!s_nus_radio_hold) {
                start_nus_advertising();
            }
        }
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "subscribe conn=%d attr=%d cur_notify=%d",
                 event->subscribe.conn_handle, event->subscribe.attr_handle,
                 event->subscribe.cur_notify);
        if (event->subscribe.conn_handle != s_phone_conn &&
                gap_conn_is_phone_slave(event->subscribe.conn_handle)) {
            s_phone_conn = event->subscribe.conn_handle;
        }
        if (event->subscribe.conn_handle == s_phone_conn &&
                event->subscribe.attr_handle == s_tx_val_handle) {
            s_tx_subscribed = (event->subscribe.cur_notify != 0);
        }
        return 0;

    case BLE_GAP_EVENT_MTU:
        if (event->mtu.conn_handle == s_phone_conn) {
            ESP_LOGI(TAG, "MTU updated conn=%d mtu=%d",
                     event->mtu.conn_handle, event->mtu.value);
        }
        return 0;

    default:
        return 0;
    }
}

void app_gateway_set_pawr_radio_hold(bool hold)
{
    if (hold == s_nus_radio_hold) {
        return;
    }
    s_nus_radio_hold = hold;
    if (hold) {
        if (ble_gap_ext_adv_active(NUS_EXT_ADV_INSTANCE)) {
            (void)ble_gap_ext_adv_stop(NUS_EXT_ADV_INSTANCE);
        }
        if (s_phone_conn != BLE_HS_CONN_HANDLE_NONE) {
            (void)ble_gap_terminate(s_phone_conn, BLE_ERR_REM_USER_CONN_TERM);
        }
        ESP_LOGI(TAG, "NUS held for PAwR response slot");
        return;
    }
    if (s_started && s_phone_conn == BLE_HS_CONN_HANDLE_NONE) {
        (void)start_nus_advertising();
    }
    ESP_LOGI(TAG, "NUS resumed after PAwR");
}

bool app_gateway_is_phone_connected(void)
{
    return s_phone_conn != BLE_HS_CONN_HANDLE_NONE && s_tx_subscribed;
}

bool app_gateway_is_started(void)
{
    return s_started;
}

void app_gateway_emit_event_line(const char *json_object)
{
    if (json_object) {
        emit_raw(json_object);
    }
}

#define GW_SCAN_EMIT_SLOTS  8
#define GW_SCAN_EMIT_MIN_MS 400

static struct {
    uint8_t addr[6];
    int64_t last_ms;
} s_scan_emit[GW_SCAN_EMIT_SLOTS];

static bool scan_result_emit_due(const uint8_t addr[6])
{
    int64_t now = esp_timer_get_time() / 1000;
    int oldest = 0;
    for (int i = 0; i < GW_SCAN_EMIT_SLOTS; i++) {
        if (memcmp(s_scan_emit[i].addr, addr, 6) == 0) {
            if ((now - s_scan_emit[i].last_ms) < GW_SCAN_EMIT_MIN_MS) {
                return false;
            }
            s_scan_emit[i].last_ms = now;
            return true;
        }
        if (s_scan_emit[i].last_ms < s_scan_emit[oldest].last_ms) {
            oldest = i;
        }
    }
    memcpy(s_scan_emit[oldest].addr, addr, 6);
    s_scan_emit[oldest].last_ms = now;
    return true;
}

void app_gateway_on_esl_scan_result(const uint8_t addr[6], uint8_t addr_type, int8_t rssi, bool associated)
{
    char addr_str[18];
    format_addr(addr, addr_str, sizeof(addr_str));
    bool in_inv = app_inventory_has_addr(addr);
    ESP_LOGI(TAG, "scan_result %s rssi=%d associated=%d in_inventory=%d inv_count=%u",
             addr_str, (int)rssi, associated, in_inv,
             app_inventory_get()->count);
    if (s_phone_conn == BLE_HS_CONN_HANDLE_NONE) {
        return;
    }
    if (!scan_result_emit_due(addr)) {
        return;
    }
    char line[160];
    snprintf(line, sizeof(line),
             "{\"event\":\"scan_result\",\"ble_addr\":\"%s\",\"addr_type\":%u,\"rssi\":%d,\"associated\":%s}",
             addr_str, addr_type, (int)rssi, associated ? "true" : "false");
    emit_raw(line);
}

/** PAwR cmds that complete on an ESL response. ACL/OTS cmds (e.g. transfer) must not. */
static bool pending_awaits_esl_response(void)
{
    return strcmp(s_pending_cmd, "ping") == 0 ||
           strcmp(s_pending_cmd, "display_image") == 0 ||
           strcmp(s_pending_cmd, "refresh_display") == 0 ||
           strcmp(s_pending_cmd, "led_control") == 0;
}

void app_gateway_on_esl_response(uint8_t esl_id, uint8_t group_id, int resp_type)
{
    /* PAwR completions are owned by esl_ap_arbiter (opcode-matched).
     * ECP-only paths (unassociate) finish via app_esl_ap, not this hook. */
    (void)esl_id;
    (void)group_id;
    (void)resp_type;
}

void app_gateway_on_esl_cmd_timeout(uint8_t esl_id, uint8_t group_id)
{
    /* Profile CMD_TIMEOUT is ECP-only; PAwR timeouts come from arbiter. */
    (void)esl_id;
    (void)group_id;
}

void app_gateway_on_arbiter_complete(const esl_ap_arbiter_request_t *request, esp_err_t status)
{
    if (request == NULL || !s_busy || s_pending_cmd[0] == '\0') {
        return;
    }
    uint8_t esl_id = (uint8_t)(request->esl_addr & 0xFF);
    uint8_t group_id = request->group_id;
    if (esl_id != s_pending_esl_id || group_id != s_pending_group_id ||
            request->command_opcode != s_pending_opcode) {
        return;
    }
    if (!pending_awaits_esl_response()) {
        return;
    }

    const char *result = "ok";
    if (status == ESP_ERR_TIMEOUT) {
        result = "timeout";
    } else if (status != ESP_OK) {
        result = "error";
    }

    char line[220];
    if (status == ESP_OK) {
        snprintf(line, sizeof(line),
                 "{\"event\":\"command_result\",\"cmd\":\"%s\",\"tag\":{\"esl_id\":%u,\"group_id\":%u},"
                 "\"status\":\"ok\",\"basic_state\":{\"service_needed\":false,\"synchronized\":true,\"active_led\":false}}",
                 s_pending_cmd, esl_id, group_id);
    } else {
        snprintf(line, sizeof(line),
                 "{\"event\":\"command_result\",\"cmd\":\"%s\",\"tag\":{\"esl_id\":%u,\"group_id\":%u},"
                 "\"status\":\"%s\"}",
                 s_pending_cmd, esl_id, group_id, result);
    }
    emit_raw_ex(line, pdMS_TO_TICKS(50));
    s_pending_cmd[0] = '\0';
    s_pending_opcode = 0;
    s_busy = false;
}

void app_gateway_on_esl_state_changed(uint16_t esl_addr, int old_state, int new_state)
{
    (void)old_state;
    uint8_t esl_id = (uint8_t)(esl_addr & 0xFF);
    uint8_t group_id = (uint8_t)((esl_addr >> 8) & 0x7F);
    char line[180];
    snprintf(line, sizeof(line),
             "{\"event\":\"tag_state_changed\",\"tag\":{\"esl_id\":%u,\"group_id\":%u},\"state\":\"%s\"}",
             esl_id, group_id, state_to_str((ble_esl_state_t)new_state));
    emit_raw(line);
}

void app_gateway_on_image_transferred(uint16_t conn_handle, uint8_t image_index, int status)
{
    (void)conn_handle;
    if (strcmp(s_pending_cmd, "transfer_image_to_tag") != 0) {
        ESP_LOGW(TAG, "image_transferred status=%d index=%u but pending=%s (ignored)",
                 status, image_index, s_pending_cmd[0] ? s_pending_cmd : "(none)");
        return;
    }
    char line[180];
    snprintf(line, sizeof(line),
             "{\"event\":\"command_result\",\"cmd\":\"transfer_image_to_tag\","
             "\"tag\":{\"esl_id\":%u,\"group_id\":%u},\"status\":\"%s\",\"image_index\":%u}",
             s_pending_esl_id, s_pending_group_id,
             (status == 0) ? "ok" : "error", image_index);
    emit_raw(line);
    s_pending_cmd[0] = '\0';
    s_busy = false;
}

void app_gateway_finish_command(const char *cmd, uint8_t esl_id, uint8_t group_id,
                                const char *status)
{
    if (cmd == NULL || status == NULL) {
        return;
    }
    char line[200];
    snprintf(line, sizeof(line),
             "{\"event\":\"command_result\",\"cmd\":\"%s\",\"tag\":{\"esl_id\":%u,\"group_id\":%u},"
             "\"status\":\"%s\"}",
             cmd, esl_id, group_id, status);
    emit_raw(line);
    if (s_pending_cmd[0] != '\0' && strcmp(s_pending_cmd, cmd) == 0) {
        ESP_LOGI(TAG, "finish cmd=%s esl_id=%u group=%u status=%s -> busy=0",
                 cmd, esl_id, group_id, status);
        s_pending_cmd[0] = '\0';
        s_busy = false;
    } else {
        ESP_LOGW(TAG, "finish cmd=%s esl_id=%u group=%u status=%s pending=%s busy=%d (no clear)",
                 cmd, esl_id, group_id, status,
                 s_pending_cmd[0] ? s_pending_cmd : "(none)", (int)s_busy);
    }
}

static bool post_gw_work(uint8_t type, uint8_t slot)
{
    if (s_work_queue == NULL) {
        return false;
    }
    gw_work_msg_t msg = {
        .type = type,
        .slot = slot,
    };
    return xQueueSend(s_work_queue, &msg, 0) == pdTRUE;
}

static void emit_recovery_summary_on_worker(void)
{
    esl_ap_lifecycle_status_t life;
    app_recovery_get_lifecycle_status(&life);
    char boot_id[17];
    format_boot_id(boot_id, sizeof(boot_id));
    char line[GW_TX_LINE_MAX];
    int n = snprintf(line, sizeof(line),
                     "{\"event\":\"recovery_summary_changed\",\"boot_id\":\"%s\","
                     "\"recovery_phase\":\"%s\",\"health\":\"%s\",\"pawr_active\":%s}",
                     boot_id,
                     lifecycle_str(life.lifecycle),
                     app_recovery_is_healthy() ? "healthy" : "degraded",
                     app_esl_ap_is_started() ? "true" : "false");
    if (n > 0 && n < (int)sizeof(line)) {
        emit_raw_ex(line, pdMS_TO_TICKS(50));
    }
}

static void emit_tag_recovery_on_worker(uint8_t slot)
{
    esl_product_tag_record_t view;
    if (!app_inventory_tag_view(slot, &view) || view.wire == NULL || view.rt == NULL) {
        return;
    }
    char boot_id[17];
    format_boot_id(boot_id, sizeof(boot_id));
    char tag_json[GW_TAG_JSON_MAX];
    int tag_n = format_tag_json(tag_json, sizeof(tag_json), &view);
    if (tag_n <= 0 || tag_n >= (int)sizeof(tag_json)) {
        return;
    }
    char line[GW_TX_LINE_MAX];
    int n = snprintf(line, sizeof(line),
                     "{\"event\":\"tag_recovery_changed\",\"boot_id\":\"%s\",\"slot\":%u,\"tag\":%s}",
                     boot_id, (unsigned)slot, tag_json);
    if (n > 0 && n < (int)sizeof(line)) {
        emit_raw_ex(line, pdMS_TO_TICKS(50));
    }
}

void app_gateway_on_recovery_summary_changed(void)
{
    /* May run on esp_timer: defer heavy buffers/notify to gw_worker. */
    if (!post_gw_work(GW_WORK_RECOVERY_SUMMARY, 0)) {
        ESP_LOGW(TAG, "recovery_summary work queue full");
    }
}

void app_gateway_on_tag_recovery_changed(uint8_t slot)
{
    /* May run on esp_timer: defer heavy buffers/notify to gw_worker. */
    if (!post_gw_work(GW_WORK_TAG_RECOVERY, slot)) {
        ESP_LOGW(TAG, "tag_recovery work queue full slot=%u", (unsigned)slot);
    }
}

/* GAP subscribe tracking — call from application when phone CCCD changes if available.
 * Fallback: mark subscribed on first successful TX. */
void app_gateway_mark_subscribed(bool subscribed)
{
    s_tx_subscribed = subscribed;
}

static void emit_raw_ex(const char *json_object, TickType_t wait_ticks)
{
    if (json_object == NULL || s_phone_conn == BLE_HS_CONN_HANDLE_NONE || s_tx_val_handle == 0) {
        ESP_LOGW(TAG, "notify skipped (conn=%u handle=%u)",
                 s_phone_conn, s_tx_val_handle);
        return;
    }
    if (s_emit_mutex == NULL ||
            xSemaphoreTake(s_emit_mutex, wait_ticks) != pdTRUE) {
        ESP_LOGW(TAG, "emit_raw: TX mutex busy");
        return;
    }

    char line[GW_TX_LINE_MAX];
    int n = snprintf(line, sizeof(line), "%s\n", json_object);
    if (n <= 0 || n >= (int)sizeof(line)) {
        ESP_LOGW(TAG, "notify line too long n=%d", n);
        xSemaphoreGive(s_emit_mutex);
        return;
    }

    uint16_t mtu = ble_att_mtu(s_phone_conn);
    uint16_t chunk_max = (mtu > 3) ? (uint16_t)(mtu - 3) : 20;

    ESP_LOGI(TAG, "notify %d bytes conn=%u mtu=%u sub=%d",
             n, s_phone_conn, mtu, s_tx_subscribed);

    const int max_retries = 3;
    for (int off = 0; off < n;) {
        uint16_t chunk_len = (uint16_t)(n - off);
        if (chunk_len > chunk_max) {
            chunk_len = chunk_max;
        }
        int rc = BLE_HS_ENOMEM;
        for (int attempt = 0; attempt <= max_retries; attempt++) {
            struct os_mbuf *om = ble_hs_mbuf_from_flat(line + off, chunk_len);
            if (om == NULL) {
                ESP_LOGE(TAG, "notify mbuf alloc failed off=%d attempt=%d", off, attempt);
                rc = BLE_HS_ENOMEM;
            } else {
                /* ble_gatts_notify_custom always consumes om (success or fail).
                 * Do not free it here; retry allocates a fresh mbuf. */
                rc = ble_gatts_notify_custom(s_phone_conn, s_tx_val_handle, om);
                if (rc == 0) {
                    s_tx_subscribed = true;
                    ESP_LOGD(TAG, "notify chunk off=%d len=%u ok", off, chunk_len);
                    break;
                }
                ESP_LOGW(TAG, "notify chunk off=%d len=%u rc=%d attempt=%d",
                         off, chunk_len, rc, attempt);
                if (rc == BLE_HS_ENOTCONN) {
                    xSemaphoreGive(s_emit_mutex);
                    return;
                }
            }
            if (attempt < max_retries && (rc == BLE_HS_ENOMEM || rc == BLE_HS_EREJECT)) {
                vTaskDelay(pdMS_TO_TICKS(5));
                continue;
            }
            break;
        }
        if (rc != 0) {
            if (off > 0) {
                const char nl = '\n';
                struct os_mbuf *om_nl = ble_hs_mbuf_from_flat(&nl, 1);
                if (om_nl != NULL) {
                    (void)ble_gatts_notify_custom(s_phone_conn, s_tx_val_handle, om_nl);
                }
            }
            xSemaphoreGive(s_emit_mutex);
            return;
        }
        off += chunk_len;
    }
    xSemaphoreGive(s_emit_mutex);
}

static void emit_raw(const char *json_object)
{
    emit_raw_ex(json_object, pdMS_TO_TICKS(5000));
}

static void feed_bytes(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        char c = (char)data[i];
        if (s_line_discard) {
            if (c == '\n') {
                s_line_discard = false;
                s_line_len = 0;
            }
            continue;
        }
        if (c == '\n') {
            s_line_acc[s_line_len] = '\0';
            if (s_line_len > 0 && s_line_acc[s_line_len - 1] == '\r') {
                s_line_acc[--s_line_len] = '\0';
            }
            if (s_line_len > 0) {
                handle_line(s_line_acc);
            }
            s_line_len = 0;
            continue;
        }
        if (s_line_len + 1 < sizeof(s_line_acc)) {
            s_line_acc[s_line_len++] = c;
        } else {
            ESP_LOGW(TAG, "line overflow, discard until newline");
            s_line_len = 0;
            s_line_discard = true;
        }
    }
}

static void gw_worker(void *arg)
{
    (void)arg;
    gw_work_msg_t msg;
    while (1) {
        if (xQueueReceive(s_work_queue, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        switch (msg.type) {
        case GW_WORK_NUS_RX:
            feed_bytes(msg.data, msg.len);
            break;
        case GW_WORK_TAG_RECOVERY:
            emit_tag_recovery_on_worker(msg.slot);
            break;
        case GW_WORK_RECOVERY_SUMMARY:
            emit_recovery_summary_on_worker();
            break;
        default:
            break;
        }
    }
}

static void reply_ok(const char *id, const char *data_json)
{
    char line[GW_TX_LINE_MAX];
    int n = snprintf(line, sizeof(line), "{\"v\":1,\"id\":\"%s\",\"ok\":true,\"data\":%s}",
                     id ? id : "", data_json ? data_json : "{}");
    if (n <= 0 || n >= (int)sizeof(line)) {
        ESP_LOGW(TAG, "reply_ok truncated n=%d", n);
        reply_err(id, "internal", "response too large");
        return;
    }
    emit_raw(line);
}

static void reply_err(const char *id, const char *code, const char *message)
{
    char line[GW_TX_LINE_MAX];
    snprintf(line, sizeof(line),
             "{\"v\":1,\"id\":\"%s\",\"ok\":false,\"error\":\"%s\",\"data\":{\"message\":\"%s\"}}",
             id ? id : "", code ? code : "internal", message ? message : "");
    emit_raw(line);
}

/* Phone NUS traffic on the same radio starves PAwR AUX_CONNECT_REQ / OTS. */
static bool reject_if_tag_transfer_busy(const char *id)
{
    if (!s_busy || strcmp(s_pending_cmd, "transfer_image_to_tag") != 0) {
        return false;
    }
    ESP_LOGW(TAG, "upload rejected: transfer_image_to_tag still in flight");
    reply_err(id, "busy", "image transfer to tag in progress");
    return true;
}

static bool accept_pawr_cmd(const char *id, const char *cmd, uint8_t esl_id, uint8_t group_id)
{
    bool allow_during_provision =
        (cmd != NULL && strcmp(cmd, "transfer_image_to_tag") == 0 &&
         app_esl_ap_can_queue_image_update(esl_id, group_id));
    if (s_busy || (app_esl_ap_pipeline_busy() && !allow_during_provision)) {
        ESP_LOGW(TAG, "accept busy: new_cmd=%s pending=%s esl_id=%u group=%u pipeline=%d",
                 cmd ? cmd : "?", s_pending_cmd, s_pending_esl_id, s_pending_group_id,
                 (int)app_esl_ap_pipeline_busy());
        reply_err(id, "busy", "command in flight");
        return false;
    }
    strncpy(s_pending_cmd, cmd, sizeof(s_pending_cmd) - 1);
    s_pending_cmd[sizeof(s_pending_cmd) - 1] = '\0';
    s_pending_esl_id = esl_id;
    s_pending_group_id = group_id;
    s_busy = true;
    ESP_LOGI(TAG, "accept cmd=%s esl_id=%u group=%u busy=1",
             cmd ? cmd : "?", esl_id, group_id);
    reply_ok(id, "{\"status\":\"accepted\"}");
    return true;
}

static void reject_pawr_cmd(const char *code, const char *message)
{
    if (s_pending_cmd[0] == '\0') {
        ESP_LOGW(TAG, "reject_pawr_cmd: no pending cmd code=%s msg=%s",
                 code ? code : "?", message ? message : "");
        return;
    }
    ESP_LOGW(TAG, "reject cmd=%s esl_id=%u group=%u code=%s msg=%s -> busy=0",
             s_pending_cmd, s_pending_esl_id, s_pending_group_id,
             code ? code : "?", message ? message : "");
    char line[220];
    snprintf(line, sizeof(line),
             "{\"event\":\"command_result\",\"cmd\":\"%s\",\"tag\":{\"esl_id\":%u,\"group_id\":%u},"
             "\"status\":\"error\",\"message\":\"%s\"}",
             s_pending_cmd, s_pending_esl_id, s_pending_group_id, message ? message : code);
    emit_raw(line);
    s_pending_cmd[0] = '\0';
    s_busy = false;
}

static bool prepare_pawr_dispatch(const char *id, const char *cmd, uint8_t esl_id, uint8_t group_id)
{
    if (!require_pawr_target_online(id, esl_id, group_id)) {
        return false;
    }
    return accept_pawr_cmd(id, cmd, esl_id, group_id);
}

static uint8_t lookup_record_slot(uint8_t esl_id, uint8_t group_id, uint32_t *out_rec_gen)
{
    esl_product_inventory_t *inv = app_inventory_get();
    if (inv == NULL) {
        return ESL_PRODUCT_MAX_TAGS;
    }
    for (uint8_t i = 0; i < ESL_PRODUCT_MAX_TAGS; i++) {
        esl_product_tag_record_t view;
        if (!app_inventory_tag_view(i, &view) || view.wire == NULL) {
            continue;
        }
        if (view.wire->esl_id == esl_id && view.wire->group_id == group_id) {
            if (out_rec_gen != NULL && view.wire != NULL) {
                *out_rec_gen = esl_product_rd_le32(view.wire->record_generation_le);
            }
            return i;
        }
    }
    return ESL_PRODUCT_MAX_TAGS;
}

static esp_err_t submit_app_pawr(uint8_t esl_id, uint8_t group_id, uint8_t opcode,
                                 esl_ap_expected_response_t expected,
                                 const uint8_t *params, uint8_t params_len)
{
    esl_ap_arbiter_request_t req;
    memset(&req, 0, sizeof(req));
    req.request_id = ++s_next_app_request_id;
    if (req.request_id == 0) {
        req.request_id = ++s_next_app_request_id;
    }
    req.boot_id = app_recovery_boot_id();
    req.recovery_generation = 0;
    uint32_t rec_gen = 0;
    req.record_slot = lookup_record_slot(esl_id, group_id, &rec_gen);
    req.record_generation = rec_gen;
    req.esl_addr = (uint16_t)(esl_id | ((uint16_t)group_id << 8));
    req.group_id = group_id;
    req.command_opcode = opcode;
    req.expected_response = expected;
    req.priority = ESL_AP_ARBITER_PRIO_APP;
    req.guard_timeout_ms = APP_GATEWAY_PAWR_GUARD_MS;
    if (params != NULL && params_len > 0) {
        if (params_len > sizeof(req.params)) {
            return ESP_ERR_INVALID_ARG;
        }
        memcpy(req.params, params, params_len);
        req.params_len = params_len;
    }
    s_pending_opcode = opcode;
    return esl_ap_arbiter_submit(&req);
}

static void finish_pawr_dispatch(const char *cmd, uint8_t esl_id, uint8_t group_id, esp_err_t ret)
{
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "%s dispatch failed esl_id=%u group_id=%u: %s",
                 cmd ? cmd : "?", esl_id, group_id, esp_err_to_name(ret));
        reject_pawr_cmd("not_ready", esp_err_to_name(ret));
        return;
    }
    /* Broadcast: arbiter send_cb completes immediately and notifies via
     * app_gateway_on_arbiter_complete. Unicast waits for Tag response/timeout. */
    (void)cmd;
    (void)esl_id;
    (void)group_id;
}

static void handle_esl_cmd(const char *id, const char *line)
{
    /* Prefer the inner data object so outer envelope values (e.g. id="cmd")
     * cannot collide with command-field key lookups. */
    const char *json = line;
    const char *data = strstr(line, "\"data\":{");
    if (data != NULL) {
        json = data + 7; /* skip "data": and point at '{' */
    }

    const char *cmd = find_cmd(json);
    if (cmd == NULL) {
        reply_err(id, "invalid_argument", "missing cmd");
        return;
    }

    int esl_id = 0, group_id = 0, display_index = 0, image_index = 0;
    parse_tag_selector(json, &esl_id, &group_id);
    json_get_int(json, "display_index", &display_index);
    json_get_int(json, "image_index", &image_index);

    if (strcmp(cmd, "get_ap_status") == 0) {
        esl_product_inventory_t *inv = app_inventory_get();
        esl_ap_lifecycle_status_t life;
        app_recovery_get_lifecycle_status(&life);
        bool started = app_esl_ap_is_started();
        char boot_id[17];
        format_boot_id(boot_id, sizeof(boot_id));

        ESP_LOGI(TAG, "get_ap_status: inv=%p count=%u esl_ap_started=%d phase=%s",
                 (void *)inv, inv ? inv->count : 0, started, lifecycle_str(life.lifecycle));
        app_inventory_dump("get_ap_status");

        char data[GW_DATA_MAX];
        int pos = snprintf(data, sizeof(data),
                           "{\"started\":%s,\"scanning\":%s,\"pawr_active\":%s,\"tag_count\":%u,"
                           "\"selected_index\":%u,\"phone_link\":\"%s\",\"last_error\":null,"
                           "\"boot_id\":\"%s\",\"recovery_phase\":\"%s\",\"health\":\"%s\"",
                           started ? "true" : "false",
                           ble_gap_disc_active() ? "true" : "false",
                           started ? "true" : "false",
                           inv ? inv->count : 0,
                           inv ? inv->selected_index : 0,
                           app_gateway_is_phone_connected() ? "connected" : "disconnected",
                           boot_id,
                           lifecycle_str(life.lifecycle),
                           app_recovery_is_healthy() ? "healthy" : "degraded");
        if (pos <= 0 || pos >= (int)sizeof(data)) {
            reply_err(id, "internal", "status too large");
            return;
        }

        if (life.lifecycle == ESL_AP_LIFECYCLE_FAILED) {
            int64_t next_ms = life.next_retry_uptime_us > 0 ?
                              (life.next_retry_uptime_us / 1000) : 0;
            int n = snprintf(data + pos, sizeof(data) - (size_t)pos,
                             ",\"retryable\":%s,\"error_code\":\"%s\",\"retry_count\":%u,"
                             "\"next_retry_uptime_ms\":%" PRId64 ",\"recommended_action\":\"%s\"",
                             life.retryable ? "true" : "false",
                             error_code_str(life.error_code),
                             (unsigned)life.retry_count,
                             next_ms,
                             recommended_action_str(life.error_code, life.retryable));
            if (n <= 0 || n >= (int)(sizeof(data) - (size_t)pos)) {
                reply_err(id, "internal", "status too large");
                return;
            }
            pos += n;
        }

        uint8_t orphan_count = 0;
        const esl_ap_orphan_bond_t *orphans = app_inventory_orphan_bonds(&orphan_count);
        if (orphan_count > 0 && orphans != NULL) {
            int checkpoint = pos;
            int n = snprintf(data + pos, sizeof(data) - (size_t)pos, ",\"orphan_bonds\":[");
            if (n > 0 && n < (int)(sizeof(data) - (size_t)pos)) {
                pos += n;
                bool first = true;
                bool orphans_ok = true;
                for (uint8_t i = 0; i < orphan_count; i++) {
                    char addr[18];
                    format_addr(orphans[i].peer_id_addr, addr, sizeof(addr));
                    int64_t seen_ms = orphans[i].first_seen_uptime_us > 0 ?
                                      (orphans[i].first_seen_uptime_us / 1000) : 0;
                    char item[128];
                    int item_n = snprintf(item, sizeof(item),
                                          "%s{\"addr\":\"%s\",\"addr_type\":%u,\"first_seen_uptime_ms\":%" PRId64 "}",
                                          first ? "" : ",",
                                          addr, orphans[i].addr_type, seen_ms);
                    if (item_n <= 0 || item_n >= (int)sizeof(item) ||
                            (size_t)item_n + 2 >= sizeof(data) - (size_t)pos) {
                        orphans_ok = false;
                        break;
                    }
                    memcpy(data + pos, item, (size_t)item_n);
                    pos += item_n;
                    first = false;
                }
                if (orphans_ok) {
                    n = snprintf(data + pos, sizeof(data) - (size_t)pos, "]");
                    if (n > 0 && n < (int)(sizeof(data) - (size_t)pos)) {
                        pos += n;
                    } else {
                        orphans_ok = false;
                    }
                }
                if (!orphans_ok) {
                    pos = checkpoint;
                    data[pos] = '\0';
                }
            }
        }

        int n = snprintf(data + pos, sizeof(data) - (size_t)pos, "}");
        if (n <= 0 || n >= (int)(sizeof(data) - (size_t)pos)) {
            reply_err(id, "internal", "status too large");
            return;
        }
        reply_ok(id, data);
        return;
    }

    if (strcmp(cmd, "list_tags") == 0) {
        esl_product_inventory_t *inv = app_inventory_get();
        uint8_t slots[ESL_PRODUCT_MAX_TAGS];
        uint8_t total = 0;
        if (inv) {
            for (uint8_t i = 0; i < ESL_PRODUCT_MAX_TAGS; i++) {
                if (inv->blob.tags[i].valid == 1) {
                    slots[total++] = i;
                }
            }
        }

        int offset = 0;
        int limit = -1;
        bool has_offset = json_get_int(json, "offset", &offset);
        bool has_limit = json_get_int(json, "limit", &limit);
        bool wants_pagination = has_offset || has_limit;
        if (offset < 0) {
            offset = 0;
        }
        if (offset > (int)total) {
            offset = (int)total;
        }
        if (has_limit && limit < 0) {
            limit = 0;
        }

        char data[GW_DATA_MAX];
        int pos = snprintf(data, sizeof(data),
                           "{\"selected_index\":%u,\"offset\":%d,\"total\":%u,\"tags\":[",
                           inv ? inv->selected_index : 0, offset, total);
        if (pos <= 0 || pos >= (int)sizeof(data)) {
            reply_err(id, "internal", "list_tags header too large");
            return;
        }

        uint8_t listed = 0;
        bool first = true;
        bool truncated = false;
        for (uint8_t i = (uint8_t)offset; i < total; i++) {
            if (has_limit && listed >= (uint8_t)limit) {
                break;
            }
            esl_product_tag_record_t view;
            if (!app_inventory_tag_view(slots[i], &view)) {
                continue;
            }
            char tag_json[GW_TAG_JSON_MAX];
            int tag_n = format_tag_json(tag_json, sizeof(tag_json), &view);
            if (tag_n <= 0 || tag_n >= (int)sizeof(tag_json)) {
                reply_err(id, "internal", "tag encode failed");
                return;
            }
            /* Need room for comma + tag + "],\"next_offset\":NN}" trailer (~32). */
            size_t need = (size_t)tag_n + (first ? 0 : 1) + 32;
            if ((size_t)pos + need >= sizeof(data)) {
                truncated = true;
                break;
            }
            if (!first) {
                data[pos++] = ',';
            }
            memcpy(data + pos, tag_json, (size_t)tag_n);
            pos += tag_n;
            first = false;
            listed++;
        }

        if (!wants_pagination && truncated) {
            reply_err(id, "upgrade_required",
                      "list_tags exceeds frame; require list-tags-pagination capability");
            return;
        }
        if (truncated && listed == 0 && offset < total) {
            reply_err(id, "upgrade_required", "single tag exceeds list_tags frame");
            return;
        }

        int next_offset = offset + listed;
        if (next_offset > total) {
            next_offset = total;
        }
        /* next_offset == total means no more pages */
        int n = snprintf(data + pos, sizeof(data) - (size_t)pos,
                         "],\"next_offset\":%d}", next_offset);
        if (n <= 0 || n >= (int)(sizeof(data) - (size_t)pos)) {
            reply_err(id, "internal", "list_tags trailer too large");
            return;
        }

        ESP_LOGI(TAG, "list_tags: total=%u offset=%d listed=%u next=%d paginate=%d",
                 total, offset, listed, next_offset, wants_pagination);
        app_inventory_dump("list_tags");
        reply_ok(id, data);
        return;
    }

    if (strcmp(cmd, "scan_start") == 0) {
        esp_err_t sret = app_esl_ap_start_scan();
        if (sret == ESP_ERR_INVALID_STATE) {
            reply_err(id, "busy", "scan start not available");
            return;
        }
        if (sret != ESP_OK) {
            reply_err(id, "internal", esp_err_to_name(sret));
            return;
        }
        ESP_LOGI(TAG, "scan_start -> discovery start ret=%s", esp_err_to_name(sret));
        reply_ok(id, "{\"status\":\"accepted\"}");
        return;
    }

    if (strcmp(cmd, "scan_stop") == 0) {
        esp_err_t sret = app_esl_ap_stop_scan();
        if (sret == ESP_ERR_INVALID_STATE) {
            reply_err(id, "busy", "scan stop not available");
            return;
        }
        if (sret != ESP_OK) {
            reply_err(id, "internal", esp_err_to_name(sret));
            return;
        }
        ESP_LOGI(TAG, "scan_stop -> discovery stopped");
        reply_ok(id, "{\"status\":\"accepted\"}");
        return;
    }

    if (strcmp(cmd, "provision_tag") == 0) {
        char addr_str[20] = { 0 };
        int addr_type = 0;
        if (!json_get_tag_ble_addr(json, addr_str, sizeof(addr_str), &addr_type)) {
            reply_err(id, "invalid_argument", "missing tag.ble_addr");
            return;
        }
        uint8_t addr[6];
        if (!parse_ble_addr(addr_str, addr)) {
            reply_err(id, "invalid_argument", "invalid ble_addr");
            return;
        }

        int esl_id = 0;
        int group_id = (int)CONFIG_ESL_AP_DEFAULT_GROUP_ID;
        bool has_esl_id = json_get_int(json, "esl_id", &esl_id);
        (void)json_get_int(json, "group_id", &group_id);

        if (!has_esl_id) {
            esl_product_tag_record_t *existing = app_inventory_find_by_addr(addr);
            if (existing != NULL &&
                    (existing->wire->provision_phase == ESL_PRODUCT_PHASE_PREPARED ||
                     existing->wire->provision_phase == ESL_PRODUCT_PHASE_BONDED ||
                     existing->wire->provision_phase == ESL_PRODUCT_PHASE_CONFIGURED)) {
                esl_id = existing->wire->esl_id;
                group_id = existing->wire->group_id;
            } else {
                uint8_t alloc_id = 0;
                uint8_t alloc_group = (uint8_t)group_id;
                if (app_inventory_allocate_esl_id(&alloc_id, &alloc_group) != ESP_OK) {
                    reply_err(id, "busy", "no free esl_id");
                    return;
                }
                esl_id = alloc_id;
                group_id = alloc_group;
            }
        } else if (esl_id < 0 || esl_id > 0xFE) {
            reply_err(id, "invalid_argument", "esl_id out of range");
            return;
        }

        if (app_esl_ap_is_provisioning()) {
            reply_err(id, "busy", "provision in progress");
            return;
        }

        ESP_LOGI(TAG, "provision_tag %s addr_type=%d esl_id=%d group=%d",
                 addr_str, addr_type, esl_id, group_id);
        esp_err_t pret = app_esl_ap_provision_tag(addr, (uint8_t)addr_type,
                                                  (uint8_t)esl_id, (uint8_t)group_id);
        if (pret == ESP_ERR_INVALID_STATE) {
            reply_err(id, "busy", "cannot start provision");
            return;
        }
        if (pret == ESP_ERR_INVALID_ARG) {
            reply_err(id, "invalid_argument", "tag or esl_id already provisioned");
            return;
        }
        if (pret != ESP_OK) {
            reply_err(id, "internal", esp_err_to_name(pret));
            return;
        }

        char data[96];
        snprintf(data, sizeof(data),
                 "{\"status\":\"accepted\",\"esl_id\":%d,\"group_id\":%d}",
                 esl_id, group_id);
        reply_ok(id, data);
        return;
    }

    if (strcmp(cmd, "ping") == 0) {
        if (!prepare_pawr_dispatch(id, cmd, (uint8_t)esl_id, (uint8_t)group_id)) {
            return;
        }
        uint8_t params[1] = { (uint8_t)esl_id };
        esp_err_t pret = submit_app_pawr((uint8_t)esl_id, (uint8_t)group_id,
                                         BLE_ESL_CMD_PING, ESL_AP_EXPECT_BASIC_STATE,
                                         params, sizeof(params));
        finish_pawr_dispatch(cmd, (uint8_t)esl_id, (uint8_t)group_id, pret);
        return;
    }

    if (strcmp(cmd, "display_image") == 0) {
        if (!prepare_pawr_dispatch(id, cmd, (uint8_t)esl_id, (uint8_t)group_id)) {
            return;
        }
        uint8_t params[3] = { (uint8_t)esl_id, (uint8_t)display_index, (uint8_t)image_index };
        esp_err_t dret = submit_app_pawr((uint8_t)esl_id, (uint8_t)group_id,
                                         BLE_ESL_CMD_DISPLAY_IMAGE,
                                         ESL_AP_EXPECT_COMMAND_SPECIFIC,
                                         params, sizeof(params));
        finish_pawr_dispatch(cmd, (uint8_t)esl_id, (uint8_t)group_id, dret);
        return;
    }

    if (strcmp(cmd, "refresh_display") == 0) {
        if (!prepare_pawr_dispatch(id, cmd, (uint8_t)esl_id, (uint8_t)group_id)) {
            return;
        }
        uint8_t params[2] = { (uint8_t)esl_id, (uint8_t)display_index };
        esp_err_t rret = submit_app_pawr((uint8_t)esl_id, (uint8_t)group_id,
                                         BLE_ESL_CMD_REFRESH_DISPLAY,
                                         ESL_AP_EXPECT_COMMAND_SPECIFIC,
                                         params, sizeof(params));
        finish_pawr_dispatch(cmd, (uint8_t)esl_id, (uint8_t)group_id, rret);
        return;
    }

    if (strcmp(cmd, "cancel_provision") == 0) {
        esp_err_t cret = app_inventory_cancel_provision((uint8_t)esl_id, (uint8_t)group_id);
        if (cret == ESP_ERR_NOT_FOUND) {
            reply_err(id, "tag_not_found", "tag not in inventory");
            return;
        }
        if (cret == ESP_ERR_INVALID_STATE) {
            reply_err(id, "invalid_argument", "cancel only for incomplete provision");
            return;
        }
        if (cret != ESP_OK) {
            reply_err(id, "internal", esp_err_to_name(cret));
            return;
        }
        reply_ok(id, "{\"status\":\"ok\"}");
        return;
    }

    if (strcmp(cmd, "unassociate") == 0) {
        if (is_group_broadcast((uint8_t)esl_id)) {
            reply_err(id, "invalid_argument", "unassociate does not support group broadcast");
            return;
        }
        if (!app_inventory_esl_id_in_use((uint8_t)esl_id, (uint8_t)group_id)) {
            reply_err(id, "tag_not_found", "tag not in inventory");
            return;
        }
        if (!accept_pawr_cmd(id, cmd, (uint8_t)esl_id, (uint8_t)group_id)) {
            return;
        }
        ESP_LOGI(TAG, "unassociate dispatch start esl_id=%u group=%u",
                 (unsigned)esl_id, (unsigned)group_id);
        esp_err_t uret = app_esl_ap_unassociate_tag((uint8_t)esl_id, (uint8_t)group_id);
        ESP_LOGI(TAG, "unassociate dispatch ret=%s", esp_err_to_name(uret));
        if (uret != ESP_OK) {
            reject_pawr_cmd("not_ready", esp_err_to_name(uret));
            return;
        }
        return;
    }

    if (strcmp(cmd, "led_control") == 0) {
        int color_red = 3, color_green = 0, color_blue = 0, brightness = 3;
        int repeat_type = 1, repeats_duration = 0, led_index = 0;
        int bit_off_period = 0, bit_on_period = 0;
        char pattern_hex[16] = { 0 };
        json_get_int(json, "color_red", &color_red);
        json_get_int(json, "color_green", &color_green);
        json_get_int(json, "color_blue", &color_blue);
        json_get_int(json, "brightness", &brightness);
        json_get_int(json, "repeat_type", &repeat_type);
        json_get_int(json, "repeats_duration", &repeats_duration);
        json_get_int(json, "led_index", &led_index);
        json_get_int(json, "bit_off_period", &bit_off_period);
        json_get_int(json, "bit_on_period", &bit_on_period);
        bool has_pattern = json_get_string(json, "pattern", pattern_hex, sizeof(pattern_hex));

        esl_ap_led_settings_t led = {
            .color_red = (uint8_t)color_red,
            .color_green = (uint8_t)color_green,
            .color_blue = (uint8_t)color_blue,
            .brightness = (uint8_t)brightness,
            .repeat_type = (uint8_t)repeat_type,
            .repeats_duration = (uint16_t)repeats_duration,
        };

        if (has_pattern) {
            /* pattern: exactly 10 hex chars = 40-bit flashing pattern (LE bytes). */
            if (strlen(pattern_hex) != 10) {
                reply_err(id, "invalid_argument", "pattern must be 10 hex chars");
                return;
            }
            uint8_t pat_bytes[5] = { 0 };
            for (int i = 0; i < 5; i++) {
                char byte_str[3] = { pattern_hex[i * 2], pattern_hex[i * 2 + 1], '\0' };
                char *end = NULL;
                unsigned long v = strtoul(byte_str, &end, 16);
                if (end == byte_str || *end != '\0' || v > 0xFFUL) {
                    reply_err(id, "invalid_argument", "pattern has invalid hex");
                    return;
                }
                pat_bytes[i] = (uint8_t)v;
            }
            if (bit_off_period < 1 || bit_off_period > 255 ||
                    bit_on_period < 1 || bit_on_period > 255) {
                reply_err(id, "invalid_argument", "bit_on/off_period must be 1-255");
                return;
            }
            if (repeats_duration <= 0) {
                reply_err(id, "invalid_argument", "flashing requires repeats_duration > 0");
                return;
            }
            ble_esl_flashing_pattern_t flash_pat;
            memset(&flash_pat, 0, sizeof(flash_pat));
            memcpy(flash_pat.pattern, pat_bytes, sizeof(pat_bytes));
            flash_pat.bit_off_period = (uint8_t)bit_off_period;
            flash_pat.bit_on_period = (uint8_t)bit_on_period;
            memcpy(led.flashing_pattern, &flash_pat, sizeof(flash_pat));
        } else if (repeats_duration > 0 &&
                   (bit_off_period != 0 || bit_on_period != 0)) {
            reply_err(id, "invalid_argument", "pattern required when bit periods set");
            return;
        }

        if (!prepare_pawr_dispatch(id, cmd, (uint8_t)esl_id, (uint8_t)group_id)) {
            return;
        }
        uint8_t params[12];
        uint8_t pos = 0;
        params[pos++] = (uint8_t)esl_id;
        params[pos++] = (uint8_t)led_index;
        params[pos++] = BLE_ESL_LED_CTRL_COLOR_BRIGHTNESS(
                            led.color_red, led.color_green, led.color_blue, led.brightness);
        memcpy(&params[pos], led.flashing_pattern, 7);
        pos += 7;
        uint16_t repeat = BLE_ESL_LED_CTRL_REPEAT(led.repeat_type, led.repeats_duration);
        params[pos++] = (uint8_t)(repeat & 0xFF);
        params[pos++] = (uint8_t)((repeat >> 8) & 0xFF);
        esp_err_t lret = submit_app_pawr((uint8_t)esl_id, (uint8_t)group_id,
                                         BLE_ESL_CMD_LED_CONTROL,
                                         ESL_AP_EXPECT_COMMAND_SPECIFIC,
                                         params, pos);
        finish_pawr_dispatch(cmd, (uint8_t)esl_id, (uint8_t)group_id, lret);
        return;
    }

    if (strcmp(cmd, "upload_image_begin") == 0) {
        if (reject_if_tag_transfer_busy(id)) {
            return;
        }
        int image_id = 0, version = 1, data_len = 0;
        char checksum[16] = { 0 };
        json_get_int(json, "image_id", &image_id);
        json_get_int(json, "version", &version);
        json_get_int(json, "data_len", &data_len);
        json_get_string(json, "checksum", checksum, sizeof(checksum));
        if (data_len <= 0 || data_len > (int)sizeof(s_up_buf)) {
            reply_err(id, "image_invalid", "data_len out of range");
            return;
        }
        s_up_active = true;
        s_up_image_id = (uint32_t)image_id;
        s_up_version = (uint32_t)version;
        s_up_expected = (uint32_t)data_len;
        s_up_received = 0;
        s_up_crc_expect = (uint32_t)strtoul(checksum, NULL, 16);
        memset(s_up_buf, 0, sizeof(s_up_buf));
        reply_ok(id, "{\"status\":\"accepted\"}");
        return;
    }

    if (strcmp(cmd, "upload_image_chunk") == 0) {
        if (reject_if_tag_transfer_busy(id)) {
            s_up_active = false;
            return;
        }
        if (!s_up_active) {
            reply_err(id, "not_ready", "no upload session");
            return;
        }
        int offset = 0;
        json_get_int(json, "offset", &offset);
        if (offset < 0) {
            reply_err(id, "invalid_argument", "offset < 0");
            return;
        }
        /* Wire shape is {"op":"esl","data":{...,"data":"<b64>"}}.
         * Match the string-valued payload ("data":"), not the outer object ("data":{). */
        const char *p = strstr(json, "\"data\":\"");
        if (p == NULL) {
            reply_err(id, "invalid_argument", "missing data");
            return;
        }
        p += strlen("\"data\":\"");
        const char *end = strchr(p, '"');
        if (end == NULL) {
            reply_err(id, "invalid_argument", "bad data end");
            return;
        }
        char b64[520];
        size_t blen = (size_t)(end - p);
        if (blen >= sizeof(b64)) {
            reply_err(id, "image_invalid", "chunk too large");
            return;
        }
        memcpy(b64, p, blen);
        b64[blen] = '\0';
        uint8_t decoded[260];
        size_t dlen = 0;
        if (b64_decode(b64, decoded, sizeof(decoded), &dlen) != 0) {
            reply_err(id, "image_invalid", "base64 decode failed");
            return;
        }
        if ((uint32_t)offset > sizeof(s_up_buf) ||
                dlen > sizeof(s_up_buf) - (uint32_t)offset ||
                (uint32_t)offset > s_up_expected ||
                dlen > s_up_expected - (uint32_t)offset) {
            reply_err(id, "image_invalid", "chunk overflow");
            return;
        }
        if ((uint32_t)offset + dlen > sizeof(s_up_buf)) {
            reply_err(id, "image_invalid", "chunk overflow");
            return;
        }
        memcpy(s_up_buf + offset, decoded, dlen);
        if ((uint32_t)offset + dlen > s_up_received) {
            s_up_received = (uint32_t)offset + (uint32_t)dlen;
        }
        char data[64];
        snprintf(data, sizeof(data), "{\"status\":\"accepted\",\"offset\":%d}", offset);
        reply_ok(id, data);
        return;
    }

    if (strcmp(cmd, "upload_image_end") == 0) {
        if (reject_if_tag_transfer_busy(id)) {
            s_up_active = false;
            return;
        }
        if (!s_up_active) {
            reply_err(id, "not_ready", "no upload session");
            return;
        }
        if (s_up_received != s_up_expected) {
            reply_err(id, "image_invalid", "length mismatch");
            s_up_active = false;
            return;
        }
        uint32_t crc = crc32_calc(s_up_buf, s_up_received);
        if (crc != s_up_crc_expect) {
            reply_err(id, "image_invalid", "checksum mismatch");
            s_up_active = false;
            return;
        }
        uint8_t slot = 0;
        json_get_int(json, "image_index", (int *)&image_index);
        slot = (uint8_t)image_index;
        esp_err_t er = esl_image_manager_store_runtime(s_up_image_id, slot, s_up_version,
                                                       s_up_buf, s_up_received);
        s_up_active = false;
        if (er != ESP_OK) {
            reply_err(id, "internal", "store failed");
            return;
        }
        char data[96];
        snprintf(data, sizeof(data), "{\"status\":\"stored\",\"image_id\":%" PRIu32 "}", s_up_image_id);
        reply_ok(id, data);
        char ev[128];
        snprintf(ev, sizeof(ev),
                 "{\"event\":\"image_uploaded\",\"image_id\":%" PRIu32 ",\"version\":%" PRIu32 ",\"data_len\":%" PRIu32 "}",
                 s_up_image_id, s_up_version, s_up_received);
        emit_raw(ev);
        return;
    }

    if (strcmp(cmd, "transfer_image_to_tag") == 0) {
        if (is_group_broadcast((uint8_t)esl_id)) {
            reply_err(id, "invalid_argument", "transfer_image_to_tag does not support group broadcast");
            return;
        }
        int image_id = 0;
        json_get_int(json, "image_id", &image_id);
        esl_image_desc_t img = { 0 };
        if (esl_image_manager_get((uint32_t)image_id, &img) != ESP_OK) {
            reply_err(id, "image_invalid", "image not found");
            return;
        }
        if (!accept_pawr_cmd(id, cmd, (uint8_t)esl_id, (uint8_t)group_id)) {
            return;
        }
        esp_err_t xret = app_esl_ap_request_image_update_id((uint8_t)esl_id,
                                                            (uint8_t)group_id,
                                                            (uint32_t)image_id);
        if (xret != ESP_OK) {
            if (s_pending_cmd[0] != '\0') {
                reject_pawr_cmd("not_ready", esp_err_to_name(xret));
            }
            return;
        }
        char prog[128];
        snprintf(prog, sizeof(prog),
                 "{\"event\":\"image_transfer_progress\",\"esl_id\":%u,\"offset\":0,\"total\":%" PRIu32 "}",
                 esl_id, img.data_len);
        emit_raw(prog);
        return;
    }

    reply_err(id, "unsupported_cmd", cmd);
}

static void handle_line(const char *line)
{
    char id[48] = { 0 };
    char op[16] = { 0 };
    json_get_string(line, "id", id, sizeof(id));
    json_get_string(line, "op", op, sizeof(op));

    if (strcmp(op, "hello") == 0) {
        ESP_LOGI(TAG, "hello from phone id=%s conn=%u subscribed=%d esl_ap_started=%d",
                 id[0] ? id : "(none)", s_phone_conn, s_tx_subscribed, app_esl_ap_is_started());
        app_inventory_dump("hello");
        reply_ok(id,
                 "{\"protocol\":\"esl-jsonl-v1\",\"fw_ver\":\"0.1.0\",\"device_name\":\"ESP-ESL-AP\","
                 "\"max_chunk\":192,\"max_line\":1024,\"encrypted\":false,"
                 "\"capabilities\":[\"hello\",\"get_ap_status\",\"list_tags\",\"scan_start\",\"scan_stop\","
                 "\"provision_tag\",\"cancel_provision\",\"unassociate\",\"ping\",\"led_control\","
                 "\"display_image\",\"refresh_display\",\"upload_image_begin\",\"upload_image_chunk\","
                 "\"upload_image_end\",\"transfer_image_to_tag\",\"recovery-v1\",\"list-tags-pagination\","
                 "\"group-broadcast\"]}");
        return;
    }
    if (strcmp(op, "ping") == 0) {
        /* Echo data object if present; otherwise empty. */
        const char *data = strstr(line, "\"data\"");
        if (data) {
            const char *brace = strchr(data, '{');
            if (brace) {
                /* naive: reply with whole remaining object truncated safely */
                char echo[256];
                strncpy(echo, brace, sizeof(echo) - 1);
                echo[sizeof(echo) - 1] = '\0';
                /* Truncation can drop the envelope '}'; do not emit broken JSON. */
                if (strlen(echo) == sizeof(echo) - 1) {
                    ESP_LOGW(TAG, "ping data truncated; echoing empty object");
                    reply_ok(id, "{}");
                    return;
                }
                /* trim trailing envelope close brace only */
                char *end = strrchr(echo, '}');
                if (end) {
                    *end = '\0';
                }
                reply_ok(id, echo);
                return;
            }
        }
        reply_ok(id, "{}");
        return;
    }
    if (strcmp(op, "esl") == 0) {
        handle_esl_cmd(id, line);
        return;
    }
    reply_err(id, "unsupported_cmd", op);
}

/* ---- tiny helpers ---- */

static int format_boot_id(char *out, size_t out_len)
{
    return snprintf(out, out_len, "%016" PRIx64, app_recovery_boot_id());
}

static const char *state_to_str(ble_esl_state_t st)
{
    switch (st) {
    case BLE_ESL_STATE_UNASSOCIATED: return "unassociated";
    case BLE_ESL_STATE_CONFIGURING: return "configuring";
    case BLE_ESL_STATE_SYNCHRONIZED: return "synchronized";
    case BLE_ESL_STATE_UPDATING: return "updating";
    case BLE_ESL_STATE_UNSYNCHRONIZED: return "unsynchronized";
    default: return "unknown";
    }
}

static const char *product_state_str(esl_product_state_t st)
{
    switch (st) {
    case ESL_PRODUCT_STATE_RECOVERING: return "recovering";
    case ESL_PRODUCT_STATE_ONLINE: return "online";
    case ESL_PRODUCT_STATE_RETRY_WAIT: return "retry_wait";
    case ESL_PRODUCT_STATE_OFFLINE: return "offline";
    case ESL_PRODUCT_STATE_QUARANTINED: return "quarantined";
    default: return "invalid";
    }
}

static const char *desired_state_str(uint8_t desired)
{
    switch (desired) {
    case ESL_PRODUCT_DESIRED_ACTIVE: return "active";
    case ESL_PRODUCT_DESIRED_DISABLED: return "disabled";
    case ESL_PRODUCT_DESIRED_RETIRED: return "retired";
    default: return "invalid";
    }
}

static const char *provision_phase_str(uint8_t phase)
{
    switch (phase) {
    case ESL_PRODUCT_PHASE_PREPARED: return "prepared";
    case ESL_PRODUCT_PHASE_BONDED: return "bonded";
    case ESL_PRODUCT_PHASE_CONFIGURED: return "configured";
    case ESL_PRODUCT_PHASE_ACTIVE: return "active";
    default: return "invalid";
    }
}

static const char *failure_reason_str(esl_ap_failure_reason_t reason)
{
    switch (reason) {
    case ESL_AP_FAIL_UNREACHABLE: return "unreachable";
    case ESL_AP_FAIL_ACL_TIMEOUT: return "acl_timeout";
    case ESL_AP_FAIL_ACL_ENC_FAILED: return "acl_enc_failed";
    case ESL_AP_FAIL_WRITE_NOT_PERMITTED: return "write_not_permitted";
    case ESL_AP_FAIL_PAST_FAILED: return "past_failed";
    case ESL_AP_FAIL_PING_TIMEOUT: return "ping_timeout";
    case ESL_AP_FAIL_TIME_SYNC_FAILED: return "time_sync_failed";
    case ESL_AP_FAIL_HEALTH_MISS: return "health_miss";
    case ESL_AP_FAIL_ADVERTISING_DETECTED: return "advertising_detected";
    case ESL_AP_FAIL_BOND_MISMATCH: return "bond_mismatch";
    case ESL_AP_FAIL_IDENTITY_UNCERTAIN: return "identity_uncertain";
    case ESL_AP_FAIL_KEY_MISSING: return "key_missing";
    case ESL_AP_FAIL_RECORD_CORRUPT: return "record_corrupt";
    case ESL_AP_FAIL_NONE:
    case ESL_AP_FAIL_INVALID:
    default:
        return NULL;
    }
}

static const char *quarantine_reason_str(uint8_t reason)
{
    switch (reason) {
    case ESL_PRODUCT_QUARANTINE_BOND_MISMATCH: return "bond_mismatch";
    case ESL_PRODUCT_QUARANTINE_IDENTITY_UNCERTAIN: return "identity_uncertain";
    case ESL_PRODUCT_QUARANTINE_KEY_MISSING: return "key_missing";
    case ESL_PRODUCT_QUARANTINE_RECORD_CORRUPT: return "record_corrupt";
    case ESL_PRODUCT_QUARANTINE_TAG_UNASSOCIATED: return "tag_unassociated";
    default: return NULL;
    }
}

static const char *lifecycle_str(esl_ap_lifecycle_t life)
{
    switch (life) {
    case ESL_AP_LIFECYCLE_BOOTING: return "booting";
    case ESL_AP_LIFECYCLE_RECOVERING: return "recovering";
    case ESL_AP_LIFECYCLE_READY: return "ready";
    case ESL_AP_LIFECYCLE_FAILED: return "failed";
    default: return "unknown";
    }
}

static const char *error_code_str(esl_ap_error_code_t err)
{
    switch (err) {
    case ESL_AP_ERROR_NONE: return "none";
    case ESL_AP_ERROR_PAWR_INIT_FAILED: return "pawr_init_failed";
    case ESL_AP_ERROR_CONTROLLER_INIT_FAILED: return "controller_init_failed";
    case ESL_AP_ERROR_INVENTORY_UNREADABLE: return "inventory_unreadable";
    case ESL_AP_ERROR_KEY_STORE_UNAVAILABLE: return "key_store_unavailable";
    default: return "invalid";
    }
}

static const char *recommended_action_str(esl_ap_error_code_t err, bool retryable)
{
    switch (err) {
    case ESL_AP_ERROR_INVENTORY_UNREADABLE:
    case ESL_AP_ERROR_KEY_STORE_UNAVAILABLE:
        return "export_diagnostics_then_physical_clear";
    case ESL_AP_ERROR_PAWR_INIT_FAILED:
    case ESL_AP_ERROR_CONTROLLER_INIT_FAILED:
        return retryable ? "retry_after_delay" : "reboot_ap";
    default:
        return retryable ? "retry_after_delay" : "reboot_ap";
    }
}

static const char *legacy_sync_state(const esl_product_tag_record_t *rec)
{
    if (rec != NULL && rec->rt != NULL &&
            rec->rt->product_state == ESL_PRODUCT_STATE_ONLINE &&
            rec->rt->time_synced) {
        return "synchronized";
    }
    return "unsynchronized";
}

static bool find_tag_view(uint8_t esl_id, uint8_t group_id, esl_product_tag_record_t *out)
{
    esl_product_inventory_t *inv = app_inventory_get();
    if (inv == NULL || out == NULL) {
        return false;
    }
    for (uint8_t i = 0; i < ESL_PRODUCT_MAX_TAGS; i++) {
        esl_product_tag_wire_t *w = &inv->blob.tags[i];
        if (w->valid == 1 && w->esl_id == esl_id && w->group_id == group_id) {
            out->wire = w;
            out->rt = &inv->runtime[i];
            out->slot = i;
            return true;
        }
    }
    return false;
}

static bool require_tag_online(const char *id, uint8_t esl_id, uint8_t group_id)
{
    esl_product_tag_record_t view;
    if (!find_tag_view(esl_id, group_id, &view)) {
        reply_err(id, "tag_not_found", "tag not in inventory");
        return false;
    }
    if (view.rt->product_state != ESL_PRODUCT_STATE_ONLINE) {
        reply_err(id, "not_ready", "tag product_state is not online");
        return false;
    }
    return true;
}

static bool is_group_broadcast(uint8_t esl_id)
{
    return esl_id == BLE_ESL_BROADCAST_ADDRESS;
}

static bool require_group_online(const char *id, uint8_t group_id)
{
    esl_product_inventory_t *inv = app_inventory_get();
    if (inv == NULL) {
        reply_err(id, "not_ready", "inventory unavailable");
        return false;
    }
    for (uint8_t i = 0; i < ESL_PRODUCT_MAX_TAGS; i++) {
        esl_product_tag_wire_t *w = &inv->blob.tags[i];
        if (w->valid == 1 && w->group_id == group_id &&
                inv->runtime[i].product_state == ESL_PRODUCT_STATE_ONLINE) {
            return true;
        }
    }
    reply_err(id, "not_ready", "no online tag in group");
    return false;
}

static bool require_pawr_target_online(const char *id, uint8_t esl_id, uint8_t group_id)
{
    if (is_group_broadcast(esl_id)) {
        return require_group_online(id, group_id);
    }
    return require_tag_online(id, esl_id, group_id);
}

static int format_tag_json(char *out, size_t out_len, const esl_product_tag_record_t *view)
{
    if (out == NULL || out_len == 0 || view == NULL || view->wire == NULL || view->rt == NULL) {
        return -1;
    }
    const esl_product_tag_wire_t *w = view->wire;
    const esl_ap_tag_runtime_t *rt = view->rt;
    char addr[18];
    format_addr(w->addr, addr, sizeof(addr));
    uint16_t esl_addr = app_inventory_esl_addr(view);
    const char *fail = failure_reason_str(rt->failure_reason);
    int64_t last_seen_ms = rt->last_seen_uptime_us > 0 ? (rt->last_seen_uptime_us / 1000) : 0;
    char fail_buf[48];
    if (fail != NULL) {
        snprintf(fail_buf, sizeof(fail_buf), "\"%s\"", fail);
    } else {
        snprintf(fail_buf, sizeof(fail_buf), "null");
    }
    const char *qreason = quarantine_reason_str(w->quarantine_reason);
    char q_buf[48];
    if (qreason != NULL) {
        snprintf(q_buf, sizeof(q_buf), "\"%s\"", qreason);
    } else {
        snprintf(q_buf, sizeof(q_buf), "null");
    }
    const char *ops_hint = "null";
    if (w->quarantine_reason == ESL_PRODUCT_QUARANTINE_TAG_UNASSOCIATED) {
        ops_hint = "\"requires reprovision\"";
    }

    return snprintf(out, out_len,
                    "{\"ble_addr\":\"%s\",\"addr_type\":%u,\"esl_id\":%u,\"group_id\":%u,"
                    "\"esl_addr\":%u,\"state\":\"%s\",\"current_image_index\":%u,"
                    "\"has_display\":%s,\"has_led\":%s,\"has_sensor\":%s,"
                    "\"num_displays\":%u,\"max_image_index\":%u,\"valid\":true,"
                    "\"product_state\":\"%s\",\"esl_runtime_state\":\"%s\",\"associated\":%s,"
                    "\"desired_state\":\"%s\",\"time_synced\":%s,\"recovery_generation\":%" PRIu32 ","
                    "\"last_seen_uptime_ms\":%" PRId64 ",\"retry_count\":%u,"
                    "\"failure_reason\":%s,\"provision_phase\":\"%s\","
                    "\"quarantine_reason\":%s,\"ops_hint\":%s}",
                    addr, w->addr_type, w->esl_id, w->group_id, esl_addr,
                    legacy_sync_state(view), w->current_image_index,
                    (w->capability_flags & ESL_PRODUCT_CAP_DISPLAY) ? "true" : "false",
                    (w->capability_flags & ESL_PRODUCT_CAP_LED) ? "true" : "false",
                    (w->capability_flags & ESL_PRODUCT_CAP_SENSOR) ? "true" : "false",
                    w->num_displays, w->max_image_index,
                    product_state_str(rt->product_state),
                    state_to_str(rt->esl_runtime_state),
                    w->associated ? "true" : "false",
                    desired_state_str(w->desired_state),
                    rt->time_synced ? "true" : "false",
                    rt->recovery_generation,
                    last_seen_ms,
                    (unsigned)rt->retry_count,
                    fail_buf,
                    provision_phase_str(w->provision_phase),
                    q_buf,
                    ops_hint);
}

static void format_addr(const uint8_t addr[6], char *out, size_t out_len)
{
    snprintf(out, out_len, "%02x:%02x:%02x:%02x:%02x:%02x",
             addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
}

static bool json_get_string(const char *json, const char *key, char *out, size_t out_len)
{
    char pat[48];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p) {
        return false;
    }
    p = strchr(p + strlen(pat), '"');
    if (!p) {
        return false;
    }
    p++;
    const char *e = strchr(p, '"');
    if (!e) {
        return false;
    }
    size_t n = (size_t)(e - p);
    if (n >= out_len) {
        n = out_len - 1;
    }
    memcpy(out, p, n);
    out[n] = '\0';
    return true;
}

static bool json_get_int(const char *json, const char *key, int *out)
{
    char pat[48];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p) {
        return false;
    }
    p = strchr(p + strlen(pat), ':');
    if (!p) {
        return false;
    }
    *out = atoi(p + 1);
    return true;
}

static void parse_tag_selector(const char *json, int *esl_id, int *group_id)
{
    const char *tag = strstr(json, "\"tag\"");
    const char *scope = tag != NULL ? tag : json;

    if (!json_get_int(scope, "esl_id", esl_id)) {
        json_get_int(json, "esl_id", esl_id);
    }
    if (!json_get_int(scope, "group_id", group_id)) {
        json_get_int(json, "group_id", group_id);
    }
}

static bool json_get_tag_ble_addr(const char *json, char *out, size_t out_len, int *addr_type)
{
    const char *tag = strstr(json, "\"tag\"");
    const char *scope = tag != NULL ? tag : json;
    if (!json_get_string(scope, "ble_addr", out, out_len)) {
        return false;
    }
    if (addr_type != NULL) {
        int at = 0;
        if (json_get_int(scope, "addr_type", &at)) {
            *addr_type = at;
        } else {
            *addr_type = 0;
        }
    }
    return true;
}

static bool parse_ble_addr(const char *str, uint8_t out[6])
{
    unsigned int b[6];
    if (sscanf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
               &b[5], &b[4], &b[3], &b[2], &b[1], &b[0]) != 6) {
        return false;
    }
    for (int i = 0; i < 6; i++) {
        out[i] = (uint8_t)b[i];
    }
    return true;
}

static const char *find_cmd(const char *json)
{
    static char cmd[32];
    if (!json_get_string(json, "cmd", cmd, sizeof(cmd))) {
        return NULL;
    }
    return cmd;
}

static uint32_t crc32_calc(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            uint32_t mask = -(crc & 1);
            crc = (crc >> 1) ^ (0xEDB88320 & mask);
        }
    }
    return ~crc;
}

static int b64_val(unsigned char c)
{
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 26;
    }
    if (c >= '0' && c <= '9') {
        return c - '0' + 52;
    }
    if (c == '+') {
        return 62;
    }
    if (c == '/') {
        return 63;
    }
    return -1;
}

static int b64_decode(const char *in, uint8_t *out, size_t out_cap, size_t *out_len)
{
    size_t len = strlen(in);
    size_t o = 0;
    uint32_t buf = 0;
    int nbits = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == '=' || c == '\r' || c == '\n') {
            continue;
        }
        int v = b64_val(c);
        if (v < 0) {
            return -1;
        }
        buf = (buf << 6) | (uint32_t)v;
        nbits += 6;
        if (nbits >= 8) {
            nbits -= 8;
            if (o >= out_cap) {
                return -1;
            }
            out[o++] = (uint8_t)((buf >> nbits) & 0xFF);
        }
    }
    *out_len = o;
    return 0;
}
