/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * BLE NUS: one JSON line per message. V1 envelope protocol (@c json_format.md)
 * for permission.request / session.status. Also handles legacy status/name/unpair.
 *
 * Firmware protocol bridge:
 * - BLE UART is byte-oriented, so this file turns incoming bytes into JSONL messages.
 * - Each complete line is either a legacy command or a v1 envelope documented in json_format.md.
 * - Permission prompts are intentionally single-flight: the wearable UI can show one decision at a time.
 * - Session status messages are fire-and-forget hints used to keep the board expression in sync with OpenCode.
 */

#include "ble_protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "ble_uart.h"
#include "host/ble_store.h"
#include "emote_demo.h"

static const char *TAG = "ble_protocol";

#define BLE_PROTOCOL_NVS_NS       "ble_uart"
#define BLE_PROTOCOL_NVS_KEY_NAME "name"
#define BLE_PROTOCOL_NAME_MAX_LEN 24
/** Mutex protects @c s_permission_pending and @c s_pending_request_id. */
#define BLE_PROTOCOL_PERMISSION_WAIT_TICKS pdMS_TO_TICKS(30000)
#define BLE_PROTOCOL_PERMISSION_OUTCOME_SIZE 8
#define BLE_PROTOCOL_PERMISSION_OUTCOME_QUEUE_LEN 4
#define BLE_PROTOCOL_PERMISSION_TASK_STACK 4096
#define BLE_PROTOCOL_PERMISSION_WAIT_POLL_TICKS pdMS_TO_TICKS(100)
#define BLE_PROTOCOL_PERMISSION_TASK_PRIORITY 5
#define BLE_PROTOCOL_METADATA_TEXT_SIZE 128
#define BLE_PROTOCOL_RX_TASK_STACK 6144
#define BLE_PROTOCOL_RX_TASK_PRIORITY 5

/* ---- Line buffering for raw BLE RX bytes ----
 *
 * BLE writes are not message-framed: a central may split one JSON object across
 * several writes, or coalesce multiple small writes. The parser therefore owns
 * a small FreeRTOS queue of byte chunks and reconstructs a full line before
 * handing it to ble_protocol_handle_line(). The newline is the only record boundary.
 */
#define BLE_PROTOCOL_RX_LINE_MAX       2048
#define BLE_PROTOCOL_RX_QUEUE_ITEM_MAX 64
#define BLE_PROTOCOL_RX_QUEUE_LEN      16
#define BLE_PROTOCOL_REQUEST_ID_MAX    128

typedef struct {
    size_t len;
    uint8_t data[BLE_PROTOCOL_RX_QUEUE_ITEM_MAX];
} ble_protocol_rx_chunk_t;

typedef void (*ble_protocol_v1_handler_t)(cJSON *root, const char *id, uint32_t line_generation);

typedef struct {
    const char *op;
    bool require_request_id;
    ble_protocol_v1_handler_t handler;
} ble_protocol_v1_dispatch_t;

static QueueHandle_t s_rx_queue;
static TaskHandle_t  s_rx_task;
static volatile bool s_rx_overflow;
static volatile uint32_t s_connection_generation;

static void ble_protocol_rx_task(void *arg);
static void ble_protocol_permission_mutex_ensure(void);
static void ble_protocol_permission_lock(void);
static void ble_protocol_permission_unlock(void);
static void ble_protocol_permission_outcome_ensure(void);
static void ble_protocol_pending_request_clear_locked(void);
static bool ble_protocol_permission_cancel_pending(void);
static void ble_protocol_permission_wait_task(void *arg);
static esp_err_t ble_protocol_nvs_save_str(const char *key, const char *value);
static void ble_protocol_send_json_line(cJSON *root);
static void ble_protocol_send_v1_error(const char *id, const char *error);
static void ble_protocol_send_v1_permission_reply(const char *id, const char *decision, const char *message);
static void ble_protocol_format_permission_metadata(const cJSON *j_meta, char *buf, size_t buf_size);
static cJSON *ble_protocol_make_legacy_ack(const char *name, bool ok);
static void ble_protocol_handle_legacy_status(const cJSON *req);
static void ble_protocol_handle_legacy_name(const cJSON *req);
static void ble_protocol_handle_legacy_unpair(const cJSON *req);
static bool ble_protocol_dispatch_v1(cJSON *root, const char *op, const char *id, uint32_t line_generation);
static void ble_protocol_handle_v1_permission_request(cJSON *root, const char *id, uint32_t line_generation);
static void ble_protocol_handle_v1_permission_cancel(cJSON *root);
static void ble_protocol_handle_v1_session_status(cJSON *root);
static void ble_protocol_handle_v1_permission_cancel_dispatch(cJSON *root, const char *id, uint32_t line_generation);
static void ble_protocol_handle_v1_session_status_dispatch(cJSON *root, const char *id, uint32_t line_generation);

static void ble_protocol_rx_task(void *arg)
{
    (void)arg;

    uint8_t *line = malloc(BLE_PROTOCOL_RX_LINE_MAX);
    size_t line_len = 0;
    bool line_overflow = false;
    ble_protocol_rx_chunk_t item;

    if (line == NULL) {
        ESP_LOGE(TAG, "rx line buffer alloc failed");
        s_rx_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    while (xQueueReceive(s_rx_queue, &item, portMAX_DELAY) == pdTRUE) {
        /* A zero-length item is an internal reset marker. It lets disconnect
         * cleanup discard a partially received JSON line without deleting and
         * recreating the RX task. */
        if (item.len == 0) {
            s_rx_overflow = false;
            line_len = 0;
            line_overflow = false;
            continue;
        }
        if (s_rx_overflow) {
            s_rx_overflow = false;
            line_len = 0;
            line_overflow = true;
        }
        for (size_t i = 0; i < item.len; i++) {
            uint8_t b = item.data[i];
            if (b == '\n') {
                /* Only complete, non-overflowed lines reach the JSON parser.
                 * Oversized/corrupt lines are ignored until their newline so
                 * the next JSONL record can be parsed normally. */
                if (!line_overflow && line_len > 0) {
                    ble_protocol_handle_line(line, line_len);
                }
                line_len = 0;
                line_overflow = false;
                continue;
            }
            if (b == '\r') {
                continue;
            }
            if (line_overflow) {
                continue;
            }
            if (line_len < BLE_PROTOCOL_RX_LINE_MAX) {
                line[line_len++] = b;
            } else {
                ESP_LOGW(TAG, "rx line overflow (>%d bytes), dropping", BLE_PROTOCOL_RX_LINE_MAX);
                line_len = 0;
                line_overflow = true;
            }
        }
    }

    free(line);
    s_rx_task = NULL;
    vTaskDelete(NULL);
}

void ble_protocol_rx_feed(const uint8_t *data, size_t len)
{
    /* BLE GATT access callbacks run on the NimBLE host task. Keep this function
     * short: copy RX bytes into the command queue and let ble_protocol_rx_task perform
     * JSON parsing, cJSON allocation, and UI coordination. */
    if (data == NULL || len == 0) {
        return;
    }
    if (s_rx_queue == NULL) {
        ESP_LOGW(TAG, "protocol rx queue not ready, dropping %u bytes", (unsigned)len);
        return;
    }

    for (size_t off = 0; off < len; off += BLE_PROTOCOL_RX_QUEUE_ITEM_MAX) {
        ble_protocol_rx_chunk_t item = { 0 };
        item.len = len - off;
        if (item.len > sizeof(item.data)) {
            item.len = sizeof(item.data);
        }
        memcpy(item.data, data + off, item.len);
        if (xQueueSend(s_rx_queue, &item, 0) != pdTRUE) {
            s_rx_overflow = true;
            ESP_LOGW(TAG, "protocol rx queue full, dropping %u bytes until next newline", (unsigned)(len - off));
            break;
        }
    }
}

/* Single-flight permission state. The request id is echoed in the reply so the
 * daemon can match the device decision to its original HTTP /request. The
 * generation ties the pending prompt to the BLE connection that created it. */
static char              s_pending_request_id[BLE_PROTOCOL_REQUEST_ID_MAX];
static uint32_t          s_pending_generation;
static bool              s_permission_pending;
static SemaphoreHandle_t s_permission_mutex;
static QueueHandle_t     s_permission_outcome_queue; /* once|reject from @ref ble_protocol_submit_permission */
static TaskHandle_t      s_permission_wait_task;

static void ble_protocol_permission_mutex_ensure(void)
{
    if (s_permission_mutex != NULL) {
        return;
    }
    s_permission_mutex = xSemaphoreCreateMutex();
    if (s_permission_mutex == NULL) {
        ESP_LOGE(TAG, "xSemaphoreCreateMutex(hook) failed");
    }
}

static void ble_protocol_permission_lock(void)
{
    ble_protocol_permission_mutex_ensure();
    if (s_permission_mutex != NULL) {
        (void)xSemaphoreTake(s_permission_mutex, portMAX_DELAY);
    }
}

static void ble_protocol_permission_unlock(void)
{
    if (s_permission_mutex != NULL) {
        (void)xSemaphoreGive(s_permission_mutex);
    }
}

static esp_err_t ble_protocol_nvs_save_str(const char *key, const char *value)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(BLE_PROTOCOL_NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_str(h, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

/* Send a cJSON object as one '\n'-terminated notification, then free it. */
static void ble_protocol_send_json_line(cJSON *root)
{
    if (root == NULL) {
        return;
    }
    char *txt = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (txt == NULL) {
        return;
    }
    size_t len = strlen(txt);
    char  *line = malloc(len + 2);
    if (line != NULL) {
        memcpy(line, txt, len);
        line[len]     = '\n';
        line[len + 1] = '\0';
        (void)ble_uart_tx((const uint8_t *)line, len + 1);
        free(line);
    }
    cJSON_free(txt);
}

/* ---- v1 envelope helpers ---- */

/** Send a v1 error envelope with @a id and @a error string. */
static void ble_protocol_send_v1_error(const char *id, const char *error)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return;
    }
    cJSON_AddNumberToObject(root, "v", 1);
    cJSON_AddStringToObject(root, "id", id ? id : "");
    cJSON_AddBoolToObject(root, "ok", false);
    cJSON_AddStringToObject(root, "error", error ? error : "unknown");
    ble_protocol_send_json_line(root);
}

/** Send a v1 permission reply envelope: @a decision (@c "once" or @c "reject") with @a message. */
static void ble_protocol_send_v1_permission_reply(const char *id, const char *decision, const char *message)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return;
    }
    cJSON_AddNumberToObject(root, "v", 1);
    cJSON_AddStringToObject(root, "id", id ? id : "");
    cJSON_AddBoolToObject(root, "ok", true);
    cJSON *data = cJSON_AddObjectToObject(root, "data");
    if (data != NULL) {
        cJSON_AddStringToObject(data, "decision", decision ? decision : "reject");
        cJSON_AddStringToObject(data, "message", message ? message : "");
    }
    char *preview = cJSON_PrintUnformatted(root);
    if (preview != NULL) {
        ESP_LOGI(TAG, "reply: %s", preview);
        cJSON_free(preview);
    }
    ble_protocol_send_json_line(root);
}

/** Extract compact "key: value" string from metadata; prefer command/path/url. */
static void ble_protocol_format_permission_metadata(const cJSON *j_meta, char *buf, size_t buf_size)
{
    if (buf_size == 0) {
        return;
    }
    buf[0] = '\0';
    if (!cJSON_IsObject(j_meta) || j_meta->child == NULL) {
        return;
    }

    static const char *preferred_keys[] = { "command", "path", "url" };
    for (size_t i = 0; i < sizeof(preferred_keys) / sizeof(preferred_keys[0]); i++) {
        const cJSON *item = cJSON_GetObjectItemCaseSensitive(j_meta, preferred_keys[i]);
        if (cJSON_IsString(item) && item->valuestring != NULL) {
            (void)snprintf(buf, buf_size, "%s: %s", preferred_keys[i], item->valuestring);
            return;
        }
    }

    for (const cJSON *child = j_meta->child; child; child = child->next) {
        if (cJSON_IsString(child) && child->valuestring != NULL && child->string != NULL) {
            (void)snprintf(buf, buf_size, "%s: %s", child->string, child->valuestring);
            return;
        }
    }
}

static void ble_protocol_permission_outcome_ensure(void)
{
    /* Lazily allocate the decision queue because it is only used by the
     * OpenCode permission toy. Queue items are small strings: "once", "reject",
     * or the internal "cancel" marker. */
    if (s_permission_outcome_queue != NULL) {
        return;
    }
    s_permission_outcome_queue = xQueueCreate(BLE_PROTOCOL_PERMISSION_OUTCOME_QUEUE_LEN,
                                              BLE_PROTOCOL_PERMISSION_OUTCOME_SIZE);
    if (s_permission_outcome_queue == NULL) {
        ESP_LOGE(TAG, "permission outcome queue create failed");
    }
}

bool ble_protocol_permission_is_pending(void)
{
    /* UI-side helper: the key handler should only consume single/long clicks
     * when a permission prompt is actually being displayed. */
    bool p;
    ble_protocol_permission_lock();
    p = s_permission_pending;
    ble_protocol_permission_unlock();
    return p;
}

void ble_protocol_submit_permission(const char *behavior)
{
    /* Called from the UI code. Only expose the two OpenCode decisions
     * supported by the single-key hardware mapping: approve once or reject. */
    if (behavior == NULL) {
        return;
    }
    if (strcmp(behavior, "once") != 0 && strcmp(behavior, "reject") != 0) {
        return;
    }
    ble_protocol_permission_outcome_ensure();
    if (s_permission_outcome_queue == NULL) {
        return;
    }
    ble_protocol_permission_lock();
    if (!s_permission_pending) {
        ble_protocol_permission_unlock();
        return;
    }
    char outb[BLE_PROTOCOL_PERMISSION_OUTCOME_SIZE] = { 0 };
    (void)strncpy(outb, behavior, sizeof(outb) - 1);
    if (xQueueSend(s_permission_outcome_queue, outb, 0) != pdTRUE) {
        ESP_LOGW(TAG, "hook outcome queue full");
    }
    ble_protocol_permission_unlock();
}

static void ble_protocol_pending_request_clear_locked(void)
{
    /* Caller must hold s_permission_mutex. Clear all fields together so readers never
     * observe a pending flag with a stale request id/generation. */
    s_permission_pending = false;
    s_pending_generation = 0;
    s_pending_request_id[0] = '\0';
}

static bool ble_protocol_permission_cancel_pending(void)
{
    /* Host-side cancellation is fire-and-forget: it clears the local prompt and
     * wakes ble_protocol_permission_wait_task with an internal marker so no permission reply is
     * emitted for a request that OpenCode no longer needs. */
    bool had_pending;

    ble_protocol_permission_outcome_ensure();

    ble_protocol_permission_lock();
    had_pending = s_permission_pending;
    if (had_pending) {
        ble_protocol_pending_request_clear_locked();
    }
    ble_protocol_permission_unlock();

    if (!had_pending) {
        return false;
    }

    if (s_permission_outcome_queue != NULL) {
        char cancel[BLE_PROTOCOL_PERMISSION_OUTCOME_SIZE] = { 0 };
        (void)strncpy(cancel, "cancel", sizeof(cancel) - 1);
        xQueueReset(s_permission_outcome_queue);
        if (xQueueSend(s_permission_outcome_queue, cancel, 0) != pdTRUE) {
            ESP_LOGW(TAG, "hook cancel queue full");
        }
    }

    ESP_LOGI(TAG, "pending permission cancelled by host state");
    emote_demo_clear_permission();
    return true;
}

static void ble_protocol_permission_wait_task(void *arg)
{
    uint32_t task_generation = (uint32_t)(uintptr_t)arg;

    char outb[BLE_PROTOCOL_PERMISSION_OUTCOME_SIZE] = { 0 };
    /* Wait for the board key decision. No input within 30 seconds fails
     * closed as "reject", matching the safety defaults in json_format.md. */
    TickType_t deadline = xTaskGetTickCount() + BLE_PROTOCOL_PERMISSION_WAIT_TICKS;
    bool got_outcome = false;
    while ((int32_t)(xTaskGetTickCount() - deadline) < 0) {
        if (xQueueReceive(s_permission_outcome_queue, &outb, BLE_PROTOCOL_PERMISSION_WAIT_POLL_TICKS) == pdTRUE) {
            got_outcome = true;
            break;
        }

        ble_protocol_permission_lock();
        bool still_current = s_permission_pending && (s_pending_generation == task_generation)
                             && (task_generation == s_connection_generation);
        ble_protocol_permission_unlock();
        if (!still_current) {
            break;
        }
    }

    char *rid = NULL;
    bool active;
    bool task_owns_prompt;
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();

    ble_protocol_permission_lock();
    /* Revalidate after the wait: the BLE link might have reconnected, the
     * prompt might have been cancelled, or a newer prompt might have replaced
     * this task. Only the current generation may send a reply. */
    task_owns_prompt = s_permission_pending && (s_pending_generation == task_generation);
    active = s_permission_pending && (s_pending_generation == task_generation) && (task_generation == s_connection_generation)
             && (s_pending_request_id[0] != '\0');
    if (active) {
        rid = strdup(s_pending_request_id);
        active = (rid != NULL);
    } else {
        rid = NULL;
    }
    if (task_owns_prompt) {
        ble_protocol_pending_request_clear_locked();
    }
    if (s_permission_wait_task == current_task) {
        s_permission_wait_task = NULL;
    }
    ble_protocol_permission_unlock();

    if (!active) {
        if (task_owns_prompt) {
            emote_demo_clear_permission();
        }
    } else if (!got_outcome) {
        ble_protocol_send_v1_permission_reply(rid, "reject", "Timed out");
        emote_demo_show_permission_timeout();
    } else if (strcmp(outb, "once") == 0 || strcmp(outb, "reject") == 0) {
        const char *msg = (strcmp(outb, "once") == 0) ? "Approved from BLE device" : "Rejected from BLE device";
        ble_protocol_send_v1_permission_reply(rid, outb, msg);
    } else {
        /* Internal cancel on BLE connection change, or an invalid queued value. */
        emote_demo_clear_permission();
    }

    free(rid);
    vTaskDelete(NULL);
}

static cJSON *ble_protocol_make_legacy_ack(const char *name, bool ok)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }
    cJSON_AddStringToObject(root, "ack", name);
    cJSON_AddBoolToObject(root, "ok",  ok);
    return root;
}

static void ble_protocol_handle_legacy_status(const cJSON *req)
{
    (void)req;
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return;
    }
    cJSON_AddStringToObject(root, "ack", "status");
    cJSON_AddBoolToObject(root, "ok",  true);

    cJSON *data = cJSON_AddObjectToObject(root, "data");
    if (data != NULL) {
        cJSON_AddBoolToObject(data, "ready",      ble_uart_is_connected());
        cJSON_AddBoolToObject(data, "connected",  ble_uart_is_connected());
        cJSON_AddBoolToObject(data, "subscribed", ble_uart_is_subscribed());

        cJSON *sys = cJSON_AddObjectToObject(data, "sys");
        if (sys != NULL) {
            cJSON_AddNumberToObject(sys, "up", (double)(esp_timer_get_time() / 1000000));
            cJSON_AddNumberToObject(sys, "heap", (double)esp_get_free_heap_size());
        }
    }

    ble_protocol_send_json_line(root);
}

static void ble_protocol_handle_legacy_name(const cJSON *req)
{
    const cJSON *jname = cJSON_GetObjectItemCaseSensitive(req, "name");
    if (!cJSON_IsString(jname) || jname->valuestring == NULL) {
        ble_protocol_send_json_line(ble_protocol_make_legacy_ack("name", false));
        return;
    }
    char clipped[BLE_PROTOCOL_NAME_MAX_LEN];
    strncpy(clipped, jname->valuestring, sizeof(clipped) - 1);
    clipped[sizeof(clipped) - 1] = '\0';

    if (ble_protocol_nvs_save_str(BLE_PROTOCOL_NVS_KEY_NAME, clipped) != ESP_OK) {
        ESP_LOGW(TAG, "nvs name save failed");
        ble_protocol_send_json_line(ble_protocol_make_legacy_ack("name", false));
        return;
    }
    ESP_LOGI(TAG, "ble name stored as '%s' (reboot to apply)", clipped);
    cJSON *ack = ble_protocol_make_legacy_ack("name", true);
    if (ack != NULL) {
        cJSON_AddBoolToObject(ack, "reboot_required", true);
    }
    ble_protocol_send_json_line(ack);
}

static void ble_protocol_handle_legacy_unpair(const cJSON *req)
{
    (void)req;
    int rc = ble_store_clear();
    if (rc != 0) {
        ESP_LOGW(TAG, "clear_bonds: rc=%d", rc);
    }
    ble_protocol_send_json_line(ble_protocol_make_legacy_ack("unpair", rc == 0));
}

static void ble_protocol_handle_v1_permission_cancel_dispatch(cJSON *root, const char *id, uint32_t line_generation)
{
    (void)id;
    (void)line_generation;
    ble_protocol_handle_v1_permission_cancel(root);
}

static void ble_protocol_handle_v1_session_status_dispatch(cJSON *root, const char *id, uint32_t line_generation)
{
    (void)id;
    (void)line_generation;
    ble_protocol_handle_v1_session_status(root);
}

static bool ble_protocol_dispatch_v1(cJSON *root, const char *op, const char *id, uint32_t line_generation)
{
    static const ble_protocol_v1_dispatch_t dispatch_table[] = {
        {
            .op = "permission.request",
            .require_request_id = true,
            .handler = ble_protocol_handle_v1_permission_request,
        },
        {
            .op = "permission.cancel",
            .require_request_id = false,
            .handler = ble_protocol_handle_v1_permission_cancel_dispatch,
        },
        {
            .op = "session.status",
            .require_request_id = false,
            .handler = ble_protocol_handle_v1_session_status_dispatch,
        },
    };

    const bool has_request_id = (id != NULL && id[0] != '\0');

    for (size_t i = 0; i < sizeof(dispatch_table) / sizeof(dispatch_table[0]); i++) {
        const ble_protocol_v1_dispatch_t *entry = &dispatch_table[i];
        if (strcmp(op, entry->op) != 0) {
            continue;
        }
        if (entry->require_request_id != has_request_id) {
            if (has_request_id) {
                ble_protocol_send_v1_error(id, "bad_request");
            }
            return true;
        }
        entry->handler(root, id, line_generation);
        return true;
    }

    return false;
}

/* ---- v1 permission request handler ---- */

static void ble_protocol_handle_v1_permission_request(cJSON *root, const char *id, uint32_t line_generation)
{
    /* OpenCode permission requests arrive through daemon POST /request. The
     * device displays the compact payload and later replies with the same
     * bridge id, allowing the daemon/plugin to resume the original permission
     * flow. */
    /* Validate data.kind == "permission.request" */
    const cJSON *j_data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!cJSON_IsObject(j_data)) {
        ble_protocol_send_v1_error(id, "bad_request");
        return;
    }
    const cJSON *j_kind = cJSON_GetObjectItemCaseSensitive(j_data, "kind");
    if (!cJSON_IsString(j_kind) || j_kind->valuestring == NULL
            || strcmp(j_kind->valuestring, "permission.request") != 0) {
        ble_protocol_send_v1_error(id, "bad_request");
        return;
    }

    /* Extract payload.type, payload.title, payload.metadata */
    const cJSON *j_payload = cJSON_GetObjectItemCaseSensitive(j_data, "payload");
    if (!cJSON_IsObject(j_payload)) {
        ble_protocol_send_v1_error(id, "bad_request");
        return;
    }

    const cJSON *j_type = cJSON_GetObjectItemCaseSensitive(j_payload, "type");
    const cJSON *j_title = cJSON_GetObjectItemCaseSensitive(j_payload, "title");
    const cJSON *j_meta = cJSON_GetObjectItemCaseSensitive(j_payload, "metadata");

    if (!cJSON_IsString(j_type) || j_type->valuestring == NULL
            || !cJSON_IsString(j_title) || j_title->valuestring == NULL
            || !cJSON_IsObject(j_meta)) {
        ble_protocol_send_v1_error(id, "bad_request");
        return;
    }

    const char *perm_type = j_type->valuestring;
    const char *title     = j_title->valuestring;

    char meta_compact[BLE_PROTOCOL_METADATA_TEXT_SIZE];
    ble_protocol_format_permission_metadata(j_meta, meta_compact, sizeof(meta_compact));

    ble_protocol_permission_outcome_ensure();
    if (s_permission_outcome_queue == NULL) {
        ble_protocol_send_v1_error(id, "internal_error");
        return;
    }

    /* Flush stale outcomes from a previous prompt before arming a new one. */
    char stale[BLE_PROTOCOL_PERMISSION_OUTCOME_SIZE];
    while (xQueueReceive(s_permission_outcome_queue, &stale, 0) == pdTRUE) {
    }

    if (line_generation != s_connection_generation) {
        /* The connection changed while parsing/allocating. Silently drop the
         * stale request; the daemon will time out or retry on the current link. */
        return;
    }

    if (strlen(id) >= sizeof(s_pending_request_id)) {
        ble_protocol_send_v1_error(id, "bad_request");
        return;
    }

    ble_protocol_permission_lock();
    if (line_generation != s_connection_generation) {
        ble_protocol_permission_unlock();
        return;
    }
    if (s_permission_pending) {
        /* The board UI is intentionally single-flight. Reject overlapping
         * permission prompts so the host can queue or retry them explicitly. */
        ble_protocol_permission_unlock();
        ble_protocol_send_v1_error(id, "busy");
        return;
    }
    (void)strncpy(s_pending_request_id, id, sizeof(s_pending_request_id) - 1);
    s_pending_request_id[sizeof(s_pending_request_id) - 1] = '\0';
    s_pending_generation = line_generation;
    s_permission_pending = true;

    if (xTaskCreate(ble_protocol_permission_wait_task, "perm_wait", BLE_PROTOCOL_PERMISSION_TASK_STACK,
                    (void *)(uintptr_t)line_generation, BLE_PROTOCOL_PERMISSION_TASK_PRIORITY,
                    &s_permission_wait_task) != pdPASS) {
        ble_protocol_pending_request_clear_locked();
        s_permission_wait_task = NULL;
        ble_protocol_permission_unlock();
        ble_protocol_send_v1_error(id, "internal_error");
        return;
    }

    emote_demo_show_permission(perm_type, title, meta_compact);
    ble_protocol_permission_unlock();
}

/* ---- v1 permission.cancel handler (no reply) ---- */

static void ble_protocol_handle_v1_permission_cancel(cJSON *root)
{
    /* OpenCode can resolve a permission from another surface (for example the
     * TUI) while the wearable is still showing it. permission.cancel clears the
     * device UI without sending a late decision back to the daemon. */
    const cJSON *j_data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!cJSON_IsObject(j_data)) {
        return;
    }
    const cJSON *j_kind = cJSON_GetObjectItemCaseSensitive(j_data, "kind");
    if (!cJSON_IsString(j_kind) || j_kind->valuestring == NULL
            || strcmp(j_kind->valuestring, "permission.cancel") != 0) {
        return;
    }
    ble_protocol_permission_cancel_pending();
}

/* ---- v1 session.status handler (no reply) ---- */

static void ble_protocol_handle_v1_session_status(cJSON *root)
{
    /* Session status is best-effort telemetry. It updates the expression/tip
     * state but does not produce an ACK; this keeps busy/idle notifications
     * cheap and avoids blocking OpenCode on wearable rendering. */
    const cJSON *j_data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!cJSON_IsObject(j_data)) {
        return;
    }
    const cJSON *j_kind = cJSON_GetObjectItemCaseSensitive(j_data, "kind");
    if (!cJSON_IsString(j_kind) || j_kind->valuestring == NULL
            || strcmp(j_kind->valuestring, "session.status") != 0) {
        return;
    }
    const cJSON *j_payload = cJSON_GetObjectItemCaseSensitive(j_data, "payload");
    if (!cJSON_IsObject(j_payload)) {
        return;
    }
    const cJSON *j_type = cJSON_GetObjectItemCaseSensitive(j_payload, "type");
    if (!cJSON_IsString(j_type) || j_type->valuestring == NULL) {
        return;
    }
    if (ble_protocol_permission_is_pending()) {
        /* Preserve a visible permission prompt over transient busy/retry hints.
         * Idle means the host no longer needs the prompt, so clear it first. */
        if (strcmp(j_type->valuestring, "idle") != 0) {
            return;
        }
        ble_protocol_permission_cancel_pending();
    }
    emote_demo_show_session_status(j_type->valuestring);
}

/* ---- main line handler ---- */

void ble_protocol_handle_line(const uint8_t *line, size_t len)
{
    /* Main JSONL dispatch point. Capture the connection generation before
     * parsing so long-running permission handling can detect reconnect races. */
    uint32_t line_generation = s_connection_generation;

    ESP_LOGI(TAG, "ble_protocol_handle_line: %.*s", (int)len, (const char *)line);

    cJSON *root = cJSON_ParseWithLength((const char *)line, len);
    if (root == NULL) {
        ESP_LOGW(TAG, "bad JSON");
        ble_protocol_send_v1_error("", "bad_json");
        return;
    }

    /* Try v1 envelope format: { "v": 1, "id": "...", "op": "...", "data": {...} }.
     * Non-empty id means request/response; empty id means notification. */
    const cJSON *j_v  = cJSON_GetObjectItemCaseSensitive(root, "v");
    const cJSON *j_id = cJSON_GetObjectItemCaseSensitive(root, "id");
    const cJSON *j_op = cJSON_GetObjectItemCaseSensitive(root, "op");

    if (cJSON_IsNumber(j_v) && j_v->valueint == 1
            && cJSON_IsString(j_id) && j_id->valuestring != NULL
            && cJSON_IsString(j_op) && j_op->valuestring != NULL) {

        const char *id = j_id->valuestring;
        const char *op = j_op->valuestring;

        if (!ble_protocol_dispatch_v1(root, op, id, line_generation)) {
            if (strlen(id) > 0) {
                ble_protocol_send_v1_error(id, "unknown_op");
            }
            /* Empty id with unknown op: fire-and-forget; silently ignore. */
        }

        cJSON_Delete(root);
        return;
    }

    if (cJSON_IsString(j_id) && j_id->valuestring != NULL && strlen(j_id->valuestring) > 0) {
        ble_protocol_send_v1_error(j_id->valuestring, "bad_request");
        cJSON_Delete(root);
        return;
    }

    /* Not a v1 request - try legacy commands: status/name/unpair. These maintenance
     * commands remain useful for console testing and should not interfere with
     * the OpenCode envelope protocol. */
    const cJSON *cmd = cJSON_GetObjectItemCaseSensitive(root, "cmd");
    if (cJSON_IsString(cmd) && cmd->valuestring) {
        const char *c = cmd->valuestring;
        if (strcmp(c, "status") == 0) {
            ble_protocol_handle_legacy_status(root);
        } else if (strcmp(c, "name")   == 0) {
            ble_protocol_handle_legacy_name(root);
        } else if (strcmp(c, "unpair") == 0) {
            ble_protocol_handle_legacy_unpair(root);
        } else {
            cJSON *ack = ble_protocol_make_legacy_ack(c, false);
            if (ack) {
                cJSON_AddStringToObject(ack, "error", "unknown");
            }
            ble_protocol_send_json_line(ack);
        }
    } else {
        ESP_LOGW(TAG, "unrecognized message format");
    }

    cJSON_Delete(root);
}

void ble_protocol_init(void)
{
    /* Create protocol resources before BLE opens. The RX task survives across
     * BLE reconnects; reconnect cleanup resets its queue and pending prompt
     * state rather than rebuilding the whole command subsystem. */
    ble_protocol_permission_mutex_ensure();
    ble_protocol_permission_outcome_ensure();

    if (s_rx_queue == NULL) {
        s_rx_queue = xQueueCreate(BLE_PROTOCOL_RX_QUEUE_LEN, sizeof(ble_protocol_rx_chunk_t));
        if (s_rx_queue == NULL) {
            ESP_LOGE(TAG, "protocol rx queue create failed");
            return;
        }
    }
    if (s_rx_task == NULL) {
        BaseType_t ok = xTaskCreate(ble_protocol_rx_task, "ble_proto_rx", BLE_PROTOCOL_RX_TASK_STACK, NULL,
                                    BLE_PROTOCOL_RX_TASK_PRIORITY, &s_rx_task);
        if (ok != pdPASS) {
            ESP_LOGE(TAG, "protocol rx task create failed");
            s_rx_task = NULL;
            vQueueDelete(s_rx_queue);
            s_rx_queue = NULL;
        }
    }
}
