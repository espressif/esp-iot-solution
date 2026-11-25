/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "esp_heap_caps.h"
#include "esp_mcp_mgr.h"
#include "esp_xiaozhi_mcp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#define MCP_WORKER_EXITED BIT0
#define MCP_QUEUE_LENGTH 4

typedef struct {
    uint32_t dialogue_generation;
    const char *session;
    const char *routing_session;
    char payload[];
} esp_xiaozhi_mcp_request_t;

typedef struct {
    esp_mcp_mgr_handle_t manager;
    esp_xiaozhi_mcp_config_t *config;
    QueueHandle_t queue;
    EventGroupHandle_t events;
    SemaphoreHandle_t mutex;
    TaskHandle_t task;
    bool stopping;
    /* Preserve connection state when only the idle worker stack is released. */
    char last_routing_session[32];
    uint32_t last_dialogue_generation;
} esp_xiaozhi_mcp_item_t;

/* esp_xiaozhi currently exposes one chat instance. */
static esp_xiaozhi_mcp_item_t *s_xiaozhi_mcp;

static void esp_xiaozhi_mcp_worker(void *arg)
{
    esp_xiaozhi_mcp_item_t *item = arg;
    esp_xiaozhi_mcp_request_t *request;
    for (;;) {
        if (xQueueReceive(item->queue, &request, pdMS_TO_TICKS(CONFIG_XIAOZHI_MCP_IDLE_TIMEOUT_MS)) != pdTRUE) {
            xSemaphoreTake(item->mutex, portMAX_DELAY);
            if (uxQueueMessagesWaiting(item->queue)) {
                xSemaphoreGive(item->mutex);
                continue;
            }
            /* Serialize retirement with enqueue/task creation and stop. */
            break;
        }
        if (!request) {
            xSemaphoreTake(item->mutex, portMAX_DELAY);
            break;
        }
        if (!__atomic_load_n(&item->stopping, __ATOMIC_ACQUIRE) &&
                (!item->config->prepare_request ||
                 item->config->prepare_request(item->config->ctx, request->routing_session, request->payload) == ESP_OK)) {
            if (strcmp(item->last_routing_session, request->routing_session)) {
                if (item->last_routing_session[0]) {
                    (void)esp_mcp_mgr_clear_session_state(item->manager, item->last_routing_session);
                }
                strcpy(item->last_routing_session, request->routing_session);
                item->last_dialogue_generation = request->dialogue_generation;
            } else if (item->last_dialogue_generation != request->dialogue_generation) {
                (void)esp_mcp_mgr_clear_recent_request_ids(item->manager, request->routing_session);
                item->last_dialogue_generation = request->dialogue_generation;
            }
            uint8_t *response = NULL;
            uint16_t length = 0;
            (void)esp_mcp_mgr_set_request_context(item->manager,
                                                  request->routing_session[0] ? request->routing_session : NULL,
                                                  NULL, true);
            (void)esp_mcp_mgr_req_handle(item->manager, "mcp_server", (const uint8_t *)request->payload,
                                         strlen(request->payload), &response, &length);
            (void)esp_mcp_mgr_set_request_context(item->manager, NULL, NULL, false);
            if (response) {
                if (!__atomic_load_n(&item->stopping, __ATOMIC_ACQUIRE) && item->config->send_message) {
                    item->config->send_message(item->config->ctx, request->routing_session,
                                               request->session, (const char *)response);
                }
                esp_mcp_mgr_req_destroy_response(item->manager, response);
            }
        }
        free(request);
        if (__atomic_load_n(&item->stopping, __ATOMIC_ACQUIRE)) {
            xSemaphoreTake(item->mutex, portMAX_DELAY);
            break;
        }
    }
    /* The lifecycle mutex is held; no producer can enqueue or start a worker. */
    while (xQueueReceive(item->queue, &request, 0) == pdTRUE) {
        free(request);
    }
    item->task = NULL;
    xEventGroupSetBits(item->events, MCP_WORKER_EXITED);
    xSemaphoreGive(item->mutex);
    /* Do not access item after unlocking: stop/deinit or a new worker may run. */
#if CONFIG_XIAOZHI_MCP_STACK_IN_PSRAM
    vTaskDeleteWithCaps(NULL);
#else
    vTaskDelete(NULL);
#endif
}

esp_err_t esp_xiaozhi_mcp_handle_message(esp_mcp_mgr_handle_t manager, const char *endpoint,
                                         const char *payload, size_t length, const char *session,
                                         const char *routing_session, uint32_t dialogue_generation)
{
    esp_xiaozhi_mcp_item_t *item = s_xiaozhi_mcp;
    if (!item || item->manager != manager || !endpoint || strcmp(endpoint, "mcp_server") ||
            !payload || !length || length > UINT16_MAX ||
            (session && strlen(session) >= 256) || (routing_session && strlen(routing_session) >= 32)) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t session_size = session ? strlen(session) + 1 : 1;
    size_t route_size = routing_session ? strlen(routing_session) + 1 : 1;
    esp_xiaozhi_mcp_request_t *request = malloc(sizeof(*request) + length + 1 + session_size + route_size);
    if (!request) {
        return ESP_ERR_NO_MEM;
    }
    request->dialogue_generation = dialogue_generation;
    memcpy(request->payload, payload, length);
    request->payload[length] = '\0';
    char *strings = request->payload + length + 1;
    memcpy(strings, session ? session : "", session_size);
    request->session = strings;
    strings += session_size;
    memcpy(strings, routing_session ? routing_session : "", route_size);
    request->routing_session = strings;
    xSemaphoreTake(item->mutex, portMAX_DELAY);
    if (!item->queue || __atomic_load_n(&item->stopping, __ATOMIC_ACQUIRE)) {
        xSemaphoreGive(item->mutex);
        free(request);
        return ESP_ERR_INVALID_STATE;
    }
    if (xQueueSend(item->queue, &request, 0) != pdTRUE) {
        xSemaphoreGive(item->mutex);
        free(request);
        return ESP_ERR_NO_MEM;
    }
    if (!item->task) {
        xEventGroupClearBits(item->events, MCP_WORKER_EXITED);
#if CONFIG_XIAOZHI_MCP_STACK_IN_PSRAM
        BaseType_t created = xTaskCreatePinnedToCoreWithCaps(esp_xiaozhi_mcp_worker, "xiaozhi_mcp",
                                                             CONFIG_XIAOZHI_MCP_WORKER_STACK_SIZE,
                                                             item, 4, &item->task, tskNO_AFFINITY,
                                                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (created != pdPASS) {
            created = xTaskCreatePinnedToCoreWithCaps(esp_xiaozhi_mcp_worker, "xiaozhi_mcp",
                                                      CONFIG_XIAOZHI_MCP_WORKER_STACK_SIZE,
                                                      item, 4, &item->task, tskNO_AFFINITY,
                                                      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        }
#else
        BaseType_t created = xTaskCreate(esp_xiaozhi_mcp_worker, "xiaozhi_mcp",
                                         CONFIG_XIAOZHI_MCP_WORKER_STACK_SIZE,
                                         item, 4, &item->task);
#endif
        if (created != pdPASS) {
            item->task = NULL;
            (void)xQueueReceive(item->queue, &request, 0);
            xEventGroupSetBits(item->events, MCP_WORKER_EXITED);
            xSemaphoreGive(item->mutex);
            free(request);
            return ESP_ERR_NO_MEM;
        }
    }
    xSemaphoreGive(item->mutex);
    return ESP_OK;
}

static void esp_xiaozhi_mcp_clear_connection(esp_xiaozhi_mcp_item_t *item)
{
    if (item->last_routing_session[0]) {
        (void)esp_mcp_mgr_clear_session_state(item->manager, item->last_routing_session);
        item->last_routing_session[0] = '\0';
        item->last_dialogue_generation = 0;
    }
}

static esp_err_t esp_xiaozhi_mcp_init(esp_mcp_mgr_handle_t manager, esp_mcp_transport_handle_t *handle)
{
    if (!handle || s_xiaozhi_mcp) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_xiaozhi_mcp_item_t *item = calloc(1, sizeof(*item));
    if (!item) {
        return ESP_ERR_NO_MEM;
    }
    item->manager = manager;
    item->events = xEventGroupCreate();
    if (!item->events) {
        free(item);
        return ESP_ERR_NO_MEM;
    }
    item->mutex = xSemaphoreCreateMutex();
    if (!item->mutex) {
        vEventGroupDelete(item->events);
        free(item);
        return ESP_ERR_NO_MEM;
    }
    item->stopping = true;
    xEventGroupSetBits(item->events, MCP_WORKER_EXITED);
    s_xiaozhi_mcp = item;
    *handle = (esp_mcp_transport_handle_t)item;
    return ESP_OK;
}

static esp_err_t esp_xiaozhi_mcp_stop(esp_mcp_transport_handle_t handle)
{
    esp_xiaozhi_mcp_item_t *item = (esp_xiaozhi_mcp_item_t *)handle;
    if (!item) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(item->mutex, portMAX_DELAY);
    if (xTaskGetCurrentTaskHandle() == item->task) {
        xSemaphoreGive(item->mutex);
        return ESP_ERR_INVALID_STATE;
    }
    __atomic_store_n(&item->stopping, true, __ATOMIC_RELEASE);
    bool active = item->task != NULL;
    if (active) {
        esp_xiaozhi_mcp_request_t *stop = NULL;
        (void)xQueueSend(item->queue, &stop, 0);
    }
    xSemaphoreGive(item->mutex);
    if (active) {
        EventBits_t bits = xEventGroupWaitBits(item->events, MCP_WORKER_EXITED, pdFALSE, pdFALSE, pdMS_TO_TICKS(5000));
        if (!(bits & MCP_WORKER_EXITED)) {
            return ESP_ERR_TIMEOUT;
        }
    }
    xSemaphoreTake(item->mutex, portMAX_DELAY);
    esp_xiaozhi_mcp_clear_connection(item);
    xSemaphoreGive(item->mutex);
    return ESP_OK;
}

static esp_err_t esp_xiaozhi_mcp_deinit(esp_mcp_transport_handle_t handle)
{
    esp_xiaozhi_mcp_item_t *item = (esp_xiaozhi_mcp_item_t *)handle;
    esp_err_t ret = esp_xiaozhi_mcp_stop(handle);
    if (ret != ESP_OK) {
        return ret;
    }
    if (item->queue) {
        vQueueDelete(item->queue);
    }
    vEventGroupDelete(item->events);
    vSemaphoreDelete(item->mutex);
    s_xiaozhi_mcp = NULL;
    free(item);
    return ESP_OK;
}

static esp_err_t esp_xiaozhi_mcp_create_config(const void *config, void **output)
{
    if (!config || !output) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_xiaozhi_mcp_config_t *copy = malloc(sizeof(*copy));
    if (!copy) {
        return ESP_ERR_NO_MEM;
    }
    *copy = *(const esp_xiaozhi_mcp_config_t *)config;
    *output = copy;
    return ESP_OK;
}

static esp_err_t esp_xiaozhi_mcp_delete_config(void *config)
{
    free(config);
    return ESP_OK;
}

static esp_err_t esp_xiaozhi_mcp_start(esp_mcp_transport_handle_t handle, void *config)
{
    esp_xiaozhi_mcp_item_t *item = (esp_xiaozhi_mcp_item_t *)handle;
    if (!item || !config || !((esp_xiaozhi_mcp_config_t *)config)->transport_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    item->config = config;
    if (!item->queue) {
        item->queue = xQueueCreate(MCP_QUEUE_LENGTH, sizeof(esp_xiaozhi_mcp_request_t *));
    }
    if (!item->queue) {
        return ESP_ERR_NO_MEM;
    }
    __atomic_store_n(&item->stopping, false, __ATOMIC_RELEASE);
    return ESP_OK;
}

static esp_err_t esp_xiaozhi_mcp_endpoint(esp_mcp_transport_handle_t handle, const char *name, void *data)
{
    (void)data;
    return handle && name ? ESP_OK : ESP_ERR_INVALID_ARG;
}

static esp_err_t esp_xiaozhi_mcp_remove_endpoint(esp_mcp_transport_handle_t handle, const char *name)
{
    return esp_xiaozhi_mcp_endpoint(handle, name, NULL);
}

static esp_err_t esp_xiaozhi_mcp_emit(esp_mcp_transport_handle_t handle, const char *session, const char *json)
{
    esp_xiaozhi_mcp_item_t *item = (esp_xiaozhi_mcp_item_t *)handle;
    if (!item || __atomic_load_n(&item->stopping, __ATOMIC_ACQUIRE) || !item->config || !item->config->send_message) {
        return ESP_ERR_INVALID_STATE;
    }
    return item->config->send_message(item->config->ctx, session, NULL, json);
}

const esp_mcp_transport_t esp_mcp_transport_xiaozhi = {
    .init = esp_xiaozhi_mcp_init,
    .deinit = esp_xiaozhi_mcp_deinit,
    .create_config = esp_xiaozhi_mcp_create_config,
    .delete_config = esp_xiaozhi_mcp_delete_config,
    .start = esp_xiaozhi_mcp_start,
    .stop = esp_xiaozhi_mcp_stop,
    .register_endpoint = esp_xiaozhi_mcp_endpoint,
    .unregister_endpoint = esp_xiaozhi_mcp_remove_endpoint,
    .emit_message = esp_xiaozhi_mcp_emit,
};
