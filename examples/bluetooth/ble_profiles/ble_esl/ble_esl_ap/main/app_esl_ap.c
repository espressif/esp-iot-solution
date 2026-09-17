/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_esl_ap.h"

#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "host/ble_gap.h"
#include "host/util/util.h"
#include "ble_esl_ap.h"
#include "ble_esl_common.h"
#include "app_inventory.h"
#include "app_recovery.h"
#include "esl_ap_led_preset.h"
#include "esl_image_manager.h"
#include "app_gateway.h"
#include "sdkconfig.h"

#if CONFIG_ESL_AP_MODE_WITH_APP
#define APP_ESL_AP_HAS_GATEWAY 1
#else
#define APP_ESL_AP_HAS_GATEWAY 0
#endif

static const char *TAG = "app_esl_ap";

#define APP_ESL_AP_EVT_QUEUE_DEPTH   64
#define APP_ESL_AP_TASK_STACK_SIZE   4096
#define APP_ESL_AP_TASK_PRIORITY     5
#define APP_ESL_AP_EVT_CRITICAL_RESERVE 8
#define APP_UNASSOC_TOTAL_TIMEOUT_US    (35ULL * 1000000ULL)
#define APP_UNASSOC_CONFIRM_TIMEOUT_US  (5ULL * 1000000ULL)
#define APP_PROVISION_TOTAL_TIMEOUT_US  (45ULL * 1000000ULL)
#define APP_IMAGE_UPDATE_TOTAL_TIMEOUT_US (60ULL * 1000000ULL)
#define APP_IMAGE_CONNECT_RETRY_DELAY_US (10ULL * 1000000ULL)
#define APP_IMAGE_CONNECT_MAX_RETRIES    3
#define APP_ESL_AP_EVT_UNASSOC_TIMEOUT  0x100
#define APP_ESL_AP_EVT_AUTO_PING        0x101
#define APP_ESL_AP_EVT_PROVISION_TIMEOUT 0x102
#define APP_ESL_AP_EVT_IMAGE_TIMEOUT    0x103
#define APP_ESL_AP_EVT_START_UNASSOCIATE 0x104
#define APP_ESL_AP_EVT_RECOVERY_CONNECT_TIMEOUT 0x105
#define APP_ESL_AP_EVT_RECOVERY_TICK    0x106
#define APP_ESL_AP_EVT_IMAGE_CONNECT_RETRY 0x108

typedef enum {
    APP_UNASSOC_IDLE = 0,
    APP_UNASSOC_CONNECTING,
    APP_UNASSOC_ECP_SENT,
    APP_UNASSOC_WAIT_CONFIRM,
} app_unassociate_phase_t;

typedef struct {
    uint8_t esl_id;
    uint8_t group_id;
} app_esl_ap_start_unassociate_t;

typedef struct {
    int event;
    union {
        ble_esl_ap_scan_result_t       scan_result;
        ble_esl_ap_conn_info_t         conn_info;
        ble_esl_ap_disconn_info_t      disconn_info;
        ble_esl_ap_esl_info_t          esl_info;
        ble_esl_ap_configured_t        configured;
        ble_esl_ap_connect_failed_t    connect_failed;
        ble_esl_ap_abs_time_written_t  abs_time_written;
        ble_esl_ap_image_transferred_t image_transferred;
        ble_esl_ap_synchronized_t      synchronized;
        esl_ap_response_t              response;
        esl_ap_cmd_timeout_t           cmd_timeout;
        esl_ap_cmd_failed_t            cmd_failed;
        ble_esl_ap_state_changed_t     state_changed;
        app_esl_ap_start_unassociate_t start_unassociate;
    } data;
} app_esl_ap_evt_msg_t;

typedef struct {
    uint16_t conn_handle;
    uint8_t active_peer_addr[6];
    bool connecting;
    bool initial_transfer_done;
    bool provision_active;
    bool provision_expect_disc;
    uint8_t provision_addr[6];
    uint8_t provision_addr_type;
    uint8_t provision_esl_id;
    uint8_t provision_group_id;
    bool pending_image_update;
    bool image_hold_recovery; /* recovery paused for image ACL until sync/fail */
    uint32_t pending_image_id;
    uint8_t pending_image_esl_id;
    uint8_t pending_image_group_id;
    uint8_t pending_image_target_addr[6];
    uint8_t pending_image_connect_retries;
    app_unassociate_phase_t unassociate_phase;
    uint8_t unassociate_esl_id;
    uint8_t unassociate_group_id;
    uint8_t unassociate_target_addr[6];
    uint8_t unassociate_target_addr_type;
    uint16_t unassociate_conn_handle;
} app_esl_ap_ctx_t;

static QueueHandle_t s_evt_queue;
static app_esl_ap_ctx_t s_ctx;
static esp_timer_handle_t s_auto_ping_timer;
static esp_timer_handle_t s_unassociate_timer;
static esp_timer_handle_t s_provision_timer;
static esp_timer_handle_t s_image_timer;
static esp_timer_handle_t s_image_connect_retry_timer;
static bool s_esl_ap_started;
static uint8_t s_last_esl_scan_addr[6];
static uint8_t s_last_esl_scan_addr_type;
static bool s_last_esl_scan_valid;

static const uint8_t s_default_ap_sync_key[16] = {
    0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
    0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
};
static const uint8_t s_default_ap_sync_iv[8] = {
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
};
static const uint8_t s_default_resp_key[16] = {
    0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33,
    0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33,
};
static const uint8_t s_default_resp_iv[8] = {
    0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44,
};

static void app_esl_ap_event_cb(ble_esl_ap_evt_t event, void *param)
{
    app_esl_ap_evt_msg_t msg = {0};
    msg.event = event;
    bool critical_scan = false;

    switch (event) {
    case BLE_ESL_AP_EVT_SCAN_RESULT:
        if (param) {
            const ble_esl_ap_scan_result_t *scan = param;
            /* Unassociated ads carry ESL UUID; Unsynchronized (reboot) is
             * flags-only. Drop ambient non-ESL noise, but always forward
             * inventory identities so recovery can classify reboot ads. */
            if (!scan->advertises_esl_service && !app_inventory_has_addr(scan->addr)) {
                return;
            }
            /* Configure/OTS block the event task. Scan flood would fill the
             * queue and drop DISCONNECTED (event 3), so ignore ads then. */
            if (s_ctx.provision_active || s_ctx.pending_image_update || s_ctx.connecting) {
                return;
            }
            critical_scan = s_ctx.unassociate_phase >= APP_UNASSOC_ECP_SENT &&
                            memcmp(scan->addr, s_ctx.unassociate_target_addr, 6) == 0;
            if (!critical_scan &&
                    uxQueueSpacesAvailable(s_evt_queue) <= APP_ESL_AP_EVT_CRITICAL_RESERVE) {
                return;
            }
            memcpy(&msg.data.scan_result, scan, sizeof(*scan));
            msg.data.scan_result.adv_data = NULL;
            msg.data.scan_result.adv_data_len = 0;
        }
        break;
    case BLE_ESL_AP_EVT_CONNECTED:
        if (param) {
            memcpy(&msg.data.conn_info, param, sizeof(msg.data.conn_info));
        }
        break;
    case BLE_ESL_AP_EVT_DISCONNECTED:
        if (param) {
            memcpy(&msg.data.disconn_info, param, sizeof(msg.data.disconn_info));
        }
        break;
    case BLE_ESL_AP_EVT_ESL_INFO:
        if (param) {
            memcpy(&msg.data.esl_info, param, sizeof(msg.data.esl_info));
            msg.data.esl_info.displays = NULL;
            msg.data.esl_info.sensors = NULL;
            msg.data.esl_info.leds = NULL;
        }
        break;
    case BLE_ESL_AP_EVT_CONFIGURED:
        if (param) {
            memcpy(&msg.data.configured, param, sizeof(msg.data.configured));
        }
        break;
    case BLE_ESL_AP_EVT_CONNECT_FAILED:
        if (param) {
            memcpy(&msg.data.connect_failed, param, sizeof(msg.data.connect_failed));
        }
        break;
    case BLE_ESL_AP_EVT_IMAGE_TRANSFERRED:
        if (param) {
            memcpy(&msg.data.image_transferred, param, sizeof(msg.data.image_transferred));
        }
        break;
    case BLE_ESL_AP_EVT_SYNCHRONIZED:
        if (param) {
            memcpy(&msg.data.synchronized, param, sizeof(msg.data.synchronized));
        }
        break;
    case BLE_ESL_AP_EVT_RESPONSE:
        if (param) {
            memcpy(&msg.data.response, param, sizeof(msg.data.response));
            msg.data.response.sensor_value.data = NULL;
            msg.data.response.sensor_value.data_len = 0;
            msg.data.response.vendor.data = NULL;
            msg.data.response.vendor.data_len = 0;
        }
        break;
    case BLE_ESL_AP_EVT_CMD_TIMEOUT:
        if (param) {
            memcpy(&msg.data.cmd_timeout, param, sizeof(msg.data.cmd_timeout));
        }
        break;
    case BLE_ESL_AP_EVT_CMD_FAILED:
        if (param) {
            memcpy(&msg.data.cmd_failed, param, sizeof(msg.data.cmd_failed));
        }
        break;
    case BLE_ESL_AP_EVT_ABS_TIME_WRITTEN:
        if (param) {
            memcpy(&msg.data.abs_time_written, param, sizeof(msg.data.abs_time_written));
        }
        break;
    case BLE_ESL_AP_EVT_STATE_CHANGED:
        if (param) {
            memcpy(&msg.data.state_changed, param, sizeof(msg.data.state_changed));
        }
        break;
    default:
        break;
    }

    TickType_t wait = (event == BLE_ESL_AP_EVT_SCAN_RESULT && !critical_scan)
                      ? 0 : pdMS_TO_TICKS(10);
    if (event == BLE_ESL_AP_EVT_DISCONNECTED || event == BLE_ESL_AP_EVT_CONFIGURED ||
            event == BLE_ESL_AP_EVT_CONNECT_FAILED) {
        wait = pdMS_TO_TICKS(100);
    }
    if (xQueueSend(s_evt_queue, &msg, wait) != pdTRUE) {
        ESP_LOGW(TAG, "Event queue full, dropped event %d", event);
        if (event == BLE_ESL_AP_EVT_ABS_TIME_WRITTEN) {
            (void)app_esl_ap_post_recovery_connect_timeout();
        }
    }
}

static bool inventory_slot_view(esl_product_inventory_t *inv, uint8_t slot,
                                esl_product_tag_record_t *out)
{
    if (inv == NULL || out == NULL || slot >= ESL_PRODUCT_MAX_TAGS) {
        return false;
    }
    if (inv->blob.tags[slot].valid != 1) {
        return false;
    }
    out->wire = &inv->blob.tags[slot];
    out->rt = &inv->runtime[slot];
    out->slot = slot;
    return true;
}

static void clear_pending_image_update(void)
{
    if (s_image_connect_retry_timer != NULL) {
        (void)esp_timer_stop(s_image_connect_retry_timer);
    }
    s_ctx.pending_image_update = false;
    s_ctx.pending_image_id = 0;
    s_ctx.pending_image_esl_id = 0;
    s_ctx.pending_image_group_id = 0;
    s_ctx.pending_image_connect_retries = 0;
}

static void finish_image_pipeline(bool ok, const char *reason, bool emit_gateway_error)
{
    if (!s_ctx.pending_image_update && !s_ctx.image_hold_recovery) {
        return;
    }

    uint8_t esl_id = s_ctx.pending_image_esl_id;
    uint8_t group_id = s_ctx.pending_image_group_id;
    bool was_pending = s_ctx.pending_image_update;

    if (s_image_timer != NULL) {
        (void)esp_timer_stop(s_image_timer);
    }

    if (!ok) {
        ESP_LOGW(TAG, "Image update failed image_id=%" PRIu32 " reason=%s",
                 s_ctx.pending_image_id, reason ? reason : "(none)");
    }

    clear_pending_image_update();
    s_ctx.image_hold_recovery = false;
    s_ctx.connecting = false;
    app_recovery_set_paused(false);

    if (!ok && emit_gateway_error && was_pending) {
#if APP_ESL_AP_HAS_GATEWAY
        app_gateway_finish_command("transfer_image_to_tag", esl_id, group_id, "error");
#else
        (void)esl_id;
        (void)group_id;
#endif
    }
}

static void fail_pending_image_update(const char *reason)
{
    finish_image_pipeline(false, reason, true);
}

static void release_image_hold(void)
{
    if (!s_ctx.image_hold_recovery) {
        return;
    }
    if (s_image_timer != NULL) {
        (void)esp_timer_stop(s_image_timer);
    }
    s_ctx.image_hold_recovery = false;
    if (!s_ctx.pending_image_update) {
        app_recovery_set_paused(false);
    }
}

static void transfer_image_by_id(uint16_t conn_handle, uint32_t image_id,
                                 uint8_t image_index)
{
    esl_image_desc_t image = {0};
    if (esl_image_manager_get(image_id, &image) != ESP_OK) {
        ESP_LOGE(TAG, "image_id=%" PRIu32 " not found", image_id);
        fail_pending_image_update("image missing");
        return;
    }

    ble_esl_ap_image_transfer_params_t params = {
        .conn_handle = conn_handle,
        .image_index = image_index,
        .data = image.data,
        .data_len = image.data_len,
        /* Full-slot rewrite: Truncate required. Without it the server treats
         * same-size overwrite as Patch and rejects (OACP 0x08). */
        .truncate = true,
    };

    ESP_LOGI(TAG, "Transferring image_id=%" PRIu32 " index=%u size=%" PRIu32,
             image_id, image_index, image.data_len);
    esp_err_t ret = ble_esl_ap_transfer_image(&params);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "transfer_image failed: %s", esp_err_to_name(ret));
        fail_pending_image_update(esp_err_to_name(ret));
    }
}

static void auto_ping_timer_cb(void *arg);
static void unassociate_timer_cb(void *arg);
static void provision_timer_cb(void *arg);
static void image_timer_cb(void *arg);
static void finish_unassociate(bool ok, const char *reason);
static void finish_provision(bool ok, const char *reason);
static void abort_provision_local(void);
static void start_unassociate_on_task(uint8_t esl_id, uint8_t group_id);

static bool pipeline_busy(void)
{
    return s_ctx.connecting || s_ctx.provision_active ||
           s_ctx.pending_image_update || s_ctx.image_hold_recovery ||
           s_ctx.unassociate_phase != APP_UNASSOC_IDLE ||
           app_recovery_tx_busy() || ble_esl_ap_synchronize_in_progress();
}

bool app_esl_ap_pipeline_busy(void)
{
    return pipeline_busy();
}

static void handle_auto_ping(void)
{
    if (pipeline_busy()) {
        return;
    }

    esl_product_inventory_t *inv = app_inventory_get();
    esl_product_tag_record_t view;
    for (uint8_t i = 0; inv && i < ESL_PRODUCT_MAX_TAGS; i++) {
        if (!inventory_slot_view(inv, i, &view)) {
            continue;
        }
        if (view.rt->product_state != ESL_PRODUCT_STATE_ONLINE || !view.rt->time_synced) {
            continue;
        }

        uint16_t esl_addr = app_inventory_esl_addr(&view);
        if (ble_esl_ap_get_esl_state(esl_product_pack_esl_addr_u16(esl_addr)) !=
                BLE_ESL_STATE_SYNCHRONIZED) {
            continue;
        }

        esp_err_t ret = app_recovery_submit_health_ping(
                            view.wire->esl_id, view.wire->group_id, i,
                            esl_product_rd_le32(view.wire->record_generation_le));
        if (ret != ESP_OK) {
            ESP_LOGD(TAG, "Health ping submit esl_id=%u failed: %s",
                     view.wire->esl_id, esp_err_to_name(ret));
        }
    }
}

static void auto_ping_timer_cb(void *arg)
{
    (void)arg;
    if (s_evt_queue == NULL) {
        return;
    }
    app_esl_ap_evt_msg_t msg = {
        .event = APP_ESL_AP_EVT_AUTO_PING,
    };
    if (xQueueSend(s_evt_queue, &msg, 0) != pdTRUE) {
        ESP_LOGD(TAG, "Auto ping event dropped (queue full)");
    }
}

static void unassociate_timer_cb(void *arg)
{
    (void)arg;
    app_esl_ap_evt_msg_t msg = {
        .event = APP_ESL_AP_EVT_UNASSOC_TIMEOUT,
    };
    if (xQueueSend(s_evt_queue, &msg, 0) != pdTRUE) {
        ESP_LOGE(TAG, "Unassociate watchdog event dropped");
        if (s_ctx.unassociate_phase != APP_UNASSOC_IDLE) {
            (void)esp_timer_start_once(s_unassociate_timer, 100000);
        }
    }
}

static void provision_timer_cb(void *arg)
{
    (void)arg;
    app_esl_ap_evt_msg_t msg = {
        .event = APP_ESL_AP_EVT_PROVISION_TIMEOUT,
    };
    if (xQueueSend(s_evt_queue, &msg, 0) != pdTRUE) {
        ESP_LOGE(TAG, "Provision watchdog event dropped");
        if (s_ctx.provision_active) {
            (void)esp_timer_start_once(s_provision_timer, 100000);
        }
    }
}

static void image_timer_cb(void *arg)
{
    (void)arg;
    app_esl_ap_evt_msg_t msg = {
        .event = APP_ESL_AP_EVT_IMAGE_TIMEOUT,
    };
    if (xQueueSend(s_evt_queue, &msg, 0) != pdTRUE) {
        ESP_LOGE(TAG, "Image update watchdog event dropped");
        if (s_ctx.pending_image_update || s_ctx.image_hold_recovery) {
            (void)esp_timer_start_once(s_image_timer, 100000);
        }
    }
}

static void image_connect_retry_timer_cb(void *arg)
{
    (void)arg;
    app_esl_ap_evt_msg_t msg = {
        .event = APP_ESL_AP_EVT_IMAGE_CONNECT_RETRY,
    };
    if (xQueueSend(s_evt_queue, &msg, 0) != pdTRUE) {
        ESP_LOGE(TAG, "Image connect retry event dropped");
        if (s_ctx.pending_image_update && !s_ctx.connecting && s_image_connect_retry_timer != NULL) {
            (void)esp_timer_start_once(s_image_connect_retry_timer, 100000);
        }
    }
}

static void start_unassociate_timer(uint64_t timeout_us)
{
    if (s_unassociate_timer == NULL) {
        return;
    }
    (void)esp_timer_stop(s_unassociate_timer);
    esp_err_t ret = esp_timer_start_once(s_unassociate_timer, timeout_us);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Unassociate watchdog start failed: %s", esp_err_to_name(ret));
    }
}

static void start_provision_timer(void)
{
    if (s_provision_timer == NULL) {
        return;
    }
    (void)esp_timer_stop(s_provision_timer);
    esp_err_t ret = esp_timer_start_once(s_provision_timer, APP_PROVISION_TOTAL_TIMEOUT_US);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Provision watchdog start failed: %s", esp_err_to_name(ret));
    }
}

static void start_image_timer(void)
{
    if (s_image_timer == NULL) {
        return;
    }
    (void)esp_timer_stop(s_image_timer);
    esp_err_t ret = esp_timer_start_once(s_image_timer, APP_IMAGE_UPDATE_TOTAL_TIMEOUT_US);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Image update watchdog start failed: %s", esp_err_to_name(ret));
    }
}

static void start_image_connect_retry_timer(void)
{
    if (s_image_connect_retry_timer == NULL) {
        return;
    }
    (void)esp_timer_stop(s_image_connect_retry_timer);
    esp_err_t ret = esp_timer_start_once(s_image_connect_retry_timer,
                                         APP_IMAGE_CONNECT_RETRY_DELAY_US);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Image connect retry timer start failed: %s", esp_err_to_name(ret));
    }
}

static void abort_provision_local(void)
{
    if (!s_ctx.provision_active) {
        return;
    }
    if (s_provision_timer != NULL) {
        (void)esp_timer_stop(s_provision_timer);
    }
    s_ctx.provision_active = false;
    s_ctx.provision_expect_disc = false;
    s_ctx.connecting = false;
    s_ctx.conn_handle = BLE_HS_CONN_HANDLE_NONE;
    memset(s_ctx.active_peer_addr, 0, sizeof(s_ctx.active_peer_addr));
    if (s_ctx.pending_image_update) {
        fail_pending_image_update("provision aborted");
    }
    app_recovery_set_paused(false);
}

/* Terminate a link to the provisioning peer that the app never adopted, i.e.
 * one for which BLE_ESL_AP_EVT_CONNECTED never arrived. Without this the ACL
 * slot stays occupied until reboot and further connects fail with ENOMEM. */
static void provision_disconnect_peer(const uint8_t addr[6], uint8_t addr_type)
{
    ble_addr_t peer;
    peer.type = addr_type;
    memcpy(peer.val, addr, sizeof(peer.val));

    struct ble_gap_conn_desc desc;
    if (ble_gap_conn_find_by_addr(&peer, &desc) != 0) {
        return;
    }

    ESP_LOGW(TAG, "Provision: tearing down stalled link conn=%u", desc.conn_handle);
    ble_esl_ap_disconnect(desc.conn_handle);
}

static void finish_provision(bool ok, const char *reason)
{
    if (!s_ctx.provision_active) {
        return;
    }

    uint8_t addr[6];
    uint8_t esl_id = s_ctx.provision_esl_id;
    uint8_t group_id = s_ctx.provision_group_id;
    memcpy(addr, s_ctx.provision_addr, sizeof(addr));

    if (s_provision_timer != NULL) {
        (void)esp_timer_stop(s_provision_timer);
    }

    if (!ok && s_ctx.pending_image_update) {
        fail_pending_image_update(reason ? reason : "provision failed");
    }

    s_ctx.provision_active = false;
    s_ctx.provision_expect_disc = false;
    s_ctx.connecting = false;
    app_recovery_set_paused(false);

#if APP_ESL_AP_HAS_GATEWAY
    char addr_str[18];
    snprintf(addr_str, sizeof(addr_str), "%02x:%02x:%02x:%02x:%02x:%02x",
             addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
    char line[200];
    if (ok) {
        snprintf(line, sizeof(line),
                 "{\"event\":\"provision_complete\",\"ble_addr\":\"%s\","
                 "\"esl_id\":%u,\"group_id\":%u}",
                 addr_str, esl_id, group_id);
    } else {
        snprintf(line, sizeof(line),
                 "{\"event\":\"provision_failed\",\"ble_addr\":\"%s\",\"reason\":\"%s\"}",
                 addr_str, reason ? reason : "error");
    }
    app_gateway_emit_event_line(line);
#else
    (void)esl_id;
    (void)group_id;
#endif

    if (ok) {
        ESP_LOGI(TAG, "Provision complete esl_id=%u group=%u", esl_id, group_id);
    } else {
        ESP_LOGW(TAG, "Provision failed esl_id=%u group=%u: %s",
                 esl_id, group_id, reason ? reason : "(none)");
    }
}

static void finish_unassociate(bool ok, const char *reason)
{
    if (s_ctx.unassociate_phase == APP_UNASSOC_IDLE) {
        ESP_LOGW(TAG, "Unassociate: finish ignored (not active) ok=%d reason=%s",
                 (int)ok, reason ? reason : "");
        return;
    }

    uint8_t esl_id = s_ctx.unassociate_esl_id;
    uint8_t group_id = s_ctx.unassociate_group_id;
    uint16_t esl_addr = (uint16_t)(esl_id | ((uint16_t)group_id << 8));
    uint8_t addr[6];
    uint8_t addr_type = s_ctx.unassociate_target_addr_type;
    uint16_t conn_handle = s_ctx.unassociate_conn_handle;
    memcpy(addr, s_ctx.unassociate_target_addr, sizeof(addr));

    ESP_LOGI(TAG, "Unassociate: finish phase=%d ok=%d reason=%s esl_id=%u group=%u",
             (int)s_ctx.unassociate_phase, (int)ok, reason ? reason : "(none)",
             esl_id, group_id);

    if (s_unassociate_timer != NULL) {
        (void)esp_timer_stop(s_unassociate_timer);
    }

    s_ctx.unassociate_phase = APP_UNASSOC_IDLE;
    s_ctx.unassociate_esl_id = 0;
    s_ctx.unassociate_group_id = 0;
    s_ctx.unassociate_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    s_ctx.connecting = false;

    if (ok) {
        esp_err_t forget_ret = ble_esl_ap_forget_tag(esl_id, group_id);
        if (forget_ret != ESP_OK && forget_ret != ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Unassociate: facade Tag release failed: %s",
                     esp_err_to_name(forget_ret));
        }

        ble_addr_t peer = {
            .type = addr_type,
        };
        memcpy(peer.val, addr, sizeof(peer.val));
        struct ble_gap_conn_desc desc;
        if (conn_handle != BLE_HS_CONN_HANDLE_NONE &&
                ble_gap_conn_find(conn_handle, &desc) == 0) {
            peer = desc.peer_id_addr;
        }
        int bond_rc = ble_store_util_delete_peer(&peer);
        if (bond_rc != 0) {
            ESP_LOGW(TAG, "Unassociate: AP bond delete failed rc=%d", bond_rc);
        }
#if APP_ESL_AP_HAS_GATEWAY
        app_gateway_on_esl_state_changed(esl_addr, (int)BLE_ESL_STATE_SYNCHRONIZED,
                                         (int)BLE_ESL_STATE_UNASSOCIATED);
#endif
        esp_err_t inv_ret = app_inventory_remove(esl_id, group_id);
        if (inv_ret != ESP_OK) {
            ESP_LOGE(TAG, "Unassociate: inventory remove/save failed: %s",
                     esp_err_to_name(inv_ret));
        }
    }

#if APP_ESL_AP_HAS_GATEWAY
    app_gateway_finish_command("unassociate", esl_id, group_id, ok ? "ok" : "error");
#endif

    if (!ok && reason != NULL) {
        ESP_LOGW(TAG, "Unassociate failed esl_id=%u group=%u: %s", esl_id, group_id, reason);
    } else if (ok) {
        ESP_LOGI(TAG, "Unassociate complete esl_id=%u group=%u", esl_id, group_id);
    }

    /* Success and failure: tear down ACL if still up (already-down is no-op). */
    if (conn_handle != BLE_HS_CONN_HANDLE_NONE &&
            ble_gap_conn_find(conn_handle, NULL) == 0) {
        ble_esl_ap_disconnect(conn_handle);
    }

    app_recovery_set_paused(false);
}

static void handle_scan_result(const ble_esl_ap_scan_result_t *scan)
{
    ESP_LOGI(TAG, "Scan: %02x:%02x:%02x:%02x:%02x:%02x rssi=%d associated=%d addr_type=%u",
             scan->addr[5], scan->addr[4], scan->addr[3],
             scan->addr[2], scan->addr[1], scan->addr[0],
             scan->rssi, scan->is_associated, scan->addr_type);
    if (scan->advertises_esl_service) {
        memcpy(s_last_esl_scan_addr, scan->addr, 6);
        s_last_esl_scan_addr_type = scan->addr_type;
        s_last_esl_scan_valid = true;
    }

#if APP_ESL_AP_HAS_GATEWAY
    app_gateway_on_esl_scan_result(scan->addr, scan->addr_type, scan->rssi, scan->is_associated);
#endif

    if (s_ctx.unassociate_phase >= APP_UNASSOC_ECP_SENT &&
            memcmp(scan->addr, s_ctx.unassociate_target_addr, 6) == 0 &&
            scan->data_complete && scan->is_connectable &&
            scan->advertises_esl_service) {
        ESP_LOGI(TAG, "Unassociate: confirmed by target Unassociated advertising");
        finish_unassociate(true, "unassociated advertising");
        return;
    }

    if (s_ctx.unassociate_phase != APP_UNASSOC_IDLE || s_ctx.provision_active) {
        if (app_inventory_find_by_addr(scan->addr) != NULL) {
            return;
        }
    } else if (app_recovery_on_scan(scan)) {
        return;
    }

    if (scan->is_associated || s_ctx.connecting || app_recovery_tx_busy()) {
        return;
    }

#if CONFIG_ESL_AP_MODE_STANDALONE
#if !CONFIG_ESL_AP_AUTO_CONNECT_UNASSOCIATED
    return;
#endif
#else
    return;
#endif

    if (app_inventory_has_synchronized_tag()) {
        return;
    }

    esp_err_t prep_ret =
        app_inventory_prepare_provision(scan->addr, scan->addr_type,
                                        CONFIG_ESL_AP_DEFAULT_ESL_ID,
                                        CONFIG_ESL_AP_DEFAULT_GROUP_ID);
    if (prep_ret != ESP_OK) {
        ESP_LOGW(TAG, "Standalone provision prepare failed: %s",
                 esp_err_to_name(prep_ret));
        return;
    }

    ESP_LOGI(TAG, "Connecting to unassociated tag...");
    ble_gap_disc_cancel();
    s_ctx.connecting = true;
    esp_err_t rc = ble_esl_ap_connect(scan->addr, scan->addr_type);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Connect failed: %s", esp_err_to_name(rc));
        s_ctx.connecting = false;
    }
}

static void start_synchronize(uint16_t conn_handle)
{
    esp_err_t ret = ble_esl_ap_synchronize(conn_handle);
    if (ret == ESP_OK) {
        if (s_ctx.provision_active) {
            s_ctx.provision_expect_disc = true;
        }
    } else {
        ESP_LOGW(TAG, "synchronize failed: %s", esp_err_to_name(ret));
        if (s_ctx.provision_active) {
            finish_provision(false, "synchronize start failed");
        }
        if (s_ctx.pending_image_update) {
            fail_pending_image_update("synchronize start failed");
        } else {
            release_image_hold();
        }
        ble_esl_ap_disconnect(conn_handle);
    }
}

static void handle_connected(const ble_esl_ap_conn_info_t *info)
{
    s_ctx.connecting = false;
    s_ctx.conn_handle = info->conn_handle;
    s_ctx.initial_transfer_done = false;
    s_ctx.provision_expect_disc = false;
    memcpy(s_ctx.active_peer_addr, info->addr, sizeof(s_ctx.active_peer_addr));

    ESP_LOGI(TAG,
             "CONNECTED conn=%u unassoc_phase=%d prov_active=%d peer=%02x:%02x:%02x:%02x:%02x:%02x",
             info->conn_handle, (int)s_ctx.unassociate_phase, (int)s_ctx.provision_active,
             info->addr[5], info->addr[4], info->addr[3],
             info->addr[2], info->addr[1], info->addr[0]);

    if (s_ctx.unassociate_phase == APP_UNASSOC_CONNECTING) {
        if (memcmp(info->addr, s_ctx.unassociate_target_addr, 6) != 0) {
            ESP_LOGW(TAG, "Unassociate: unexpected peer, terminating conn=%u",
                     info->conn_handle);
            ble_esl_ap_disconnect(info->conn_handle);
            return;
        }
        ESP_LOGI(TAG, "Unassociate: connected to target, sending ECP conn=%u",
                 info->conn_handle);
        s_ctx.unassociate_conn_handle = info->conn_handle;
        esp_err_t ret = ble_esl_ap_unassociate(s_ctx.unassociate_esl_id,
                                               s_ctx.unassociate_group_id);
        ESP_LOGI(TAG, "Unassociate: ble_esl_ap_unassociate ret=%s", esp_err_to_name(ret));
        if (ret != ESP_OK) {
            finish_unassociate(false, esp_err_to_name(ret));
        } else {
            s_ctx.unassociate_phase = APP_UNASSOC_ECP_SENT;
            start_unassociate_timer(APP_UNASSOC_TOTAL_TIMEOUT_US);
        }
        return;
    }

    esp_err_t upsert_ret =
        app_inventory_upsert_from_connection(info->conn_handle, info->addr, info->addr_type,
                                             info->has_display_info, info->has_led_info,
                                             info->has_sensor_info);
    if (upsert_ret != ESP_OK) {
        ESP_LOGE(TAG, "Inventory upsert failed: %s", esp_err_to_name(upsert_ret));
        if (s_ctx.provision_active &&
                memcmp(info->addr, s_ctx.provision_addr, 6) == 0) {
            finish_provision(false, esp_err_to_name(upsert_ret));
        } else if (s_ctx.pending_image_update) {
            fail_pending_image_update(esp_err_to_name(upsert_ret));
        }
        ble_esl_ap_disconnect(info->conn_handle);
        return;
    }

    if (s_ctx.provision_active &&
            memcmp(info->addr, s_ctx.provision_addr, 6) == 0) {
        ESP_LOGI(TAG, "Provision: read info conn=%u esl_id=%u group=%u",
                 info->conn_handle, s_ctx.provision_esl_id, s_ctx.provision_group_id);
        ble_esl_ap_read_info(info->conn_handle);
        return;
    }

    /* Image update on an ACL that is already up (Tag still Configuring / waiting
     * for PAST). Skip a second configure; OTS can start immediately. */
    if (s_ctx.pending_image_update) {
        esl_image_desc_t image = {0};
        if (esl_image_manager_get(s_ctx.pending_image_id, &image) != ESP_OK) {
            ESP_LOGE(TAG, "Pending image_id=%" PRIu32 " missing on reconnect",
                     s_ctx.pending_image_id);
            fail_pending_image_update("image missing");
            return;
        }
        ESP_LOGI(TAG, "CONNECTED: transfer pending image on existing ACL conn=%u",
                 info->conn_handle);
        transfer_image_by_id(info->conn_handle, s_ctx.pending_image_id, image.image_index);
        return;
    }

    /* Bond encrypt success is identity proof; recovery owns Absolute Time → PAST → Ping. */
    if (app_recovery_on_connected(info)) {
        ESP_LOGI(TAG, "CONNECTED: handed to recovery conn=%u", info->conn_handle);
        return;
    }

    ESP_LOGI(TAG, "Connected conn=%u, reading ESL info", info->conn_handle);
    ble_esl_ap_read_info(info->conn_handle);
}

static void handle_disconnected(const ble_esl_ap_disconn_info_t *info)
{
    ESP_LOGI(TAG, "Disconnected conn=%u reason=0x%02x unassoc_phase=%d",
             info->conn_handle, info->reason,
             (int)s_ctx.unassociate_phase);
    bool unassociate_target =
        memcmp(info->addr, s_ctx.unassociate_target_addr, 6) == 0;
    if (s_ctx.unassociate_phase == APP_UNASSOC_CONNECTING && unassociate_target) {
        finish_unassociate(false, "disconnected before ECP");
    } else if (s_ctx.unassociate_phase == APP_UNASSOC_ECP_SENT &&
               (info->conn_handle == s_ctx.unassociate_conn_handle || unassociate_target)) {
        ESP_LOGI(TAG, "Unassociate: target disconnected after ECP; waiting for confirmation");
        s_ctx.unassociate_phase = APP_UNASSOC_WAIT_CONFIRM;
        start_unassociate_timer(APP_UNASSOC_CONFIRM_TIMEOUT_US);
    } else if (s_ctx.unassociate_phase == APP_UNASSOC_WAIT_CONFIRM && unassociate_target) {
        ESP_LOGD(TAG, "Unassociate: duplicate target disconnect while waiting confirmation");
    } else if (s_ctx.unassociate_phase != APP_UNASSOC_IDLE) {
        ESP_LOGI(TAG, "Unassociate: unrelated disconnect ignored conn=%u", info->conn_handle);
    }

    if (s_ctx.provision_active && info->conn_handle == s_ctx.conn_handle) {
        if (!s_ctx.provision_expect_disc) {
            finish_provision(false, "disconnected");
        } else {
            s_ctx.provision_expect_disc = false;
        }
    }

    (void)app_recovery_on_disconnected(info);

    if (s_ctx.pending_image_update) {
        /* Do not use pending_image_target_addr for pre-connect fail: a queued
         * DISCONNECTED for the same tag would cancel a freshly started update.
         * Pre-connect failures are covered by CONNECT_FAILED. */
        bool peer_match = (memcmp(info->addr, s_ctx.active_peer_addr, 6) == 0);
        bool conn_match = (info->conn_handle == s_ctx.conn_handle &&
                           s_ctx.conn_handle != BLE_HS_CONN_HANDLE_NONE);
        if (conn_match || peer_match) {
            fail_pending_image_update("disconnected");
        }
    } else if (s_ctx.image_hold_recovery) {
        release_image_hold();
    }

    app_inventory_release_conn(info->conn_handle);
    s_ctx.connecting = false;
    if (info->conn_handle == s_ctx.conn_handle &&
            s_ctx.unassociate_phase == APP_UNASSOC_IDLE &&
            !s_ctx.provision_active &&
            !s_ctx.pending_image_update &&
            !s_ctx.image_hold_recovery) {
        memset(&s_ctx, 0, sizeof(s_ctx));
        s_ctx.unassociate_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    }
}

static void retry_pending_image_connect(const char *trigger)
{
    if (!s_ctx.pending_image_update || s_ctx.connecting) {
        return;
    }

    if (s_image_connect_retry_timer != NULL) {
        (void)esp_timer_stop(s_image_connect_retry_timer);
    }

    ble_esl_address_t profile_addr =
        esl_product_pack_esl_addr(s_ctx.pending_image_esl_id,
                                  s_ctx.pending_image_group_id);
    ESP_LOGI(TAG, "Retry image connect %u/%u trigger=%s esl=0x%04x",
             (unsigned)s_ctx.pending_image_connect_retries,
             (unsigned)APP_IMAGE_CONNECT_MAX_RETRIES,
             trigger ? trigger : "(none)", esl_product_unpack_esl_addr(profile_addr));

    s_ctx.connecting = true;
    esp_err_t ret = ble_esl_ap_connect_synced(profile_addr);
    if (ret != ESP_OK) {
        s_ctx.connecting = false;
        ESP_LOGE(TAG, "image connect retry start failed: %s", esp_err_to_name(ret));
        fail_pending_image_update(esp_err_to_name(ret));
    }
}

static void handle_connect_failed(const ble_esl_ap_connect_failed_t *info)
{
    if (info == NULL) {
        return;
    }

    ESP_LOGW(TAG, "Connect failed status=%d type=%u peer=%02x:%02x:%02x:%02x:%02x:%02x",
             info->status, info->addr_type,
             info->addr[5], info->addr[4], info->addr[3],
             info->addr[2], info->addr[1], info->addr[0]);

    if (s_ctx.pending_image_update &&
            memcmp(info->addr, s_ctx.pending_image_target_addr,
                   sizeof(s_ctx.pending_image_target_addr)) == 0) {
        s_ctx.connecting = false;
        if (s_ctx.pending_image_connect_retries >= APP_IMAGE_CONNECT_MAX_RETRIES) {
            fail_pending_image_update("connect failed");
            return;
        }
        s_ctx.pending_image_connect_retries++;
        ESP_LOGW(TAG, "Image connect failed; wait PAwR response or retry timer (%u/%u)",
                 (unsigned)s_ctx.pending_image_connect_retries,
                 (unsigned)APP_IMAGE_CONNECT_MAX_RETRIES);
        start_image_connect_retry_timer();
    }
}

static void handle_esl_info(const ble_esl_ap_esl_info_t *info)
{
    uint8_t esl_id = CONFIG_ESL_AP_DEFAULT_ESL_ID;
    uint8_t group_id = CONFIG_ESL_AP_DEFAULT_GROUP_ID;

    if (s_ctx.provision_active) {
        esl_id = s_ctx.provision_esl_id;
        group_id = s_ctx.provision_group_id;
    } else if (s_ctx.pending_image_update) {
        esl_id = s_ctx.pending_image_esl_id;
        group_id = s_ctx.pending_image_group_id;
    }

    ble_esl_ap_esl_config_t config = {0};
    config.esl_id = esl_id;
    config.group_id = group_id;
    memcpy(config.ap_sync_key.session_key, s_default_ap_sync_key, sizeof(config.ap_sync_key.session_key));
    memcpy(config.ap_sync_key.iv, s_default_ap_sync_iv, sizeof(config.ap_sync_key.iv));
    memcpy(config.resp_key.session_key, s_default_resp_key, sizeof(config.resp_key.session_key));
    memcpy(config.resp_key.iv, s_default_resp_iv, sizeof(config.resp_key.iv));

    ESP_LOGI(TAG, "Configuring tag esl_id=%u group_id=%u", esl_id, group_id);
    /* Blocking ATT writes share the controller with GAP scan. */
    (void)ble_esl_ap_stop_scan();
    esp_err_t cret = ble_esl_ap_configure(info->conn_handle, &config);
    if (cret != ESP_OK) {
        ESP_LOGE(TAG, "configure failed: %s", esp_err_to_name(cret));
        if (s_ctx.provision_active) {
            finish_provision(false, esp_err_to_name(cret));
        } else if (s_ctx.pending_image_update) {
            fail_pending_image_update(esp_err_to_name(cret));
        }
        ble_esl_ap_disconnect(info->conn_handle);
    }
}

static void persist_inventory_configured(uint16_t conn_handle)
{
    uint8_t esl_id = CONFIG_ESL_AP_DEFAULT_ESL_ID;
    uint8_t group_id = CONFIG_ESL_AP_DEFAULT_GROUP_ID;

    if (s_ctx.provision_active) {
        esl_id = s_ctx.provision_esl_id;
        group_id = s_ctx.provision_group_id;
    } else if (s_ctx.pending_image_update) {
        esl_id = s_ctx.pending_image_esl_id;
        group_id = s_ctx.pending_image_group_id;
    }

    esp_err_t cfg_ret =
        app_inventory_set_configured(conn_handle, s_ctx.active_peer_addr, esl_id, group_id,
                                     s_default_ap_sync_key, s_default_ap_sync_iv,
                                     s_default_resp_key, s_default_resp_iv);
    if (cfg_ret != ESP_OK) {
        ESP_LOGW(TAG, "set_configured after GATT ok failed: %s", esp_err_to_name(cfg_ret));
    }
}

static void handle_configured(const ble_esl_ap_configured_t *info)
{
    ESP_LOGI(TAG, "Configured conn=%u status=0x%x", info->conn_handle, info->status);

    if (info->status != 0) {
        ESP_LOGE(TAG, "Configure failed status=0x%x", info->status);
        if (s_ctx.provision_active) {
            finish_provision(false, "configure failed");
        } else {
            fail_pending_image_update("configure failed");
        }
        ble_esl_ap_disconnect(info->conn_handle);
        return;
    }

    persist_inventory_configured(info->conn_handle);

    /* Custom / gateway-requested update: transfer after configure when OTS is usable. */
    if (s_ctx.pending_image_update) {
        esl_image_desc_t image = {0};
        if (esl_image_manager_get(s_ctx.pending_image_id, &image) != ESP_OK) {
            ESP_LOGE(TAG, "Pending image_id=%" PRIu32 " missing after configure",
                     s_ctx.pending_image_id);
            fail_pending_image_update("image missing");
            start_synchronize(info->conn_handle);
            return;
        }
        transfer_image_by_id(info->conn_handle, s_ctx.pending_image_id, image.image_index);
        return;
    }

    /* Provisioning only configures and synchronizes the tag. Images are
     * transferred explicitly through transfer_image_to_tag when requested. */
    ESP_LOGI(TAG, "Configured; synchronizing without automatic image transfer");
    start_synchronize(info->conn_handle);
}

static void handle_image_transferred(const ble_esl_ap_image_transferred_t *info)
{
    ESP_LOGI(TAG, "Image transferred conn=%u index=%u status=0x%x",
             info->conn_handle, info->image_index, info->status);

    if (info->status != 0) {
        ESP_LOGE(TAG, "Image transfer failed status=0x%x", info->status);
        if (s_ctx.pending_image_update) {
            /* Single command_result via finish_image_pipeline; skip gateway double-emit. */
            fail_pending_image_update("transfer failed");
        } else {
            clear_pending_image_update();
            release_image_hold();
        }
        start_synchronize(info->conn_handle);
        return;
    }

#if APP_ESL_AP_HAS_GATEWAY
    app_gateway_on_image_transferred(info->conn_handle, info->image_index, (int)info->status);
#endif

    uint32_t image_id = s_ctx.pending_image_update
                        ? s_ctx.pending_image_id
                        : CONFIG_ESL_AP_DEFAULT_IMAGE_ID;

    esl_product_tag_record_t *rec =
        app_inventory_find_for_conn(info->conn_handle, s_ctx.active_peer_addr);
    if (rec != NULL) {
        uint16_t esl_addr = app_inventory_esl_addr(rec);
        esl_image_manager_mark_transferred(esl_addr, image_id, info->image_index);
        rec->wire->current_image_index = info->image_index;
        app_inventory_save();
    }

    /* Keep recovery paused until synchronize completes (H1). Gateway already got ok. */
    clear_pending_image_update();
    s_ctx.initial_transfer_done = true;
    start_synchronize(info->conn_handle);
}

static void handle_synchronized(const ble_esl_ap_synchronized_t *info)
{
    ESP_LOGI(TAG, "Synchronized esl_addr=0x%04x status=0x%x",
             esl_product_unpack_esl_addr(info->esl_addr), info->status);

    /* Recovery path: PAST success defers confirmation Ping until ACL drop. */
    if (app_recovery_on_synchronized(info)) {
        return;
    }

    if (info->status != ESP_OK) {
        if (s_ctx.provision_active) {
            finish_provision(false, "synchronize failed");
        }
        release_image_hold();
        return;
    }

    /* Prefer the index already persisted by handle_image_transferred. */
    uint8_t image_index = CONFIG_ESL_AP_DEFAULT_IMAGE_INDEX;
    esl_product_inventory_t *inv = app_inventory_get();
    esl_product_tag_record_t view;
    for (uint8_t i = 0; inv && i < ESL_PRODUCT_MAX_TAGS; i++) {
        if (inventory_slot_view(inv, i, &view) &&
                app_inventory_esl_addr(&view) == esl_product_unpack_esl_addr(info->esl_addr)) {
            image_index = view.wire->current_image_index;
            break;
        }
    }

    /* First-config provision: CONFIGURED → ACTIVE + ONLINE. */
    esp_err_t ret = app_inventory_set_synchronized(esl_product_unpack_esl_addr(info->esl_addr),
                                                   image_index);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "inventory sync update failed for esl_addr=0x%04x: %s",
                 esl_product_unpack_esl_addr(info->esl_addr), esp_err_to_name(ret));
        if (s_ctx.provision_active) {
            finish_provision(false, esp_err_to_name(ret));
            return;
        }
    }

    if (s_ctx.provision_active) {
        finish_provision(true, NULL);
        return;
    }

    /* Keep recovery paused until the image ACL drops. Unpausing at PAST
     * set_info restarts discovery while PAwR is still settling and races
     * instance 0 on ESP32-C6. handle_disconnected releases the hold. */
#if CONFIG_ESL_AP_MODE_WITH_APP
    ESP_LOGI(TAG, "Tag ready. Phone app can send PAwR commands.");
#else
    ESP_LOGI(TAG, "Tag ready.");
#endif

    esl_product_tag_record_t *tag = app_inventory_selected_tag();
    if (tag != NULL && tag->wire->valid == 1 &&
            tag->rt->product_state == ESL_PRODUCT_STATE_ONLINE && tag->rt->time_synced) {
        esl_ap_led_settings_t led;
        esl_ap_led_fill_default_settings(&led);
        ESP_LOGI(TAG, "Sending LED control esl_id=%u rgb=%u/%u/%u",
                 tag->wire->esl_id, led.color_red, led.color_green, led.color_blue);
        ble_esl_ap_led_control(tag->wire->esl_id, tag->wire->group_id, 0, &led);
    }
}

static void handle_response(const esl_ap_response_t *resp)
{
    ESP_LOGI(TAG, "Response esl_id=%u group=%u type=%d",
             resp->esl_id, resp->group_id, resp->type);

    if (s_ctx.unassociate_phase >= APP_UNASSOC_ECP_SENT &&
            resp->esl_id == s_ctx.unassociate_esl_id &&
            resp->group_id == s_ctx.unassociate_group_id) {
        if (resp->type == ESL_AP_RESP_ERROR) {
            finish_unassociate(false, "tag error response");
        } else if (resp->type == ESL_AP_RESP_BASIC_STATE) {
            finish_unassociate(true, "basic state response");
        } else {
            ESP_LOGW(TAG, "Unassociate: unexpected response type=%d", resp->type);
        }
        return;
    }

    app_recovery_on_pawr_response(resp);

    if (s_ctx.pending_image_update && !s_ctx.connecting &&
            s_ctx.pending_image_connect_retries > 0 &&
            resp->esl_id == s_ctx.pending_image_esl_id &&
            resp->group_id == s_ctx.pending_image_group_id) {
        retry_pending_image_connect("pawr response");
    }

#if APP_ESL_AP_HAS_GATEWAY
    app_gateway_on_esl_response(resp->esl_id, resp->group_id, (int)resp->type);
#endif
}

static void app_esl_ap_task(void *arg)
{
    (void)arg;
    app_esl_ap_evt_msg_t msg;

    while (1) {
        if (xQueueReceive(s_evt_queue, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (msg.event) {
        case BLE_ESL_AP_EVT_SCAN_RESULT:
            handle_scan_result(&msg.data.scan_result);
            break;
        case BLE_ESL_AP_EVT_CONNECTED:
            handle_connected(&msg.data.conn_info);
            break;
        case BLE_ESL_AP_EVT_DISCONNECTED:
            handle_disconnected(&msg.data.disconn_info);
            break;
        case BLE_ESL_AP_EVT_ESL_INFO:
            handle_esl_info(&msg.data.esl_info);
            break;
        case BLE_ESL_AP_EVT_CONFIGURED:
            handle_configured(&msg.data.configured);
            break;
        case BLE_ESL_AP_EVT_CONNECT_FAILED:
            handle_connect_failed(&msg.data.connect_failed);
            break;
        case BLE_ESL_AP_EVT_IMAGE_TRANSFERRED:
            handle_image_transferred(&msg.data.image_transferred);
            break;
        case BLE_ESL_AP_EVT_SYNCHRONIZED:
            handle_synchronized(&msg.data.synchronized);
            break;
        case BLE_ESL_AP_EVT_RESPONSE:
            handle_response(&msg.data.response);
            break;
        case BLE_ESL_AP_EVT_CMD_TIMEOUT:
            ESP_LOGW(TAG, "Command timeout esl_id=%u group=%u",
                     msg.data.cmd_timeout.esl_id, msg.data.cmd_timeout.group_id);
            if (s_ctx.unassociate_phase >= APP_UNASSOC_ECP_SENT &&
                    msg.data.cmd_timeout.esl_id == s_ctx.unassociate_esl_id &&
                    msg.data.cmd_timeout.group_id == s_ctx.unassociate_group_id) {
                finish_unassociate(false, "timeout");
                break;
            }
#if APP_ESL_AP_HAS_GATEWAY
            app_gateway_on_esl_cmd_timeout(msg.data.cmd_timeout.esl_id, msg.data.cmd_timeout.group_id);
#endif
            break;
        case BLE_ESL_AP_EVT_CMD_FAILED:
            ESP_LOGW(TAG, "Command transport failed esl_id=%u group=%u status=%s",
                     msg.data.cmd_failed.esl_id, msg.data.cmd_failed.group_id,
                     esp_err_to_name(msg.data.cmd_failed.status));
            if (s_ctx.unassociate_phase == APP_UNASSOC_ECP_SENT &&
                    msg.data.cmd_failed.esl_id == s_ctx.unassociate_esl_id &&
                    msg.data.cmd_failed.group_id == s_ctx.unassociate_group_id) {
                s_ctx.unassociate_phase = APP_UNASSOC_WAIT_CONFIRM;
                start_unassociate_timer(APP_UNASSOC_CONFIRM_TIMEOUT_US);
                break;
            }
#if APP_ESL_AP_HAS_GATEWAY
            app_gateway_on_esl_cmd_timeout(msg.data.cmd_failed.esl_id,
                                           msg.data.cmd_failed.group_id);
#endif
            break;
        case BLE_ESL_AP_EVT_ABS_TIME_WRITTEN:
            if (!app_recovery_on_abs_time_written(&msg.data.abs_time_written)) {
                ESP_LOGD(TAG, "ABS_TIME_WRITTEN ignored (not recovery TX)");
            }
            break;
        case BLE_ESL_AP_EVT_STATE_CHANGED:
            ESP_LOGI(TAG, "State changed esl_addr=0x%04x %d -> %d",
                     esl_product_unpack_esl_addr(msg.data.state_changed.esl_addr),
                     msg.data.state_changed.old_state,
                     msg.data.state_changed.new_state);
            if (s_ctx.unassociate_phase >= APP_UNASSOC_ECP_SENT &&
                    esl_product_esl_addr_eq(msg.data.state_changed.esl_addr,
                                            esl_product_pack_esl_addr(s_ctx.unassociate_esl_id,
                                                                      s_ctx.unassociate_group_id)) &&
                    msg.data.state_changed.new_state ==
                    BLE_ESL_STATE_UNASSOCIATED) {
                finish_unassociate(true, "profile state unassociated");
                break;
            }
            /* Do not promote product ONLINE from ESL runtime SYNCHRONIZED alone. */
#if APP_ESL_AP_HAS_GATEWAY
            app_gateway_on_esl_state_changed(
                esl_product_unpack_esl_addr(msg.data.state_changed.esl_addr),
                (int)msg.data.state_changed.old_state,
                (int)msg.data.state_changed.new_state);
#endif
            break;
        case APP_ESL_AP_EVT_AUTO_PING:
            handle_auto_ping();
            break;
        case APP_ESL_AP_EVT_UNASSOC_TIMEOUT:
            if (s_ctx.unassociate_phase != APP_UNASSOC_IDLE) {
                finish_unassociate(false,
                                   s_ctx.unassociate_phase == APP_UNASSOC_WAIT_CONFIRM
                                   ? "confirmation timeout" : "transaction timeout");
            }
            break;
        case APP_ESL_AP_EVT_PROVISION_TIMEOUT:
            if (s_ctx.provision_active) {
                uint16_t conn = s_ctx.conn_handle;
                uint8_t peer[6];
                uint8_t peer_type = s_ctx.provision_addr_type;
                memcpy(peer, s_ctx.provision_addr, sizeof(peer));
                finish_provision(false, "transaction timeout");
                if (conn != BLE_HS_CONN_HANDLE_NONE &&
                        ble_gap_conn_find(conn, NULL) == 0) {
                    ble_esl_ap_disconnect(conn);
                } else {
                    /* s_ctx.conn_handle is only set once CONNECTED arrives, i.e.
                     * after encryption and service discovery. A link that stalls
                     * before that would leak an ACL slot forever, so look it up
                     * by peer address instead. */
                    provision_disconnect_peer(peer, peer_type);
                    ble_esl_ap_abort_tag_connections();
                    (void)ble_esl_ap_start_scan();
                }
            }
            break;
        case APP_ESL_AP_EVT_IMAGE_TIMEOUT:
            if (s_ctx.pending_image_update || s_ctx.image_hold_recovery) {
                uint16_t conn = s_ctx.conn_handle;
                if (s_ctx.pending_image_update) {
                    fail_pending_image_update("transaction timeout");
                } else {
                    release_image_hold();
                }
                if (conn != BLE_HS_CONN_HANDLE_NONE &&
                        ble_gap_conn_find(conn, NULL) == 0) {
                    ble_esl_ap_disconnect(conn);
                } else {
                    ble_esl_ap_abort_tag_connections();
                }
            }
            break;
        case APP_ESL_AP_EVT_IMAGE_CONNECT_RETRY:
            retry_pending_image_connect("retry timer");
            break;
        case APP_ESL_AP_EVT_START_UNASSOCIATE:
            start_unassociate_on_task(msg.data.start_unassociate.esl_id,
                                      msg.data.start_unassociate.group_id);
            break;
        case APP_ESL_AP_EVT_RECOVERY_CONNECT_TIMEOUT:
            app_recovery_handle_connect_timeout();
            break;
        case APP_ESL_AP_EVT_RECOVERY_TICK:
            app_recovery_handle_tick();
            break;
        default:
            break;
        }
    }
}

esp_err_t app_esl_ap_init(void)
{
    s_evt_queue = xQueueCreate(APP_ESL_AP_EVT_QUEUE_DEPTH, sizeof(app_esl_ap_evt_msg_t));
    if (s_evt_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.unassociate_conn_handle = BLE_HS_CONN_HANDLE_NONE;

    const esp_timer_create_args_t unassociate_timer_args = {
        .callback = unassociate_timer_cb,
        .name = "esl_unassoc",
    };
    esp_err_t timer_ret = esp_timer_create(&unassociate_timer_args, &s_unassociate_timer);
    if (timer_ret != ESP_OK) {
        vQueueDelete(s_evt_queue);
        s_evt_queue = NULL;
        return timer_ret;
    }

    const esp_timer_create_args_t provision_timer_args = {
        .callback = provision_timer_cb,
        .name = "esl_provision",
    };
    timer_ret = esp_timer_create(&provision_timer_args, &s_provision_timer);
    if (timer_ret != ESP_OK) {
        esp_timer_delete(s_unassociate_timer);
        s_unassociate_timer = NULL;
        vQueueDelete(s_evt_queue);
        s_evt_queue = NULL;
        return timer_ret;
    }

    const esp_timer_create_args_t image_timer_args = {
        .callback = image_timer_cb,
        .name = "esl_image_upd",
    };
    timer_ret = esp_timer_create(&image_timer_args, &s_image_timer);
    if (timer_ret != ESP_OK) {
        esp_timer_delete(s_provision_timer);
        s_provision_timer = NULL;
        esp_timer_delete(s_unassociate_timer);
        s_unassociate_timer = NULL;
        vQueueDelete(s_evt_queue);
        s_evt_queue = NULL;
        return timer_ret;
    }

    const esp_timer_create_args_t image_connect_retry_timer_args = {
        .callback = image_connect_retry_timer_cb,
        .name = "esl_img_conn_retry",
    };
    timer_ret = esp_timer_create(&image_connect_retry_timer_args, &s_image_connect_retry_timer);
    if (timer_ret != ESP_OK) {
        esp_timer_delete(s_image_timer);
        s_image_timer = NULL;
        esp_timer_delete(s_provision_timer);
        s_provision_timer = NULL;
        esp_timer_delete(s_unassociate_timer);
        s_unassociate_timer = NULL;
        vQueueDelete(s_evt_queue);
        s_evt_queue = NULL;
        return timer_ret;
    }

    ESP_ERROR_CHECK(app_inventory_init());
    ESP_ERROR_CHECK(app_recovery_init());
    {
        esp_err_t inv_ret = app_inventory_load();
        if (inv_ret == ESP_ERR_INVALID_CRC || inv_ret == ESP_ERR_INVALID_STATE ||
                inv_ret == ESP_ERR_INVALID_SIZE) {
            ESP_LOGE(TAG, "Inventory unreadable (%s); lifecycle FAILED", esp_err_to_name(inv_ret));
            app_recovery_set_inventory_unreadable();
        } else {
            ESP_ERROR_CHECK(inv_ret);
        }
    }
    ESP_ERROR_CHECK(esl_image_manager_init());

    ble_esl_ap_config_t config = {
        .callback = app_esl_ap_event_cb,
        .pawr_config = {
            .periodic_adv_interval_min = CONFIG_ESL_AP_PAWR_INTERVAL_MIN,
            .periodic_adv_interval_max = CONFIG_ESL_AP_PAWR_INTERVAL_MAX,
            .num_subevents = CONFIG_ESL_AP_PAWR_NUM_SUBEVENTS,
            .subevent_interval = CONFIG_ESL_AP_PAWR_SUBEVENT_INTERVAL,
            .response_slot_delay = CONFIG_ESL_AP_PAWR_RESPONSE_SLOT_DELAY,
            .response_slot_spacing = CONFIG_ESL_AP_PAWR_RESPONSE_SLOT_SPACING,
            .num_response_slots = CONFIG_ESL_AP_PAWR_NUM_RESPONSE_SLOTS,
        },
    };

    esp_err_t ret = ble_esl_ap_init(&config);
    if (ret != ESP_OK) {
        vQueueDelete(s_evt_queue);
        s_evt_queue = NULL;
        return ret;
    }

    /* NUS GATT is registered from app_main after ble_conn_mgr starts the host
     * (CONFIG_BT_NIMBLE_DYNAMIC_SERVICE). */

    return ESP_OK;
}

esp_err_t app_esl_ap_start(void)
{
    static TaskHandle_t task_handle;

    if (s_esl_ap_started || task_handle != NULL) {
        ESP_LOGI(TAG, "app_esl_ap_start: already running");
        s_esl_ap_started = true;
        return ESP_OK;
    }

    ESP_LOGI(TAG, "app_esl_ap_start: begin");
    app_inventory_dump("esl_ap_start_begin");

    esp_err_t ret = ble_esl_ap_start_pawr();
    if (ret != ESP_OK) {
        return ret;
    }
    ret = ble_esl_ap_start_scan();
    if (ret != ESP_OK) {
        ble_esl_ap_stop_pawr();
        return ret;
    }

    if (xTaskCreate(app_esl_ap_task, "app_esl_ap", APP_ESL_AP_TASK_STACK_SIZE,
                    NULL, APP_ESL_AP_TASK_PRIORITY, &task_handle) != pdPASS) {
        ble_esl_ap_stop_scan();
        ble_esl_ap_stop_pawr();
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(app_inventory_restore_runtime());

#if CONFIG_ESL_AP_AUTO_PING_INTERVAL_SEC > 0
    esp_timer_create_args_t ping_args = {
        .callback = auto_ping_timer_cb,
        .name = "esl_auto_ping",
    };
    if (esp_timer_create(&ping_args, &s_auto_ping_timer) == ESP_OK) {
        uint64_t ping_us = (uint64_t)CONFIG_ESL_AP_AUTO_PING_INTERVAL_SEC * 1000000ULL;
        esp_timer_start_periodic(s_auto_ping_timer, ping_us);
        ESP_LOGI(TAG, "Auto ping enabled (%d s)", CONFIG_ESL_AP_AUTO_PING_INTERVAL_SEC);
    } else {
        ESP_LOGW(TAG, "Failed to create auto ping timer");
    }
#endif

    /* Hard-reboot recovery: scan → ACL → Absolute Time → PAST → arbiter Ping. */
    {
        esl_ap_lifecycle_status_t life;
        app_recovery_get_lifecycle_status(&life);
        if (life.lifecycle != ESL_AP_LIFECYCLE_FAILED) {
            (void)app_recovery_start();
        } else {
            ESP_LOGW(TAG, "Skipping recovery start: lifecycle FAILED");
        }
    }

    s_esl_ap_started = true;
    app_inventory_dump("esl_ap_start_done");
    ESP_LOGI(TAG, "app_esl_ap_start: done");
    return ESP_OK;
}

bool app_esl_ap_is_started(void)
{
    return s_esl_ap_started;
}

bool app_esl_ap_is_provisioning(void)
{
    return s_ctx.provision_active || s_ctx.connecting;
}

bool app_esl_ap_can_queue_image_update(uint8_t esl_id, uint8_t group_id)
{
    return s_ctx.provision_active &&
           !s_ctx.pending_image_update &&
           s_ctx.unassociate_phase == APP_UNASSOC_IDLE &&
           s_ctx.provision_esl_id == esl_id &&
           s_ctx.provision_group_id == group_id;
}

esp_err_t app_esl_ap_start_scan(void)
{
    /* Restarting discovery cancels an in-flight GAP connect. Provision,
     * PAwR sync, and recovery writes can keep scanning. */
    if (s_ctx.connecting || s_ctx.provision_active || app_recovery_tx_busy()) {
        ESP_LOGW(TAG, "scan start skipped: connecting");
        return ESP_ERR_INVALID_STATE;
    }
    return ble_esl_ap_start_scan();
}

esp_err_t app_esl_ap_stop_scan(void)
{
    return ble_esl_ap_stop_scan();
}

esp_err_t app_esl_ap_unassociate_tag(uint8_t esl_id, uint8_t group_id)
{
    ESP_LOGI(TAG, "Unassociate: request esl_id=%u group=%u phase=%d prov=%d connecting=%d",
             esl_id, group_id, (int)s_ctx.unassociate_phase, (int)s_ctx.provision_active,
             (int)s_ctx.connecting);
    if (pipeline_busy()) {
        ESP_LOGW(TAG, "unassociate busy");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_evt_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    app_esl_ap_evt_msg_t msg = {
        .event = APP_ESL_AP_EVT_START_UNASSOCIATE,
        .data.start_unassociate = {
            .esl_id = esl_id,
            .group_id = group_id,
        },
    };
    if (xQueueSend(s_evt_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Unassociate: evt queue full");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static void start_unassociate_on_task(uint8_t esl_id, uint8_t group_id)
{
    ESP_LOGI(TAG, "Unassociate: start on task esl_id=%u group=%u", esl_id, group_id);
    if (pipeline_busy()) {
        ESP_LOGW(TAG, "unassociate busy on task");
#if APP_ESL_AP_HAS_GATEWAY
        app_gateway_finish_command("unassociate", esl_id, group_id, "error");
#endif
        return;
    }

    /* Pause recovery immediately after busy check to close TOCTOU with tick. */
    app_recovery_set_paused(true);

    static esl_product_tag_record_t s_unassoc_view;
    esl_product_tag_record_t *rec = NULL;
    esl_product_inventory_t *inv = app_inventory_get();
    for (uint8_t i = 0; inv && i < ESL_PRODUCT_MAX_TAGS; i++) {
        if (!inventory_slot_view(inv, i, &s_unassoc_view)) {
            continue;
        }
        if (s_unassoc_view.wire->esl_id == esl_id && s_unassoc_view.wire->group_id == group_id) {
            rec = &s_unassoc_view;
            break;
        }
    }
    if (rec == NULL) {
        ESP_LOGW(TAG, "Unassociate: tag not in inventory esl_id=%u group=%u", esl_id, group_id);
        app_recovery_set_paused(false);
#if APP_ESL_AP_HAS_GATEWAY
        app_gateway_finish_command("unassociate", esl_id, group_id, "error");
#endif
        return;
    }

    s_ctx.unassociate_phase = APP_UNASSOC_CONNECTING;
    s_ctx.unassociate_esl_id = esl_id;
    s_ctx.unassociate_group_id = group_id;
    s_ctx.unassociate_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    memcpy(s_ctx.unassociate_target_addr, rec->wire->addr, 6);
    s_ctx.unassociate_target_addr_type = rec->wire->addr_type;

    start_unassociate_timer(APP_UNASSOC_TOTAL_TIMEOUT_US);

    uint16_t esl_addr = app_inventory_esl_addr(rec);
    ble_esl_address_t profile_addr = esl_product_pack_esl_addr_u16(esl_addr);
    ble_esl_state_t rt_state = ble_esl_ap_get_esl_state(profile_addr);
    ESP_LOGI(TAG,
             "Unassociate: target=%02x:%02x:%02x:%02x:%02x:%02x esl_addr=0x%04x rt_state=%d",
             rec->wire->addr[5], rec->wire->addr[4], rec->wire->addr[3],
             rec->wire->addr[2], rec->wire->addr[1], rec->wire->addr[0],
             esl_addr, (int)rt_state);

    if (rt_state == BLE_ESL_STATE_SYNCHRONIZED ||
            rt_state == BLE_ESL_STATE_UNSYNCHRONIZED) {
        ESP_LOGI(TAG, "Unassociate: path=connect_synced esl_addr=0x%04x", esl_addr);
        s_ctx.connecting = true;
        esp_err_t ret = ble_esl_ap_connect_synced(profile_addr);
        ESP_LOGI(TAG, "Unassociate: connect_synced ret=%s", esp_err_to_name(ret));
        if (ret != ESP_OK) {
            finish_unassociate(false, esp_err_to_name(ret));
        }
        return;
    }

    if (rt_state == BLE_ESL_STATE_CONFIGURING ||
            rt_state == BLE_ESL_STATE_UPDATING) {
        if (rec->rt->conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            ESP_LOGI(TAG, "Unassociate: path=ecp_existing conn=%u", rec->rt->conn_handle);
            s_ctx.unassociate_conn_handle = rec->rt->conn_handle;
            esp_err_t ret = ble_esl_ap_unassociate(esl_id, group_id);
            ESP_LOGI(TAG, "Unassociate: ecp_existing ret=%s", esp_err_to_name(ret));
            if (ret != ESP_OK) {
                finish_unassociate(false, esp_err_to_name(ret));
            } else {
                s_ctx.unassociate_phase = APP_UNASSOC_ECP_SENT;
                start_unassociate_timer(APP_UNASSOC_TOTAL_TIMEOUT_US);
            }
            return;
        }
        ESP_LOGW(TAG, "Unassociate: UPDATING/CONFIGURING but no inventory conn_handle");
    }

    ESP_LOGI(TAG, "Unassociate: path=connect_addr");
    s_ctx.connecting = true;
    esp_err_t ret = ble_esl_ap_connect(rec->wire->addr, rec->wire->addr_type);
    ESP_LOGI(TAG, "Unassociate: connect_addr ret=%s", esp_err_to_name(ret));
    if (ret != ESP_OK) {
        finish_unassociate(false, esp_err_to_name(ret));
    }
}

esp_err_t app_esl_ap_provision_tag(const uint8_t addr[6], uint8_t addr_type,
                                   uint8_t esl_id, uint8_t group_id)
{
    if (addr == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (pipeline_busy()) {
        ESP_LOGW(TAG, "provision busy");
        return ESP_ERR_INVALID_STATE;
    }

    /* Pause recovery immediately after busy check to close TOCTOU with tick. */
    app_recovery_set_paused(true);

    if (s_last_esl_scan_valid && memcmp(addr, s_last_esl_scan_addr, 6) == 0 &&
            addr_type != s_last_esl_scan_addr_type) {
        ESP_LOGW(TAG, "provision: addr_type App=%u scan=%u, using scan",
                 addr_type, s_last_esl_scan_addr_type);
        addr_type = s_last_esl_scan_addr_type;
    }

    esl_product_tag_record_t *existing = app_inventory_find_by_addr(addr);
    if (existing != NULL && existing->wire->valid &&
            existing->rt->esl_runtime_state == BLE_ESL_STATE_SYNCHRONIZED) {
        ESP_LOGW(TAG, "provision: tag already synchronized");
        app_recovery_set_paused(false);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t prep_ret =
        app_inventory_prepare_provision(addr, addr_type, esl_id, group_id);
    if (prep_ret != ESP_OK) {
        ESP_LOGW(TAG, "provision: prepare esl_id=%u group=%u failed: %s",
                 esl_id, group_id, esp_err_to_name(prep_ret));
        app_recovery_set_paused(false);
        return prep_ret;
    }

    if (ble_gap_disc_active()) {
        int rc = ble_gap_disc_cancel();
        if (rc != 0 && rc != BLE_HS_EALREADY) {
            ESP_LOGW(TAG, "disc_cancel before provision failed: %d", rc);
        }
    }

    memcpy(s_ctx.provision_addr, addr, 6);
    s_ctx.provision_addr_type = addr_type;
    s_ctx.provision_esl_id = esl_id;
    s_ctx.provision_group_id = group_id;
    /* Drop stale identity before provision_active so a queued DISCONNECTED cannot
     * match the old conn_handle and cancel this provision. */
    s_ctx.conn_handle = BLE_HS_CONN_HANDLE_NONE;
    memset(s_ctx.active_peer_addr, 0, sizeof(s_ctx.active_peer_addr));
    s_ctx.provision_active = true;
    s_ctx.connecting = true;
    start_provision_timer();

    ESP_LOGI(TAG, "Provision connect %02x:%02x:%02x:%02x:%02x:%02x type=%u esl_id=%u group=%u",
             addr[5], addr[4], addr[3], addr[2], addr[1], addr[0], addr_type, esl_id, group_id);
    esp_err_t ret = ble_esl_ap_connect(addr, addr_type);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Provision connect failed: %s", esp_err_to_name(ret));
        abort_provision_local();
        return ret;
    }
    return ESP_OK;
}

void app_esl_ap_request_image_update(uint8_t esl_id)
{
    (void)app_esl_ap_request_image_update_id(esl_id, CONFIG_ESL_AP_DEFAULT_GROUP_ID,
                                             CONFIG_ESL_AP_DEFAULT_IMAGE_ID);
}

esp_err_t app_esl_ap_request_image_update_id(uint8_t esl_id, uint8_t group_id, uint32_t image_id)
{
    esl_image_desc_t image = {0};
    if (esl_image_manager_get(image_id, &image) != ESP_OK) {
        ESP_LOGE(TAG, "image_id=%" PRIu32 " not found", image_id);
        return ESP_ERR_NOT_FOUND;
    }

    /* Phone often uploads+transfers while provision GATT writes are still in
     * flight. Queue the OTS write for handle_configured on the same ACL. */
    if (app_esl_ap_can_queue_image_update(esl_id, group_id)) {
        s_ctx.pending_image_update = true;
        s_ctx.pending_image_id = image_id;
        s_ctx.pending_image_esl_id = esl_id;
        s_ctx.pending_image_group_id = group_id;
        s_ctx.pending_image_connect_retries = 0;
        memcpy(s_ctx.pending_image_target_addr, s_ctx.provision_addr, 6);
        start_image_timer();
        ESP_LOGI(TAG, "Image update queued during provision image_id=%" PRIu32
                 " index=%u esl_id=%u group=%u",
                 image_id, image.image_index, esl_id, group_id);
        return ESP_OK;
    }

    if (pipeline_busy()) {
        ESP_LOGW(TAG, "image update busy");
        return ESP_ERR_INVALID_STATE;
    }

    /* Pause recovery immediately after busy check to close TOCTOU with tick. */
    app_recovery_set_paused(true);

    esl_product_tag_record_t *tag = app_inventory_selected_tag();
    if (tag == NULL || tag->wire->esl_id != esl_id || tag->wire->group_id != group_id) {
        esl_product_inventory_t *inv = app_inventory_get();
        static esl_product_tag_record_t s_img_view;
        tag = NULL;
        for (uint8_t i = 0; inv && i < ESL_PRODUCT_MAX_TAGS; i++) {
            if (inventory_slot_view(inv, i, &s_img_view) &&
                    s_img_view.wire->esl_id == esl_id &&
                    s_img_view.wire->group_id == group_id) {
                tag = &s_img_view;
                break;
            }
        }
    }
    if (tag == NULL) {
        ESP_LOGW(TAG, "No tag for image update esl_id=%u group=%u", esl_id, group_id);
        app_recovery_set_paused(false);
        return ESP_ERR_NOT_FOUND;
    }

    /* Defer OTS write until CONFIGURED: connect_synced is async and conn_handle
     * is not valid yet. handle_configured consumes pending_image_*. */
    s_ctx.image_hold_recovery = true;
    s_ctx.pending_image_update = true;
    s_ctx.pending_image_id = image_id;
    s_ctx.pending_image_esl_id = esl_id;
    s_ctx.pending_image_group_id = group_id;
    s_ctx.pending_image_connect_retries = 0;
    memcpy(s_ctx.pending_image_target_addr, tag->wire->addr, 6);
    /* Drop stale identity so a queued DISCONNECTED cannot cancel this update. */
    s_ctx.conn_handle = BLE_HS_CONN_HANDLE_NONE;
    memset(s_ctx.active_peer_addr, 0, sizeof(s_ctx.active_peer_addr));
    s_ctx.connecting = true;
    start_image_timer();

    ble_esl_address_t profile_addr = esl_product_pack_esl_addr(esl_id, group_id);
    ESP_LOGI(TAG, "Image update pending image_id=%" PRIu32 " index=%u esl=0x%04x",
             image_id, image.image_index, esl_product_unpack_esl_addr(profile_addr));

    esp_err_t ret = ble_esl_ap_connect_synced(profile_addr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "connect_synced failed: %s", esp_err_to_name(ret));
        fail_pending_image_update(esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}

bool app_esl_ap_post_recovery_connect_timeout(void)
{
    if (s_evt_queue == NULL) {
        return false;
    }
    app_esl_ap_evt_msg_t msg = {
        .event = APP_ESL_AP_EVT_RECOVERY_CONNECT_TIMEOUT,
    };
    return xQueueSend(s_evt_queue, &msg, 0) == pdTRUE;
}

bool app_esl_ap_post_recovery_tick(void)
{
    if (s_evt_queue == NULL) {
        return false;
    }
    app_esl_ap_evt_msg_t msg = {
        .event = APP_ESL_AP_EVT_RECOVERY_TICK,
    };
    return xQueueSend(s_evt_queue, &msg, 0) == pdTRUE;
}
