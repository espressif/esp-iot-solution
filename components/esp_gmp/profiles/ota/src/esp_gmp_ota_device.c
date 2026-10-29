/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_gmp_ota.h"
#include "esp_gmp_ota_proto.h"
#include "esp_gmp_sha256.h"
#include "esp_gmp.h"
#include "esp_gmp_frame.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "sdkconfig.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#if CONFIG_ESP_GMP_PROFILE_OS
#include "esp_gmp_os.h"
#endif

static const char *TAG = "esp_gmp_ota";

#define OTA_DATA_HOLD_SLOTS 2
#define OTA_RB_ITEM_TYPE_DATA 0x01u
#define OTA_RB_RECV_TICKS     pdMS_TO_TICKS(50)

#ifndef CONFIG_ESP_GMP_OTA_RINGBUF_SLOTS
#define CONFIG_ESP_GMP_OTA_RINGBUF_SLOTS 3
#endif

#ifndef CONFIG_ESP_GMP_OTA_RX_QUEUE_DEPTH
#define CONFIG_ESP_GMP_OTA_RX_QUEUE_DEPTH 4
#endif

#ifndef CONFIG_ESP_GMP_OTA_RX_TASK_STACK
#define CONFIG_ESP_GMP_OTA_RX_TASK_STACK 4096
#endif

#ifndef CONFIG_ESP_GMP_OTA_RX_TASK_PRIO
#define CONFIG_ESP_GMP_OTA_RX_TASK_PRIO 9
#endif

typedef enum {
    OTA_SESS_IDLE = 0,
    OTA_SESS_RECEIVING,
    OTA_SESS_APPLYING,
} ota_sess_state_t;

typedef struct {
    uint8_t type;
    uint32_t image_offset;
    uint16_t data_len;
    uint8_t *payload;
    uint8_t *frame_buf;
} ota_rb_item_t;

static size_t ota_ringbuf_bytes(void)
{
    return sizeof(ota_rb_item_t) * (size_t)CONFIG_ESP_GMP_OTA_RINGBUF_SLOTS;
}

typedef struct {
    bool valid;
    uint16_t seq;
    uint8_t cmd;
    uint32_t image_offset;
    uint16_t data_len;
    uint8_t *payload;
    uint8_t *frame_buf;
} ota_data_hold_t;

typedef struct {
    bool stop;
    esp_gmp_link_t link;
    uint16_t sequence;
    uint8_t command_id;
    uint8_t *frame_buf;
    size_t frame_len;
} ota_rx_msg_t;

typedef struct {
    bool used;
    bool closing;
    esp_gmp_link_t link;
    SemaphoreHandle_t lock;

    ota_sess_state_t state;
    uint8_t session_id;
    uint32_t image_size;
    uint32_t bytes_received;
    uint32_t bytes_written;
    uint32_t queued_bytes;
    uint32_t next_data_offset;
    uint32_t partition_bytes_written;

    const esp_partition_t *ota_part;
    esp_ota_handle_t ota_handle;
    esp_gmp_sha256_ctx_t sha256;

    RingbufHandle_t rb;
    TaskHandle_t worker;
    bool abort_requested;
    bool finish_pending;
    uint16_t finish_seq;
    uint8_t finish_cmd;
    uint8_t finish_sha256[32];
    bool finish_result_valid;
    uint8_t finish_result_status;
    uint8_t finish_result_session_id;
    uint8_t finish_result_sha256[32];

    ota_data_hold_t data_holds[OTA_DATA_HOLD_SLOTS];
} ota_sess_t;

typedef struct {
    bool valid;
    bool read_rsp;
    esp_gmp_link_t link;
    uint16_t seq;
    uint8_t cmd;
    uint8_t status;
    uint8_t payload[ESP_GMP_OTA_QUERY_RSP_LEN];
    size_t payload_len;
} ota_pending_rsp_t;

typedef struct {
    char ota_partition_label[17];
    uint16_t chunk_hint;
    uint32_t restart_delay_ms;
    esp_gmp_ota_apply_policy_t apply_policy;
    esp_gmp_ota_accept_cb_t accept_cb;
    void *accept_ctx;
    esp_gmp_ota_event_cb_t event_cb;
    void *event_ctx;
} ota_cfg_t;

static ota_cfg_t s_cfg;
static ota_sess_t s_sess[CONFIG_ESP_GMP_MAX_LINKS];
static QueueHandle_t s_rx_queue;
static TaskHandle_t s_rx_task;
static bool s_inited;
static bool s_manual_apply_pending;
static uint8_t s_next_sid = 1;
static portMUX_TYPE s_sid_mux = portMUX_INITIALIZER_UNLOCKED;
static esp_gmp_link_event_sub_t s_link_sub;

static void ota_emit(uint8_t event_id, uint64_t transferred, uint64_t total, uint16_t reason,
                     esp_err_t detail)
{
    if (!s_cfg.event_cb) {
        return;
    }
    uint8_t percent = total == 0 ? 0 : (uint8_t)((transferred * 100) / total);
    if (percent > 100) {
        percent = 100;
    }
    esp_gmp_profile_event_t ev = {
        .event_id = event_id,
        .role = ESP_GMP_OTA_ROLE_DEVICE,
        .transferred = transferred,
        .total = total,
        .percent = percent,
        .reason = reason,
        .detail = detail,
        .profile_extra = NULL,
    };
    s_cfg.event_cb(&ev, s_cfg.event_ctx);
}

static uint8_t ota_alloc_session_id(void)
{
    uint8_t sid;
    portENTER_CRITICAL(&s_sid_mux);
    sid = s_next_sid++;
    if (sid == 0) {
        sid = s_next_sid++;
    }
    if (s_next_sid == 0) {
        s_next_sid = 1;
    }
    portEXIT_CRITICAL(&s_sid_mux);
    return sid;
}

static ota_sess_t *sess_find(esp_gmp_link_t link)
{
    for (int i = 0; i < CONFIG_ESP_GMP_MAX_LINKS; i++) {
        if (s_sess[i].used && s_sess[i].link == link) {
            return &s_sess[i];
        }
    }
    return NULL;
}

static ota_sess_t *sess_get(esp_gmp_link_t link)
{
    ota_sess_t *s = sess_find(link);
    if (s) {
        return s;
    }
    for (int i = 0; i < CONFIG_ESP_GMP_MAX_LINKS; i++) {
        if (!s_sess[i].used) {
            SemaphoreHandle_t existing_lock = s_sess[i].lock;
            memset(&s_sess[i], 0, sizeof(s_sess[i]));
            s_sess[i].lock = existing_lock ? existing_lock : xSemaphoreCreateMutex();
            if (!s_sess[i].lock) {
                return NULL;
            }
            s_sess[i].used = true;
            s_sess[i].closing = false;
            s_sess[i].link = link;
            s_sess[i].state = OTA_SESS_IDLE;
            return &s_sess[i];
        }
    }
    return NULL;
}

static void data_holds_clear(ota_sess_t *s)
{
    if (!s) {
        return;
    }
    for (int i = 0; i < OTA_DATA_HOLD_SLOTS; i++) {
        if (s->data_holds[i].frame_buf) {
            free(s->data_holds[i].frame_buf);
        }
        memset(&s->data_holds[i], 0, sizeof(s->data_holds[i]));
    }
}

static void ota_ringbuf_drain_free(RingbufHandle_t rb);

static void sess_abort_ota(ota_sess_t *s)
{
    if (!s) {
        return;
    }
    if (s->ota_handle) {
        esp_ota_abort(s->ota_handle);
        s->ota_handle = 0;
    }
    esp_gmp_sha256_abort(&s->sha256);
    s->ota_part = NULL;
}

static void ota_worker_finish(ota_sess_t *s, bool lock_held)
{
    RingbufHandle_t rb = NULL;

    if (!s) {
        return;
    }

    if (lock_held) {
        rb = s->rb;
        s->rb = NULL;
        s->worker = NULL;
        s->queued_bytes = 0;
        s->finish_pending = false;
    } else if (xSemaphoreTake(s->lock, portMAX_DELAY) == pdTRUE) {
        rb = s->rb;
        s->rb = NULL;
        s->worker = NULL;
        s->queued_bytes = 0;
        s->finish_pending = false;
        xSemaphoreGive(s->lock);
    }

    if (rb) {
        ota_ringbuf_drain_free(rb);
        vRingbufferDelete(rb);
    }
}

static void ota_sess_stop_worker(ota_sess_t *s)
{
    if (!s || s->worker == NULL) {
        return;
    }

    if (xSemaphoreTake(s->lock, portMAX_DELAY) == pdTRUE) {
        s->abort_requested = true;
        xSemaphoreGive(s->lock);
    }

    while (s->worker != NULL) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void sess_clear_state(ota_sess_t *s)
{
    data_holds_clear(s);
    s->state = OTA_SESS_IDLE;
    s->session_id = 0;
    s->image_size = 0;
    s->bytes_received = 0;
    s->bytes_written = 0;
    s->queued_bytes = 0;
    s->next_data_offset = 0;
    s->partition_bytes_written = 0;
    s->abort_requested = false;
    s->finish_pending = false;
}

static void sess_set_finish_result(ota_sess_t *s, uint8_t status, const uint8_t *sha256)
{
    s->finish_result_valid = true;
    s->finish_result_status = status;
    s->finish_result_session_id = s->session_id;
    if (sha256) {
        memcpy(s->finish_result_sha256, sha256, sizeof(s->finish_result_sha256));
    } else {
        memset(s->finish_result_sha256, 0, sizeof(s->finish_result_sha256));
    }
}

static void sess_abort_async(ota_sess_t *s)
{
    if (!s) {
        return;
    }
    data_holds_clear(s);
    s->abort_requested = true;
    if (s->worker == NULL) {
        ota_worker_finish(s, true);
        sess_abort_ota(s);
        sess_clear_state(s);
    }
}

static uint16_t effective_chunk_hint(esp_gmp_link_t link)
{
    if (s_cfg.chunk_hint > 0) {
        return esp_gmp_ota_align_chunk(s_cfg.chunk_hint);
    }
    return esp_gmp_ota_chunk_for_flash(esp_gmp_max_payload_effective(link));
}

static esp_err_t send_write_rsp(esp_gmp_link_t link, uint16_t seq, uint8_t cmd, uint8_t status, const uint8_t *pl, size_t pl_len)
{
    esp_gmp_tx_params_t tx = {
        .ver = ESP_GMP_VER,
        .op = ESP_GMP_OP_WRITE_RSP,
        .group_id = ESP_GMP_GRP_OTA,
        .sequence = seq,
        .command_id = cmd,
        .flags = 0,
        .status = status,
    };
    return esp_gmp_send(link, &tx, pl, pl_len);
}

static bool ota_finish_reply_status(ota_sess_t *s, esp_gmp_link_t link, uint16_t seq, uint8_t cmd, uint8_t status, const uint8_t *sha256)
{
    uint32_t transferred = 0;
    uint32_t total = 0;
    if (xSemaphoreTake(s->lock, portMAX_DELAY) == pdTRUE) {
        sess_set_finish_result(s, status, sha256);
        s->finish_pending = false;
        transferred = s->bytes_written;
        total = s->image_size;
        xSemaphoreGive(s->lock);
    }
    (void)send_write_rsp(link, seq, cmd, status, NULL, 0);
    if (status != ESP_GMP_STATUS_OK) {
        ota_emit(ESP_GMP_OTA_EVT_FAILED, transferred, total, status, ESP_FAIL);
    }
    return false;
}

static esp_err_t send_read_rsp(esp_gmp_link_t link, uint16_t seq, uint8_t cmd, uint8_t status, const uint8_t *pl, size_t pl_len)
{
    esp_gmp_tx_params_t tx = {
        .ver = ESP_GMP_VER,
        .op = ESP_GMP_OP_READ_RSP,
        .group_id = ESP_GMP_GRP_OTA,
        .sequence = seq,
        .command_id = cmd,
        .flags = 0,
        .status = status,
    };
    return esp_gmp_send(link, &tx, pl, pl_len);
}

static esp_err_t pending_write_rsp(ota_pending_rsp_t *rsp, esp_gmp_link_t link, uint16_t seq,
                                   uint8_t cmd, uint8_t status, const uint8_t *pl, size_t pl_len)
{
    if (!rsp || pl_len > sizeof(rsp->payload)) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(rsp, 0, sizeof(*rsp));
    rsp->valid = true;
    rsp->read_rsp = false;
    rsp->link = link;
    rsp->seq = seq;
    rsp->cmd = cmd;
    rsp->status = status;
    rsp->payload_len = pl_len;
    if (pl_len > 0 && pl) {
        memcpy(rsp->payload, pl, pl_len);
    }
    return ESP_OK;
}

static esp_err_t pending_read_rsp(ota_pending_rsp_t *rsp, esp_gmp_link_t link, uint16_t seq,
                                  uint8_t cmd, uint8_t status, const uint8_t *pl, size_t pl_len)
{
    esp_err_t err = pending_write_rsp(rsp, link, seq, cmd, status, pl, pl_len);
    if (err == ESP_OK) {
        rsp->read_rsp = true;
    }
    return err;
}

static esp_err_t pending_rsp_send(const ota_pending_rsp_t *rsp)
{
    if (!rsp || !rsp->valid) {
        return ESP_OK;
    }
    if (rsp->read_rsp) {
        return send_read_rsp(rsp->link, rsp->seq, rsp->cmd, rsp->status,
                             rsp->payload, rsp->payload_len);
    }
    return send_write_rsp(rsp->link, rsp->seq, rsp->cmd, rsp->status,
                          rsp->payload, rsp->payload_len);
}

static const esp_partition_t *ota_partition_resolve(void)
{
    if (s_cfg.ota_partition_label[0] != '\0') {
        return esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                        ESP_PARTITION_SUBTYPE_ANY,
                                        s_cfg.ota_partition_label);
    }
    return esp_ota_get_next_update_partition(NULL);
}

static void restart_task(void *arg)
{
    uint32_t delay_ms = (uint32_t)(uintptr_t)arg;
    uint32_t action = 0;

    if (xTaskNotifyWait(0, UINT32_MAX, &action, portMAX_DELAY) == pdTRUE && action == 1) {
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        esp_restart();
    }
    vTaskDelete(NULL);
}

static uint32_t ota_transfer_bytes_expected(const ota_sess_t *s)
{
    return s->image_size;
}

static esp_err_t ota_write_at_image_offset(ota_sess_t *s, uint32_t image_offset, const uint8_t *data, uint16_t data_len)
{
    if (image_offset != s->partition_bytes_written) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = esp_ota_write(s->ota_handle, data, data_len);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_gmp_sha256_update(&s->sha256, data, data_len);
    if (err != ESP_OK) {
        return err;
    }
    s->partition_bytes_written = image_offset + (uint32_t)data_len;
    return ESP_OK;
}

static bool ota_data_range_valid(const ota_sess_t *s, uint32_t image_offset, uint16_t data_len, uint32_t *out_end)
{
    if (!s || data_len == 0 || data_len > s->image_size ||
            image_offset > s->image_size - (uint32_t)data_len) {
        return false;
    }
    if (out_end) {
        *out_end = image_offset + (uint32_t)data_len;
    }
    return true;
}

static esp_err_t schedule_restart(uint32_t delay_ms, TaskHandle_t *out_task)
{
    TaskHandle_t task = NULL;

    if (!out_task) {
        return ESP_ERR_INVALID_ARG;
    }

    BaseType_t ok = xTaskCreate(restart_task, "gmp_ota_rst",
                                CONFIG_ESP_GMP_OTA_APPLY_TASK_STACK,
                                (void *)(uintptr_t)delay_ms,
                                CONFIG_ESP_GMP_OTA_APPLY_TASK_PRIO, &task);
    if (ok != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    *out_task = task;
    return ESP_OK;
}

static void cancel_restart(TaskHandle_t task)
{
    if (task) {
        (void)xTaskNotify(task, 0, eSetValueWithOverwrite);
    }
}

static void trigger_restart(TaskHandle_t task)
{
    if (task) {
        (void)xTaskNotify(task, 1, eSetValueWithOverwrite);
    }
}

static void ota_ringbuf_drain_free(RingbufHandle_t rb)
{
    size_t item_size = 0;
    ota_rb_item_t *item = NULL;

    if (!rb) {
        return;
    }

    while ((item = (ota_rb_item_t *)xRingbufferReceive(rb, &item_size, 0)) != NULL) {
        if (item_size == sizeof(ota_rb_item_t) &&
                item->type == OTA_RB_ITEM_TYPE_DATA &&
                item->frame_buf != NULL) {
            free(item->frame_buf);
        }
        vRingbufferReturnItem(rb, item);
    }
}

static bool ota_worker_drain_one(ota_sess_t *s, RingbufHandle_t rb, TickType_t wait_ticks)
{
    size_t item_size = 0;
    ota_rb_item_t *item = (ota_rb_item_t *)xRingbufferReceive(rb, &item_size, wait_ticks);
    if (!item) {
        return true;
    }

    bool ok = true;
    if (item_size != sizeof(ota_rb_item_t) || item->type != OTA_RB_ITEM_TYPE_DATA ||
            item->payload == NULL || item->frame_buf == NULL || item->data_len == 0) {
        ESP_LOGE(TAG, "bad ring item size=%u", (unsigned)item_size);
        if (item->frame_buf) {
            free(item->frame_buf);
        }
        vRingbufferReturnItem(rb, item);
        return false;
    }

    uint32_t image_offset = item->image_offset;
    uint16_t data_len = item->data_len;
    uint8_t *payload = item->payload;
    uint8_t *frame_buf = item->frame_buf;
    vRingbufferReturnItem(rb, item);

    esp_err_t err = ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s->lock, portMAX_DELAY) == pdTRUE) {
        if (s->ota_handle != 0 && !s->abort_requested) {
            err = ota_write_at_image_offset(s, image_offset, payload, data_len);
        }
        if (err == ESP_OK) {
            s->bytes_written += data_len;
            if (s->queued_bytes >= data_len) {
                s->queued_bytes -= data_len;
            } else {
                s->queued_bytes = 0;
            }
            uint32_t written = s->bytes_written;
            uint32_t total = s->image_size;
            xSemaphoreGive(s->lock);
            if ((written & 0xFFFF) == 0 || written == total) {
                ota_emit(ESP_GMP_OTA_EVT_PROGRESS, written, total, 0, ESP_OK);
            }
        } else {
            xSemaphoreGive(s->lock);
        }
    }

    free(frame_buf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "worker flash write failed: %s", esp_err_to_name(err));
        if (xSemaphoreTake(s->lock, portMAX_DELAY) == pdTRUE) {
            s->abort_requested = true;
            xSemaphoreGive(s->lock);
        }
        ok = false;
    }
    return ok;
}

static bool ota_worker_try_finish(ota_sess_t *s, RingbufHandle_t rb)
{
    bool finish_pending = false;
    bool abort_requested = false;
    uint32_t queued = 0;
    uint32_t received = 0;
    uint32_t written = 0;
    uint32_t expected = 0;
    esp_gmp_link_t link = NULL;
    uint16_t finish_seq = 0;
    uint8_t finish_cmd = 0;
    uint8_t expect_sha256[32];
    esp_ota_handle_t handle = 0;
    const esp_partition_t *part = NULL;
    TaskHandle_t restart = NULL;

    if (xSemaphoreTake(s->lock, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    finish_pending = s->finish_pending;
    abort_requested = s->abort_requested;
    queued = s->queued_bytes;
    received = s->bytes_received;
    written = s->bytes_written;
    expected = ota_transfer_bytes_expected(s);
    link = s->link;
    finish_seq = s->finish_seq;
    finish_cmd = s->finish_cmd;
    memcpy(expect_sha256, s->finish_sha256, sizeof(expect_sha256));
    xSemaphoreGive(s->lock);

    if (!finish_pending) {
        return true;
    }
    if (abort_requested) {
        return false;
    }

    if (queued != 0) {
        return true;
    }

    if (written != received || received != expected) {
        ESP_LOGE(TAG, "FINISH length mismatch written=%" PRIu32 " received=%" PRIu32
                 " expected=%" PRIu32, written, received, expected);
        return ota_finish_reply_status(s, link, finish_seq, finish_cmd,
                                       ESP_GMP_STATUS_BAD_STATE, expect_sha256);
    }

    uint8_t digest[32];
    if (xSemaphoreTake(s->lock, portMAX_DELAY) == pdTRUE) {
        if (esp_gmp_sha256_finish(&s->sha256, digest) != ESP_OK) {
            xSemaphoreGive(s->lock);
            return ota_finish_reply_status(s, link, finish_seq, finish_cmd,
                                           ESP_GMP_STATUS_INTERNAL, expect_sha256);
        }
        handle = s->ota_handle;
        part = s->ota_part;
        s->ota_handle = 0;
        s->finish_pending = false;
        s->state = OTA_SESS_APPLYING;
        xSemaphoreGive(s->lock);
    }

    ota_emit(ESP_GMP_OTA_EVT_VERIFYING, written, expected, 0, ESP_OK);

    if (memcmp(digest, expect_sha256, 32) != 0) {
        if (handle) {
            esp_ota_abort(handle);
        }
        return ota_finish_reply_status(s, link, finish_seq, finish_cmd,
                                       ESP_GMP_STATUS_BAD_STATE, expect_sha256);
    }

    if (xSemaphoreTake(s->lock, portMAX_DELAY) == pdTRUE) {
        abort_requested = s->abort_requested;
        xSemaphoreGive(s->lock);
    }
    if (abort_requested) {
        if (handle) {
            esp_ota_abort(handle);
        }
        return ota_finish_reply_status(s, link, finish_seq, finish_cmd,
                                       ESP_GMP_STATUS_BAD_STATE, expect_sha256);
    }

    esp_err_t err = esp_ota_end(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        return ota_finish_reply_status(s, link, finish_seq, finish_cmd,
                                       ESP_GMP_STATUS_INTERNAL, expect_sha256);
    }

    if (xSemaphoreTake(s->lock, portMAX_DELAY) == pdTRUE) {
        abort_requested = s->abort_requested;
        xSemaphoreGive(s->lock);
    }
    if (abort_requested) {
        return ota_finish_reply_status(s, link, finish_seq, finish_cmd,
                                       ESP_GMP_STATUS_BAD_STATE, expect_sha256);
    }

    uint32_t delay_ms = s_cfg.restart_delay_ms;
    if (s_cfg.apply_policy == ESP_GMP_OTA_APPLY_RESTART_IMMEDIATE) {
        delay_ms = 0;
    }

    if (s_cfg.apply_policy != ESP_GMP_OTA_APPLY_MANUAL) {
        err = schedule_restart(delay_ms, &restart);
        if (err != ESP_OK) {
            return ota_finish_reply_status(s, link, finish_seq, finish_cmd,
                                           ESP_GMP_STATUS_INTERNAL, expect_sha256);
        }
    }

    err = esp_ota_set_boot_partition(part);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set_boot_partition failed: %s", esp_err_to_name(err));
        cancel_restart(restart);
        return ota_finish_reply_status(s, link, finish_seq, finish_cmd,
                                       ESP_GMP_STATUS_INTERNAL, expect_sha256);
    }

    if (xSemaphoreTake(s->lock, portMAX_DELAY) == pdTRUE) {
        sess_set_finish_result(s, ESP_GMP_STATUS_OK, expect_sha256);
        xSemaphoreGive(s->lock);
    }
    err = send_write_rsp(link, finish_seq, finish_cmd, ESP_GMP_STATUS_OK, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "send finish OK response failed: %s", esp_err_to_name(err));
    }

    if (s_cfg.apply_policy == ESP_GMP_OTA_APPLY_MANUAL) {
        s_manual_apply_pending = true;
    }

    ota_emit(ESP_GMP_OTA_EVT_COMPLETED, s->image_size, s->image_size, 0, ESP_OK);

    if (s_cfg.apply_policy == ESP_GMP_OTA_APPLY_MANUAL) {
        ESP_LOGI(TAG, "OTA OK, waiting for esp_gmp_ota_apply()");
    } else {
        ESP_LOGI(TAG, "OTA OK, restart in %" PRIu32 " ms", delay_ms);
        trigger_restart(restart);
    }
    return true;
}

static void ota_worker_task(void *arg)
{
    ota_sess_t *s = (ota_sess_t *)arg;
    RingbufHandle_t rb = NULL;
    bool finished_ok = false;

    if (xSemaphoreTake(s->lock, portMAX_DELAY) == pdTRUE) {
        rb = s->rb;
        xSemaphoreGive(s->lock);
    }

    ESP_LOGI(TAG, "OTA worker started");

    while (rb) {
        bool abort_requested = false;
        bool finish_pending = false;

        if (xSemaphoreTake(s->lock, portMAX_DELAY) == pdTRUE) {
            abort_requested = s->abort_requested;
            finish_pending = s->finish_pending;
            xSemaphoreGive(s->lock);
        }
        if (abort_requested) {
            break;
        }

        if (finish_pending) {
            uint32_t queued = 0;
            if (xSemaphoreTake(s->lock, portMAX_DELAY) == pdTRUE) {
                queued = s->queued_bytes;
                xSemaphoreGive(s->lock);
            }
            if (queued == 0) {
                if (ota_worker_try_finish(s, rb)) {
                    finished_ok = true;
                }
                break;
            }
        }

        if (!ota_worker_drain_one(s, rb, OTA_RB_RECV_TICKS)) {
            if (xSemaphoreTake(s->lock, portMAX_DELAY) == pdTRUE) {
                s->abort_requested = true;
                xSemaphoreGive(s->lock);
            }
            break;
        }
    }

    bool need_finish_rsp = false;
    esp_gmp_link_t finish_link = NULL;
    uint16_t finish_seq = 0;
    uint8_t finish_cmd = 0;

    if (xSemaphoreTake(s->lock, portMAX_DELAY) == pdTRUE) {
        if (!finished_ok && s->finish_pending) {
            need_finish_rsp = true;
            finish_link = s->link;
            finish_seq = s->finish_seq;
            finish_cmd = s->finish_cmd;
        }
        if (!finished_ok && s->ota_handle) {
            esp_ota_abort(s->ota_handle);
            s->ota_handle = 0;
            esp_gmp_sha256_abort(&s->sha256);
        }
        ota_worker_finish(s, true);
        if (!finished_ok) {
            sess_clear_state(s);
        }
        xSemaphoreGive(s->lock);
    }

    if (need_finish_rsp) {
        (void)send_write_rsp(finish_link, finish_seq, finish_cmd,
                             ESP_GMP_STATUS_BAD_STATE, NULL, 0);
    }

    ESP_LOGI(TAG, "OTA worker stopped");
    vTaskDelete(NULL);
}

static esp_err_t ota_enqueue_data_chunk(ota_sess_t *s, uint32_t image_offset, uint8_t *payload, uint16_t data_len, uint8_t *frame_buf)
{
    if (!s->rb || !s->worker || !payload || !frame_buf) {
        return ESP_ERR_INVALID_STATE;
    }

    ota_rb_item_t item = {
        .type = OTA_RB_ITEM_TYPE_DATA,
        .image_offset = image_offset,
        .data_len = data_len,
        .payload = payload,
        .frame_buf = frame_buf,
    };

    if (xRingbufferSend(s->rb, &item, sizeof(item), 0) != pdTRUE) {
        return ESP_ERR_NO_MEM;
    }

    s->queued_bytes += data_len;
    return ESP_OK;
}

static esp_err_t handle_control(const esp_gmp_rx_t *pkt, ota_sess_t *s, ota_pending_rsp_t *rsp,
                                bool *emit_started, uint64_t *started_total)
{
    esp_gmp_ota_ctrl_req_t req;
    if (!esp_gmp_ota_ctrl_parse_req(pkt->payload, pkt->payload_len, &req)) {
        return pending_write_rsp(rsp, pkt->link, pkt->sequence, pkt->command_id,
                                 ESP_GMP_STATUS_BAD_LENGTH, NULL, 0);
    }

    switch (req.action) {
    case ESP_GMP_OTA_CTRL_ACTION_START: {
        if (s->closing || s->state != OTA_SESS_IDLE || s->worker || s->rb) {
            return pending_write_rsp(rsp, pkt->link, pkt->sequence, pkt->command_id,
                                     ESP_GMP_STATUS_SESSION_IN_USE, NULL, 0);
        }
        data_holds_clear(s);
        if (req.session_id != 0 || req.image_size == 0) {
            return pending_write_rsp(rsp, pkt->link, pkt->sequence, pkt->command_id,
                                     ESP_GMP_STATUS_BAD_LENGTH, NULL, 0);
        }

        if (s_cfg.accept_cb &&
                !s_cfg.accept_cb(req.image_size, s_cfg.accept_ctx)) {
            return pending_write_rsp(rsp, pkt->link, pkt->sequence, pkt->command_id,
                                     ESP_GMP_STATUS_NOT_SUPPORTED, NULL, 0);
        }

        const esp_partition_t *part = ota_partition_resolve();
        if (!part || req.image_size > part->size) {
            return pending_write_rsp(rsp, pkt->link, pkt->sequence, pkt->command_id,
                                     ESP_GMP_STATUS_BAD_LENGTH, NULL, 0);
        }

        /*
         * Erase incrementally during writes. Passing image_size here would
         * erase the whole image synchronously and block the START response
         * (host control timeout is 40s; ~600KB+ erase often exceeds that).
         */
        esp_err_t err = esp_ota_begin(part, OTA_WITH_SEQUENTIAL_WRITES, &s->ota_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
            return pending_write_rsp(rsp, pkt->link, pkt->sequence, pkt->command_id,
                                     ESP_GMP_STATUS_INTERNAL, NULL, 0);
        }

        if (esp_gmp_sha256_begin(&s->sha256) != ESP_OK) {
            esp_ota_abort(s->ota_handle);
            s->ota_handle = 0;
            return pending_write_rsp(rsp, pkt->link, pkt->sequence, pkt->command_id,
                                     ESP_GMP_STATUS_INTERNAL, NULL, 0);
        }

        RingbufHandle_t rb = xRingbufferCreate(ota_ringbuf_bytes(), RINGBUF_TYPE_NOSPLIT);
        if (!rb) {
            esp_ota_abort(s->ota_handle);
            s->ota_handle = 0;
            esp_gmp_sha256_abort(&s->sha256);
            return pending_write_rsp(rsp, pkt->link, pkt->sequence, pkt->command_id,
                                     ESP_GMP_STATUS_INTERNAL, NULL, 0);
        }

        s->ota_part = part;
        s->rb = rb;
        s->abort_requested = false;
        s->finish_pending = false;
        s->finish_result_valid = false;
        s->bytes_written = 0;
        s->queued_bytes = 0;

        s->session_id = ota_alloc_session_id();
        s->state = OTA_SESS_RECEIVING;
        s->image_size = req.image_size;
        s->bytes_received = 0;
        s->next_data_offset = 0;
        s->partition_bytes_written = 0;

        BaseType_t ok = xTaskCreate(ota_worker_task, "gmp_ota_wr",
                                    CONFIG_ESP_GMP_OTA_APPLY_TASK_STACK, s,
                                    CONFIG_ESP_GMP_OTA_APPLY_TASK_PRIO, &s->worker);
        if (ok != pdPASS) {
            vRingbufferDelete(rb);
            s->rb = NULL;
            esp_ota_abort(s->ota_handle);
            s->ota_handle = 0;
            esp_gmp_sha256_abort(&s->sha256);
            s->state = OTA_SESS_IDLE;
            return pending_write_rsp(rsp, pkt->link, pkt->sequence, pkt->command_id,
                                     ESP_GMP_STATUS_INTERNAL, NULL, 0);
        }

        esp_gmp_ota_ctrl_rsp_t crsp = {
            .session_id = s->session_id,
            .chunk_hint = effective_chunk_hint(pkt->link),
        };
        uint8_t pl[ESP_GMP_OTA_CTRL_RSP_LEN];
        size_t n = esp_gmp_ota_ctrl_build_start_rsp(pl, sizeof(pl), &crsp);
        ESP_LOGI(TAG, "OTA START async worker rb=%u B (%u slots)",
                 (unsigned)ota_ringbuf_bytes(), (unsigned)CONFIG_ESP_GMP_OTA_RINGBUF_SLOTS);
        if (emit_started) {
            *emit_started = true;
        }
        if (started_total) {
            *started_total = req.image_size;
        }
        return pending_write_rsp(rsp, pkt->link, pkt->sequence, pkt->command_id,
                                 ESP_GMP_STATUS_OK, pl, n);
    }

    case ESP_GMP_OTA_CTRL_ACTION_FINISH: {
        if (s->finish_result_valid &&
                req.session_id == s->finish_result_session_id &&
                req.has_sha256 &&
                memcmp(req.image_sha256, s->finish_result_sha256, sizeof(s->finish_result_sha256)) == 0) {
            return pending_write_rsp(rsp, pkt->link, pkt->sequence, pkt->command_id,
                                     s->finish_result_status, NULL, 0);
        }
        if (s->state != OTA_SESS_RECEIVING || req.session_id != s->session_id) {
            return pending_write_rsp(rsp, pkt->link, pkt->sequence, pkt->command_id,
                                     s->state == OTA_SESS_IDLE ? ESP_GMP_STATUS_NO_SESSION
                                     : ESP_GMP_STATUS_BAD_STATE,
                                     NULL, 0);
        }
        if (s->bytes_received != ota_transfer_bytes_expected(s)) {
            return pending_write_rsp(rsp, pkt->link, pkt->sequence, pkt->command_id,
                                     ESP_GMP_STATUS_BAD_STATE, NULL, 0);
        }
        if (req.image_size != s->image_size) {
            return pending_write_rsp(rsp, pkt->link, pkt->sequence, pkt->command_id,
                                     ESP_GMP_STATUS_BAD_LENGTH, NULL, 0);
        }
        if (!req.has_sha256) {
            return pending_write_rsp(rsp, pkt->link, pkt->sequence, pkt->command_id,
                                     ESP_GMP_STATUS_BAD_LENGTH, NULL, 0);
        }

        if (s->finish_pending) {
            if (memcmp(req.image_sha256, s->finish_sha256, sizeof(s->finish_sha256)) != 0) {
                return pending_write_rsp(rsp, pkt->link, pkt->sequence, pkt->command_id,
                                         ESP_GMP_STATUS_BAD_STATE, NULL, 0);
            }
            return pending_write_rsp(rsp, pkt->link, pkt->sequence, pkt->command_id,
                                     ESP_GMP_STATUS_BUSY, NULL, 0);
        }

        s->finish_pending = true;
        s->finish_seq = pkt->sequence;
        s->finish_cmd = pkt->command_id;
        memcpy(s->finish_sha256, req.image_sha256, sizeof(s->finish_sha256));
        ESP_LOGI(TAG, "FINISH pending queued=%" PRIu32 " B", s->queued_bytes);
        return ESP_OK;
    }

    case ESP_GMP_OTA_CTRL_ACTION_ABORT: {
        if (s->state == OTA_SESS_IDLE) {
            return pending_write_rsp(rsp, pkt->link, pkt->sequence, pkt->command_id,
                                     ESP_GMP_STATUS_OK, NULL, 0);
        }
        if (s->state == OTA_SESS_APPLYING) {
            return pending_write_rsp(rsp, pkt->link, pkt->sequence, pkt->command_id,
                                     ESP_GMP_STATUS_BAD_STATE, NULL, 0);
        }
        if (req.session_id != 0 && req.session_id != s->session_id) {
            return pending_write_rsp(rsp, pkt->link, pkt->sequence, pkt->command_id,
                                     ESP_GMP_STATUS_NO_SESSION, NULL, 0);
        }
        sess_abort_async(s);
        return pending_write_rsp(rsp, pkt->link, pkt->sequence, pkt->command_id,
                                 ESP_GMP_STATUS_OK, NULL, 0);
    }

    case ESP_GMP_OTA_CTRL_ACTION_ERASE: {
        if (s->state == OTA_SESS_RECEIVING || s->state == OTA_SESS_APPLYING) {
            if (req.session_id != 0 && req.session_id != s->session_id) {
                return pending_write_rsp(rsp, pkt->link, pkt->sequence, pkt->command_id,
                                         ESP_GMP_STATUS_NO_SESSION, NULL, 0);
            }
            return pending_write_rsp(rsp, pkt->link, pkt->sequence, pkt->command_id,
                                     ESP_GMP_STATUS_BAD_STATE, NULL, 0);
        }
        sess_abort_async(s);
        return pending_write_rsp(rsp, pkt->link, pkt->sequence, pkt->command_id,
                                 ESP_GMP_STATUS_OK, NULL, 0);
    }

    default:
        return pending_write_rsp(rsp, pkt->link, pkt->sequence, pkt->command_id,
                                 ESP_GMP_STATUS_BAD_LENGTH, NULL, 0);
    }
}

static esp_err_t apply_data_chunk(ota_sess_t *s, esp_gmp_link_t link, uint16_t seq, uint8_t cmd,
                                  uint8_t *payload, uint16_t data_len, uint32_t image_offset,
                                  uint8_t *frame_buf, ota_pending_rsp_t *rsp)
{
    if (s->state != OTA_SESS_RECEIVING || !s->ota_handle || !s->worker || s->abort_requested) {
        (void)pending_write_rsp(rsp, link, seq, cmd, ESP_GMP_STATUS_BAD_STATE, NULL, 0);
        return ESP_ERR_INVALID_STATE;
    }
    if (s->finish_pending) {
        (void)pending_write_rsp(rsp, link, seq, cmd, ESP_GMP_STATUS_BAD_STATE, NULL, 0);
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t end = 0;
    if (!ota_data_range_valid(s, image_offset, data_len, &end)) {
        sess_abort_async(s);
        (void)pending_write_rsp(rsp, link, seq, cmd, ESP_GMP_STATUS_BAD_LENGTH, NULL, 0);
        return ESP_ERR_INVALID_SIZE;
    }

    if (image_offset != s->next_data_offset) {
        (void)pending_write_rsp(rsp, link, seq, cmd, ESP_GMP_STATUS_BAD_STATE, NULL, 0);
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = ota_enqueue_data_chunk(s, image_offset, payload, data_len, frame_buf);
    if (err == ESP_ERR_NO_MEM) {
        (void)pending_write_rsp(rsp, link, seq, cmd, ESP_GMP_STATUS_BUSY, NULL, 0);
        return err;
    }
    if (err != ESP_OK) {
        sess_abort_async(s);
        (void)pending_write_rsp(rsp, link, seq, cmd, ESP_GMP_STATUS_INTERNAL, NULL, 0);
        return err;
    }

    s->next_data_offset = end;
    s->bytes_received += data_len;

    return pending_write_rsp(rsp, link, seq, cmd, ESP_GMP_STATUS_OK, NULL, 0);
}

static esp_err_t data_hold_store(ota_sess_t *s, esp_gmp_link_t link, uint16_t seq, uint8_t cmd,
                                 const esp_gmp_ota_data_req_t *req, uint8_t *frame_buf)
{
    for (int i = 0; i < OTA_DATA_HOLD_SLOTS; i++) {
        if (s->data_holds[i].valid && s->data_holds[i].image_offset == req->image_offset) {
            return ESP_ERR_INVALID_STATE;
        }
    }

    for (int i = 0; i < OTA_DATA_HOLD_SLOTS; i++) {
        if (s->data_holds[i].valid) {
            continue;
        }

        s->data_holds[i].valid = true;
        s->data_holds[i].seq = seq;
        s->data_holds[i].cmd = cmd;
        s->data_holds[i].image_offset = req->image_offset;
        s->data_holds[i].data_len = req->data_len;
        s->data_holds[i].payload = (uint8_t *)req->data;
        s->data_holds[i].frame_buf = frame_buf;
        ESP_LOGD(TAG, "hold DATA offset=%" PRIu32 " seq=%u", req->image_offset, (unsigned)seq);
        return ESP_OK;
    }

    return ESP_ERR_NO_MEM;
}

static esp_err_t data_holds_drain(ota_sess_t *s, esp_gmp_link_t link, ota_pending_rsp_t *rsps, size_t rsp_cap, size_t *rsp_count)
{
    esp_err_t err = ESP_OK;
    bool progressed;

    do {
        progressed = false;
        for (int i = 0; i < OTA_DATA_HOLD_SLOTS; i++) {
            ota_data_hold_t *h = &s->data_holds[i];
            if (!h->valid || h->image_offset != s->next_data_offset) {
                continue;
            }

            if (*rsp_count >= rsp_cap) {
                return ESP_ERR_NO_MEM;
            }
            err = apply_data_chunk(s, link, h->seq, h->cmd, h->payload, h->data_len,
                                   h->image_offset, h->frame_buf, &rsps[*rsp_count]);
            if (rsps[*rsp_count].valid) {
                (*rsp_count)++;
            }
            if (err != ESP_OK) {
                if (h->frame_buf) {
                    free(h->frame_buf);
                }
                memset(h, 0, sizeof(*h));
                return err;
            }
            h->frame_buf = NULL;
            memset(h, 0, sizeof(*h));
            progressed = true;
        }
    } while (progressed);

    return err;
}

static void ota_rx_free_msg(ota_rx_msg_t *msg)
{
    if (msg && msg->frame_buf) {
        free(msg->frame_buf);
        msg->frame_buf = NULL;
    }
}

static void ota_rx_process_msg(const ota_rx_msg_t *msg)
{
    ota_sess_t *s = sess_find(msg->link);
    if (!s) {
        send_write_rsp(msg->link, msg->sequence, msg->command_id,
                       ESP_GMP_STATUS_NO_SESSION, NULL, 0);
        ota_rx_free_msg((ota_rx_msg_t *)msg);
        return;
    }

    esp_gmp_parsed_t f;
    if (esp_gmp_frame_parse(msg->frame_buf, msg->frame_len, &f) != 0) {
        send_write_rsp(msg->link, msg->sequence, msg->command_id,
                       ESP_GMP_STATUS_BAD_LENGTH, NULL, 0);
        free(msg->frame_buf);
        return;
    }

    esp_gmp_ota_data_req_t req;
    if (esp_gmp_ota_data_parse_req(f.payload, f.payload_len, &req) == 0) {
        send_write_rsp(msg->link, msg->sequence, msg->command_id,
                       ESP_GMP_STATUS_BAD_LENGTH, NULL, 0);
        free(msg->frame_buf);
        return;
    }

    uint8_t *frame_buf = msg->frame_buf;
    esp_gmp_link_t link = msg->link;
    uint16_t seq = msg->sequence;
    uint8_t cmd = msg->command_id;
    ota_pending_rsp_t rsps[OTA_DATA_HOLD_SLOTS + 1] = { 0 };
    size_t rsp_count = 0;

    if (xSemaphoreTake(s->lock, portMAX_DELAY) != pdTRUE) {
        free(frame_buf);
        return;
    }

    esp_err_t err = ESP_OK;
    if (s->closing || s->state != OTA_SESS_RECEIVING || req.session_id != s->session_id ||
            !s->ota_handle || s->abort_requested) {
        uint8_t st = ESP_GMP_STATUS_BAD_STATE;
        if (req.session_id != s->session_id && s->state != OTA_SESS_IDLE) {
            st = ESP_GMP_STATUS_NO_SESSION;
        }
        err = pending_write_rsp(&rsps[rsp_count++], link, seq, cmd, st, NULL, 0);
        xSemaphoreGive(s->lock);
        free(frame_buf);
        (void)pending_rsp_send(&rsps[0]);
        return;
    }

    uint32_t end = 0;
    if (!ota_data_range_valid(s, req.image_offset, req.data_len, &end)) {
        sess_abort_async(s);
        err = pending_write_rsp(&rsps[rsp_count++], link, seq, cmd,
                                ESP_GMP_STATUS_BAD_LENGTH, NULL, 0);
        xSemaphoreGive(s->lock);
        free(frame_buf);
        (void)pending_rsp_send(&rsps[0]);
        return;
    }

    if (req.image_offset != s->next_data_offset) {
        if (req.image_offset > s->next_data_offset) {
            err = data_hold_store(s, link, seq, cmd, &req, frame_buf);
            if (err == ESP_ERR_NO_MEM) {
                (void)pending_write_rsp(&rsps[rsp_count++], link, seq, cmd,
                                        ESP_GMP_STATUS_BUSY, NULL, 0);
            } else if (err == ESP_ERR_INVALID_STATE) {
                /* Duplicate future offset (e.g. transport redelivery): re-ack. */
                (void)pending_write_rsp(&rsps[rsp_count++], link, seq, cmd,
                                        ESP_GMP_STATUS_OK, NULL, 0);
            }
            xSemaphoreGive(s->lock);
            if (err != ESP_OK) {
                free(frame_buf);
            }
            for (size_t i = 0; i < rsp_count; i++) {
                (void)pending_rsp_send(&rsps[i]);
            }
            return;
        }
        // Duplicate/backward chunk: already applied, re-ack instead of aborting the session.
        err = pending_write_rsp(&rsps[rsp_count++], link, seq, cmd,
                                ESP_GMP_STATUS_OK, NULL, 0);
        xSemaphoreGive(s->lock);
        free(frame_buf);
        (void)pending_rsp_send(&rsps[0]);
        return;
    }

    err = apply_data_chunk(s, link, seq, cmd, (uint8_t *)req.data, req.data_len,
                           req.image_offset, frame_buf, &rsps[rsp_count]);
    if (rsps[rsp_count].valid) {
        rsp_count++;
    }
    if (err == ESP_OK) {
        frame_buf = NULL;
        err = data_holds_drain(s, link, rsps, OTA_DATA_HOLD_SLOTS + 1, &rsp_count);
    }
    xSemaphoreGive(s->lock);

    if (frame_buf) {
        free(frame_buf);
    }
    for (size_t i = 0; i < rsp_count; i++) {
        (void)pending_rsp_send(&rsps[i]);
    }
}

static void ota_rx_task(void *arg)
{
    (void)arg;
    ota_rx_msg_t msg;

    ESP_LOGI(TAG, "OTA RX task started");
    while (true) {
        if (xQueueReceive(s_rx_queue, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (msg.stop) {
            break;
        }
        ota_rx_process_msg(&msg);
    }
    s_rx_task = NULL;
    ESP_LOGI(TAG, "OTA RX task stopped");
    vTaskDelete(NULL);
}

static void ota_rx_queue_drain_all(void)
{
    ota_rx_msg_t msg;

    if (!s_rx_queue) {
        return;
    }
    while (xQueueReceive(s_rx_queue, &msg, 0) == pdTRUE) {
        ota_rx_free_msg(&msg);
    }
}

static void ota_rx_queue_drain_link(esp_gmp_link_t link)
{
    ota_rx_msg_t msg;
    ota_rx_msg_t keep[CONFIG_ESP_GMP_OTA_RX_QUEUE_DEPTH];
    int keep_n = 0;

    if (!s_rx_queue) {
        return;
    }
    while (xQueueReceive(s_rx_queue, &msg, 0) == pdTRUE) {
        if (msg.link == link) {
            ota_rx_free_msg(&msg);
        } else if (keep_n < CONFIG_ESP_GMP_OTA_RX_QUEUE_DEPTH) {
            keep[keep_n++] = msg;
        } else {
            ota_rx_free_msg(&msg);
        }
    }
    for (int i = 0; i < keep_n; i++) {
        if (xQueueSend(s_rx_queue, &keep[i], 0) != pdTRUE) {
            ota_rx_free_msg(&keep[i]);
        }
    }
}

static esp_err_t handle_query(const esp_gmp_rx_t *pkt, ota_sess_t *s, ota_pending_rsp_t *rsp)
{
    if (pkt->payload_len != 0) {
        return pending_read_rsp(rsp, pkt->link, pkt->sequence, pkt->command_id,
                                ESP_GMP_STATUS_BAD_LENGTH, NULL, 0);
    }

    esp_gmp_ota_query_rsp_t qr = { 0 };
    if (s->state == OTA_SESS_IDLE) {
        qr.session_state = ESP_GMP_OTA_SESSION_STATE_IDLE;
    } else if (s->state == OTA_SESS_RECEIVING) {
        qr.session_state = ESP_GMP_OTA_SESSION_STATE_OPEN;
        qr.active_session_id = s->session_id;
        qr.bytes_received = s->bytes_received;
        qr.bytes_expected = ota_transfer_bytes_expected(s);
    } else {
        qr.session_state = ESP_GMP_OTA_SESSION_STATE_CLOSING;
        qr.active_session_id = s->session_id;
        qr.bytes_received = s->bytes_received;
        qr.bytes_expected = ota_transfer_bytes_expected(s);
    }

    uint8_t pl[ESP_GMP_OTA_QUERY_RSP_LEN];
    size_t n = esp_gmp_ota_query_build_rsp(pl, sizeof(pl), &qr);
    return pending_read_rsp(rsp, pkt->link, pkt->sequence, pkt->command_id,
                            ESP_GMP_STATUS_OK, pl, n);
}

static bool ota_device_handler(const esp_gmp_rx_t *pkt)
{
    return esp_gmp_ota_on_packet(pkt);
}

static void ota_link_event_handler(esp_gmp_link_t link, esp_gmp_link_event_type_t event,
                                   esp_err_t error, void *ctx)
{
    (void)error;
    (void)ctx;
    if (event == ESP_GMP_LINK_EVENT_DOWN || event == ESP_GMP_LINK_EVENT_TRANSPORT_ERROR) {
        esp_gmp_ota_on_link_down(link);
    }
}

static void ota_rx_teardown(void)
{
    ota_rx_queue_drain_all();
    if (s_rx_task && s_rx_queue) {
        ota_rx_msg_t stop = {
            .stop = true,
        };
        (void)xQueueSend(s_rx_queue, &stop, portMAX_DELAY);
        while (s_rx_task) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    if (s_rx_queue) {
        vQueueDelete(s_rx_queue);
        s_rx_queue = NULL;
    }
}

esp_err_t esp_gmp_ota_init(const esp_gmp_ota_config_t *cfg)
{
    if (s_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(&s_cfg, 0, sizeof(s_cfg));
    s_cfg.chunk_hint = (uint16_t)CONFIG_ESP_GMP_OTA_CHUNK_HINT;
    s_cfg.restart_delay_ms = (uint32_t)CONFIG_ESP_GMP_OTA_RESTART_DELAY_MS;
    s_cfg.apply_policy = ESP_GMP_OTA_APPLY_RESTART_DELAYED;
    s_manual_apply_pending = false;

    if (cfg) {
        if (cfg->ota_partition_label) {
            strncpy(s_cfg.ota_partition_label, cfg->ota_partition_label,
                    sizeof(s_cfg.ota_partition_label) - 1);
        }
        if (cfg->chunk_hint > 0) {
            s_cfg.chunk_hint = cfg->chunk_hint;
        }
        if (cfg->restart_delay_ms > 0) {
            s_cfg.restart_delay_ms = cfg->restart_delay_ms;
        }
        s_cfg.apply_policy = cfg->apply_policy;
        s_cfg.accept_cb = cfg->accept_cb;
        s_cfg.accept_ctx = cfg->accept_ctx;
        s_cfg.event_cb = cfg->event_cb;
        s_cfg.event_ctx = cfg->event_ctx;
    }

    memset(s_sess, 0, sizeof(s_sess));

    if (s_rx_queue == NULL) {
        s_rx_queue = xQueueCreate(CONFIG_ESP_GMP_OTA_RX_QUEUE_DEPTH, sizeof(ota_rx_msg_t));
        if (!s_rx_queue) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (s_rx_task == NULL) {
        BaseType_t ok = xTaskCreate(ota_rx_task, "gmp_ota_rx",
                                    CONFIG_ESP_GMP_OTA_RX_TASK_STACK, NULL,
                                    CONFIG_ESP_GMP_OTA_RX_TASK_PRIO, &s_rx_task);
        if (ok != pdPASS) {
            vQueueDelete(s_rx_queue);
            s_rx_queue = NULL;
            return ESP_ERR_NO_MEM;
        }
    }

    esp_err_t err = esp_gmp_register_handler(ESP_GMP_GRP_OTA, ota_device_handler);
    if (err != ESP_OK) {
        ota_rx_teardown();
        return err;
    }

    s_link_sub = esp_gmp_link_event_subscribe(ota_link_event_handler, NULL);
    if (!s_link_sub) {
        esp_gmp_unregister_handler(ESP_GMP_GRP_OTA, ota_device_handler);
        ota_rx_teardown();
        return ESP_ERR_NO_MEM;
    }

#if CONFIG_ESP_GMP_PROFILE_OS
    esp_gmp_os_register_capability(ESP_GMP_OS_CAP_OTA_SUPPORTED);
#endif

    s_inited = true;
    ESP_LOGI(TAG, "OTA handler ready (async RX + flash worker)");
    return ESP_OK;
}

void esp_gmp_ota_deinit(void)
{
#if CONFIG_ESP_GMP_PROFILE_OS
    esp_gmp_os_unregister_capability(ESP_GMP_OS_CAP_OTA_SUPPORTED);
#endif

    if (s_link_sub) {
        esp_gmp_link_event_unsubscribe(s_link_sub);
        s_link_sub = NULL;
    }

    esp_gmp_unregister_handler(ESP_GMP_GRP_OTA, ota_device_handler);

    s_inited = false;

    ota_rx_teardown();

    for (int i = 0; i < CONFIG_ESP_GMP_MAX_LINKS; i++) {
        if (s_sess[i].used) {
            esp_gmp_ota_on_link_down(s_sess[i].link);
        }
        if (s_sess[i].lock) {
            vSemaphoreDelete(s_sess[i].lock);
        }
        memset(&s_sess[i], 0, sizeof(s_sess[i]));
    }
}

bool esp_gmp_ota_on_packet(const esp_gmp_rx_t *pkt)
{
    if (!s_inited || !pkt || pkt->group_id != ESP_GMP_GRP_OTA) {
        return false;
    }

    /* Host co-registers for GRP_OTA RSPs; device only consumes requests. */
    if (pkt->op == ESP_GMP_OP_WRITE_RSP || pkt->op == ESP_GMP_OP_READ_RSP) {
        return false;
    }

    if (pkt->command_id == ESP_GMP_OTA_UPLOAD_DATA &&
            pkt->op == ESP_GMP_OP_WRITE_REQ) {
        if (!pkt->frame_buf || pkt->frame_len == 0) {
            return false;
        }
        esp_gmp_ota_data_req_t req_probe;
        if (esp_gmp_ota_data_parse_req(pkt->payload, pkt->payload_len, &req_probe) == 0) {
            send_write_rsp(pkt->link, pkt->sequence, pkt->command_id,
                           ESP_GMP_STATUS_BAD_LENGTH, NULL, 0);
            return false;
        }

        ota_sess_t *s = sess_find(pkt->link);
        if (s && xSemaphoreTake(s->lock, portMAX_DELAY) == pdTRUE) {
            bool aborting = (s->closing || s->abort_requested) && s->state != OTA_SESS_IDLE;
            uint8_t status = req_probe.session_id == s->session_id ?
                             ESP_GMP_STATUS_BAD_STATE : ESP_GMP_STATUS_NO_SESSION;
            xSemaphoreGive(s->lock);
            if (aborting) {
                send_write_rsp(pkt->link, pkt->sequence, pkt->command_id, status, NULL, 0);
                return false;
            }
        }

        ota_rx_msg_t msg = {
            .link = pkt->link,
            .sequence = pkt->sequence,
            .command_id = pkt->command_id,
            .frame_buf = pkt->frame_buf,
            .frame_len = pkt->frame_len,
        };
        if (xQueueSend(s_rx_queue, &msg, 0) != pdTRUE) {
            send_write_rsp(pkt->link, pkt->sequence, pkt->command_id,
                           ESP_GMP_STATUS_BUSY, NULL, 0);
            return false;
        }
        return true;
    }

    ota_sess_t *s = sess_get(pkt->link);
    if (!s) {
        return false;
    }

    if (xSemaphoreTake(s->lock, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    esp_err_t err = ESP_ERR_NOT_SUPPORTED;
    ota_pending_rsp_t rsp = { 0 };
    bool emit_started = false;
    uint64_t started_total = 0;
    if (s->closing) {
        if (pkt->op == ESP_GMP_OP_READ_REQ) {
            err = pending_read_rsp(&rsp, pkt->link, pkt->sequence, pkt->command_id,
                                   ESP_GMP_STATUS_BAD_STATE, NULL, 0);
        } else if (pkt->op == ESP_GMP_OP_WRITE_REQ) {
            err = pending_write_rsp(&rsp, pkt->link, pkt->sequence, pkt->command_id,
                                    ESP_GMP_STATUS_BAD_STATE, NULL, 0);
        }
    } else if (pkt->command_id == ESP_GMP_OTA_UPLOAD_CONTROL &&
               pkt->op == ESP_GMP_OP_WRITE_REQ) {
        ESP_LOGI(TAG, "OTA CONTROL req seq=%u len=%u",
                 (unsigned)pkt->sequence, (unsigned)pkt->payload_len);
        err = handle_control(pkt, s, &rsp, &emit_started, &started_total);
    } else if (pkt->command_id == ESP_GMP_OTA_UPLOAD_QUERY &&
               pkt->op == ESP_GMP_OP_READ_REQ) {
        err = handle_query(pkt, s, &rsp);
    }

    xSemaphoreGive(s->lock);
    if (emit_started) {
        ota_emit(ESP_GMP_OTA_EVT_STARTED, 0, started_total, 0, ESP_OK);
    }
    if (rsp.valid) {
        (void)pending_rsp_send(&rsp);
    }
    (void)err;
    return false;
}

void esp_gmp_ota_on_link_down(esp_gmp_link_t link)
{
    ota_sess_t *s = sess_find(link);
    if (!s) {
        return;
    }

    if (xSemaphoreTake(s->lock, portMAX_DELAY) == pdTRUE) {
        s->closing = true;
        s->abort_requested = true;
        xSemaphoreGive(s->lock);
    }

    ota_rx_queue_drain_link(link);
    ota_sess_stop_worker(s);

    if (xSemaphoreTake(s->lock, portMAX_DELAY) == pdTRUE) {
        ota_worker_finish(s, true);
        sess_abort_ota(s);
        data_holds_clear(s);
        sess_clear_state(s);
        s->finish_result_valid = false;
        s->used = false;
        s->closing = false;
        s->link = NULL;
        xSemaphoreGive(s->lock);
    }

    ota_rx_queue_drain_link(link);
}

esp_err_t esp_gmp_ota_get_status(esp_gmp_link_t link, esp_gmp_ota_status_t *status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    memset(status, 0, sizeof(*status));

    ota_sess_t *s = NULL;
    if (link) {
        s = sess_find(link);
    } else {
        for (int i = 0; i < CONFIG_ESP_GMP_MAX_LINKS; i++) {
            if (s_sess[i].used && s_sess[i].state != OTA_SESS_IDLE) {
                s = &s_sess[i];
                break;
            }
        }
    }
    if (!s) {
        return ESP_OK;
    }

    if (xSemaphoreTake(s->lock, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    status->in_progress = (s->state == OTA_SESS_RECEIVING || s->state == OTA_SESS_APPLYING);
    status->link = s->link;
    status->session_id = s->session_id;
    status->transferred = s->bytes_written;
    status->total = s->image_size;
    status->percent = status->total == 0 ? 0
                      : (uint8_t)((status->transferred * 100) / status->total);
    if (status->percent > 100) {
        status->percent = 100;
    }
    xSemaphoreGive(s->lock);
    return ESP_OK;
}

esp_err_t esp_gmp_ota_apply(void)
{
    if (!s_inited || !s_manual_apply_pending) {
        return ESP_ERR_INVALID_STATE;
    }
    s_manual_apply_pending = false;
    esp_restart();
    return ESP_OK;
}
