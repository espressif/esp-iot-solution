/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * JSONL dispatcher for Signal Light Control Protocol over ESP-BLE-UART.
 */

#include "ble_jsonl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "indicator.h"
#include "ble_uart.h"

static const char *TAG = "ble_jsonl";

#define BLE_JSONL_PROTOCOL_VERSION  1
#define BLE_JSONL_RX_LINE_MAX       2048
#define BLE_JSONL_TX_LINE_MAX       2048
#define BLE_JSONL_RX_QUEUE_DEPTH    8
#define BLE_JSONL_RX_CHUNK_MAX      512
#define BLE_JSONL_MAX_PAYLOAD_ITEMS 32
#define BLE_JSONL_WORKER_STACK      6144
#define BLE_JSONL_WORKER_PRIO       5

typedef struct {
    uint16_t len;
    uint8_t  data[BLE_JSONL_RX_CHUNK_MAX];
} ble_jsonl_rx_msg_t;

static uint8_t      s_rx_line[BLE_JSONL_RX_LINE_MAX];
static size_t       s_rx_line_len;
static bool         s_rx_overflow;
static QueueHandle_t s_rx_queue;
static TaskHandle_t s_worker_task;
static bool         s_worker_started;

static bool send_json_line(cJSON *root);
static void rx_feed(const uint8_t *data, size_t len);
static void handle_line(const uint8_t *line, size_t len);

static bool send_json_line(cJSON *root)
{
    if (root == NULL) {
        return false;
    }
    char *txt = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (txt == NULL) {
        ESP_LOGW(TAG, "tx json print failed");
        return false;
    }

    size_t len = strlen(txt);
    if (len + 1 > BLE_JSONL_TX_LINE_MAX) {
        ESP_LOGW(TAG, "tx line too long (%u bytes)", (unsigned)len);
        cJSON_free(txt);
        return false;
    }

    char *line = realloc(txt, len + 2);
    if (line == NULL) {
        ESP_LOGW(TAG, "tx line alloc failed");
        cJSON_free(txt);
        return false;
    }
    line[len] = '\n';
    line[len + 1] = '\0';

    int rc = ble_uart_tx((const uint8_t *)line, len + 1);
    cJSON_free(line);
    if (rc != BLE_UART_OK) {
        ESP_LOGW(TAG, "ble_uart_tx failed (%d)", rc);
        return false;
    }
    return true;
}

static void send_v1_error(const char *id, const char *error)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGW(TAG, "tx error response alloc failed");
        return;
    }
    cJSON_AddNumberToObject(root, "v", BLE_JSONL_PROTOCOL_VERSION);
    cJSON_AddStringToObject(root, "id", id ? id : "");
    cJSON_AddBoolToObject(root, "ok", false);
    cJSON_AddStringToObject(root, "error", error ? error : "unknown");
    (void)send_json_line(root);
}

static void send_v1_app_error(const char *id, const char *error, const char *message)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGW(TAG, "tx app error response alloc failed");
        return;
    }
    cJSON_AddNumberToObject(root, "v", BLE_JSONL_PROTOCOL_VERSION);
    cJSON_AddStringToObject(root, "id", id ? id : "");
    cJSON_AddBoolToObject(root, "ok", false);
    cJSON_AddStringToObject(root, "error", error ? error : "unknown");
    if (message != NULL && message[0] != '\0') {
        cJSON *data = cJSON_AddObjectToObject(root, "data");
        if (data != NULL) {
            cJSON_AddStringToObject(data, "message", message);
        }
    }
    (void)send_json_line(root);
}

static void send_v1_ok(const char *id, cJSON *data)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        if (data != NULL) {
            cJSON_Delete(data);
        }
        ESP_LOGW(TAG, "tx ok response alloc failed");
        return;
    }
    cJSON_AddNumberToObject(root, "v", BLE_JSONL_PROTOCOL_VERSION);
    cJSON_AddStringToObject(root, "id", id ? id : "");
    cJSON_AddBoolToObject(root, "ok", true);
    if (data != NULL) {
        if (!cJSON_AddItemToObject(root, "data", data)) {
            cJSON_Delete(data);
        }
    } else {
        cJSON_AddObjectToObject(root, "data");
    }
    (void)send_json_line(root);
}

static bool request_id_nonempty(const char *id)
{
    return id != NULL && id[0] != '\0';
}

static const char *extract_request_id(const cJSON *root)
{
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "id");
    if (!cJSON_IsString(id) || id->valuestring == NULL) {
        return "";
    }
    return id->valuestring;
}

static void log_parsed_json(const cJSON *root)
{
    char *txt = cJSON_PrintUnformatted(root);
    if (txt == NULL) {
        ESP_LOGW(TAG, "failed to serialize parsed JSON for logging");
        return;
    }
    ESP_LOGI(TAG, "rx: %s", txt);
    cJSON_free(txt);
}

static const char *json_get_cmd_string(const cJSON *data)
{
    const cJSON *cmd = cJSON_GetObjectItemCaseSensitive(data, "cmd");
    if (!cJSON_IsString(cmd) || cmd->valuestring == NULL || cmd->valuestring[0] == '\0') {
        return NULL;
    }
    return cmd->valuestring;
}

static void handle_query_indicator_count(const char *id, const cJSON *data)
{
    const char *cmd = json_get_cmd_string(data);
    if (cmd == NULL || strcmp(cmd, "query") != 0) {
        send_v1_app_error(id, "unsupported_command", "unknown cmd");
        return;
    }

    const cJSON *type = cJSON_GetObjectItemCaseSensitive(data, "type");
    if (!cJSON_IsString(type) || type->valuestring == NULL ||
            strcmp(type->valuestring, "indicator_count") != 0) {
        send_v1_app_error(id, "unsupported_command", "unknown query type");
        return;
    }

    cJSON *resp = cJSON_CreateObject();
    if (resp == NULL) {
        send_v1_error(id, "bad_request");
        return;
    }
    cJSON_AddNumberToObject(resp, "count", indicator_get_channel_count());
    send_v1_ok(id, resp);
}

static const char *parse_control_item(const cJSON *item, int *indicator_id,
                                      int *light_id, int *light_action)
{
    if (!cJSON_IsObject(item)) {
        return "payload item must be an object";
    }

    const cJSON *j_indicator = cJSON_GetObjectItemCaseSensitive(item, "indicator_id");
    const cJSON *j_light = cJSON_GetObjectItemCaseSensitive(item, "light_id");
    const cJSON *j_action = cJSON_GetObjectItemCaseSensitive(item, "light_action");

    if (!cJSON_IsNumber(j_indicator) || !cJSON_IsNumber(j_light) || !cJSON_IsNumber(j_action)) {
        return "payload item missing indicator_id, light_id, or light_action";
    }

    *indicator_id = (int)j_indicator->valuedouble;
    *light_id = (int)j_light->valuedouble;
    *light_action = (int)j_action->valuedouble;
    return indicator_validate_light(*indicator_id, *light_id, *light_action);
}

static void handle_control(const char *id, const cJSON *data)
{
    const char *cmd = json_get_cmd_string(data);
    if (cmd == NULL || strcmp(cmd, "control") != 0) {
        send_v1_app_error(id, "unsupported_command", "unknown cmd");
        return;
    }

    if (indicator_get_channel_count() == 0) {
        send_v1_app_error(id, "invalid_parameter", "indicator_count is 0");
        return;
    }

    const cJSON *payload = cJSON_GetObjectItemCaseSensitive(data, "payload");
    if (!cJSON_IsArray(payload) || cJSON_GetArraySize(payload) == 0) {
        send_v1_app_error(id, "invalid_parameter", "payload must be a non-empty array");
        return;
    }

    const int count = cJSON_GetArraySize(payload);
    if (count > BLE_JSONL_MAX_PAYLOAD_ITEMS) {
        send_v1_app_error(id, "invalid_parameter", "payload too many items");
        return;
    }

    for (int i = 0; i < count; i++) {
        const cJSON *item = cJSON_GetArrayItem(payload, i);
        int indicator_id = 0;
        int light_id = 0;
        int light_action = 0;
        const char *err = parse_control_item(item, &indicator_id, &light_id, &light_action);
        if (err != NULL) {
            send_v1_app_error(id, "invalid_parameter", err);
            return;
        }
    }

    cJSON *echo = cJSON_Duplicate(payload, true);
    if (echo == NULL) {
        send_v1_error(id, "bad_request");
        return;
    }

    cJSON *resp = cJSON_CreateObject();
    if (resp == NULL) {
        cJSON_Delete(echo);
        send_v1_error(id, "bad_request");
        return;
    }
    if (!cJSON_AddItemToObject(resp, "payload", echo)) {
        cJSON_Delete(echo);
        cJSON_Delete(resp);
        send_v1_error(id, "bad_request");
        return;
    }

    for (int i = 0; i < count; i++) {
        const cJSON *item = cJSON_GetArrayItem(payload, i);
        const cJSON *j_indicator = cJSON_GetObjectItemCaseSensitive(item, "indicator_id");
        const cJSON *j_light = cJSON_GetObjectItemCaseSensitive(item, "light_id");
        const cJSON *j_action = cJSON_GetObjectItemCaseSensitive(item, "light_action");
        (void)indicator_set_light((int)j_indicator->valuedouble,
                                  (int)j_light->valuedouble,
                                  (int)j_action->valuedouble);
    }

    send_v1_ok(id, resp);
}

static void handle_command(const char *id, const cJSON *data)
{
    const char *cmd = json_get_cmd_string(data);
    if (cmd == NULL) {
        send_v1_app_error(id, "unsupported_command", "missing cmd");
        return;
    }

    if (strcmp(cmd, "query") == 0) {
        handle_query_indicator_count(id, data);
    } else if (strcmp(cmd, "control") == 0) {
        handle_control(id, data);
    } else {
        char message[48];
        snprintf(message, sizeof(message), "unknown cmd: %s", cmd);
        send_v1_app_error(id, "unsupported_command", message);
    }
}

static bool envelope_valid(const cJSON *root, const char **out_id, const char **out_op,
                           const cJSON **out_data)
{
    const cJSON *ver = cJSON_GetObjectItemCaseSensitive(root, "v");
    if (!cJSON_IsNumber(ver) || (int)ver->valuedouble != BLE_JSONL_PROTOCOL_VERSION) {
        return false;
    }

    const cJSON *op = cJSON_GetObjectItemCaseSensitive(root, "op");
    if (!cJSON_IsString(op) || op->valuestring == NULL || op->valuestring[0] == '\0') {
        return false;
    }

    const cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "id");
    if (!cJSON_IsString(id) || id->valuestring == NULL) {
        return false;
    }

    const cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!cJSON_IsObject(data)) {
        return false;
    }

    *out_id = id->valuestring;
    *out_op = op->valuestring;
    *out_data = data;
    return true;
}

static void handle_line(const uint8_t *line, size_t len)
{
    cJSON *root = cJSON_ParseWithLength((const char *)line, len);
    if (root == NULL) {
        ESP_LOGW(TAG, "bad JSON (%u bytes): %.*s",
                 (unsigned)len, (int)(len > 64 ? 64 : len), line);
        send_v1_error("", "bad_json");
        return;
    }

    log_parsed_json(root);

    const char *req_id = extract_request_id(root);
    const char *id = "";
    const char *op = NULL;
    const cJSON *data = NULL;

    if (!envelope_valid(root, &id, &op, &data)) {
        ESP_LOGW(TAG, "bad envelope: %.*s", (int)(len > 64 ? 64 : len), line);
        if (request_id_nonempty(req_id)) {
            send_v1_error(req_id, "bad_request");
        } else {
            send_v1_error("", "id_not_specified");
        }
        cJSON_Delete(root);
        return;
    }

    if (!request_id_nonempty(id)) {
        send_v1_error("", "id_not_specified");
        cJSON_Delete(root);
        return;
    }

    if (strcmp(op, "command") != 0) {
        ESP_LOGW(TAG, "unknown op '%s'", op);
        send_v1_error(id, "unknown_op");
        cJSON_Delete(root);
        return;
    }

    handle_command(id, data);
    cJSON_Delete(root);
}

static void on_rx_line(const uint8_t *line, size_t len)
{
    if (line == NULL || len == 0) {
        return;
    }
    handle_line(line, len);
}

static void rx_feed(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        uint8_t b = data[i];

        if (s_rx_overflow) {
            if (b != '\n') {
                continue;
            }
            s_rx_overflow = false;
            s_rx_line_len = 0;
            continue;
        }

        if (b == '\n') {
            on_rx_line(s_rx_line, s_rx_line_len);
            s_rx_line_len = 0;
            continue;
        }
        if (b == '\r') {
            continue;
        }
        if (s_rx_line_len < sizeof(s_rx_line)) {
            s_rx_line[s_rx_line_len++] = b;
        } else {
            ESP_LOGW(TAG, "rx line overflow (>%u bytes), dropping",
                     (unsigned)sizeof(s_rx_line));
            s_rx_overflow = true;
            s_rx_line_len = 0;
        }
    }
}

static void jsonl_worker_task(void *arg)
{
    (void)arg;
    ble_jsonl_rx_msg_t msg;

    for (;;) {
        if (xQueueReceive(s_rx_queue, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        rx_feed(msg.data, msg.len);
    }
}

static esp_err_t ble_jsonl_start_worker(void)
{
    if (s_worker_started) {
        return ESP_OK;
    }

    s_rx_queue = xQueueCreate(BLE_JSONL_RX_QUEUE_DEPTH, sizeof(ble_jsonl_rx_msg_t));
    if (s_rx_queue == NULL) {
        ESP_LOGE(TAG, "rx queue create failed");
        return ESP_ERR_NO_MEM;
    }

    BaseType_t ok = xTaskCreate(jsonl_worker_task, "ble_jsonl",
                                BLE_JSONL_WORKER_STACK, NULL,
                                BLE_JSONL_WORKER_PRIO, &s_worker_task);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "worker task create failed");
        vQueueDelete(s_rx_queue);
        s_rx_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_worker_started = true;
    ESP_LOGI(TAG, "worker task started");
    return ESP_OK;
}

esp_err_t ble_jsonl_init(void)
{
    s_rx_line_len = 0;
    s_rx_overflow = false;
    return ble_jsonl_start_worker();
}

void ble_jsonl_rx_feed(const uint8_t *data, size_t len)
{
    if (data == NULL || len == 0 || s_rx_queue == NULL) {
        return;
    }

    size_t offset = 0;
    while (offset < len) {
        ble_jsonl_rx_msg_t msg = { 0 };
        size_t chunk = len - offset;
        if (chunk > BLE_JSONL_RX_CHUNK_MAX) {
            chunk = BLE_JSONL_RX_CHUNK_MAX;
        }
        msg.len = (uint16_t)chunk;
        memcpy(msg.data, data + offset, chunk);

        if (xQueueSend(s_rx_queue, &msg, 0) != pdTRUE) {
            ESP_LOGW(TAG, "rx queue full, dropping %u bytes", (unsigned)(len - offset));
            return;
        }
        offset += chunk;
    }
}
