/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_mcp_engine.h"
#include "esp_xiaozhi_mcp.h"

#define ADAPTER_META "\"_meta\":{\"io.modelcontextprotocol/protocolVersion\":\"2026-07-28\",\"io.modelcontextprotocol/clientCapabilities\":{}}"

typedef struct {
    SemaphoreHandle_t entered;
    SemaphoreHandle_t release;
    SemaphoreHandle_t response;
    unsigned calls;
    unsigned replies;
    bool reconnected;
    bool context_valid;
    bool predialogue_reply;
    const char *route;
    const char *expected_session;
    bool session_copied;
    unsigned errors;
} adapter_test_t;

static esp_err_t adapter_prepare(void *ctx, const char *route, const char *payload)
{
    (void)payload;
    adapter_test_t *state = ctx;
    if (state->route) {
        return !strcmp(state->route, route) ? ESP_OK : ESP_ERR_INVALID_STATE;
    }
    const char *current = __atomic_load_n(&state->reconnected, __ATOMIC_ACQUIRE) ? "connection-b" : "connection-a";
    return !strcmp(current, route) ? ESP_OK : ESP_ERR_INVALID_STATE;
}

static esp_err_t adapter_reply(void *ctx, const char *route, const char *session, const char *payload)
{
    adapter_test_t *state = ctx;
    if (adapter_prepare(ctx, route, payload) != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }
    state->replies++;
    state->predialogue_reply = !session || !session[0];
    if (state->expected_session && (!session || strcmp(state->expected_session, session))) {
        state->session_copied = false;
    }
    if (strstr(payload, "\"error\"")) {
        state->errors++;
    }
    xSemaphoreGive(state->response);
    return ESP_OK;
}

static esp_err_t adapter_tool(const esp_mcp_property_list_t *properties,
                              const esp_mcp_request_context_t *request,
                              esp_mcp_tool_result_t *result, void *ctx)
{
    (void)properties;
    adapter_test_t *state = ctx;
    state->calls++;
    state->context_valid = request && request->meta_json && request->protocol_version &&
                           !strcmp(request->protocol_version, "2026-07-28") && request->is_authenticated;
    if (state->calls == 1) {
        xSemaphoreGive(state->entered);
        xSemaphoreTake(state->release, pdMS_TO_TICKS(2000));
    }
    return esp_mcp_tool_result_add_text(result, "done");
}

TEST_CASE("MCP adapter discards stale queued work and supports pre-dialogue modern requests", "[xiaozhi_protocol]")
{
    adapter_test_t state = {0};
    state.entered = xSemaphoreCreateBinary();
    state.release = xSemaphoreCreateBinary();
    state.response = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(state.entered);
    TEST_ASSERT_NOT_NULL(state.release);
    TEST_ASSERT_NOT_NULL(state.response);
    esp_mcp_t *engine;
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_create(&engine));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_set_xiaozhi_compatibility(engine, true));
    esp_mcp_tool_t *tool = esp_mcp_tool_create_with_context("test", "test", adapter_tool, &state);
    TEST_ASSERT_NOT_NULL(tool);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_add_tool(engine, tool));
    esp_xiaozhi_mcp_config_t adapter = {
        .ctx = &state, .transport_handle = 1,
        .send_message = adapter_reply, .prepare_request = adapter_prepare,
    };
    esp_mcp_mgr_config_t config = {.transport = esp_mcp_transport_xiaozhi, .config = &adapter, .instance = engine};
    esp_mcp_mgr_handle_t manager;
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_mgr_init(config, &manager));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_mgr_register_endpoint(manager, "mcp_server", NULL));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_mgr_start(manager));
    const char *call = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"test\"," ADAPTER_META "}}";
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_mcp_handle_message(manager, "mcp_server", call, strlen(call), NULL, "connection-a", 0));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(state.entered, pdMS_TO_TICKS(1000)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_mcp_handle_message(manager, "mcp_server", call, strlen(call), "old-dialogue", "connection-a", 0));
    __atomic_store_n(&state.reconnected, true, __ATOMIC_RELEASE);
    xSemaphoreGive(state.release);
    TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_mcp_handle_message(manager, "mcp_server", call, strlen(call), NULL, "connection-b", 0));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(state.response, pdMS_TO_TICKS(1000)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_mgr_stop(manager));
    TEST_ASSERT_EQUAL(2, state.calls);
    TEST_ASSERT_EQUAL(1, state.replies);
    TEST_ASSERT_TRUE(state.context_valid);
    TEST_ASSERT_TRUE(state.predialogue_reply);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_mgr_deinit(manager));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_destroy(engine));
    vSemaphoreDelete(state.entered);
    vSemaphoreDelete(state.release);
    vSemaphoreDelete(state.response);
}

TEST_CASE("MCP adapter releases idle stack and preserves queued strings and stateless requests", "[xiaozhi_protocol]")
{
    char expected_session[256];
    memset(expected_session, 's', sizeof(expected_session) - 1);
    expected_session[sizeof(expected_session) - 1] = '\0';
    char expected_route[32];
    memset(expected_route, 'r', sizeof(expected_route) - 1);
    expected_route[sizeof(expected_route) - 1] = '\0';
    adapter_test_t state = {
        .route = expected_route, .expected_session = expected_session, .session_copied = true,
        .entered = xSemaphoreCreateBinary(), .release = xSemaphoreCreateBinary(),
        .response = xSemaphoreCreateCounting(5, 0),
    };
    TEST_ASSERT_NOT_NULL(state.entered);
    TEST_ASSERT_NOT_NULL(state.release);
    TEST_ASSERT_NOT_NULL(state.response);
    esp_mcp_t *engine;
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_create(&engine));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_set_xiaozhi_compatibility(engine, true));
    esp_mcp_tool_t *tool = esp_mcp_tool_create_with_context("test", "test", adapter_tool, &state);
    TEST_ASSERT_NOT_NULL(tool);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_add_tool(engine, tool));
    esp_xiaozhi_mcp_config_t adapter = {
        .ctx = &state, .transport_handle = 1,
        .send_message = adapter_reply, .prepare_request = adapter_prepare,
    };
    esp_mcp_mgr_config_t config = {.transport = esp_mcp_transport_xiaozhi, .config = &adapter, .instance = engine};
    esp_mcp_mgr_handle_t manager;
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_mgr_init(config, &manager));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_mgr_register_endpoint(manager, "mcp_server", NULL));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_mgr_start(manager));
    TEST_ASSERT_NULL(xTaskGetHandle("xiaozhi_mcp"));
    char call[256];
    char session[sizeof(expected_session)];
    char route[sizeof(expected_route)];
    for (unsigned id = 1; id <= 6; id++) {
        snprintf(call, sizeof(call), "{\"jsonrpc\":\"2.0\",\"id\":%u,\"method\":\"tools/call\",\"params\":{\"name\":\"test\"," ADAPTER_META "}}", id);
        strcpy(session, expected_session);
        strcpy(route, expected_route);
        TEST_ASSERT_EQUAL(id == 6 ? ESP_ERR_NO_MEM : ESP_OK,
                          esp_xiaozhi_mcp_handle_message(manager, "mcp_server", call, strlen(call), session, route, 0));
        memset(session, 'x', sizeof(session) - 1);
        memset(route, 'x', sizeof(route) - 1);
        memset(call, 'x', sizeof(call));
        if (id == 1) {
            TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(state.entered, pdMS_TO_TICKS(1000)));
            TEST_ASSERT_NOT_NULL(xTaskGetHandle("xiaozhi_mcp"));
        }
    }
    xSemaphoreGive(state.release);
    for (unsigned i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(state.response, pdMS_TO_TICKS(1000)));
    }
    TEST_ASSERT_EQUAL(5, state.calls);
    TEST_ASSERT_EQUAL(0, state.errors);
    TEST_ASSERT_TRUE(state.session_copied);
    vTaskDelay(pdMS_TO_TICKS(CONFIG_XIAOZHI_MCP_IDLE_TIMEOUT_MS + 100));
    TEST_ASSERT_NULL(xTaskGetHandle("xiaozhi_mcp"));
    /* Xiaozhi permits ID reuse; idle retirement must retain that behavior. */
    for (unsigned id = 5; id <= 6; id++) {
        snprintf(call, sizeof(call), "{\"jsonrpc\":\"2.0\",\"id\":%u,\"method\":\"tools/call\",\"params\":{\"name\":\"test\"," ADAPTER_META "}}", id);
        TEST_ASSERT_EQUAL(ESP_OK, esp_xiaozhi_mcp_handle_message(manager, "mcp_server", call, strlen(call), expected_session, expected_route, 0));
        TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(state.response, pdMS_TO_TICKS(1000)));
    }
    TEST_ASSERT_EQUAL(7, state.calls);
    TEST_ASSERT_EQUAL(0, state.errors);
    TEST_ASSERT_TRUE(state.session_copied);
    vTaskDelay(pdMS_TO_TICKS(CONFIG_XIAOZHI_MCP_IDLE_TIMEOUT_MS + 100));
    TEST_ASSERT_NULL(xTaskGetHandle("xiaozhi_mcp"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_mgr_stop(manager));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_mgr_deinit(manager));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_destroy(engine));
    vSemaphoreDelete(state.entered);
    vSemaphoreDelete(state.release);
    vSemaphoreDelete(state.response);
}
