/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_gmp_ota_host.h"
#include "esp_gmp_ota_proto.h"
#include "esp_gmp_sha256.h"
#include "esp_gmp.h"
#include "esp_gmp_profile.h"
#include "esp_log.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "esp_gmp_ota_host";

/** Max in-flight OTA_UPLOAD_DATA GMP messages (matches Flux / esp_gmp TX concurrency). */
#ifndef ESP_GMP_OTA_HOST_PIPE_DEPTH
#define ESP_GMP_OTA_HOST_PIPE_DEPTH 2
#endif

#define GMP_RSP_TIMEOUT_MS 40000
#define GMP_DATA_BUSY_MAX_RETRIES 5
#define GMP_CTRL_BUSY_MAX_RETRIES 5
#define GMP_TX_FULL_MAX_RETRIES 8

static uint16_t host_next_seq(esp_gmp_link_t link)
{
    uint16_t seq = esp_gmp_seq_next(link);
    return seq ? seq : 1;
}

typedef struct {
    uint16_t seq;
    esp_gmp_link_t link;
    esp_gmp_tx_params_t tx;
    uint8_t *payload;
    size_t payload_len;
    uint8_t status;
    uint8_t retries;
    bool active;
    bool done;
} gmp_pipe_slot_t;

static bool s_inited;
static esp_gmp_ota_host_config_t s_cfg;
static esp_gmp_link_event_sub_t s_link_sub;
static bool s_registered_ota;
static bool s_registered_os;

static SemaphoreHandle_t s_rsp_sem;
static SemaphoreHandle_t s_pipe_sem;

static bool s_ctrl_wait_active;
static uint16_t s_ctrl_wait_seq;
static uint8_t s_rsp_status;
static uint8_t s_rsp_payload[64];
static size_t s_rsp_payload_len;

static gmp_pipe_slot_t s_pipe[ESP_GMP_OTA_HOST_PIPE_DEPTH];
static portMUX_TYPE s_ctrl_lock = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE s_pipe_lock = portMUX_INITIALIZER_UNLOCKED;
static volatile bool s_upload_cancel;

static uint32_t rd_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void emit_event(uint8_t event_id, uint64_t transferred, uint64_t total)
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
        .role = ESP_GMP_OTA_ROLE_HOST,
        .transferred = transferred,
        .total = total,
        .percent = percent,
        .reason = 0,
        .detail = ESP_OK,
        .profile_extra = NULL,
    };
    s_cfg.event_cb(&ev, s_cfg.event_ctx);
}

static void pipe_reset(void)
{
    uint8_t *to_free[ESP_GMP_OTA_HOST_PIPE_DEPTH];
    int n = 0;

    portENTER_CRITICAL(&s_pipe_lock);
    for (int i = 0; i < ESP_GMP_OTA_HOST_PIPE_DEPTH; i++) {
        if (s_pipe[i].payload) {
            to_free[n++] = s_pipe[i].payload;
        }
    }
    memset(s_pipe, 0, sizeof(s_pipe));
    portEXIT_CRITICAL(&s_pipe_lock);

    for (int i = 0; i < n; i++) {
        free(to_free[i]);
    }
    if (s_pipe_sem) {
        while (xSemaphoreTake(s_pipe_sem, 0) == pdTRUE) {
        }
    }
}

static int pipe_in_flight(void)
{
    int count = 0;

    portENTER_CRITICAL(&s_pipe_lock);
    for (int i = 0; i < ESP_GMP_OTA_HOST_PIPE_DEPTH; i++) {
        if (s_pipe[i].active && !s_pipe[i].done) {
            count++;
        }
    }
    portEXIT_CRITICAL(&s_pipe_lock);
    return count;
}

static esp_err_t pipe_track(esp_gmp_link_t link, const esp_gmp_tx_params_t *tx, const uint8_t *pl, size_t pl_len)
{
    uint8_t *copy = malloc(pl_len);
    if (!copy) {
        return ESP_ERR_NO_MEM;
    }
    memcpy(copy, pl, pl_len);

    portENTER_CRITICAL(&s_pipe_lock);
    for (int i = 0; i < ESP_GMP_OTA_HOST_PIPE_DEPTH; i++) {
        if (!s_pipe[i].active) {
            s_pipe[i].seq = tx->sequence;
            s_pipe[i].link = link;
            s_pipe[i].tx = *tx;
            s_pipe[i].payload = copy;
            s_pipe[i].payload_len = pl_len;
            s_pipe[i].status = ESP_GMP_STATUS_OK;
            s_pipe[i].active = true;
            s_pipe[i].done = false;
            portEXIT_CRITICAL(&s_pipe_lock);
            return ESP_OK;
        }
    }
    portEXIT_CRITICAL(&s_pipe_lock);
    free(copy);
    return ESP_ERR_NO_MEM;
}

static uint8_t *pipe_detach_payload_locked(int i)
{
    uint8_t *payload = s_pipe[i].payload;
    memset(&s_pipe[i], 0, sizeof(s_pipe[i]));
    return payload;
}

static void pipe_on_response(uint16_t seq, uint8_t status)
{
    bool signaled = false;

    portENTER_CRITICAL(&s_pipe_lock);
    for (int i = 0; i < ESP_GMP_OTA_HOST_PIPE_DEPTH; i++) {
        if (s_pipe[i].active && !s_pipe[i].done && s_pipe[i].seq == seq) {
            s_pipe[i].status = status;
            s_pipe[i].done = true;
            signaled = true;
            break;
        }
    }
    portEXIT_CRITICAL(&s_pipe_lock);

    if (signaled && s_pipe_sem) {
        xSemaphoreGive(s_pipe_sem);
    }
}

static esp_err_t pipe_wait_one(uint8_t *out_status)
{
    while (true) {
        if (s_upload_cancel) {
            return ESP_ERR_INVALID_STATE;
        }
        if (xSemaphoreTake(s_pipe_sem, pdMS_TO_TICKS(GMP_RSP_TIMEOUT_MS)) != pdTRUE) {
            ESP_LOGE(TAG, "DATA pipeline response timeout (in_flight=%d)", pipe_in_flight());
            return ESP_ERR_TIMEOUT;
        }

        portENTER_CRITICAL(&s_pipe_lock);
        for (int i = 0; i < ESP_GMP_OTA_HOST_PIPE_DEPTH; i++) {
            if (s_pipe[i].active && s_pipe[i].done) {
                uint8_t status = s_pipe[i].status;
                if (status == ESP_GMP_STATUS_BUSY &&
                        s_pipe[i].retries < GMP_DATA_BUSY_MAX_RETRIES) {
                    esp_gmp_tx_params_t tx = s_pipe[i].tx;
                    esp_gmp_link_t link = s_pipe[i].link;
                    uint8_t *payload = s_pipe[i].payload;
                    size_t payload_len = s_pipe[i].payload_len;
                    uint8_t retries = s_pipe[i].retries + 1;
                    s_pipe[i].done = false;
                    s_pipe[i].retries = retries;
                    portEXIT_CRITICAL(&s_pipe_lock);
                    vTaskDelay(pdMS_TO_TICKS(20U * retries));
                    esp_err_t err = ESP_FAIL;
                    for (uint8_t tx_try = 0; tx_try <= GMP_TX_FULL_MAX_RETRIES; tx_try++) {
                        err = esp_gmp_send(link, &tx, payload, payload_len);
                        if (err == ESP_OK) {
                            break;
                        }
                        if (err != ESP_ERR_ESP_GMP_TX_QUEUE_FULL && err != ESP_ERR_NO_MEM) {
                            break;
                        }
                        esp_gmp_poll();
                        vTaskDelay(pdMS_TO_TICKS(10U * (tx_try + 1U)));
                    }
                    if (err != ESP_OK) {
                        uint8_t *orphan = NULL;
                        portENTER_CRITICAL(&s_pipe_lock);
                        for (int j = 0; j < ESP_GMP_OTA_HOST_PIPE_DEPTH; j++) {
                            if (s_pipe[j].active && s_pipe[j].seq == tx.sequence) {
                                orphan = pipe_detach_payload_locked(j);
                                break;
                            }
                        }
                        portEXIT_CRITICAL(&s_pipe_lock);
                        free(orphan);
                        return err;
                    }
                    continue;
                }
                uint8_t *done_payload = pipe_detach_payload_locked(i);
                portEXIT_CRITICAL(&s_pipe_lock);
                free(done_payload);
                if (out_status) {
                    *out_status = status;
                }
                if (status != ESP_GMP_STATUS_OK) {
                    ESP_LOGE(TAG, "pipelined DATA status=%02x", status);
                    return ESP_FAIL;
                }
                return ESP_OK;
            }
        }
        portEXIT_CRITICAL(&s_pipe_lock);
    }
}

bool esp_gmp_ota_host_on_rsp(const esp_gmp_rx_t *p)
{
    if (!s_inited || !p) {
        return false;
    }
    if (p->op != ESP_GMP_OP_WRITE_RSP && p->op != ESP_GMP_OP_READ_RSP) {
        return false;
    }

    bool ctrl_signaled = false;

    portENTER_CRITICAL(&s_ctrl_lock);
    if (s_ctrl_wait_active && p->sequence == s_ctrl_wait_seq) {
        s_rsp_status = p->status;
        s_rsp_payload_len = p->payload_len < sizeof(s_rsp_payload) ? p->payload_len : sizeof(s_rsp_payload);
        if (s_rsp_payload_len > 0) {
            memcpy(s_rsp_payload, p->payload, s_rsp_payload_len);
        }
        s_ctrl_wait_active = false;
        ctrl_signaled = true;
    }
    portEXIT_CRITICAL(&s_ctrl_lock);

    if (ctrl_signaled) {
        if (s_rsp_sem) {
            xSemaphoreGive(s_rsp_sem);
        }
        return false;
    }

    /* DATA pipeline responses are GRP_OTA WRITE_RSP */
    if (p->group_id == ESP_GMP_GRP_OTA) {
        pipe_on_response(p->sequence, p->status);
    }
    return false;
}

static bool host_group_handler(const esp_gmp_rx_t *pkt)
{
    return esp_gmp_ota_host_on_rsp(pkt);
}

static void ctrl_wait_begin(uint16_t seq)
{
    while (xSemaphoreTake(s_rsp_sem, 0) == pdTRUE) {
    }

    portENTER_CRITICAL(&s_ctrl_lock);
    s_ctrl_wait_active = true;
    s_ctrl_wait_seq = seq;
    s_rsp_status = ESP_GMP_STATUS_OK;
    s_rsp_payload_len = 0;
    portEXIT_CRITICAL(&s_ctrl_lock);
}

static void ctrl_wait_cancel(void)
{
    portENTER_CRITICAL(&s_ctrl_lock);
    s_ctrl_wait_active = false;
    portEXIT_CRITICAL(&s_ctrl_lock);
}

static esp_err_t wait_ctrl_rsp(uint16_t seq, uint32_t timeout_ms)
{
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);
    while (xTaskGetTickCount() < deadline) {
        if (s_upload_cancel) {
            ctrl_wait_cancel();
            return ESP_ERR_INVALID_STATE;
        }
        /* Drain queued GMP TX while waiting (e.g. after CAP occupies a flux slot). */
        esp_gmp_poll();
        TickType_t now = xTaskGetTickCount();
        TickType_t remain = (now < deadline) ? (deadline - now) : 0;
        TickType_t slice = pdMS_TO_TICKS(20);
        if (remain > slice) {
            remain = slice;
        }
        if (remain == 0) {
            break;
        }
        if (xSemaphoreTake(s_rsp_sem, remain) == pdTRUE) {
            if (s_rsp_status != ESP_GMP_STATUS_OK) {
                ESP_LOGE(TAG, "GMP status=%02x seq=%u", s_rsp_status, (unsigned)seq);
                return ESP_FAIL;
            }
            return ESP_OK;
        }
    }
    ctrl_wait_cancel();
    ESP_LOGE(TAG, "control response timeout seq=%u", (unsigned)seq);
    return ESP_ERR_TIMEOUT;
}

static esp_err_t gmp_send_and_wait(esp_gmp_link_t link, esp_gmp_tx_params_t *tx, const uint8_t *pl, size_t pl_len)
{
    for (uint8_t retries = 0; retries <= GMP_CTRL_BUSY_MAX_RETRIES; retries++) {
        ctrl_wait_begin(tx->sequence);

        esp_err_t err = ESP_FAIL;
        for (uint8_t tx_try = 0; tx_try <= GMP_TX_FULL_MAX_RETRIES; tx_try++) {
            err = esp_gmp_send(link, tx, pl, pl_len);
            if (err == ESP_OK) {
                break;
            }
            if (err != ESP_ERR_ESP_GMP_TX_QUEUE_FULL && err != ESP_ERR_NO_MEM) {
                ctrl_wait_cancel();
                return err;
            }
            esp_gmp_poll();
            vTaskDelay(pdMS_TO_TICKS(10U * (tx_try + 1U)));
        }
        if (err != ESP_OK) {
            ctrl_wait_cancel();
            return err;
        }

        err = wait_ctrl_rsp(tx->sequence, GMP_RSP_TIMEOUT_MS);
        if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
            return err;
        }
        if (s_rsp_status != ESP_GMP_STATUS_BUSY || retries == GMP_CTRL_BUSY_MAX_RETRIES) {
            return err;
        }

        vTaskDelay(pdMS_TO_TICKS(20U * (retries + 1U)));
    }

    return ESP_FAIL;
}

static esp_err_t gmp_send_data_pipelined(esp_gmp_link_t link, esp_gmp_tx_params_t *tx, const uint8_t *pl, size_t pl_len)
{
    esp_err_t err = pipe_track(link, tx, pl, pl_len);
    if (err != ESP_OK) {
        return err;
    }

    for (uint8_t tx_try = 0; tx_try <= GMP_TX_FULL_MAX_RETRIES; tx_try++) {
        err = esp_gmp_send(link, tx, pl, pl_len);
        if (err == ESP_OK) {
            esp_gmp_poll();
            return ESP_OK;
        }
        if (err != ESP_ERR_ESP_GMP_TX_QUEUE_FULL && err != ESP_ERR_NO_MEM) {
            break;
        }
        esp_gmp_poll();
        vTaskDelay(pdMS_TO_TICKS(10U * (tx_try + 1U)));
    }

    portENTER_CRITICAL(&s_pipe_lock);
    uint8_t *orphan = NULL;
    for (int i = 0; i < ESP_GMP_OTA_HOST_PIPE_DEPTH; i++) {
        if (s_pipe[i].active && s_pipe[i].seq == tx->sequence) {
            orphan = pipe_detach_payload_locked(i);
            break;
        }
    }
    portEXIT_CRITICAL(&s_pipe_lock);
    free(orphan);
    return err;
}

static void send_abort_best_effort(esp_gmp_link_t link, uint16_t *seq, uint8_t session_id)
{
    uint8_t abort_pl[ESP_GMP_OTA_CTRL_ABORT_ERASE_LEN];
    esp_gmp_ota_ctrl_req_t abort_req = {
        .action = ESP_GMP_OTA_CTRL_ACTION_ABORT,
        .session_id = session_id,
    };
    size_t n = esp_gmp_ota_ctrl_build_req(abort_pl, sizeof(abort_pl), &abort_req);
    if (n == 0) {
        return;
    }

    *seq = host_next_seq(link);
    esp_gmp_tx_params_t tx = {
        .ver = ESP_GMP_VER,
        .op = ESP_GMP_OP_WRITE_REQ,
        .group_id = ESP_GMP_GRP_OTA,
        .sequence = *seq,
        .command_id = ESP_GMP_OTA_UPLOAD_CONTROL,
        .flags = 0,
        .status = 0,
    };
    (void)esp_gmp_send(link, &tx, abort_pl, n);
    esp_gmp_poll();
}

static esp_err_t upload_fail_cleanup(esp_gmp_link_t link, uint16_t *seq, uint8_t session_id,
                                     esp_gmp_sha256_ctx_t *sha, bool sha_started, uint8_t *data_pl,
                                     esp_err_t err, uint64_t transferred, uint64_t total)
{
    send_abort_best_effort(link, seq, session_id);
    if (sha_started && sha) {
        esp_gmp_sha256_abort(sha);
    }
    if (data_pl) {
        free(data_pl);
    }
    pipe_reset();
    if (s_upload_cancel) {
        emit_event(ESP_GMP_OTA_EVT_CANCELLED, transferred, total);
    } else {
        emit_event(ESP_GMP_OTA_EVT_FAILED, transferred, total);
    }
    return err;
}

static void link_event_trampoline(esp_gmp_link_t link, esp_gmp_link_event_type_t event, esp_err_t err,
                                  void *ctx)
{
    (void)ctx;
    esp_gmp_ota_host_on_link_event(link, event, err);
}

esp_err_t esp_gmp_ota_host_init(const esp_gmp_ota_host_config_t *cfg)
{
    if (s_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(&s_cfg, 0, sizeof(s_cfg));
    if (cfg) {
        s_cfg = *cfg;
    }

    if (!s_rsp_sem) {
        s_rsp_sem = xSemaphoreCreateBinary();
        if (!s_rsp_sem) {
            return ESP_ERR_NO_MEM;
        }
    }
    if (!s_pipe_sem) {
        s_pipe_sem = xSemaphoreCreateCounting(ESP_GMP_OTA_HOST_PIPE_DEPTH, 0);
        if (!s_pipe_sem) {
            vSemaphoreDelete(s_rsp_sem);
            s_rsp_sem = NULL;
            return ESP_ERR_NO_MEM;
        }
    }

    pipe_reset();
    s_upload_cancel = false;
    s_registered_ota = false;
    s_registered_os = false;

    /*
     * Host always registers GRP_OTA and GRP_OS handlers. GMP fans out to all
     * co-registered handlers for a group, so device/OS profiles remain
     * independent and need not forward RSPs to the host.
     */
    esp_err_t err = esp_gmp_register_handler(ESP_GMP_GRP_OTA, host_group_handler);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register OTA handler: %s", esp_err_to_name(err));
        goto fail_sem;
    }
    s_registered_ota = true;

    err = esp_gmp_register_handler(ESP_GMP_GRP_OS, host_group_handler);
    if (err != ESP_OK) {
        esp_gmp_unregister_handler(ESP_GMP_GRP_OTA, host_group_handler);
        s_registered_ota = false;
        ESP_LOGE(TAG, "Failed to register OS RSP handler: %s", esp_err_to_name(err));
        goto fail_sem;
    }
    s_registered_os = true;

    s_link_sub = esp_gmp_link_event_subscribe(link_event_trampoline, NULL);
    if (!s_link_sub) {
        esp_gmp_unregister_handler(ESP_GMP_GRP_OS, host_group_handler);
        s_registered_os = false;
        esp_gmp_unregister_handler(ESP_GMP_GRP_OTA, host_group_handler);
        s_registered_ota = false;
        goto fail_sem;
    }

    s_inited = true;
    ESP_LOGI(TAG, "OTA host ready (pipe_depth=%d)", ESP_GMP_OTA_HOST_PIPE_DEPTH);
    return ESP_OK;

fail_sem:
    if (s_pipe_sem) {
        vSemaphoreDelete(s_pipe_sem);
        s_pipe_sem = NULL;
    }
    if (s_rsp_sem) {
        vSemaphoreDelete(s_rsp_sem);
        s_rsp_sem = NULL;
    }
    return err == ESP_OK ? ESP_ERR_NO_MEM : err;
}

void esp_gmp_ota_host_deinit(void)
{
    if (!s_inited) {
        return;
    }

    esp_gmp_ota_host_cancel();
    pipe_reset();

    if (s_link_sub) {
        esp_gmp_link_event_unsubscribe(s_link_sub);
        s_link_sub = NULL;
    }

    if (s_registered_os) {
        esp_gmp_unregister_handler(ESP_GMP_GRP_OS, host_group_handler);
        s_registered_os = false;
    }
    if (s_registered_ota) {
        esp_gmp_unregister_handler(ESP_GMP_GRP_OTA, host_group_handler);
        s_registered_ota = false;
    }

    s_inited = false;
    if (s_rsp_sem) {
        vSemaphoreDelete(s_rsp_sem);
        s_rsp_sem = NULL;
    }
    if (s_pipe_sem) {
        vSemaphoreDelete(s_pipe_sem);
        s_pipe_sem = NULL;
    }
    memset(&s_cfg, 0, sizeof(s_cfg));
    ESP_LOGI(TAG, "OTA host deinitialized");
}

void esp_gmp_ota_host_on_link_event(esp_gmp_link_t link, esp_gmp_link_event_type_t ev,
                                    esp_err_t err)
{
    (void)link;
    (void)err;

    if (!s_inited) {
        return;
    }

    if (ev == ESP_GMP_LINK_EVENT_DOWN || ev == ESP_GMP_LINK_EVENT_TRANSPORT_ERROR) {
        esp_gmp_ota_host_cancel();
    }
}

void esp_gmp_ota_host_cancel(void)
{
    s_upload_cancel = true;
}

esp_err_t esp_gmp_ota_host_query_caps(esp_gmp_link_t link, esp_gmp_ota_host_caps_t *out)
{
    if (!s_inited || !link || !out) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t seq = host_next_seq(link);

    esp_gmp_tx_params_t tx = {
        .ver = ESP_GMP_VER,
        .op = ESP_GMP_OP_READ_REQ,
        .group_id = ESP_GMP_GRP_OS,
        .sequence = seq,
        .command_id = ESP_GMP_OS_CAP_QUERY,
        .flags = 0,
        .status = 0,
    };

    esp_err_t err = gmp_send_and_wait(link, &tx, NULL, 0);
    if (err != ESP_OK) {
        return err;
    }

    if (s_rsp_payload_len < 12) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    out->link = link;
    out->max_gmp_payload = rd_be32(&s_rsp_payload[6]);
    out->chunk_size = esp_gmp_ota_chunk_for_flash(out->max_gmp_payload);
    return ESP_OK;
}

static esp_err_t mem_read_fn(void *ctx, size_t offset, uint8_t *buf, size_t len, size_t *out_len)
{
    const uint8_t *image = ctx;

    memcpy(buf, image + offset, len);
    *out_len = len;
    return ESP_OK;
}

esp_err_t esp_gmp_ota_host_upload_stream(esp_gmp_link_t link, size_t image_len,
                                         esp_gmp_ota_host_read_fn_t read_fn, void *read_ctx)
{
    if (!s_inited || !link || !read_fn || image_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    pipe_reset();
    s_upload_cancel = false;

    esp_gmp_ota_host_caps_t caps;
    esp_err_t err = esp_gmp_ota_host_query_caps(link, &caps);
    if (err != ESP_OK) {
        return err;
    }

    uint16_t chunk = caps.chunk_size;
    if (chunk == 0) {
        chunk = ESP_GMP_OTA_FLASH_WRITE_ALIGN;
    }

    emit_event(ESP_GMP_OTA_EVT_STARTED, 0, image_len);

    uint16_t seq = host_next_seq(link);
    uint8_t start_pl[ESP_GMP_OTA_CTRL_START_LEN];
    esp_gmp_ota_ctrl_req_t start_req = {
        .action = ESP_GMP_OTA_CTRL_ACTION_START,
        .session_id = 0,
        .flags = 0,
        .image_size = (uint32_t)image_len,
    };
    size_t start_n = esp_gmp_ota_ctrl_build_req(start_pl, sizeof(start_pl), &start_req);
    if (start_n == 0) {
        emit_event(ESP_GMP_OTA_EVT_FAILED, 0, image_len);
        return ESP_ERR_INVALID_SIZE;
    }

    esp_gmp_tx_params_t tx = {
        .ver = ESP_GMP_VER,
        .op = ESP_GMP_OP_WRITE_REQ,
        .group_id = ESP_GMP_GRP_OTA,
        .sequence = seq,
        .command_id = ESP_GMP_OTA_UPLOAD_CONTROL,
        .flags = 0,
        .status = 0,
    };

    ESP_LOGI(TAG, "OTA START send seq=%u image=%zu chunk=%u",
             (unsigned)seq, image_len, (unsigned)chunk);
    err = gmp_send_and_wait(link, &tx, start_pl, start_n);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA START failed: %s", esp_err_to_name(err));
        emit_event(s_upload_cancel ? ESP_GMP_OTA_EVT_CANCELLED : ESP_GMP_OTA_EVT_FAILED,
                   0, image_len);
        return err;
    }
    ESP_LOGI(TAG, "OTA START accepted seq=%u", (unsigned)seq);

    esp_gmp_ota_ctrl_rsp_t start_rsp;
    if (!esp_gmp_ota_ctrl_parse_rsp(s_rsp_payload, s_rsp_payload_len, &start_rsp)) {
        emit_event(ESP_GMP_OTA_EVT_FAILED, 0, image_len);
        return ESP_ERR_INVALID_RESPONSE;
    }

    uint8_t session_id = start_rsp.session_id;
    uint16_t hint = start_rsp.chunk_hint;
    if (hint > 0) {
        hint = esp_gmp_ota_align_chunk(hint);
        if (hint < chunk) {
            chunk = hint;
        }
    }
    chunk = esp_gmp_ota_align_chunk(chunk);

    uint8_t *data_pl = malloc(ESP_GMP_OTA_DATA_HDR_LEN + chunk);
    if (!data_pl) {
        return upload_fail_cleanup(link, &seq, session_id, NULL, false, NULL, ESP_ERR_NO_MEM, 0, image_len);
    }

    esp_gmp_sha256_ctx_t sha;
    err = esp_gmp_sha256_begin(&sha);
    if (err != ESP_OK) {
        return upload_fail_cleanup(link, &seq, session_id, NULL, false, data_pl, err, 0, image_len);
    }

    size_t send_offset = 0;
    size_t chunks_acked = 0;
    int in_flight = 0;

    while (send_offset < image_len || in_flight > 0) {
        if (s_upload_cancel) {
            return upload_fail_cleanup(link, &seq, session_id, &sha, true, data_pl,
                                       ESP_ERR_INVALID_STATE, send_offset, image_len);
        }
        while (send_offset < image_len && in_flight < ESP_GMP_OTA_HOST_PIPE_DEPTH) {
            size_t data_len = image_len - send_offset;
            if (data_len > chunk) {
                data_len = chunk;
            }

            size_t got = 0;
            err = read_fn(read_ctx, send_offset, &data_pl[ESP_GMP_OTA_DATA_HDR_LEN], data_len, &got);
            if (err != ESP_OK || got != data_len) {
                ESP_LOGE(TAG, "read failed at offset=%zu want=%zu got=%zu",
                         send_offset, data_len, got);
                return upload_fail_cleanup(link, &seq, session_id, &sha, true, data_pl,
                                           err != ESP_OK ? err : ESP_FAIL, send_offset, image_len);
            }

            err = esp_gmp_sha256_update(&sha, &data_pl[ESP_GMP_OTA_DATA_HDR_LEN], data_len);
            if (err != ESP_OK) {
                return upload_fail_cleanup(link, &seq, session_id, &sha, true, data_pl, err,
                                           send_offset, image_len);
            }

            esp_gmp_ota_data_req_t data_req = {
                .session_id = session_id,
                .flags = (send_offset + data_len >= image_len) ? ESP_GMP_OTA_DATA_FLAG_LAST_CHUNK : 0,
                .data_len = (uint16_t)data_len,
                .image_offset = (uint32_t)send_offset,
                .data = NULL, /* payload already filled after HDR */
            };
            size_t data_n = esp_gmp_ota_data_build_req(data_pl, ESP_GMP_OTA_DATA_HDR_LEN + data_len,
                                                       &data_req);
            if (data_n == 0) {
                return upload_fail_cleanup(link, &seq, session_id, &sha, true, data_pl,
                                           ESP_ERR_INVALID_SIZE, send_offset, image_len);
            }

            seq = host_next_seq(link);
            tx.sequence = seq;
            tx.command_id = ESP_GMP_OTA_UPLOAD_DATA;
            err = gmp_send_data_pipelined(link, &tx, data_pl, data_n);
            if (err != ESP_OK) {
                return upload_fail_cleanup(link, &seq, session_id, &sha, true, data_pl, err,
                                           send_offset, image_len);
            }

            send_offset += data_len;
            in_flight++;
        }

        err = pipe_wait_one(NULL);
        if (err != ESP_OK) {
            return upload_fail_cleanup(link, &seq, session_id, &sha, true, data_pl, err,
                                       send_offset, image_len);
        }

        in_flight--;
        chunks_acked++;
        if ((chunks_acked % 16) == 0 || (send_offset == image_len && in_flight == 0)) {
            ESP_LOGI(TAG, "upload pipelined %zu/%zu chunks (in_flight=%d)",
                     chunks_acked, (image_len + chunk - 1) / chunk, in_flight);
            emit_event(ESP_GMP_OTA_EVT_PROGRESS, send_offset, image_len);
        }

        esp_gmp_poll();
    }

    free(data_pl);
    pipe_reset();

    uint8_t digest[32];
    err = esp_gmp_sha256_finish(&sha, digest);
    if (err != ESP_OK) {
        return upload_fail_cleanup(link, &seq, session_id, &sha, true, NULL, err,
                                   image_len, image_len);
    }

    uint8_t finish_pl[ESP_GMP_OTA_CTRL_FINISH_LEN];
    esp_gmp_ota_ctrl_req_t finish_req = {
        .action = ESP_GMP_OTA_CTRL_ACTION_FINISH,
        .session_id = session_id,
        .flags = ESP_GMP_OTA_CTRL_FLAG_SHA256,
        .image_size = (uint32_t)image_len,
        .has_sha256 = true,
    };
    memcpy(finish_req.image_sha256, digest, 32);
    size_t finish_n = esp_gmp_ota_ctrl_build_req(finish_pl, sizeof(finish_pl), &finish_req);
    if (finish_n == 0) {
        return upload_fail_cleanup(link, &seq, session_id, NULL, false, NULL,
                                   ESP_ERR_INVALID_SIZE, image_len, image_len);
    }

    seq = host_next_seq(link);
    tx.sequence = seq;
    tx.command_id = ESP_GMP_OTA_UPLOAD_CONTROL;
    err = gmp_send_and_wait(link, &tx, finish_pl, finish_n);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "OTA finish OK — device should reboot");
        emit_event(ESP_GMP_OTA_EVT_COMPLETED, image_len, image_len);
        return ESP_OK;
    }
    return upload_fail_cleanup(link, &seq, session_id, NULL, false, NULL, err, image_len, image_len);
}

esp_err_t esp_gmp_ota_host_upload(esp_gmp_link_t link, const uint8_t *image, size_t image_len)
{
    if (!image) {
        return ESP_ERR_INVALID_ARG;
    }

    return esp_gmp_ota_host_upload_stream(link, image_len, mem_read_fn, (void *)image);
}
