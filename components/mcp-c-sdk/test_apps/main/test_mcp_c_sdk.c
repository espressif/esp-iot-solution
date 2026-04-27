/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "unity.h"
#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_mcp_engine.h"
#include "esp_mcp_mgr.h"
#include "esp_mcp_tool.h"
#include "esp_mcp_property.h"
#include "esp_mcp_data.h"

static const char *TAG = "mcp_test";

#define TEST_MEMORY_LEAK_THRESHOLD (-3000)
#define MCP_TEST_INIT_DEFAULT_PARAMS_JSON \
    "{\"protocolVersion\":\"2025-11-25\",\"clientInfo\":{\"name\":\"test_client\",\"version\":\"1.0\"},\"capabilities\":{}}"
#define MCP_TEST_INIT_DEFAULT_REQ_JSON \
    "{\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"params\":" MCP_TEST_INIT_DEFAULT_PARAMS_JSON ",\"id\":1}"

static esp_err_t test_http_auth_callback(const esp_mcp_mgr_auth_context_t *context,
                                         char *out_bearer_token,
                                         size_t out_bearer_token_len,
                                         void *user_ctx)
{
    (void)context;
    (void)user_ctx;
    if (out_bearer_token && out_bearer_token_len > 0) {
        out_bearer_token[0] = '\0';
    }
    return ESP_ERR_NOT_SUPPORTED;
}

// Test callback functions
static esp_mcp_value_t test_tool_callback_bool(const esp_mcp_property_list_t *properties)
{
    bool value = esp_mcp_property_list_get_property_bool(properties, "enabled");
    ESP_LOGI(TAG, "test_tool_callback_bool: enabled = %d", value);
    return esp_mcp_value_create_bool(value);
}

static esp_mcp_value_t test_tool_callback_int(const esp_mcp_property_list_t *properties)
{
    int value = esp_mcp_property_list_get_property_int(properties, "count");
    ESP_LOGI(TAG, "test_tool_callback_int: count = %d", value);
    return esp_mcp_value_create_int(value * 2);
}

static esp_mcp_value_t test_tool_callback_float(const esp_mcp_property_list_t *properties)
{
    float value = esp_mcp_property_list_get_property_float(properties, "temperature");
    ESP_LOGI(TAG, "test_tool_callback_float: temperature = %.2f", value);
    return esp_mcp_value_create_float(value + 1.0f);
}

static esp_mcp_value_t test_tool_callback_string(const esp_mcp_property_list_t *properties)
{
    const char *value = esp_mcp_property_list_get_property_string(properties, "message");
    ESP_LOGI(TAG, "test_tool_callback_string: message = %s", value ? value : "NULL");
    char result[64];
    if (value) {
        snprintf(result, sizeof(result), "Echo: %s", value);
    } else {
        snprintf(result, sizeof(result), "Echo: NULL");
    }
    return esp_mcp_value_create_string(result);
}

static esp_mcp_value_t test_tool_callback_multi(const esp_mcp_property_list_t *properties)
{
    bool enabled = esp_mcp_property_list_get_property_bool(properties, "enabled");
    int count = esp_mcp_property_list_get_property_int(properties, "count");
    float temp = esp_mcp_property_list_get_property_float(properties, "temperature");
    const char *msg = esp_mcp_property_list_get_property_string(properties, "message");

    ESP_LOGI(TAG, "test_tool_callback_multi: enabled=%d, count=%d, temp=%.2f, msg=%s",
             enabled, count, temp, msg ? msg : "NULL");

    return esp_mcp_value_create_string("OK");
}

// Thread safety test data
static SemaphoreHandle_t thread_test_mutex;
static int thread_test_counter = 0;
static const int THREAD_TEST_ITERATIONS = 100;
static const int THREAD_TEST_THREADS = 4;
static bool relax_memory_check;
static char s_last_emit_message[1024];
static char s_last_emit_session_id[64];
static int s_emit_count;
static char s_emit_history[8][1024];
static char s_emit_session_history[8][64];
static char s_last_request_message[1024];
static char s_request_history[8][1024];
static int s_request_count;
static int s_open_stream_count;
static bool s_ex_cb_arr_ok;
static bool s_ex_cb_obj_ok;

typedef struct {
    esp_mcp_t *mcp;
    int thread_id;
} thread_test_params_t;

static void assert_rfc3339_utc_field(cJSON *obj, const char *field)
{
    cJSON *value = cJSON_GetObjectItem(obj, field);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_TRUE(cJSON_IsString(value));
    TEST_ASSERT_NOT_NULL(value->valuestring);

    const char *s = value->valuestring;
    TEST_ASSERT_EQUAL_UINT32(20, strlen(s));
    TEST_ASSERT_EQUAL_CHAR('-', s[4]);
    TEST_ASSERT_EQUAL_CHAR('-', s[7]);
    TEST_ASSERT_EQUAL_CHAR('T', s[10]);
    TEST_ASSERT_EQUAL_CHAR(':', s[13]);
    TEST_ASSERT_EQUAL_CHAR(':', s[16]);
    TEST_ASSERT_EQUAL_CHAR('Z', s[19]);

    const int digit_positions[] = {0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18};
    for (size_t i = 0; i < sizeof(digit_positions) / sizeof(digit_positions[0]); ++i) {
        TEST_ASSERT_TRUE(isdigit((unsigned char)s[digit_positions[i]]));
    }
}

static void assert_related_task_meta(cJSON *result_obj, const char *expected_task_id)
{
    cJSON *meta = cJSON_GetObjectItem(result_obj, "_meta");
    TEST_ASSERT_NOT_NULL(meta);
    TEST_ASSERT_TRUE(cJSON_IsObject(meta));

    cJSON *related = cJSON_GetObjectItem(meta, "io.modelcontextprotocol/related-task");
    TEST_ASSERT_NOT_NULL(related);
    TEST_ASSERT_TRUE(cJSON_IsObject(related));

    cJSON *task_id = cJSON_GetObjectItem(related, "taskId");
    TEST_ASSERT_NOT_NULL(task_id);
    TEST_ASSERT_TRUE(cJSON_IsString(task_id));
    TEST_ASSERT_EQUAL_STRING(expected_task_id, task_id->valuestring);
}

static void assert_error_code(cJSON *root, int expected_code)
{
    cJSON *error = cJSON_GetObjectItem(root, "error");
    TEST_ASSERT_NOT_NULL(error);
    cJSON *code = cJSON_GetObjectItem(error, "code");
    TEST_ASSERT_NOT_NULL(code);
    TEST_ASSERT_TRUE(cJSON_IsNumber(code));
    TEST_ASSERT_EQUAL_INT(expected_code, code->valueint);
}

static void assert_task_ttl(cJSON *obj, int expected_ttl)
{
    cJSON *ttl = cJSON_GetObjectItem(obj, "ttl");
    TEST_ASSERT_NOT_NULL(ttl);
    TEST_ASSERT_TRUE(cJSON_IsNumber(ttl));
    TEST_ASSERT_EQUAL_INT(expected_ttl, ttl->valueint);
}

static void assert_task_ttl_null(cJSON *obj)
{
    cJSON *ttl = cJSON_GetObjectItem(obj, "ttl");
    TEST_ASSERT_NOT_NULL(ttl);
    TEST_ASSERT_TRUE(cJSON_IsNull(ttl));
}

static void thread_test_task(void *pvParameters)
{
    thread_test_params_t *params = (thread_test_params_t *)pvParameters;
    esp_mcp_t *mcp = params->mcp;
    int thread_id = params->thread_id;

    for (int i = 0; i < THREAD_TEST_ITERATIONS; i++) {
        // Create and add tool
        char tool_name[32];
        snprintf(tool_name, sizeof(tool_name), "thread_tool_%d_%d", thread_id, i);

        esp_mcp_tool_t *tool = esp_mcp_tool_create(tool_name, "Thread test tool", test_tool_callback_bool);
        TEST_ASSERT_NOT_NULL(tool);

        esp_err_t ret = esp_mcp_add_tool(mcp, tool);
        if (ret == ESP_ERR_INVALID_STATE) {
            // Tool already exists (from another thread), destroy it
            esp_mcp_tool_destroy(tool);
        } else {
            TEST_ASSERT_EQUAL(ESP_OK, ret);
        }

        // Small delay to increase chance of race conditions
        vTaskDelay(1 / portTICK_PERIOD_MS);

        // Remove tool (if it was added); list remove does not destroy the tool object
        if (ret == ESP_OK) {
            ret = esp_mcp_remove_tool(mcp, tool);
            TEST_ASSERT_EQUAL(ESP_OK, ret);
            esp_mcp_tool_destroy(tool);
        }

        xSemaphoreTake(thread_test_mutex, portMAX_DELAY);
        thread_test_counter++;
        xSemaphoreGive(thread_test_mutex);
    }

    vTaskDelete(NULL);
}

// ========== Server API Tests ==========

TEST_CASE("test_server_create_destroy", "[mcp]")
{
    esp_mcp_t *mcp = NULL;

    // Test create
    esp_err_t ret = esp_mcp_create(&mcp);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_NULL(mcp);

    // Test destroy
    ret = esp_mcp_destroy(mcp);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("test_server_create_null", "[mcp]")
{
    esp_err_t ret = esp_mcp_create(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

TEST_CASE("test_server_destroy_null", "[mcp]")
{
    esp_err_t ret = esp_mcp_destroy(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

// ========== Tool API Tests ==========

TEST_CASE("test_tool_create_destroy", "[tool]")
{
    esp_mcp_tool_t *tool = esp_mcp_tool_create("test_tool", "Test tool description", test_tool_callback_bool);
    TEST_ASSERT_NOT_NULL(tool);

    esp_err_t ret = esp_mcp_tool_destroy(tool);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("test_tool_create_null_callback", "[tool]")
{
    esp_mcp_tool_t *tool = esp_mcp_tool_create("test_tool", "Test tool", NULL);
    TEST_ASSERT_NULL(tool);
}

TEST_CASE("test_tool_create_null_name", "[tool]")
{
    esp_mcp_tool_t *tool = esp_mcp_tool_create(NULL, "Test tool", test_tool_callback_bool);
    TEST_ASSERT_NULL(tool);
}

TEST_CASE("test_tool_create_null_description", "[tool]")
{
    esp_mcp_tool_t *tool = esp_mcp_tool_create("test_tool", NULL, test_tool_callback_bool);
    TEST_ASSERT_NULL(tool);
}

TEST_CASE("test_tool_destroy_null", "[tool]")
{
    esp_err_t ret = esp_mcp_tool_destroy(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

// ========== Property API Tests ==========

TEST_CASE("test_property_create_bool", "[property]")
{
    esp_mcp_property_t *prop = esp_mcp_property_create_with_bool("enabled", true);
    TEST_ASSERT_NOT_NULL(prop);

    esp_err_t ret = esp_mcp_property_destroy(prop);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("test_property_create_int", "[property]")
{
    esp_mcp_property_t *prop = esp_mcp_property_create_with_int("count", 42);
    TEST_ASSERT_NOT_NULL(prop);

    esp_err_t ret = esp_mcp_property_destroy(prop);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("test_property_create_float", "[property]")
{
    esp_mcp_property_t *prop = esp_mcp_property_create_with_float("temperature", 25.5f);
    TEST_ASSERT_NOT_NULL(prop);

    esp_err_t ret = esp_mcp_property_destroy(prop);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("test_property_create_string", "[property]")
{
    esp_mcp_property_t *prop = esp_mcp_property_create_with_string("message", "Hello World");
    TEST_ASSERT_NOT_NULL(prop);

    esp_err_t ret = esp_mcp_property_destroy(prop);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("test_property_create_array", "[property]")
{
    esp_mcp_property_t *prop = esp_mcp_property_create_with_array("items", "[1, 2, 3]");
    TEST_ASSERT_NOT_NULL(prop);

    esp_err_t ret = esp_mcp_property_destroy(prop);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("test_property_create_object", "[property]")
{
    esp_mcp_property_t *prop = esp_mcp_property_create_with_object("config", "{\"key\":\"value\"}");
    TEST_ASSERT_NOT_NULL(prop);

    esp_err_t ret = esp_mcp_property_destroy(prop);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("test_property_create_with_range", "[property]")
{
    esp_mcp_property_t *prop = esp_mcp_property_create_with_range("volume", 0, 100);
    TEST_ASSERT_NOT_NULL(prop);

    esp_err_t ret = esp_mcp_property_destroy(prop);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("test_property_create_with_int_and_range", "[property]")
{
    esp_mcp_property_t *prop = esp_mcp_property_create_with_int_and_range("volume", 50, 0, 100);
    TEST_ASSERT_NOT_NULL(prop);

    esp_err_t ret = esp_mcp_property_destroy(prop);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("test_property_create_null_name", "[property]")
{
    esp_mcp_property_t *prop = esp_mcp_property_create_with_bool(NULL, true);
    TEST_ASSERT_NULL(prop);
}

TEST_CASE("test_property_destroy_null", "[property]")
{
    esp_err_t ret = esp_mcp_property_destroy(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

// ========== Value API Tests ==========

TEST_CASE("test_value_create_bool", "[value]")
{
    esp_mcp_value_t value = esp_mcp_value_create_bool(true);
    TEST_ASSERT_EQUAL(ESP_MCP_VALUE_TYPE_BOOLEAN, value.type);
    TEST_ASSERT_TRUE(value.data.bool_value);

    esp_err_t ret = esp_mcp_value_destroy(&value);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("test_value_create_int", "[value]")
{
    esp_mcp_value_t value = esp_mcp_value_create_int(42);
    TEST_ASSERT_EQUAL(ESP_MCP_VALUE_TYPE_INTEGER, value.type);
    TEST_ASSERT_EQUAL(42, value.data.int_value);

    esp_err_t ret = esp_mcp_value_destroy(&value);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("test_value_create_float", "[value]")
{
    esp_mcp_value_t value = esp_mcp_value_create_float(3.14f);
    TEST_ASSERT_EQUAL(ESP_MCP_VALUE_TYPE_FLOAT, value.type);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.14f, value.data.float_value);

    esp_err_t ret = esp_mcp_value_destroy(&value);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("test_value_create_string", "[value]")
{
    esp_mcp_value_t value = esp_mcp_value_create_string("Hello World");
    TEST_ASSERT_EQUAL(ESP_MCP_VALUE_TYPE_STRING, value.type);
    TEST_ASSERT_NOT_NULL(value.data.string_value);
    TEST_ASSERT_EQUAL_STRING("Hello World", value.data.string_value);

    esp_err_t ret = esp_mcp_value_destroy(&value);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NULL(value.data.string_value);
}

TEST_CASE("test_value_create_string_null", "[value]")
{
    esp_mcp_value_t value = esp_mcp_value_create_string(NULL);
    TEST_ASSERT_EQUAL(ESP_MCP_VALUE_TYPE_INVALID, value.type);
}

TEST_CASE("test_value_destroy_null", "[value]")
{
    esp_err_t ret = esp_mcp_value_destroy(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

// ========== Server + Tool Integration Tests ==========

TEST_CASE("test_server_add_remove_tool", "[mcp][tool]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_tool_t *tool = esp_mcp_tool_create("test_tool", "Test tool", test_tool_callback_bool);
    TEST_ASSERT_NOT_NULL(tool);

    // Add tool
    esp_err_t ret = esp_mcp_add_tool(mcp, tool);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Try to add same tool again (should fail)
    esp_mcp_tool_t *tool2 = esp_mcp_tool_create("test_tool", "Test tool 2", test_tool_callback_bool);
    TEST_ASSERT_NOT_NULL(tool2);
    ret = esp_mcp_add_tool(mcp, tool2);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
    esp_mcp_tool_destroy(tool2);

    // Remove tool
    ret = esp_mcp_remove_tool(mcp, tool);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Remove again (should fail)
    ret = esp_mcp_remove_tool(mcp, tool);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);

    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_server_add_tool_null", "[mcp][tool]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_err_t ret = esp_mcp_add_tool(mcp, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    esp_mcp_tool_t *tool = esp_mcp_tool_create("test_tool", "Test tool", test_tool_callback_bool);
    ret = esp_mcp_add_tool(NULL, tool);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    esp_mcp_tool_destroy(tool);
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_server_remove_tool_null", "[mcp][tool]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_err_t ret = esp_mcp_remove_tool(mcp, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ret = esp_mcp_remove_tool(NULL, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

// ========== Tool + Property Integration Tests ==========

TEST_CASE("test_tool_add_remove_property", "[tool][property]")
{
    esp_mcp_tool_t *tool = esp_mcp_tool_create("test_tool", "Test tool", test_tool_callback_bool);
    TEST_ASSERT_NOT_NULL(tool);

    esp_mcp_property_t *prop = esp_mcp_property_create_with_bool("enabled", true);
    TEST_ASSERT_NOT_NULL(prop);

    // Add property
    esp_err_t ret = esp_mcp_tool_add_property(tool, prop);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Remove property
    ret = esp_mcp_tool_remove_property(tool, prop);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    esp_mcp_property_destroy(prop);
    esp_mcp_tool_destroy(tool);
}

TEST_CASE("test_tool_add_property_null", "[tool][property]")
{
    esp_mcp_tool_t *tool = esp_mcp_tool_create("test_tool", "Test tool", test_tool_callback_bool);
    TEST_ASSERT_NOT_NULL(tool);

    esp_mcp_property_t *prop = esp_mcp_property_create_with_bool("enabled", true);
    TEST_ASSERT_NOT_NULL(prop);

    esp_err_t ret = esp_mcp_tool_add_property(tool, NULL);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);

    ret = esp_mcp_tool_add_property(NULL, prop);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);

    esp_mcp_property_destroy(prop);
    esp_mcp_tool_destroy(tool);
}

TEST_CASE("test_tool_multiple_properties", "[tool][property]")
{
    esp_mcp_tool_t *tool = esp_mcp_tool_create("test_tool", "Test tool", test_tool_callback_multi);
    TEST_ASSERT_NOT_NULL(tool);

    esp_mcp_property_t *prop1 = esp_mcp_property_create_with_bool("enabled", true);
    esp_mcp_property_t *prop2 = esp_mcp_property_create_with_int("count", 10);
    esp_mcp_property_t *prop3 = esp_mcp_property_create_with_float("temperature", 25.5f);
    esp_mcp_property_t *prop4 = esp_mcp_property_create_with_string("message", "Hello");

    TEST_ASSERT_NOT_NULL(prop1);
    TEST_ASSERT_NOT_NULL(prop2);
    TEST_ASSERT_NOT_NULL(prop3);
    TEST_ASSERT_NOT_NULL(prop4);

    ESP_ERROR_CHECK(esp_mcp_tool_add_property(tool, prop1));
    ESP_ERROR_CHECK(esp_mcp_tool_add_property(tool, prop2));
    ESP_ERROR_CHECK(esp_mcp_tool_add_property(tool, prop3));
    ESP_ERROR_CHECK(esp_mcp_tool_add_property(tool, prop4));

    // Properties are owned by tool, so we don't destroy them separately
    esp_mcp_tool_destroy(tool);
}

// ========== Thread Safety Tests ==========

TEST_CASE("test_server_thread_safety", "[mcp][thread_safety]")
{
    relax_memory_check = true;
    esp_mcp_t *mcp = NULL;
    esp_err_t create_ret = esp_mcp_create(&mcp);
    if (create_ret != ESP_OK) {
        TEST_FAIL_MESSAGE("Failed to create MCP instance");
        return;
    }

    thread_test_mutex = xSemaphoreCreateMutex();
    if (!thread_test_mutex) {
        (void)esp_mcp_destroy(mcp);
        TEST_FAIL_MESSAGE("Failed to create thread_test_mutex");
        return;
    }
    thread_test_counter = 0;

    // Create multiple threads that add/remove tools concurrently
    TaskHandle_t tasks[THREAD_TEST_THREADS] = {0};
    thread_test_params_t params[THREAD_TEST_THREADS];
    bool task_create_failed = false;

    for (int i = 0; i < THREAD_TEST_THREADS; i++) {
        params[i].mcp = mcp;
        params[i].thread_id = i;
        if (xTaskCreate(thread_test_task, "thread_test", 4096, &params[i], 5, &tasks[i]) != pdPASS) {
            task_create_failed = true;
            break;
        }
    }

    // Wait until all iterations are completed (or timeout) before cleanup.
    const int expected_counter = THREAD_TEST_ITERATIONS * THREAD_TEST_THREADS;
    int wait_rounds = 0;
    int final_counter = 0;
    while (!task_create_failed && wait_rounds < 300) { // up to ~30s
        vTaskDelay(pdMS_TO_TICKS(100));
        xSemaphoreTake(thread_test_mutex, portMAX_DELAY);
        final_counter = thread_test_counter;
        xSemaphoreGive(thread_test_mutex);
        if (final_counter >= expected_counter) {
            break;
        }
        wait_rounds++;
    }

    // Verify all threads completed
    xSemaphoreTake(thread_test_mutex, portMAX_DELAY);
    final_counter = thread_test_counter;
    xSemaphoreGive(thread_test_mutex);

    ESP_LOGI(TAG, "Thread test completed: %d iterations", final_counter);
    vSemaphoreDelete(thread_test_mutex);
    thread_test_mutex = NULL;
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));

    if (task_create_failed) {
        TEST_FAIL_MESSAGE("Failed to create one or more thread_test tasks");
    }
    TEST_ASSERT_EQUAL(expected_counter, final_counter);
}

// ========== Memory Leak Tests ==========

TEST_CASE("test_memory_leak_server", "[memory]")
{
    for (int i = 0; i < 10; i++) {
        esp_mcp_t *mcp = NULL;
        ESP_ERROR_CHECK(esp_mcp_create(&mcp));
        ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
    }
}

TEST_CASE("test_memory_leak_tool", "[memory]")
{
    for (int i = 0; i < 10; i++) {
        esp_mcp_tool_t *tool = esp_mcp_tool_create("test_tool", "Test tool", test_tool_callback_bool);
        TEST_ASSERT_NOT_NULL(tool);
        ESP_ERROR_CHECK(esp_mcp_tool_destroy(tool));
    }
}

TEST_CASE("test_memory_leak_property", "[memory]")
{
    for (int i = 0; i < 10; i++) {
        esp_mcp_property_t *prop = esp_mcp_property_create_with_string("test", "value");
        TEST_ASSERT_NOT_NULL(prop);
        ESP_ERROR_CHECK(esp_mcp_property_destroy(prop));
    }
}

TEST_CASE("test_memory_leak_value", "[memory]")
{
    for (int i = 0; i < 10; i++) {
        esp_mcp_value_t value = esp_mcp_value_create_string("Hello World");
        TEST_ASSERT_EQUAL(ESP_MCP_VALUE_TYPE_STRING, value.type);
        ESP_ERROR_CHECK(esp_mcp_value_destroy(&value));
    }
}

TEST_CASE("test_memory_leak_full_workflow", "[memory]")
{
    for (int i = 0; i < 5; i++) {
        esp_mcp_t *mcp = NULL;
        ESP_ERROR_CHECK(esp_mcp_create(&mcp));

        esp_mcp_tool_t *tool = esp_mcp_tool_create("test_tool", "Test tool", test_tool_callback_bool);
        TEST_ASSERT_NOT_NULL(tool);

        esp_mcp_property_t *prop = esp_mcp_property_create_with_bool("enabled", true);
        TEST_ASSERT_NOT_NULL(prop);

        ESP_ERROR_CHECK(esp_mcp_tool_add_property(tool, prop));
        ESP_ERROR_CHECK(esp_mcp_add_tool(mcp, tool));
        ESP_ERROR_CHECK(esp_mcp_remove_tool(mcp, tool));

        ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
    }
}

// ========== Mock Transport for Testing ==========

typedef struct {
    esp_mcp_mgr_handle_t handle;
    bool started;
    void *config;
} mock_transport_item_t;

static void mcp_test_reset_emit_capture(void)
{
    s_last_emit_message[0] = '\0';
    s_last_emit_session_id[0] = '\0';
    s_emit_count = 0;
    memset(s_emit_history, 0, sizeof(s_emit_history));
    memset(s_emit_session_history, 0, sizeof(s_emit_session_history));
}

static int mcp_test_find_emit_session(const char *session_id)
{
    for (int i = 0; i < s_emit_count && i < (int)(sizeof(s_emit_session_history) / sizeof(s_emit_session_history[0])); ++i) {
        if (strcmp(s_emit_session_history[i], session_id ? session_id : "") == 0) {
            return i;
        }
    }
    return -1;
}

static void mcp_test_reset_request_capture(void)
{
    s_last_request_message[0] = '\0';
    memset(s_request_history, 0, sizeof(s_request_history));
    s_request_count = 0;
}

static esp_err_t mock_transport_init(esp_mcp_mgr_handle_t handle, esp_mcp_transport_handle_t *transport_handle)
{
    mock_transport_item_t *mock = calloc(1, sizeof(mock_transport_item_t));
    if (!mock) {
        return ESP_ERR_NO_MEM;
    }
    mock->handle = handle;
    *transport_handle = (esp_mcp_transport_handle_t)mock;
    return ESP_OK;
}

static esp_err_t mock_transport_deinit(esp_mcp_transport_handle_t handle)
{
    mock_transport_item_t *mock = (mock_transport_item_t *)handle;
    if (!mock) {
        return ESP_ERR_INVALID_ARG;
    }
    free(mock);
    return ESP_OK;
}

static esp_err_t mock_transport_start(esp_mcp_transport_handle_t handle, void *config)
{
    mock_transport_item_t *mock = (mock_transport_item_t *)handle;
    if (!mock) {
        return ESP_ERR_INVALID_ARG;
    }
    mock->started = true;
    mock->config = config;
    return ESP_OK;
}

static esp_err_t mock_transport_stop(esp_mcp_transport_handle_t handle)
{
    mock_transport_item_t *mock = (mock_transport_item_t *)handle;
    if (!mock) {
        return ESP_ERR_INVALID_ARG;
    }
    mock->started = false;
    return ESP_OK;
}

static esp_err_t mock_transport_create_config(const void *config, void **config_out)
{
    if (!config || !config_out) {
        return ESP_ERR_INVALID_ARG;
    }
    int *new_config = calloc(1, sizeof(int));
    if (!new_config) {
        return ESP_ERR_NO_MEM;
    }
    *new_config = *(int *)config;
    *config_out = new_config;
    return ESP_OK;
}

static esp_err_t mock_transport_delete_config(void *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    free(config);
    return ESP_OK;
}

static esp_err_t mock_transport_register_endpoint(esp_mcp_transport_handle_t handle, const char *ep_name, void *priv_data)
{
    (void)handle;
    (void)ep_name;
    (void)priv_data;
    return ESP_OK;
}

static esp_err_t mock_transport_unregister_endpoint(esp_mcp_transport_handle_t handle, const char *ep_name)
{
    (void)handle;
    (void)ep_name;
    return ESP_OK;
}

static esp_err_t mock_transport_emit_message(esp_mcp_transport_handle_t handle, const char *session_id, const char *jsonrpc_message)
{
    (void)handle;
    s_emit_count++;
    if (session_id) {
        snprintf(s_last_emit_session_id, sizeof(s_last_emit_session_id), "%s", session_id);
    } else {
        s_last_emit_session_id[0] = '\0';
    }
    if (jsonrpc_message) {
        snprintf(s_last_emit_message, sizeof(s_last_emit_message), "%s", jsonrpc_message);
    } else {
        s_last_emit_message[0] = '\0';
    }
    if (s_emit_count <= (int)(sizeof(s_emit_history) / sizeof(s_emit_history[0]))) {
        snprintf(s_emit_history[s_emit_count - 1], sizeof(s_emit_history[0]), "%s", jsonrpc_message ? jsonrpc_message : "");
        snprintf(s_emit_session_history[s_emit_count - 1], sizeof(s_emit_session_history[0]), "%s", session_id ? session_id : "");
    }
    return ESP_OK;
}

static esp_err_t mock_transport_request(esp_mcp_transport_handle_t handle, const uint8_t *inbuf, uint16_t inlen)
{
    mock_transport_item_t *mock = (mock_transport_item_t *)handle;
    if (!mock || !inbuf || inlen == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    char *req = calloc(1, (size_t)inlen + 1);
    if (!req) {
        return ESP_ERR_NO_MEM;
    }
    memcpy(req, inbuf, inlen);
    s_request_count++;
    snprintf(s_last_request_message, sizeof(s_last_request_message), "%s", req);
    if (s_request_count <= (int)(sizeof(s_request_history) / sizeof(s_request_history[0]))) {
        snprintf(s_request_history[s_request_count - 1], sizeof(s_request_history[0]), "%s", req);
    }

    cJSON *root = cJSON_Parse(req);
    free(req);
    if (!root) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *resp = cJSON_CreateObject();
    if (!resp) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(resp, "jsonrpc", "2.0");

    cJSON *id = cJSON_GetObjectItem(root, "id");
    if (id && (cJSON_IsString(id) || cJSON_IsNumber(id))) {
        cJSON_AddItemToObject(resp, "id", cJSON_Duplicate(id, 1));
    }

    cJSON *method = cJSON_GetObjectItem(root, "method");
    cJSON *result = cJSON_GetObjectItem(root, "result");
    cJSON *error = cJSON_GetObjectItem(root, "error");
    if ((!method || !cJSON_IsString(method) || !method->valuestring) &&
            id && (result || error)) {
        cJSON_Delete(resp);
        cJSON_Delete(root);
        return ESP_OK;
    }
    if (method && cJSON_IsString(method) && method->valuestring && !id) {
        cJSON_Delete(resp);
        cJSON_Delete(root);
        return ESP_OK;
    }
    if (!(method && cJSON_IsString(method) && method->valuestring)) {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddItemToObject(resp, "error", err);
        cJSON_AddNumberToObject(err, "code", -32600);
        cJSON_AddStringToObject(err, "message", "Invalid Request");
    } else if (strcmp(method->valuestring, "initialize") == 0) {
        cJSON *result = cJSON_CreateObject();
        cJSON_AddItemToObject(resp, "result", result);
        cJSON_AddStringToObject(result, "protocolVersion", "2025-11-25");
        cJSON_AddItemToObject(result, "capabilities", cJSON_CreateObject());
        cJSON *server_info = cJSON_CreateObject();
        cJSON_AddItemToObject(result, "serverInfo", server_info);
        cJSON_AddStringToObject(server_info, "name", "mock");
        cJSON_AddStringToObject(server_info, "version", "1.0");
    } else if (strcmp(method->valuestring, "ping") == 0) {
        cJSON_AddItemToObject(resp, "result", cJSON_CreateObject());
    } else if (strcmp(method->valuestring, "tools/list") == 0) {
        cJSON *result = cJSON_CreateObject();
        cJSON_AddItemToObject(resp, "result", result);
        cJSON_AddItemToObject(result, "tools", cJSON_CreateArray());
    } else if (strcmp(method->valuestring, "tools/call") == 0) {
        cJSON *result = cJSON_CreateObject();
        cJSON_AddItemToObject(resp, "result", result);
        cJSON *content = cJSON_CreateArray();
        cJSON_AddItemToObject(result, "content", content);
        cJSON *text_item = cJSON_CreateObject();
        cJSON_AddItemToArray(content, text_item);
        cJSON_AddStringToObject(text_item, "type", "text");
        cJSON_AddStringToObject(text_item, "text", "ok");
        cJSON_AddBoolToObject(result, "isError", false);
    } else {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddItemToObject(resp, "error", err);
        cJSON_AddNumberToObject(err, "code", -32601);
        cJSON_AddStringToObject(err, "message", "Method not found");
    }

    char *resp_json = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    cJSON_Delete(root);
    if (!resp_json) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = esp_mcp_mgr_perform_handle(mock->handle, (const uint8_t *)resp_json, (uint16_t)strlen(resp_json));
    cJSON_free(resp_json);
    return ret;
}

static esp_err_t mock_transport_open_stream(esp_mcp_transport_handle_t handle)
{
    mock_transport_item_t *mock = (mock_transport_item_t *)handle;
    if (!mock) {
        return ESP_ERR_INVALID_ARG;
    }
    s_open_stream_count++;
    return ESP_OK;
}

static const esp_mcp_transport_t mock_transport = {
    .init = mock_transport_init,
    .deinit = mock_transport_deinit,
    .start = mock_transport_start,
    .stop = mock_transport_stop,
    .create_config = mock_transport_create_config,
    .delete_config = mock_transport_delete_config,
    .register_endpoint = mock_transport_register_endpoint,
    .unregister_endpoint = mock_transport_unregister_endpoint,
    .emit_message = mock_transport_emit_message,
};

static const esp_mcp_transport_t mock_transport_with_request = {
    .init = mock_transport_init,
    .deinit = mock_transport_deinit,
    .start = mock_transport_start,
    .stop = mock_transport_stop,
    .create_config = mock_transport_create_config,
    .delete_config = mock_transport_delete_config,
    .register_endpoint = mock_transport_register_endpoint,
    .unregister_endpoint = mock_transport_unregister_endpoint,
    .request = mock_transport_request,
    .open_stream = mock_transport_open_stream,
    .emit_message = mock_transport_emit_message,
};

// ========== MCP Transport Manager API Tests ==========

TEST_CASE("test_mcp_init_deinit", "[mcp]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    esp_err_t ret = esp_mcp_mgr_init(config, &handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_EQUAL(0, handle);

    ret = esp_mcp_mgr_deinit(handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_mcp_init_null_handle", "[mcp]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_err_t ret = esp_mcp_mgr_init(config, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_mcp_init_invalid_transport", "[mcp]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = {0},  // All NULL functions
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    esp_err_t ret = esp_mcp_mgr_init(config, &handle);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_mcp_deinit_invalid_handle", "[mcp]")
{
    esp_err_t ret = esp_mcp_mgr_deinit(0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

TEST_CASE("test_mcp_start_stop", "[mcp]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));

    // Start transport
    esp_err_t ret = esp_mcp_mgr_start(handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Stop transport
    ret = esp_mcp_mgr_stop(handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Deinit (must stop before deinit)
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_mcp_start_invalid_handle", "[mcp]")
{
    esp_err_t ret = esp_mcp_mgr_start(0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

TEST_CASE("test_mcp_stop_invalid_handle", "[mcp]")
{
    esp_err_t ret = esp_mcp_mgr_stop(0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

TEST_CASE("test_mcp_open_stream_not_supported", "[mcp]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    esp_err_t ret = esp_mcp_mgr_open_stream(handle);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, ret);

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_mcp_open_stream_supported", "[mcp]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport_with_request,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    s_open_stream_count = 0;
    esp_err_t ret = esp_mcp_mgr_open_stream(handle);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL_INT(1, s_open_stream_count);

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_http_client_auth_api_rejects_non_http_transport", "[mcp][http][auth]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));

    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_mcp_mgr_http_client_set_bearer_token(handle, "token"));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      esp_mcp_mgr_http_client_set_auth_callback(handle, test_http_auth_callback, NULL));

    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

#if CONFIG_MCP_TRANSPORT_HTTP_CLIENT
TEST_CASE("test_http_client_auth_api_accepts_http_client_transport", "[mcp][http][auth]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_http_client_config_t http_cfg = {
        .url = "http://example.com",
        .timeout_ms = 1000,
    };
    esp_mcp_mgr_config_t config = {
        .transport = esp_mcp_transport_http_client,
        .config = &http_cfg,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));

    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_mgr_http_client_set_bearer_token(handle, "token"));
    TEST_ASSERT_EQUAL(ESP_OK,
                      esp_mcp_mgr_http_client_set_auth_callback(handle, test_http_auth_callback, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_mgr_http_client_set_bearer_token(handle, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_mgr_http_client_set_auth_callback(handle, NULL, NULL));

    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}
#endif

TEST_CASE("test_mcp_register_unregister_endpoint", "[mcp]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));

    // Register endpoint
    esp_err_t ret = esp_mcp_mgr_register_endpoint(handle, "test_endpoint", NULL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Try to register same endpoint again (should fail)
    ret = esp_mcp_mgr_register_endpoint(handle, "test_endpoint", NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);

    // Unregister endpoint
    ret = esp_mcp_mgr_unregister_endpoint(handle, "test_endpoint");
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Unregister again (should fail)
    ret = esp_mcp_mgr_unregister_endpoint(handle, "test_endpoint");
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, ret);

    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_mcp_register_endpoint_null", "[mcp]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));

    esp_err_t ret = esp_mcp_mgr_register_endpoint(handle, NULL, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ret = esp_mcp_mgr_register_endpoint(0, "test", NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_mcp_unregister_endpoint_null", "[mcp]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));

    esp_err_t ret = esp_mcp_mgr_unregister_endpoint(handle, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ret = esp_mcp_mgr_unregister_endpoint(0, "test");
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

// ========== MCP Request Handling Tests ==========

TEST_CASE("test_mcp_req_handle", "[mcp]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    // Add a tool to mcp
    esp_mcp_tool_t *tool = esp_mcp_tool_create("test_tool", "Test tool", test_tool_callback_bool);
    TEST_ASSERT_NOT_NULL(tool);
    ESP_ERROR_CHECK(esp_mcp_add_tool(mcp, tool));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));

    // Register endpoint
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));

    // Test request handling with initialize message
    const char *init_msg = MCP_TEST_INIT_DEFAULT_REQ_JSON;
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    esp_err_t ret = esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)init_msg, strlen(init_msg), &outbuf, &outlen);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_NULL(outbuf);
    TEST_ASSERT_GREATER_THAN(0, outlen);

    // Clean up response buffer
    if (outbuf) {
        esp_mcp_mgr_req_destroy_response(handle, outbuf);
    }

    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_mcp_req_handle_invalid_endpoint", "[mcp]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));

    const char *msg = MCP_TEST_INIT_DEFAULT_REQ_JSON;
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    esp_err_t ret = esp_mcp_mgr_req_handle(handle, "nonexistent", (const uint8_t *)msg, strlen(msg), &outbuf, &outlen);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, ret);

    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_mcp_req_handle_null_args", "[mcp]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));

    const char *msg = MCP_TEST_INIT_DEFAULT_REQ_JSON;
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    esp_err_t ret = esp_mcp_mgr_req_handle(0, "test", (const uint8_t *)msg, strlen(msg), &outbuf, &outlen);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ret = esp_mcp_mgr_req_handle(handle, NULL, (const uint8_t *)msg, strlen(msg), &outbuf, &outlen);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_mcp_req_destroy_response", "[mcp]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));

    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));

    const char *init_msg = MCP_TEST_INIT_DEFAULT_REQ_JSON;
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    ESP_ERROR_CHECK(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)init_msg, strlen(init_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    // Destroy response
    esp_err_t ret = esp_mcp_mgr_req_destroy_response(handle, outbuf);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_mcp_req_destroy_response_null", "[mcp]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));

    esp_err_t ret = esp_mcp_mgr_req_destroy_response(handle, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ret = esp_mcp_mgr_req_destroy_response(0, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

// ========== MCP + Server Integration Tests ==========

static void mcp_test_send_initialize_with_capabilities_for_session(esp_mcp_mgr_handle_t handle,
                                                                   const char *session_id,
                                                                   const char *capabilities_json)
{
    static unsigned s_initialize_req_id = 1000;
    const char *caps = capabilities_json ? capabilities_json : "{}";
    char init_msg[512] = {0};
    snprintf(init_msg, sizeof(init_msg),
             "{\"jsonrpc\":\"2.0\",\"method\":\"initialize\","
             "\"params\":{\"protocolVersion\":\"2025-11-25\","
             "\"clientInfo\":{\"name\":\"test_client\",\"version\":\"1.0\"},"
             "\"capabilities\":%s},\"id\":%u}",
             caps,
             s_initialize_req_id++);
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    ESP_ERROR_CHECK(esp_mcp_mgr_set_request_context(handle, session_id, NULL, true));
    ESP_ERROR_CHECK(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)init_msg, strlen(init_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    outbuf = NULL;
    outlen = 0;
    const char *initialized_msg =
        "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\",\"params\":{}}";
    ESP_ERROR_CHECK(esp_mcp_mgr_req_handle(handle, "mcp_server",
                                           (const uint8_t *)initialized_msg,
                                           strlen(initialized_msg),
                                           &outbuf,
                                           &outlen));
    TEST_ASSERT_NULL(outbuf);
    TEST_ASSERT_EQUAL_UINT16(0, outlen);
    // Preserve the default "authenticated" test context while clearing session scoping.
    ESP_ERROR_CHECK(esp_mcp_mgr_set_request_context(handle, NULL, NULL, true));
}

static void mcp_test_send_initialize(esp_mcp_mgr_handle_t handle)
{
    mcp_test_send_initialize_with_capabilities_for_session(handle, NULL, NULL);
}

#define MCP_TEST_REQ_HANDLE(_expr) do {                                  \
        esp_err_t __ret = (_expr);                                       \
        TEST_ASSERT_TRUE(__ret == ESP_OK ||                              \
                         __ret == ESP_ERR_INVALID_ARG ||                 \
                         __ret == ESP_ERR_INVALID_STATE);                \
    } while (0)

TEST_CASE("test_mcp_full_workflow_with_server", "[mcp][mcp][integration]")
{
    // Create mcp
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    // Create and add tool
    esp_mcp_tool_t *tool = esp_mcp_tool_create("test_tool", "Test tool", test_tool_callback_bool);
    TEST_ASSERT_NOT_NULL(tool);

    esp_mcp_property_t *prop = esp_mcp_property_create_with_bool("enabled", true);
    TEST_ASSERT_NOT_NULL(prop);
    ESP_ERROR_CHECK(esp_mcp_tool_add_property(tool, prop));

    ESP_ERROR_CHECK(esp_mcp_add_tool(mcp, tool));

    // Initialize MCP with mock transport
    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));

    // Register endpoint
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));

    // Start transport
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    // Test tools/list request
    const char *tools_list_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"tools/list\",\"params\":{},\"id\":2}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    esp_err_t ret = esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)tools_list_msg, strlen(tools_list_msg), &outbuf, &outlen);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_NULL(outbuf);
    TEST_ASSERT_GREATER_THAN(0, outlen);

    // Clean up
    if (outbuf) {
        esp_mcp_mgr_req_destroy_response(handle, outbuf);
    }

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_mcp_tools_call_integration", "[mcp][mcp][tool][integration]")
{
    // Create mcp
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    // Create tool with callback
    esp_mcp_tool_t *tool = esp_mcp_tool_create("test_tool", "Test tool", test_tool_callback_bool);
    TEST_ASSERT_NOT_NULL(tool);

    esp_mcp_property_t *prop = esp_mcp_property_create_with_bool("enabled", true);
    TEST_ASSERT_NOT_NULL(prop);
    ESP_ERROR_CHECK(esp_mcp_tool_add_property(tool, prop));

    ESP_ERROR_CHECK(esp_mcp_add_tool(mcp, tool));

    // Initialize MCP
    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    // Test tools/call request
    const char *tools_call_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"test_tool\",\"arguments\":{\"enabled\":true}},\"id\":3}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    esp_err_t ret = esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)tools_call_msg, strlen(tools_call_msg), &outbuf, &outlen);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_NULL(outbuf);
    TEST_ASSERT_GREATER_THAN(0, outlen);

    // Clean up
    if (outbuf) {
        esp_mcp_mgr_req_destroy_response(handle, outbuf);
    }

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

// ============================================================================
// MCP 2024-11-05 Protocol Compliance Tests
// ============================================================================

// ========== Helper Functions for Protocol Testing ==========

static cJSON* parse_json_response(const uint8_t *outbuf, uint16_t outlen)
{
    if (!outbuf || outlen == 0) {
        return NULL;
    }
    return cJSON_Parse((const char *)outbuf);
}

static bool validate_jsonrpc_field(const cJSON *json, const char *version)
{
    cJSON *jsonrpc = cJSON_GetObjectItem(json, "jsonrpc");
    if (!jsonrpc || !cJSON_IsString(jsonrpc)) {
        return false;
    }
    return strcmp(jsonrpc->valuestring, version) == 0;
    return false;
}

static cJSON* validate_and_parse_response(const uint8_t *outbuf, uint16_t outlen, bool *is_error)
{
    cJSON *root = parse_json_response(outbuf, outlen);
    if (!root) {
        return NULL;
    }

    if (!validate_jsonrpc_field(root, "2.0")) {
        cJSON_Delete(root);
        return NULL;
    }

    if (is_error) {
        cJSON *error = cJSON_GetObjectItem(root, "error");
        *is_error = (error != NULL);
    }

    return root;
}

static char *mcp_test_strdup(const char *s)
{
    if (!s) {
        return NULL;
    }
    size_t len = strlen(s) + 1;
    char *p = calloc(1, len);
    if (!p) {
        return NULL;
    }
    memcpy(p, s, len);
    return p;
}

static esp_err_t test_resource_read_cb(const char *uri,
                                       char **out_mime_type,
                                       char **out_text,
                                       char **out_blob_base64,
                                       void *user_ctx)
{
    (void)user_ctx;
    if (!uri || !out_mime_type || !out_text || !out_blob_base64) {
        return ESP_ERR_INVALID_ARG;
    }
    if (strcmp(uri, "device://status") != 0) {
        return ESP_ERR_NOT_FOUND;
    }
    *out_mime_type = mcp_test_strdup("application/json");
    *out_text = mcp_test_strdup("{\"online\":true,\"name\":\"esp32\"}");
    *out_blob_base64 = NULL;
    if (!*out_mime_type || !*out_text) {
        free(*out_mime_type);
        free(*out_text);
        *out_mime_type = NULL;
        *out_text = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t __attribute__((unused)) test_prompt_render_cb(const char *arguments_json,
                                                               char **out_description,
                                                               char **out_messages_json,
                                                               void *user_ctx)
{
    (void)arguments_json;
    (void)user_ctx;
    if (!out_description || !out_messages_json) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_description = mcp_test_strdup("status summary prompt");
    *out_messages_json = mcp_test_strdup("[{\"role\":\"user\",\"content\":{\"type\":\"text\",\"text\":\"status summary\"}}]");
    if (!*out_description || !*out_messages_json) {
        free(*out_description);
        free(*out_messages_json);
        *out_description = NULL;
        *out_messages_json = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t test_completion_cb(const char *ref_type,
                                    const char *name_or_uri,
                                    const char *argument_name,
                                    const char *argument_value,
                                    const char *context_args_json,
                                    cJSON **out_result_obj,
                                    void *user_ctx)
{
    (void)ref_type;
    (void)name_or_uri;
    (void)argument_name;
    (void)argument_value;
    (void)context_args_json;
    (void)user_ctx;
    if (!out_result_obj) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return ESP_ERR_NO_MEM;
    }
    cJSON *completion = cJSON_AddObjectToObject(root, "completion");
    if (!completion) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    cJSON *values = cJSON_AddArrayToObject(completion, "values");
    if (!values) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    cJSON *v0 = cJSON_CreateString("screen");
    cJSON *v1 = cJSON_CreateString("speaker");
    if (!v0 || !v1) {
        cJSON_Delete(v0);
        cJSON_Delete(v1);
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddItemToArray(values, v0);
    cJSON_AddItemToArray(values, v1);
    if (!cJSON_AddBoolToObject(completion, "hasMore", false)) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    *out_result_obj = root;
    return ESP_OK;
}

static esp_err_t test_completion_invalid_cb(const char *ref_type,
                                            const char *name_or_uri,
                                            const char *argument_name,
                                            const char *argument_value,
                                            const char *context_args_json,
                                            cJSON **out_result_obj,
                                            void *user_ctx)
{
    (void)ref_type;
    (void)name_or_uri;
    (void)argument_name;
    (void)argument_value;
    (void)context_args_json;
    (void)user_ctx;
    if (out_result_obj) {
        *out_result_obj = NULL;
    }
    return ESP_OK;
}

static esp_err_t test_resource_read_fail_cb(const char *uri,
                                            char **out_mime_type,
                                            char **out_text,
                                            char **out_blob_base64,
                                            void *user_ctx)
{
    (void)uri;
    (void)out_mime_type;
    (void)out_text;
    (void)out_blob_base64;
    (void)user_ctx;
    return ESP_ERR_NO_MEM;
}

static esp_err_t test_prompt_render_fail_cb(const char *arguments_json,
                                            char **out_description,
                                            char **out_messages_json,
                                            void *user_ctx)
{
    (void)arguments_json;
    (void)out_description;
    (void)out_messages_json;
    (void)user_ctx;
    return ESP_ERR_INVALID_STATE;
}

typedef struct {
    int error_code;
    char name[64];
    char payload[128];
    int called;
    uint32_t last_jsonrpc_id;
} pending_cb_result_t;

static esp_err_t pending_req_cb_capture(int error_code, const char *name, const char *resp_json, void *user_ctx,
                                        uint32_t jsonrpc_request_id)
{
    pending_cb_result_t *result = (pending_cb_result_t *)user_ctx;
    if (!result) {
        return ESP_ERR_INVALID_ARG;
    }
    result->error_code = error_code;
    result->last_jsonrpc_id = jsonrpc_request_id;
    result->called++;
    if (name) {
        snprintf(result->name, sizeof(result->name), "%s", name);
    }
    if (resp_json) {
        snprintf(result->payload, sizeof(result->payload), "%s", resp_json);
    }
    return ESP_OK;
}

typedef struct {
    int called;
    esp_err_t return_code;
    char method[64];
    char params[256];
    char result_json[256];
} incoming_request_capture_t;

typedef struct {
    int called;
    char method[64];
    char params[256];
} incoming_notification_capture_t;

static esp_err_t incoming_request_capture_cb(const char *method,
                                             const char *params_json,
                                             char **out_result_json,
                                             void *user_ctx)
{
    incoming_request_capture_t *capture = (incoming_request_capture_t *)user_ctx;
    if (!capture) {
        return ESP_ERR_INVALID_ARG;
    }
    capture->called++;
    if (method) {
        snprintf(capture->method, sizeof(capture->method), "%s", method);
    }
    if (params_json) {
        snprintf(capture->params, sizeof(capture->params), "%s", params_json);
    } else {
        capture->params[0] = '\0';
    }
    if (capture->return_code == ESP_OK && capture->result_json[0] && out_result_json) {
        *out_result_json = strdup(capture->result_json);
        if (!*out_result_json) {
            return ESP_ERR_NO_MEM;
        }
    }
    return capture->return_code;
}

static esp_err_t incoming_notification_capture_cb(const char *method,
                                                  const char *params_json,
                                                  void *user_ctx)
{
    incoming_notification_capture_t *capture = (incoming_notification_capture_t *)user_ctx;
    if (!capture) {
        return ESP_ERR_INVALID_ARG;
    }
    capture->called++;
    if (method) {
        snprintf(capture->method, sizeof(capture->method), "%s", method);
    }
    if (params_json) {
        snprintf(capture->params, sizeof(capture->params), "%s", params_json);
    } else {
        capture->params[0] = '\0';
    }
    return ESP_OK;
}

static void mcp_test_reset_ex_cb_flags(void)
{
    s_ex_cb_arr_ok = false;
    s_ex_cb_obj_ok = false;
}

static esp_err_t test_tool_callback_ex_rich(const esp_mcp_property_list_t *properties, esp_mcp_tool_result_t *result)
{
    if (!properties || !result) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *arr = esp_mcp_property_list_get_property_array(properties, "arr");
    const char *obj = esp_mcp_property_list_get_property_object(properties, "obj");
    s_ex_cb_arr_ok = (arr && strstr(arr, "[1,2]"));
    s_ex_cb_obj_ok = (obj && strstr(obj, "\"k\":\"v\""));

    esp_err_t ret = esp_mcp_tool_result_add_text(result, "ok");
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_mcp_tool_result_add_image_base64(result, "image/png", "AAAA");
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_mcp_tool_result_add_audio_base64(result, "audio/wav", "BBBB");
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_mcp_tool_result_add_resource_link(result, "device://status", "status", "desc", "application/json");
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_mcp_tool_result_add_embedded_resource_text(result, "device://status", "application/json", "{\"ok\":true}");
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_mcp_tool_result_add_embedded_resource_blob(result, "device://blob", "application/octet-stream", "QkJC");
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_mcp_tool_result_set_structured_json(result, "{\"x\":1}");
    if (ret != ESP_OK) {
        return ret;
    }
    return esp_mcp_tool_result_set_is_error(result, false);
}

static esp_mcp_value_t test_tool_callback_noarg(const esp_mcp_property_list_t *properties)
{
    (void)properties;
    return esp_mcp_value_create_string("ok");
}

// ========== initialize Method Tests ==========

TEST_CASE("test_initialize_response_protocol_version", "[mcp][protocol][initialize]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    const char *init_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2024-11-05\",\"clientInfo\":{\"name\":\"test_client\",\"version\":\"1.0\"},\"capabilities\":{}},\"id\":1}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)init_msg, strlen(init_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);
    TEST_ASSERT_GREATER_THAN(0, outlen);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_FALSE(is_error);

    cJSON *result = cJSON_GetObjectItem(root, "result");
    TEST_ASSERT_NOT_NULL(result);

    cJSON *protocol_version = cJSON_GetObjectItem(result, "protocolVersion");
    TEST_ASSERT_NOT_NULL(protocol_version);
    TEST_ASSERT_TRUE(cJSON_IsString(protocol_version));
    TEST_ASSERT_EQUAL_STRING("2024-11-05", protocol_version->valuestring);

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_initialize_unknown_protocol_version_returns_invalid_params", "[mcp][protocol][initialize]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    const char *init_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2026-01-01\",\"clientInfo\":{\"name\":\"test_client\",\"version\":\"1.0\"},\"capabilities\":{}},\"id\":1}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)init_msg, strlen(init_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);
    TEST_ASSERT_GREATER_THAN(0, outlen);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_TRUE(is_error);

    cJSON *error = cJSON_GetObjectItem(root, "error");
    TEST_ASSERT_NOT_NULL(error);
    cJSON *code = cJSON_GetObjectItem(error, "code");
    TEST_ASSERT_TRUE(cJSON_IsNumber(code));
    TEST_ASSERT_EQUAL_INT(-32602, code->valueint);
    cJSON *data = cJSON_GetObjectItem(error, "data");
    TEST_ASSERT_TRUE(cJSON_IsObject(data));
    cJSON *field = cJSON_GetObjectItem(data, "field");
    TEST_ASSERT_TRUE(cJSON_IsString(field));
    TEST_ASSERT_EQUAL_STRING("params.protocolVersion", field->valuestring);

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_initialize_missing_required_fields_returns_invalid_params", "[mcp][protocol][initialize]")
{
    static const char *const bad_init_msgs[] = {
        "{\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"params\":{\"clientInfo\":{\"name\":\"test_client\",\"version\":\"1.0\"},\"capabilities\":{}},\"id\":\"missing-proto\"}",
        "{\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2025-11-25\",\"capabilities\":{}},\"id\":\"missing-client\"}",
        "{\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2025-11-25\",\"clientInfo\":{\"name\":\"test_client\",\"version\":\"1.0\"}},\"id\":\"missing-caps\"}",
        "{\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2025-11-25\",\"clientInfo\":{\"version\":\"1.0\"},\"capabilities\":{}},\"id\":\"missing-client-name\"}",
        "{\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2025-11-25\",\"clientInfo\":{\"name\":\"test_client\"},\"capabilities\":{}},\"id\":\"missing-client-version\"}",
    };
    static const char *const expected_fields[] = {
        "params.protocolVersion",
        "params.clientInfo",
        "params.capabilities",
        "params.clientInfo.name",
        "params.clientInfo.version",
    };

    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    for (size_t i = 0; i < sizeof(bad_init_msgs) / sizeof(bad_init_msgs[0]); ++i) {
        uint8_t *outbuf = NULL;
        uint16_t outlen = 0;
        MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle,
                                                   "mcp_server",
                                                   (const uint8_t *)bad_init_msgs[i],
                                                   strlen(bad_init_msgs[i]),
                                                   &outbuf,
                                                   &outlen));
        TEST_ASSERT_NOT_NULL(outbuf);
        TEST_ASSERT_GREATER_THAN(0, outlen);

        bool is_error = false;
        cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
        TEST_ASSERT_NOT_NULL(root);
        TEST_ASSERT_TRUE(is_error);

        cJSON *error = cJSON_GetObjectItem(root, "error");
        TEST_ASSERT_NOT_NULL(error);
        cJSON *code = cJSON_GetObjectItem(error, "code");
        TEST_ASSERT_TRUE(cJSON_IsNumber(code));
        TEST_ASSERT_EQUAL_INT(-32602, code->valueint);
        cJSON *data = cJSON_GetObjectItem(error, "data");
        TEST_ASSERT_TRUE(cJSON_IsObject(data));
        cJSON *field = cJSON_GetObjectItem(data, "field");
        TEST_ASSERT_TRUE(cJSON_IsString(field));
        TEST_ASSERT_EQUAL_STRING(expected_fields[i], field->valuestring);

        cJSON_Delete(root);
        esp_mcp_mgr_req_destroy_response(handle, outbuf);
    }

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_initialize_duplicate_request_returns_invalid_request", "[mcp][protocol][initialize]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *init_msg_a =
        "{\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"params\":"
        MCP_TEST_INIT_DEFAULT_PARAMS_JSON ",\"id\":\"init-a\"}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server",
                                               (const uint8_t *)init_msg_a,
                                               strlen(init_msg_a),
                                               &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    const char *init_msg_b =
        "{\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"params\":"
        MCP_TEST_INIT_DEFAULT_PARAMS_JSON ",\"id\":\"init-b\"}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server",
                                               (const uint8_t *)init_msg_b,
                                               strlen(init_msg_b),
                                               &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);
    TEST_ASSERT_GREATER_THAN(0, outlen);

    cJSON *root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(root);
    cJSON *error = cJSON_GetObjectItem(root, "error");
    TEST_ASSERT_NOT_NULL(error);
    cJSON *code = cJSON_GetObjectItem(error, "code");
    TEST_ASSERT_TRUE(cJSON_IsNumber(code));
    TEST_ASSERT_EQUAL_INT(-32600, code->valueint);
    cJSON *data = cJSON_GetObjectItem(error, "data");
    TEST_ASSERT_TRUE(cJSON_IsObject(data));
    cJSON *reason = cJSON_GetObjectItem(data, "reason");
    TEST_ASSERT_TRUE(cJSON_IsString(reason));
    TEST_ASSERT_NOT_NULL(strstr(reason->valuestring, "only be sent once"));

    cJSON_Delete(root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_initialize_is_session_scoped", "[mcp][protocol][initialize][session]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    ESP_ERROR_CHECK(esp_mcp_mgr_set_request_context(handle, "sess-a", NULL, true));
    const char *init_msg_a =
        "{\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"params\":"
        MCP_TEST_INIT_DEFAULT_PARAMS_JSON ",\"id\":\"sess-a-init\"}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server",
                                               (const uint8_t *)init_msg_a,
                                               strlen(init_msg_a),
                                               &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);
    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_FALSE(is_error);
    cJSON_Delete(root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    outbuf = NULL;
    outlen = 0;
    const char *initialized_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\",\"params\":{}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server",
                                               (const uint8_t *)initialized_msg,
                                               strlen(initialized_msg),
                                               &outbuf, &outlen));
    TEST_ASSERT_NULL(outbuf);

    ESP_ERROR_CHECK(esp_mcp_mgr_set_request_context(handle, "sess-b", NULL, true));
    const char *init_msg_b =
        "{\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"params\":"
        MCP_TEST_INIT_DEFAULT_PARAMS_JSON ",\"id\":\"sess-b-init\"}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server",
                                               (const uint8_t *)init_msg_b,
                                               strlen(init_msg_b),
                                               &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);
    root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_FALSE(is_error);
    cJSON_Delete(root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    outbuf = NULL;
    outlen = 0;
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server",
                                               (const uint8_t *)initialized_msg,
                                               strlen(initialized_msg),
                                               &outbuf, &outlen));
    TEST_ASSERT_NULL(outbuf);

    ESP_ERROR_CHECK(esp_mcp_mgr_set_request_context(handle, NULL, NULL, false));
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_clear_session_state_resets_initialize_lifecycle", "[mcp][protocol][initialize][session]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    char request_id[40] = {0};
    mcp_test_send_initialize_with_capabilities_for_session(handle, "sess-reset", "{\"roots\":{}}");

    mcp_test_reset_emit_capture();
    ESP_ERROR_CHECK(esp_mcp_request_roots_list(mcp, "sess-reset", NULL, NULL, 1000, request_id, sizeof(request_id)));
    TEST_ASSERT_NOT_EMPTY(request_id);

    ESP_ERROR_CHECK(esp_mcp_clear_session_state(mcp, "sess-reset"));

    mcp_test_reset_emit_capture();
    request_id[0] = '\0';
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      esp_mcp_request_roots_list(mcp, "sess-reset", NULL, NULL, 1000, request_id, sizeof(request_id)));
    TEST_ASSERT_EQUAL_INT(0, s_emit_count);

    mcp_test_send_initialize_with_capabilities_for_session(handle, "sess-reset", "{\"roots\":{}}");

    mcp_test_reset_emit_capture();
    request_id[0] = '\0';
    ESP_ERROR_CHECK(esp_mcp_request_roots_list(mcp, "sess-reset", NULL, NULL, 1000, request_id, sizeof(request_id)));
    TEST_ASSERT_NOT_EMPTY(request_id);

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_initialize_response_capabilities", "[mcp][protocol][initialize]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    const char *init_msg = MCP_TEST_INIT_DEFAULT_REQ_JSON;
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)init_msg, strlen(init_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_FALSE(is_error);

    cJSON *result = cJSON_GetObjectItem(root, "result");
    TEST_ASSERT_NOT_NULL(result);

    cJSON *capabilities = cJSON_GetObjectItem(result, "capabilities");
    TEST_ASSERT_NOT_NULL(capabilities);
    TEST_ASSERT_TRUE(cJSON_IsObject(capabilities));

    cJSON *tools = cJSON_GetObjectItem(capabilities, "tools");
    TEST_ASSERT_NOT_NULL(tools);
    TEST_ASSERT_TRUE(cJSON_IsObject(tools));

    cJSON *list_changed = cJSON_GetObjectItem(tools, "listChanged");
    TEST_ASSERT_NOT_NULL(list_changed);
    TEST_ASSERT_TRUE(cJSON_IsBool(list_changed));

    cJSON *tasks = cJSON_GetObjectItem(capabilities, "tasks");
    TEST_ASSERT_NOT_NULL(tasks);
    TEST_ASSERT_TRUE(cJSON_IsObject(tasks));

    cJSON *tasks_list = cJSON_GetObjectItem(tasks, "list");
    TEST_ASSERT_NOT_NULL(tasks_list);
    TEST_ASSERT_TRUE(cJSON_IsObject(tasks_list));

    cJSON *tasks_cancel = cJSON_GetObjectItem(tasks, "cancel");
    TEST_ASSERT_NOT_NULL(tasks_cancel);
    TEST_ASSERT_TRUE(cJSON_IsObject(tasks_cancel));

    cJSON *tasks_requests = cJSON_GetObjectItem(tasks, "requests");
    TEST_ASSERT_NOT_NULL(tasks_requests);
    TEST_ASSERT_TRUE(cJSON_IsObject(tasks_requests));

    cJSON *tasks_tools = cJSON_GetObjectItem(tasks_requests, "tools");
    TEST_ASSERT_NOT_NULL(tasks_tools);
    TEST_ASSERT_TRUE(cJSON_IsObject(tasks_tools));

    cJSON *tasks_tools_call = cJSON_GetObjectItem(tasks_tools, "call");
    TEST_ASSERT_NOT_NULL(tasks_tools_call);
    TEST_ASSERT_TRUE(cJSON_IsObject(tasks_tools_call));

    TEST_ASSERT_NULL(cJSON_GetObjectItem(capabilities, "completions"));

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_initialize_response_capabilities_with_completion_provider", "[mcp][protocol][initialize]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_completion_provider_t *provider = esp_mcp_completion_provider_create(test_completion_cb, NULL);
    TEST_ASSERT_NOT_NULL(provider);
    ESP_ERROR_CHECK(esp_mcp_set_completion_provider(mcp, provider));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    const char *init_msg = MCP_TEST_INIT_DEFAULT_REQ_JSON;
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)init_msg, strlen(init_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_FALSE(is_error);

    cJSON *result = cJSON_GetObjectItem(root, "result");
    TEST_ASSERT_NOT_NULL(result);

    cJSON *capabilities = cJSON_GetObjectItem(result, "capabilities");
    TEST_ASSERT_NOT_NULL(capabilities);
    TEST_ASSERT_TRUE(cJSON_IsObject(capabilities));

    cJSON *completions = cJSON_GetObjectItem(capabilities, "completions");
    TEST_ASSERT_NOT_NULL(completions);
    TEST_ASSERT_TRUE(cJSON_IsObject(completions));

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_initialize_response_server_info", "[mcp][protocol][initialize]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    const char *init_msg = MCP_TEST_INIT_DEFAULT_REQ_JSON;
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)init_msg, strlen(init_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_FALSE(is_error);

    cJSON *result = cJSON_GetObjectItem(root, "result");
    TEST_ASSERT_NOT_NULL(result);

    cJSON *server_info = cJSON_GetObjectItem(result, "serverInfo");
    TEST_ASSERT_NOT_NULL(server_info);
    TEST_ASSERT_TRUE(cJSON_IsObject(server_info));

    cJSON *name = cJSON_GetObjectItem(server_info, "name");
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_TRUE(cJSON_IsString(name));
    TEST_ASSERT_NOT_EMPTY(name->valuestring);

    cJSON *version = cJSON_GetObjectItem(server_info, "version");
    TEST_ASSERT_NOT_NULL(version);
    TEST_ASSERT_TRUE(cJSON_IsString(version));
    TEST_ASSERT_NOT_EMPTY(version->valuestring);

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_initialize_with_client_capabilities", "[mcp][protocol][initialize]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    const char *init_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2024-11-05\",\"clientInfo\":{\"name\":\"test_client\",\"version\":\"1.0\"},\"capabilities\":{\"vision\":{\"url\":\"http://example.com/vision\",\"token\":\"test_token_12345\"}}},\"id\":1}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)init_msg, strlen(init_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_FALSE(is_error);

    cJSON *result = cJSON_GetObjectItem(root, "result");
    TEST_ASSERT_NOT_NULL(result);

    cJSON *protocol_version = cJSON_GetObjectItem(result, "protocolVersion");
    TEST_ASSERT_NOT_NULL(protocol_version);
    TEST_ASSERT_EQUAL_STRING("2024-11-05", protocol_version->valuestring);

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

// ========== JSON-RPC 2.0 Specification Tests ==========

TEST_CASE("test_jsonrpc_version_in_requests", "[mcp][protocol][jsonrpc]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    const char *init_msg = MCP_TEST_INIT_DEFAULT_REQ_JSON;
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)init_msg, strlen(init_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);

    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_FALSE(is_error);
    TEST_ASSERT_TRUE(validate_jsonrpc_field(root, "2.0"));

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_jsonrpc_version_in_responses", "[mcp][protocol][jsonrpc]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_tool_t *tool = esp_mcp_tool_create("test_tool", "Test tool", test_tool_callback_bool);
    TEST_ASSERT_NOT_NULL(tool);
    ESP_ERROR_CHECK(esp_mcp_add_tool(mcp, tool));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    const char *tools_list_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"tools/list\",\"params\":{},\"id\":2}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)tools_list_msg, strlen(tools_list_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);

    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_FALSE(is_error);
    TEST_ASSERT_TRUE(validate_jsonrpc_field(root, "2.0"));

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_request_response_id_matching", "[mcp][protocol][jsonrpc]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    int request_id = 42;
    char init_msg[256];
    snprintf(init_msg, sizeof(init_msg),
             "{\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"params\":"
             MCP_TEST_INIT_DEFAULT_PARAMS_JSON ",\"id\":%d}",
             request_id);

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)init_msg, strlen(init_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    cJSON *root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(root);

    cJSON *id = cJSON_GetObjectItem(root, "id");
    TEST_ASSERT_NOT_NULL(id);
    TEST_ASSERT_TRUE(cJSON_IsNumber(id));
    TEST_ASSERT_EQUAL_INT(request_id, id->valueint);

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_result_error_mutually_exclusive", "[mcp][protocol][jsonrpc]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    const char *valid_msg = MCP_TEST_INIT_DEFAULT_REQ_JSON;
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)valid_msg, strlen(valid_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    cJSON *root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(root);

    cJSON *result = cJSON_GetObjectItem(root, "result");
    cJSON *error = cJSON_GetObjectItem(root, "error");

    TEST_ASSERT_TRUE((result != NULL) ^ (error != NULL));

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

// ========== tools/list Complete Tests ==========

TEST_CASE("test_tools_list_with_cursor", "[mcp][protocol][tools][list]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    const char *tools_list_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"tools/list\",\"params\":{\"cursor\":\"\"},\"id\":2}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)tools_list_msg, strlen(tools_list_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_FALSE(is_error);

    cJSON *result = cJSON_GetObjectItem(root, "result");
    TEST_ASSERT_NOT_NULL(result);

    cJSON *tools = cJSON_GetObjectItem(result, "tools");
    TEST_ASSERT_NOT_NULL(tools);
    TEST_ASSERT_TRUE(cJSON_IsArray(tools));

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_tools_list_pagination", "[mcp][protocol][tools][list]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    const char *tools_list_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"tools/list\",\"params\":{},\"id\":2}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)tools_list_msg, strlen(tools_list_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_FALSE(is_error);

    cJSON *result = cJSON_GetObjectItem(root, "result");
    TEST_ASSERT_NOT_NULL(result);

    cJSON *tools = cJSON_GetObjectItem(result, "tools");
    TEST_ASSERT_NOT_NULL(tools);
    TEST_ASSERT_TRUE(cJSON_IsArray(tools));

    cJSON *next_cursor = cJSON_GetObjectItem(result, "nextCursor");
    if (next_cursor && cJSON_IsString(next_cursor) && strlen(next_cursor->valuestring) > 0) {
        ESP_LOGI(TAG, "Pagination detected, nextCursor: %s", next_cursor->valuestring);
    }

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_tools_list_next_cursor", "[mcp][protocol][tools][list]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    for (int i = 0; i < 5; i++) {
        char tool_name[64];
        snprintf(tool_name, sizeof(tool_name), "test_tool_%d", i);

        esp_mcp_tool_t *tool = esp_mcp_tool_create(tool_name, "Test tool", test_tool_callback_bool);
        TEST_ASSERT_NOT_NULL(tool);
        ESP_ERROR_CHECK(esp_mcp_add_tool(mcp, tool));
    }

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    const char *tools_list_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"tools/list\",\"params\":{},\"id\":2}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)tools_list_msg, strlen(tools_list_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_FALSE(is_error);

    cJSON *result = cJSON_GetObjectItem(root, "result");
    TEST_ASSERT_NOT_NULL(result);

    cJSON *tools = cJSON_GetObjectItem(result, "tools");
    TEST_ASSERT_NOT_NULL(tools);
    TEST_ASSERT_TRUE(cJSON_IsArray(tools));

    int tool_count = cJSON_GetArraySize(tools);
    TEST_ASSERT_GREATER_THAN_INT(0, tool_count);

    if (tool_count > 0) {
        ESP_LOGI(TAG, "Tools list returned %d tools", tool_count);
    }

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_tools_list_invalid_cursor", "[mcp][protocol][tools][list]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    const char *tools_list_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"tools/list\",\"params\":{\"cursor\":\"nonexistent_tool\"},\"id\":2}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)tools_list_msg, strlen(tools_list_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_TRUE(is_error);
    assert_error_code(root, -32602);

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_tools_list_invalid_cursor_type", "[mcp][protocol][tools][list]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    const char *tools_list_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"tools/list\",\"params\":{\"cursor\":1},\"id\":2}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)tools_list_msg, strlen(tools_list_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_TRUE(is_error);
    assert_error_code(root, -32602);

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_tools_list_limit_pagination", "[mcp][protocol][tools][list]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    for (int i = 0; i < 4; i++) {
        char tool_name[64];
        snprintf(tool_name, sizeof(tool_name), "paglim_%d", i);
        esp_mcp_tool_t *tool = esp_mcp_tool_create(tool_name, "t", test_tool_callback_bool);
        TEST_ASSERT_NOT_NULL(tool);
        ESP_ERROR_CHECK(esp_mcp_add_tool(mcp, tool));
    }

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    const char *page1 =
        "{\"jsonrpc\":\"2.0\",\"method\":\"tools/list\",\"params\":{\"limit\":2},\"id\":2}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)page1, strlen(page1), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_FALSE(is_error);
    cJSON *result = cJSON_GetObjectItem(root, "result");
    TEST_ASSERT_NOT_NULL(result);
    cJSON *tools = cJSON_GetObjectItem(result, "tools");
    TEST_ASSERT_NOT_NULL(tools);
    TEST_ASSERT_EQUAL_INT(2, cJSON_GetArraySize(tools));
    cJSON *t0 = cJSON_GetArrayItem(tools, 0);
    TEST_ASSERT_EQUAL_STRING("paglim_0", cJSON_GetObjectItem(t0, "name")->valuestring);
    cJSON *t1 = cJSON_GetArrayItem(tools, 1);
    TEST_ASSERT_EQUAL_STRING("paglim_1", cJSON_GetObjectItem(t1, "name")->valuestring);
    cJSON *nc = cJSON_GetObjectItem(result, "nextCursor");
    TEST_ASSERT_NOT_NULL(nc);
    TEST_ASSERT_TRUE(cJSON_IsString(nc));
    TEST_ASSERT_EQUAL_STRING("paglim_2", nc->valuestring);
    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);

    char page2[256];
    snprintf(page2,
             sizeof(page2),
             "{\"jsonrpc\":\"2.0\",\"method\":\"tools/list\",\"params\":{\"cursor\":\"paglim_2\",\"limit\":2},\"id\":3}");
    outbuf = NULL;
    outlen = 0;
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)page2, strlen(page2), &outbuf, &outlen));
    root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_FALSE(is_error);
    result = cJSON_GetObjectItem(root, "result");
    tools = cJSON_GetObjectItem(result, "tools");
    TEST_ASSERT_EQUAL_INT(2, cJSON_GetArraySize(tools));
    TEST_ASSERT_EQUAL_STRING("paglim_2", cJSON_GetObjectItem(cJSON_GetArrayItem(tools, 0), "name")->valuestring);
    TEST_ASSERT_EQUAL_STRING("paglim_3", cJSON_GetObjectItem(cJSON_GetArrayItem(tools, 1), "name")->valuestring);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(result, "nextCursor"));
    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_tools_list_invalid_limit_type", "[mcp][protocol][tools][list]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    const char *tools_list_msg =
        "{\"jsonrpc\":\"2.0\",\"method\":\"tools/list\",\"params\":{\"limit\":\"2\"},\"id\":2}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)tools_list_msg, strlen(tools_list_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_TRUE(is_error);
    assert_error_code(root, -32602);

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_tools_list_invalid_limit_non_positive", "[mcp][protocol][tools][list]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    const char *tools_list_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"tools/list\",\"params\":{\"limit\":0},\"id\":2}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)tools_list_msg, strlen(tools_list_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_TRUE(is_error);
    assert_error_code(root, -32602);

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

// ========== tools/call Deep Tests ==========

TEST_CASE("test_tools_call_parameter_validation", "[mcp][protocol][tools][call]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_tool_t *tool = esp_mcp_tool_create("test_tool", "Test tool", test_tool_callback_bool);
    TEST_ASSERT_NOT_NULL(tool);

    esp_mcp_property_t *prop = esp_mcp_property_create_with_bool("enabled", true);
    TEST_ASSERT_NOT_NULL(prop);
    ESP_ERROR_CHECK(esp_mcp_tool_add_property(tool, prop));

    ESP_ERROR_CHECK(esp_mcp_add_tool(mcp, tool));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    const char *tools_call_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"test_tool\",\"arguments\":{\"enabled\":true}},\"id\":3}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)tools_call_msg, strlen(tools_call_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_FALSE(is_error);

    cJSON *result = cJSON_GetObjectItem(root, "result");
    TEST_ASSERT_NOT_NULL(result);

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_tools_call_parameter_range_check", "[mcp][protocol][tools][call]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_tool_t *tool = esp_mcp_tool_create("test_tool", "Test tool", test_tool_callback_int);
    TEST_ASSERT_NOT_NULL(tool);

    esp_mcp_property_t *prop = esp_mcp_property_create_with_int_and_range("count", 50, 0, 100);
    TEST_ASSERT_NOT_NULL(prop);
    ESP_ERROR_CHECK(esp_mcp_tool_add_property(tool, prop));

    ESP_ERROR_CHECK(esp_mcp_add_tool(mcp, tool));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    const char *tools_call_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"test_tool\",\"arguments\":{\"count\":50}},\"id\":3}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)tools_call_msg, strlen(tools_call_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_FALSE(is_error);

    cJSON *result = cJSON_GetObjectItem(root, "result");
    TEST_ASSERT_NOT_NULL(result);

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_tools_call_parameter_out_of_range", "[mcp][protocol][tools][call]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_tool_t *tool = esp_mcp_tool_create("test_tool", "Test tool", test_tool_callback_int);
    TEST_ASSERT_NOT_NULL(tool);

    esp_mcp_property_t *prop = esp_mcp_property_create_with_int_and_range("count", 50, 0, 100);
    TEST_ASSERT_NOT_NULL(prop);
    ESP_ERROR_CHECK(esp_mcp_tool_add_property(tool, prop));

    ESP_ERROR_CHECK(esp_mcp_add_tool(mcp, tool));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    const char *tools_call_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"test_tool\",\"arguments\":{\"count\":150}},\"id\":3}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)tools_call_msg, strlen(tools_call_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);

    cJSON *error = cJSON_GetObjectItem(root, "error");
    TEST_ASSERT_NOT_NULL(error);

    cJSON *code = cJSON_GetObjectItem(error, "code");
    TEST_ASSERT_NOT_NULL(code);
    TEST_ASSERT_TRUE(cJSON_IsNumber(code));
    TEST_ASSERT_TRUE(code->valueint == -32602 || code->valueint == -32002);

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_tools_call_parameter_float", "[mcp][protocol][tools][call]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_tool_t *tool = esp_mcp_tool_create("test_tool", "Test tool", test_tool_callback_float);
    TEST_ASSERT_NOT_NULL(tool);

    esp_mcp_property_t *prop = esp_mcp_property_create_with_float("temperature", 25.5f);
    TEST_ASSERT_NOT_NULL(prop);
    ESP_ERROR_CHECK(esp_mcp_tool_add_property(tool, prop));

    ESP_ERROR_CHECK(esp_mcp_add_tool(mcp, tool));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    const char *tools_call_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"test_tool\",\"arguments\":{\"temperature\":36.5}},\"id\":3}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)tools_call_msg, strlen(tools_call_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_FALSE(is_error);

    cJSON *result = cJSON_GetObjectItem(root, "result");
    TEST_ASSERT_NOT_NULL(result);
    cJSON *content = cJSON_GetObjectItem(result, "content");
    TEST_ASSERT_NOT_NULL(content);
    TEST_ASSERT_TRUE(cJSON_IsArray(content));
    TEST_ASSERT_TRUE(cJSON_GetArraySize(content) >= 1);
    cJSON *first = cJSON_GetArrayItem(content, 0);
    TEST_ASSERT_NOT_NULL(first);
    cJSON *text = cJSON_GetObjectItem(first, "text");
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_TRUE(cJSON_IsString(text));
    TEST_ASSERT_EQUAL_STRING("37.5", text->valuestring);

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_tools_call_parameter_string", "[mcp][protocol][tools][call]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_tool_t *tool = esp_mcp_tool_create("test_tool", "Test tool", test_tool_callback_string);
    TEST_ASSERT_NOT_NULL(tool);

    esp_mcp_property_t *prop = esp_mcp_property_create_with_string("message", "default");
    TEST_ASSERT_NOT_NULL(prop);
    ESP_ERROR_CHECK(esp_mcp_tool_add_property(tool, prop));

    ESP_ERROR_CHECK(esp_mcp_add_tool(mcp, tool));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    const char *tools_call_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"test_tool\",\"arguments\":{\"message\":\"hello\"}},\"id\":3}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)tools_call_msg, strlen(tools_call_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_FALSE(is_error);

    cJSON *result = cJSON_GetObjectItem(root, "result");
    TEST_ASSERT_NOT_NULL(result);
    cJSON *content = cJSON_GetObjectItem(result, "content");
    TEST_ASSERT_NOT_NULL(content);
    TEST_ASSERT_TRUE(cJSON_IsArray(content));
    TEST_ASSERT_TRUE(cJSON_GetArraySize(content) >= 1);
    cJSON *first = cJSON_GetArrayItem(content, 0);
    TEST_ASSERT_NOT_NULL(first);
    cJSON *text = cJSON_GetObjectItem(first, "text");
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_TRUE(cJSON_IsString(text));
    TEST_ASSERT_EQUAL_STRING("Echo: hello", text->valuestring);

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_tools_call_stacksize", "[mcp][protocol][tools][call]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_tool_t *tool = esp_mcp_tool_create("test_tool", "Test tool", test_tool_callback_bool);
    TEST_ASSERT_NOT_NULL(tool);

    esp_mcp_property_t *prop = esp_mcp_property_create_with_bool("enabled", true);
    TEST_ASSERT_NOT_NULL(prop);
    ESP_ERROR_CHECK(esp_mcp_tool_add_property(tool, prop));

    ESP_ERROR_CHECK(esp_mcp_add_tool(mcp, tool));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    const char *tools_call_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"test_tool\",\"arguments\":{\"enabled\":true},\"stackSize\":8192},\"id\":3}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)tools_call_msg, strlen(tools_call_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_FALSE(is_error);

    cJSON *result = cJSON_GetObjectItem(root, "result");
    TEST_ASSERT_NOT_NULL(result);

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_tools_call_nonexistent_tool", "[mcp][protocol][tools][call]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    const char *tools_call_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"nonexistent_tool\",\"arguments\":{}},\"id\":3}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)tools_call_msg, strlen(tools_call_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);

    cJSON *error = cJSON_GetObjectItem(root, "error");
    TEST_ASSERT_NOT_NULL(error);

    cJSON *code = cJSON_GetObjectItem(error, "code");
    TEST_ASSERT_NOT_NULL(code);
    TEST_ASSERT_TRUE(cJSON_IsNumber(code));
    TEST_ASSERT_TRUE(code->valueint == -32602 || code->valueint == -32002);

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_tools_call_missing_parameter", "[mcp][protocol][tools][call]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_tool_t *tool = esp_mcp_tool_create("test_tool", "Test tool", test_tool_callback_bool);
    TEST_ASSERT_NOT_NULL(tool);

    esp_mcp_property_t *prop = esp_mcp_property_create_with_bool("enabled", true);
    TEST_ASSERT_NOT_NULL(prop);
    ESP_ERROR_CHECK(esp_mcp_tool_add_property(tool, prop));

    ESP_ERROR_CHECK(esp_mcp_add_tool(mcp, tool));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    const char *tools_call_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"test_tool\",\"arguments\":{}},\"id\":3}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)tools_call_msg, strlen(tools_call_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);

    cJSON *error = cJSON_GetObjectItem(root, "error");
    TEST_ASSERT_NOT_NULL(error);

    cJSON *code = cJSON_GetObjectItem(error, "code");
    TEST_ASSERT_NOT_NULL(code);
    TEST_ASSERT_TRUE(cJSON_IsNumber(code));
    TEST_ASSERT_EQUAL_INT(-32602, code->valueint);

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_tools_call_invalid_stacksize", "[mcp][protocol][tools][call]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_tool_t *tool = esp_mcp_tool_create("test_tool", "Test tool", test_tool_callback_bool);
    TEST_ASSERT_NOT_NULL(tool);

    esp_mcp_property_t *prop = esp_mcp_property_create_with_bool("enabled", true);
    TEST_ASSERT_NOT_NULL(prop);
    ESP_ERROR_CHECK(esp_mcp_tool_add_property(tool, prop));

    ESP_ERROR_CHECK(esp_mcp_add_tool(mcp, tool));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    const char *tools_call_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"test_tool\",\"arguments\":{\"enabled\":true},\"stackSize\":-1},\"id\":3}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)tools_call_msg, strlen(tools_call_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);

    cJSON *error = cJSON_GetObjectItem(root, "error");
    TEST_ASSERT_NOT_NULL(error);

    cJSON *code = cJSON_GetObjectItem(error, "code");
    TEST_ASSERT_NOT_NULL(code);
    TEST_ASSERT_TRUE(cJSON_IsNumber(code));
    TEST_ASSERT_EQUAL_INT(-32602, code->valueint);

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

// ========== Error Handling Complete Tests ==========

TEST_CASE("test_error_parse_error", "[mcp][protocol][error]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    const char *invalid_json = "{invalid json}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    esp_err_t ret = esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)invalid_json, strlen(invalid_json), &outbuf, &outlen);
    TEST_ASSERT_TRUE(ret == ESP_OK || ret == ESP_ERR_INVALID_ARG);
    TEST_ASSERT_NOT_NULL(outbuf);

    cJSON *root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(root);

    cJSON *error = cJSON_GetObjectItem(root, "error");
    TEST_ASSERT_NOT_NULL(error);

    cJSON *code = cJSON_GetObjectItem(error, "code");
    TEST_ASSERT_NOT_NULL(code);
    TEST_ASSERT_TRUE(cJSON_IsNumber(code));
    TEST_ASSERT_EQUAL_INT(-32700, code->valueint);

    cJSON *message = cJSON_GetObjectItem(error, "message");
    TEST_ASSERT_NOT_NULL(message);
    TEST_ASSERT_TRUE(cJSON_IsString(message));

    cJSON *id = cJSON_GetObjectItem(root, "id");
    TEST_ASSERT_NOT_NULL(id);
    TEST_ASSERT_TRUE(cJSON_IsNull(id));

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_error_invalid_request", "[mcp][protocol][error]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    const char *invalid_request = "{\"jsonrpc\":\"1.0\",\"method\":\"initialize\",\"params\":{},\"id\":1}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    esp_err_t ret = esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)invalid_request, strlen(invalid_request), &outbuf, &outlen);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
    TEST_ASSERT_NOT_NULL(outbuf);

    cJSON *root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(root);

    cJSON *error = cJSON_GetObjectItem(root, "error");
    TEST_ASSERT_NOT_NULL(error);

    cJSON *code = cJSON_GetObjectItem(error, "code");
    TEST_ASSERT_NOT_NULL(code);
    TEST_ASSERT_TRUE(cJSON_IsNumber(code));
    TEST_ASSERT_EQUAL_INT(-32600, code->valueint);

    cJSON *id = cJSON_GetObjectItem(root, "id");
    TEST_ASSERT_NOT_NULL(id);
    TEST_ASSERT_TRUE(cJSON_IsNull(id));

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_error_method_not_found", "[mcp][protocol][error]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    const char *invalid_method = "{\"jsonrpc\":\"2.0\",\"method\":\"nonexistent_method\",\"params\":{},\"id\":1}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)invalid_method, strlen(invalid_method), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_TRUE(is_error);

    cJSON *error = cJSON_GetObjectItem(root, "error");
    TEST_ASSERT_NOT_NULL(error);

    cJSON *code = cJSON_GetObjectItem(error, "code");
    TEST_ASSERT_NOT_NULL(code);
    TEST_ASSERT_TRUE(cJSON_IsNumber(code));
    TEST_ASSERT_TRUE(code->valueint == -32601 || code->valueint == -32600);

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_error_invalid_params", "[mcp][protocol][error]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    const char *invalid_params = "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{},\"id\":3}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)invalid_params, strlen(invalid_params), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_TRUE(is_error);

    cJSON *error = cJSON_GetObjectItem(root, "error");
    TEST_ASSERT_NOT_NULL(error);

    cJSON *code = cJSON_GetObjectItem(error, "code");
    TEST_ASSERT_NOT_NULL(code);
    TEST_ASSERT_TRUE(cJSON_IsNumber(code));
    TEST_ASSERT_EQUAL_INT(-32602, code->valueint);

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_error_response_format", "[mcp][protocol][error]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    const char *invalid_request = "{\"jsonrpc\":\"1.0\",\"method\":\"initialize\",\"params\":{},\"id\":1}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    esp_err_t ret = esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)invalid_request, strlen(invalid_request), &outbuf, &outlen);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
    TEST_ASSERT_NOT_NULL(outbuf);

    cJSON *root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(root);

    cJSON *error = cJSON_GetObjectItem(root, "error");
    TEST_ASSERT_NOT_NULL(error);
    TEST_ASSERT_TRUE(cJSON_IsObject(error));

    cJSON *code = cJSON_GetObjectItem(error, "code");
    TEST_ASSERT_NOT_NULL(code);
    TEST_ASSERT_TRUE(cJSON_IsNumber(code));

    cJSON *message = cJSON_GetObjectItem(error, "message");
    TEST_ASSERT_NOT_NULL(message);
    TEST_ASSERT_TRUE(cJSON_IsString(message));
    TEST_ASSERT_NOT_EMPTY(message->valuestring);

    cJSON *result = cJSON_GetObjectItem(root, "result");
    TEST_ASSERT_NULL(result);

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_error_custom_codes", "[mcp][protocol][error]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    const char *tools_call_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"nonexistent_tool\",\"arguments\":{}},\"id\":3}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)tools_call_msg, strlen(tools_call_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_TRUE(is_error);

    cJSON *error = cJSON_GetObjectItem(root, "error");
    TEST_ASSERT_NOT_NULL(error);

    cJSON *code = cJSON_GetObjectItem(error, "code");
    TEST_ASSERT_NOT_NULL(code);
    TEST_ASSERT_TRUE(cJSON_IsNumber(code));

    TEST_ASSERT_TRUE(code->valueint == -32602 || (code->valueint >= -32099 && code->valueint <= -32000));

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

// ========== Response Parsing Tests ==========

TEST_CASE("test_response_parse_result", "[mcp][protocol][response]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    const char *init_msg = MCP_TEST_INIT_DEFAULT_REQ_JSON;
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)init_msg, strlen(init_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_FALSE(is_error);

    cJSON *result = cJSON_GetObjectItem(root, "result");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE(cJSON_IsObject(result));

    cJSON *error = cJSON_GetObjectItem(root, "error");
    TEST_ASSERT_NULL(error);

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_response_parse_error", "[mcp][protocol][response]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    const char *invalid_request = "{\"jsonrpc\":\"1.0\",\"method\":\"initialize\",\"params\":{},\"id\":1}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    esp_err_t ret = esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)invalid_request, strlen(invalid_request), &outbuf, &outlen);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
    TEST_ASSERT_NOT_NULL(outbuf);

    cJSON *root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(root);

    cJSON *error = cJSON_GetObjectItem(root, "error");
    TEST_ASSERT_NOT_NULL(error);
    TEST_ASSERT_TRUE(cJSON_IsObject(error));

    cJSON *code = cJSON_GetObjectItem(error, "code");
    TEST_ASSERT_NOT_NULL(code);
    TEST_ASSERT_TRUE(cJSON_IsNumber(code));

    cJSON *message = cJSON_GetObjectItem(error, "message");
    TEST_ASSERT_NOT_NULL(message);
    TEST_ASSERT_TRUE(cJSON_IsString(message));

    cJSON *result = cJSON_GetObjectItem(root, "result");
    TEST_ASSERT_NULL(result);

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_response_parse_is_error", "[mcp][protocol][response]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_tool_t *tool = esp_mcp_tool_create("test_tool", "Test tool", test_tool_callback_bool);
    TEST_ASSERT_NOT_NULL(tool);

    esp_mcp_property_t *prop = esp_mcp_property_create_with_bool("enabled", true);
    TEST_ASSERT_NOT_NULL(prop);
    ESP_ERROR_CHECK(esp_mcp_tool_add_property(tool, prop));

    ESP_ERROR_CHECK(esp_mcp_add_tool(mcp, tool));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    const char *tools_call_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"test_tool\",\"arguments\":{\"enabled\":true}},\"id\":3}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)tools_call_msg, strlen(tools_call_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_FALSE(is_error);

    cJSON *result = cJSON_GetObjectItem(root, "result");
    TEST_ASSERT_NOT_NULL(result);

    cJSON *is_error_obj = cJSON_GetObjectItem(result, "isError");
    if (is_error_obj && cJSON_IsBool(is_error_obj)) {
        bool app_error = cJSON_IsTrue(is_error_obj);
        TEST_ASSERT_FALSE(app_error);
    }

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_response_parse_invalid_json", "[mcp][protocol][response]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    const char *invalid_json = "{not json}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)invalid_json, strlen(invalid_json), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    cJSON *root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(root);

    cJSON *error = cJSON_GetObjectItem(root, "error");
    TEST_ASSERT_NOT_NULL(error);

    cJSON *code = cJSON_GetObjectItem(error, "code");
    TEST_ASSERT_NOT_NULL(code);
    TEST_ASSERT_TRUE(cJSON_IsNumber(code));
    TEST_ASSERT_EQUAL_INT(-32700, code->valueint);

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

// ========== ping Method Tests ==========

TEST_CASE("test_ping_response", "[mcp][protocol][ping]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };

    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    const char *ping_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"ping\",\"params\":{},\"id\":99}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)ping_msg, strlen(ping_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_FALSE(is_error);

    cJSON *result = cJSON_GetObjectItem(root, "result");
    TEST_ASSERT_NOT_NULL(result);

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

// ========== MCP 2025-11-25 New Feature Tests ==========

TEST_CASE("test_protocol_2025_resources_list_read", "[mcp][protocol][2025][resources]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_resource_t *res = esp_mcp_resource_create("device://status", "device.status", "Device Status",
                                                      "status resource", "application/json", test_resource_read_cb, NULL);
    TEST_ASSERT_NOT_NULL(res);
    ESP_ERROR_CHECK(esp_mcp_resource_set_annotations(res, "[\"assistant\"]", 0.5, "2026-01-01T00:00:00Z"));
    ESP_ERROR_CHECK(esp_mcp_add_resource(mcp, res));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *list_msg = "{\"jsonrpc\":\"2.0\",\"id\":\"res-list\",\"method\":\"resources/list\",\"params\":{}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)list_msg, strlen(list_msg), &outbuf, &outlen));
    cJSON *list_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(list_root);
    cJSON *list_error = cJSON_GetObjectItem(list_root, "error");
    TEST_ASSERT_NULL(list_error);
    cJSON *list_result = cJSON_GetObjectItem(list_root, "result");
    TEST_ASSERT_NOT_NULL(list_result);
    cJSON *resources = cJSON_GetObjectItem(list_result, "resources");
    TEST_ASSERT_NOT_NULL(resources);
    TEST_ASSERT_TRUE(cJSON_IsArray(resources));
    TEST_ASSERT_GREATER_THAN_INT(0, cJSON_GetArraySize(resources));
    cJSON_Delete(list_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    const char *read_msg = "{\"jsonrpc\":\"2.0\",\"id\":\"res-read\",\"method\":\"resources/read\",\"params\":{\"uri\":\"device://status\"}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)read_msg, strlen(read_msg), &outbuf, &outlen));
    cJSON *read_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(read_root);
    cJSON *read_error = cJSON_GetObjectItem(read_root, "error");
    TEST_ASSERT_NULL(read_error);
    cJSON *read_result = cJSON_GetObjectItem(read_root, "result");
    TEST_ASSERT_NOT_NULL(read_result);
    cJSON *contents = cJSON_GetObjectItem(read_result, "contents");
    TEST_ASSERT_NOT_NULL(contents);
    TEST_ASSERT_TRUE(cJSON_IsArray(contents));
    TEST_ASSERT_GREATER_THAN_INT(0, cJSON_GetArraySize(contents));
    cJSON_Delete(read_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    ESP_ERROR_CHECK(esp_mcp_remove_resource(mcp, res));
    ESP_ERROR_CHECK(esp_mcp_resource_destroy(res));
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_protocol_2025_prompts_list_get", "[mcp][protocol][2025][prompts]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_prompt_t *prompt = esp_mcp_prompt_create("status.summary", "Status Summary", "Prompt for status",
                                                     "[{\"role\":\"user\",\"content\":{\"type\":\"text\",\"text\":\"status summary\"}}]",
                                                     NULL, NULL);
    TEST_ASSERT_NOT_NULL(prompt);
    ESP_ERROR_CHECK(esp_mcp_prompt_set_annotations(prompt, "[\"assistant\"]", 0.7, "2026-01-01T00:00:00Z"));
    ESP_ERROR_CHECK(esp_mcp_add_prompt(mcp, prompt));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *list_msg = "{\"jsonrpc\":\"2.0\",\"id\":\"prompt-list\",\"method\":\"prompts/list\",\"params\":{}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)list_msg, strlen(list_msg), &outbuf, &outlen));
    cJSON *list_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(list_root);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(list_root, "error"));
    cJSON *list_result = cJSON_GetObjectItem(list_root, "result");
    TEST_ASSERT_NOT_NULL(list_result);
    cJSON *prompts = cJSON_GetObjectItem(list_result, "prompts");
    TEST_ASSERT_NOT_NULL(prompts);
    TEST_ASSERT_TRUE(cJSON_IsArray(prompts));
    TEST_ASSERT_GREATER_THAN_INT(0, cJSON_GetArraySize(prompts));
    cJSON_Delete(list_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    const char *get_msg = "{\"jsonrpc\":\"2.0\",\"id\":\"prompt-get\",\"method\":\"prompts/get\",\"params\":{\"name\":\"status.summary\",\"arguments\":{\"device\":\"screen\"}}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)get_msg, strlen(get_msg), &outbuf, &outlen));
    cJSON *get_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(get_root);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(get_root, "error"));
    cJSON *get_result = cJSON_GetObjectItem(get_root, "result");
    TEST_ASSERT_NOT_NULL(get_result);
    cJSON *messages = cJSON_GetObjectItem(get_result, "messages");
    TEST_ASSERT_NOT_NULL(messages);
    TEST_ASSERT_TRUE(cJSON_IsArray(messages));
    TEST_ASSERT_GREATER_THAN_INT(0, cJSON_GetArraySize(messages));
    cJSON_Delete(get_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    ESP_ERROR_CHECK(esp_mcp_remove_prompt(mcp, prompt));
    ESP_ERROR_CHECK(esp_mcp_prompt_destroy(prompt));
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_protocol_2025_resources_list_pagination_and_invalid_cursor", "[mcp][protocol][2025][resources]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_resource_t *res_a = esp_mcp_resource_create("device://alpha", "device.alpha", "Alpha",
                                                        "alpha resource", "application/json", test_resource_read_cb, NULL);
    esp_mcp_resource_t *res_b = esp_mcp_resource_create("device://beta", "device.beta", "Beta",
                                                        "beta resource", "application/json", test_resource_read_cb, NULL);
    TEST_ASSERT_NOT_NULL(res_a);
    TEST_ASSERT_NOT_NULL(res_b);
    ESP_ERROR_CHECK(esp_mcp_add_resource(mcp, res_a));
    ESP_ERROR_CHECK(esp_mcp_add_resource(mcp, res_b));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *page1_msg = "{\"jsonrpc\":\"2.0\",\"id\":\"res-page1\",\"method\":\"resources/list\",\"params\":{\"limit\":1}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)page1_msg, strlen(page1_msg), &outbuf, &outlen));
    cJSON *page1_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(page1_root);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(page1_root, "error"));
    cJSON *page1_result = cJSON_GetObjectItem(page1_root, "result");
    TEST_ASSERT_NOT_NULL(page1_result);
    cJSON *page1_resources = cJSON_GetObjectItem(page1_result, "resources");
    TEST_ASSERT_TRUE(cJSON_IsArray(page1_resources));
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetArraySize(page1_resources));
    cJSON *page1_next_cursor = cJSON_GetObjectItem(page1_result, "nextCursor");
    TEST_ASSERT_TRUE(cJSON_IsString(page1_next_cursor));
    char page1_cursor[128] = {0};
    snprintf(page1_cursor, sizeof(page1_cursor), "%s", page1_next_cursor->valuestring);
    cJSON_Delete(page1_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    char page2_msg[256] = {0};
    snprintf(page2_msg, sizeof(page2_msg),
             "{\"jsonrpc\":\"2.0\",\"id\":\"res-page2\",\"method\":\"resources/list\",\"params\":{\"cursor\":\"%s\",\"limit\":1}}",
             page1_cursor);
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)page2_msg, strlen(page2_msg), &outbuf, &outlen));
    cJSON *page2_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(page2_root);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(page2_root, "error"));
    cJSON *page2_result = cJSON_GetObjectItem(page2_root, "result");
    TEST_ASSERT_NOT_NULL(page2_result);
    cJSON *page2_resources = cJSON_GetObjectItem(page2_result, "resources");
    TEST_ASSERT_TRUE(cJSON_IsArray(page2_resources));
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetArraySize(page2_resources));
    cJSON *page2_first = cJSON_GetArrayItem(page2_resources, 0);
    TEST_ASSERT_NOT_NULL(page2_first);
    cJSON *page2_uri = cJSON_GetObjectItem(page2_first, "uri");
    TEST_ASSERT_TRUE(cJSON_IsString(page2_uri));
    TEST_ASSERT_EQUAL_STRING(page1_cursor, page2_uri->valuestring);
    cJSON_Delete(page2_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    const char *invalid_cursor_msg =
        "{\"jsonrpc\":\"2.0\",\"id\":\"res-invalid\",\"method\":\"resources/list\",\"params\":{\"cursor\":\"missing-resource\"}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)invalid_cursor_msg, strlen(invalid_cursor_msg), &outbuf, &outlen));
    cJSON *invalid_cursor_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(invalid_cursor_root);
    assert_error_code(invalid_cursor_root, -32602);
    cJSON_Delete(invalid_cursor_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    const char *invalid_cursor_type_msg =
        "{\"jsonrpc\":\"2.0\",\"id\":\"res-invalid-type\",\"method\":\"resources/list\",\"params\":{\"cursor\":1}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)invalid_cursor_type_msg, strlen(invalid_cursor_type_msg), &outbuf, &outlen));
    cJSON *invalid_cursor_type_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(invalid_cursor_type_root);
    assert_error_code(invalid_cursor_type_root, -32602);
    cJSON_Delete(invalid_cursor_type_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    const char *invalid_limit_type_msg =
        "{\"jsonrpc\":\"2.0\",\"id\":\"res-invalid-limit\",\"method\":\"resources/list\",\"params\":{\"limit\":\"bad\"}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)invalid_limit_type_msg, strlen(invalid_limit_type_msg), &outbuf, &outlen));
    cJSON *invalid_limit_type_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(invalid_limit_type_root);
    assert_error_code(invalid_limit_type_root, -32602);
    cJSON_Delete(invalid_limit_type_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    ESP_ERROR_CHECK(esp_mcp_remove_resource(mcp, res_a));
    ESP_ERROR_CHECK(esp_mcp_remove_resource(mcp, res_b));
    ESP_ERROR_CHECK(esp_mcp_resource_destroy(res_a));
    ESP_ERROR_CHECK(esp_mcp_resource_destroy(res_b));
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_protocol_2025_completion_complete", "[mcp][protocol][2025][completion]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_completion_provider_t *provider = esp_mcp_completion_provider_create(test_completion_cb, NULL);
    TEST_ASSERT_NOT_NULL(provider);
    ESP_ERROR_CHECK(esp_mcp_set_completion_provider(mcp, provider));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *msg = "{\"jsonrpc\":\"2.0\",\"id\":\"completion-1\",\"method\":\"completion/complete\",\"params\":{\"ref\":{\"type\":\"ref/prompt\",\"name\":\"status.summary\"},\"argument\":{\"name\":\"device\",\"value\":\"s\"}}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)msg, strlen(msg), &outbuf, &outlen));
    cJSON *root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(root, "error"));
    cJSON *result = cJSON_GetObjectItem(root, "result");
    TEST_ASSERT_NOT_NULL(result);
    cJSON *completion = cJSON_GetObjectItem(result, "completion");
    TEST_ASSERT_NOT_NULL(completion);
    cJSON *values = cJSON_GetObjectItem(completion, "values");
    TEST_ASSERT_NOT_NULL(values);
    TEST_ASSERT_TRUE(cJSON_IsArray(values));
    TEST_ASSERT_GREATER_THAN_INT(0, cJSON_GetArraySize(values));
    cJSON_Delete(root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_protocol_2025_completion_complete_without_provider", "[mcp][protocol][2025][completion]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *msg = "{\"jsonrpc\":\"2.0\",\"id\":\"completion-missing-provider\",\"method\":\"completion/complete\",\"params\":{\"ref\":{\"type\":\"ref/prompt\",\"name\":\"status.summary\"},\"argument\":{\"name\":\"device\",\"value\":\"s\"}}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)msg, strlen(msg), &outbuf, &outlen));
    cJSON *root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(root);
    assert_error_code(root, -32601);
    cJSON_Delete(root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_protocol_2025_tasks_flow", "[mcp][protocol][2025][tasks]")
{
    relax_memory_check = true;
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_tool_t *tool = esp_mcp_tool_create("task.tool", "Task tool", test_tool_callback_noarg);
    TEST_ASSERT_NOT_NULL(tool);
    ESP_ERROR_CHECK(esp_mcp_add_tool(mcp, tool));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *call_msg =
        "{\"jsonrpc\":\"2.0\",\"id\":\"task-call\",\"method\":\"tools/call\","
        "\"params\":{\"name\":\"task.tool\",\"arguments\":{\"__simulateWorkMs\":200},\"task\":{\"ttl\":1500}}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)call_msg, strlen(call_msg), &outbuf, &outlen));
    cJSON *call_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(call_root);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(call_root, "error"));
    cJSON *call_result = cJSON_GetObjectItem(call_root, "result");
    TEST_ASSERT_NOT_NULL(call_result);
    cJSON *task_obj = cJSON_GetObjectItem(call_result, "task");
    TEST_ASSERT_NOT_NULL(task_obj);
    cJSON *task_id = cJSON_GetObjectItem(task_obj, "taskId");
    TEST_ASSERT_NOT_NULL(task_id);
    TEST_ASSERT_TRUE(cJSON_IsString(task_id));
    TEST_ASSERT_NOT_EMPTY(task_id->valuestring);
    assert_rfc3339_utc_field(task_obj, "createdAt");
    assert_rfc3339_utc_field(task_obj, "lastUpdatedAt");
    assert_task_ttl(task_obj, 1500);
    char task_id_buf[80] = {0};
    snprintf(task_id_buf, sizeof(task_id_buf), "%s", task_id->valuestring);
    cJSON_Delete(call_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    char get_msg[256] = {0};
    snprintf(get_msg, sizeof(get_msg), "{\"jsonrpc\":\"2.0\",\"id\":\"task-get\",\"method\":\"tasks/get\",\"params\":{\"taskId\":\"%s\"}}", task_id_buf);
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)get_msg, strlen(get_msg), &outbuf, &outlen));
    cJSON *get_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(get_root);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(get_root, "error"));
    cJSON *get_result = cJSON_GetObjectItem(get_root, "result");
    TEST_ASSERT_NOT_NULL(get_result);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(get_result, "_meta"));
    TEST_ASSERT_NULL(cJSON_GetObjectItem(get_result, "task"));
    cJSON *get_task_id = cJSON_GetObjectItem(get_result, "taskId");
    TEST_ASSERT_NOT_NULL(get_task_id);
    TEST_ASSERT_TRUE(cJSON_IsString(get_task_id));
    TEST_ASSERT_EQUAL_STRING(task_id_buf, get_task_id->valuestring);
    assert_rfc3339_utc_field(get_result, "createdAt");
    assert_rfc3339_utc_field(get_result, "lastUpdatedAt");
    assert_task_ttl(get_result, 1500);
    cJSON_Delete(get_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    char result_msg[256] = {0};
    snprintf(result_msg, sizeof(result_msg), "{\"jsonrpc\":\"2.0\",\"id\":\"task-result\",\"method\":\"tasks/result\",\"params\":{\"taskId\":\"%s\"}}", task_id_buf);
    TickType_t wait_start = xTaskGetTickCount();
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)result_msg, strlen(result_msg), &outbuf, &outlen));
    uint32_t wait_ms = (uint32_t)((xTaskGetTickCount() - wait_start) * portTICK_PERIOD_MS);
    cJSON *result_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(result_root);
    cJSON *result_result = cJSON_GetObjectItem(result_root, "result");
    TEST_ASSERT_NOT_NULL(result_result);
    assert_related_task_meta(result_result, task_id_buf);
    TEST_ASSERT_TRUE(wait_ms >= 150);
    cJSON_Delete(result_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_protocol_2025_tasks_cancel_unknown", "[mcp][protocol][2025][tasks]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *msg = "{\"jsonrpc\":\"2.0\",\"id\":\"task-cancel-missing\",\"method\":\"tasks/cancel\",\"params\":{\"taskId\":\"ffffffffffffffffffffffffffffffff\"}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)msg, strlen(msg), &outbuf, &outlen));
    cJSON *root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(root);
    assert_error_code(root, -32602);
    cJSON_Delete(root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_protocol_2025_tasks_cancel_related_meta", "[mcp][protocol][2025][tasks]")
{
    relax_memory_check = true;
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_tool_t *tool = esp_mcp_tool_create("task.tool", "Task tool", test_tool_callback_noarg);
    TEST_ASSERT_NOT_NULL(tool);
    ESP_ERROR_CHECK(esp_mcp_add_tool(mcp, tool));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *call_msg = "{\"jsonrpc\":\"2.0\",\"id\":\"task-call-cancel\",\"method\":\"tools/call\",\"params\":{\"name\":\"task.tool\",\"arguments\":{\"__simulateWorkMs\":1500},\"task\":{}}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)call_msg, strlen(call_msg), &outbuf, &outlen));
    cJSON *call_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(call_root);
    cJSON *call_result = cJSON_GetObjectItem(call_root, "result");
    TEST_ASSERT_NOT_NULL(call_result);
    cJSON *task_obj = cJSON_GetObjectItem(call_result, "task");
    TEST_ASSERT_NOT_NULL(task_obj);
    cJSON *task_id = cJSON_GetObjectItem(task_obj, "taskId");
    TEST_ASSERT_NOT_NULL(task_id);
    TEST_ASSERT_TRUE(cJSON_IsString(task_id));
    char task_id_buf[80] = {0};
    snprintf(task_id_buf, sizeof(task_id_buf), "%s", task_id->valuestring);
    cJSON_Delete(call_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    char cancel_msg[320] = {0};
    snprintf(cancel_msg, sizeof(cancel_msg), "{\"jsonrpc\":\"2.0\",\"id\":\"task-cancel\",\"method\":\"tasks/cancel\",\"params\":{\"taskId\":\"%s\"}}", task_id_buf);
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)cancel_msg, strlen(cancel_msg), &outbuf, &outlen));
    cJSON *cancel_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(cancel_root);
    cJSON *cancel_result = cJSON_GetObjectItem(cancel_root, "result");
    TEST_ASSERT_NOT_NULL(cancel_result);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(cancel_result, "_meta"));
    TEST_ASSERT_NULL(cJSON_GetObjectItem(cancel_result, "task"));
    cJSON *cancel_task_id = cJSON_GetObjectItem(cancel_result, "taskId");
    TEST_ASSERT_NOT_NULL(cancel_task_id);
    TEST_ASSERT_TRUE(cJSON_IsString(cancel_task_id));
    TEST_ASSERT_EQUAL_STRING(task_id_buf, cancel_task_id->valuestring);
    cJSON *cancel_status = cJSON_GetObjectItem(cancel_result, "status");
    TEST_ASSERT_NOT_NULL(cancel_status);
    TEST_ASSERT_TRUE(cJSON_IsString(cancel_status));
    TEST_ASSERT_EQUAL_STRING("cancelled", cancel_status->valuestring);
    assert_task_ttl_null(cancel_result);
    cJSON_Delete(cancel_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_protocol_2025_logging_set_level", "[mcp][protocol][2025][logging]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *msg = "{\"jsonrpc\":\"2.0\",\"id\":\"log-level\",\"method\":\"logging/setLevel\",\"params\":{\"level\":\"info\"}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)msg, strlen(msg), &outbuf, &outlen));
    cJSON *root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(root, "error"));
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(root, "result"));
    cJSON_Delete(root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    const char *bad_level =
        "{\"jsonrpc\":\"2.0\",\"id\":\"log-level-bad\",\"method\":\"logging/setLevel\",\"params\":{\"level\":\"verbose\"}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server",
                                               (const uint8_t *)bad_level,
                                               strlen(bad_level),
                                               &outbuf, &outlen));
    root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(root);
    assert_error_code(root, -32602);
    cJSON_Delete(root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_logging_notifications_respect_session_levels", "[mcp][protocol][2025][logging][session]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    mcp_test_send_initialize_with_capabilities_for_session(handle, "sess-a", NULL);
    mcp_test_send_initialize_with_capabilities_for_session(handle, "sess-b", NULL);

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *set_error =
        "{\"jsonrpc\":\"2.0\",\"id\":\"log-level-a\",\"method\":\"logging/setLevel\",\"params\":{\"level\":\"error\"}}";
    ESP_ERROR_CHECK(esp_mcp_mgr_set_request_context(handle, "sess-a", NULL, true));
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server",
                                               (const uint8_t *)set_error,
                                               strlen(set_error),
                                               &outbuf, &outlen));
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    const char *set_info =
        "{\"jsonrpc\":\"2.0\",\"id\":\"log-level-b\",\"method\":\"logging/setLevel\",\"params\":{\"level\":\"info\"}}";
    ESP_ERROR_CHECK(esp_mcp_mgr_set_request_context(handle, "sess-b", NULL, true));
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server",
                                               (const uint8_t *)set_info,
                                               strlen(set_info),
                                               &outbuf, &outlen));
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    ESP_ERROR_CHECK(esp_mcp_mgr_set_request_context(handle, NULL, NULL, true));
    mcp_test_reset_emit_capture();
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_notify_log_message(mcp, "warning", "ut", "{\"warning\":true}"));
    TEST_ASSERT_EQUAL_INT(1, s_emit_count);
    TEST_ASSERT_EQUAL_INT(-1, mcp_test_find_emit_session("sess-a"));
    int sess_b_warning = mcp_test_find_emit_session("sess-b");
    TEST_ASSERT_TRUE(sess_b_warning >= 0);
    TEST_ASSERT_NOT_NULL(strstr(s_emit_history[sess_b_warning], "\"level\":\"warning\""));

    mcp_test_reset_emit_capture();
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_notify_log_message(mcp, "error", "ut", "{\"error\":true}"));
    TEST_ASSERT_EQUAL_INT(2, s_emit_count);
    int sess_a_error = mcp_test_find_emit_session("sess-a");
    int sess_b_error = mcp_test_find_emit_session("sess-b");
    TEST_ASSERT_TRUE(sess_a_error >= 0);
    TEST_ASSERT_TRUE(sess_b_error >= 0);
    TEST_ASSERT_NOT_NULL(strstr(s_emit_history[sess_a_error], "\"level\":\"error\""));
    TEST_ASSERT_NOT_NULL(strstr(s_emit_history[sess_b_error], "\"level\":\"error\""));

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_lifecycle_remove_resource_after_mgr_deinit", "[mcp][lifecycle]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    esp_mcp_resource_t *res = esp_mcp_resource_create("device://status", "device.status", "Device Status",
                                                      "status resource", "application/json", test_resource_read_cb, NULL);
    TEST_ASSERT_NOT_NULL(res);
    ESP_ERROR_CHECK(esp_mcp_add_resource(mcp, res));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));

    /* Must not crash even if manager is already deinitialized. */
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_remove_resource(mcp, res));
    ESP_ERROR_CHECK(esp_mcp_resource_destroy(res));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_protocol_2025_resources_templates_list", "[mcp][protocol][2025][resources]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_resource_t *res = esp_mcp_resource_create("device://sensor/{id}", "sensor.by_id", "Sensor by ID",
                                                      "template resource", "application/json", test_resource_read_cb, NULL);
    TEST_ASSERT_NOT_NULL(res);
    ESP_ERROR_CHECK(esp_mcp_add_resource(mcp, res));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *msg = "{\"jsonrpc\":\"2.0\",\"id\":\"res-tpl\",\"method\":\"resources/templates/list\",\"params\":{}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)msg, strlen(msg), &outbuf, &outlen));
    cJSON *root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(root, "error"));
    cJSON *result = cJSON_GetObjectItem(root, "result");
    TEST_ASSERT_NOT_NULL(result);
    cJSON *templates = cJSON_GetObjectItem(result, "resourceTemplates");
    TEST_ASSERT_NOT_NULL(templates);
    TEST_ASSERT_TRUE(cJSON_IsArray(templates));
    TEST_ASSERT_GREATER_THAN_INT(0, cJSON_GetArraySize(templates));
    cJSON_Delete(root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    ESP_ERROR_CHECK(esp_mcp_remove_resource(mcp, res));
    ESP_ERROR_CHECK(esp_mcp_resource_destroy(res));
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_protocol_2025_resources_templates_list_pagination", "[mcp][protocol][2025][resources]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_resource_t *res_plain = esp_mcp_resource_create("device://status", "device.status", "Device Status",
                                                            "non template resource", "application/json", test_resource_read_cb, NULL);
    esp_mcp_resource_t *res_tpl_1 = esp_mcp_resource_create("device://sensor/{id}", "sensor.by_id", "Sensor by ID",
                                                            "template resource 1", "application/json", test_resource_read_cb, NULL);
    esp_mcp_resource_t *res_tpl_2 = esp_mcp_resource_create("device://room/{roomId}", "room.by_id", "Room by ID",
                                                            "template resource 2", "application/json", test_resource_read_cb, NULL);
    esp_mcp_resource_t *res_tpl_3 = esp_mcp_resource_create("device://zone/{zoneId}", "zone.by_id", "Zone by ID",
                                                            "template resource 3", "application/json", test_resource_read_cb, NULL);
    TEST_ASSERT_NOT_NULL(res_plain);
    TEST_ASSERT_NOT_NULL(res_tpl_1);
    TEST_ASSERT_NOT_NULL(res_tpl_2);
    TEST_ASSERT_NOT_NULL(res_tpl_3);
    ESP_ERROR_CHECK(esp_mcp_add_resource(mcp, res_plain));
    ESP_ERROR_CHECK(esp_mcp_add_resource(mcp, res_tpl_1));
    ESP_ERROR_CHECK(esp_mcp_add_resource(mcp, res_tpl_2));
    ESP_ERROR_CHECK(esp_mcp_add_resource(mcp, res_tpl_3));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *page1_msg = "{\"jsonrpc\":\"2.0\",\"id\":\"res-tpl-page1\",\"method\":\"resources/templates/list\",\"params\":{\"limit\":1}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)page1_msg, strlen(page1_msg), &outbuf, &outlen));
    cJSON *page1_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(page1_root);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(page1_root, "error"));
    cJSON *page1_result = cJSON_GetObjectItem(page1_root, "result");
    TEST_ASSERT_NOT_NULL(page1_result);
    cJSON *page1_templates = cJSON_GetObjectItem(page1_result, "resourceTemplates");
    TEST_ASSERT_NOT_NULL(page1_templates);
    TEST_ASSERT_TRUE(cJSON_IsArray(page1_templates));
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetArraySize(page1_templates));
    cJSON *page1_next_cursor = cJSON_GetObjectItem(page1_result, "nextCursor");
    TEST_ASSERT_NOT_NULL(page1_next_cursor);
    TEST_ASSERT_TRUE(cJSON_IsString(page1_next_cursor));
    TEST_ASSERT_GREATER_THAN_INT(0, (int)strlen(page1_next_cursor->valuestring));
    char page1_cursor[128] = {0};
    snprintf(page1_cursor, sizeof(page1_cursor), "%s", page1_next_cursor->valuestring);
    cJSON_Delete(page1_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    char page2_msg[256] = {0};
    snprintf(page2_msg, sizeof(page2_msg),
             "{\"jsonrpc\":\"2.0\",\"id\":\"res-tpl-page2\",\"method\":\"resources/templates/list\",\"params\":{\"cursor\":\"%s\",\"limit\":1}}",
             page1_cursor);
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)page2_msg, strlen(page2_msg), &outbuf, &outlen));
    cJSON *page2_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(page2_root);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(page2_root, "error"));
    cJSON *page2_result = cJSON_GetObjectItem(page2_root, "result");
    TEST_ASSERT_NOT_NULL(page2_result);
    cJSON *page2_templates = cJSON_GetObjectItem(page2_result, "resourceTemplates");
    TEST_ASSERT_NOT_NULL(page2_templates);
    TEST_ASSERT_TRUE(cJSON_IsArray(page2_templates));
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetArraySize(page2_templates));
    cJSON *page2_template = cJSON_GetArrayItem(page2_templates, 0);
    TEST_ASSERT_NOT_NULL(page2_template);
    cJSON *page2_uri_template = cJSON_GetObjectItem(page2_template, "uriTemplate");
    TEST_ASSERT_TRUE(cJSON_IsString(page2_uri_template));
    TEST_ASSERT_EQUAL_STRING(page1_cursor, page2_uri_template->valuestring);
    cJSON_Delete(page2_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    ESP_ERROR_CHECK(esp_mcp_remove_resource(mcp, res_plain));
    ESP_ERROR_CHECK(esp_mcp_remove_resource(mcp, res_tpl_1));
    ESP_ERROR_CHECK(esp_mcp_remove_resource(mcp, res_tpl_2));
    ESP_ERROR_CHECK(esp_mcp_remove_resource(mcp, res_tpl_3));
    ESP_ERROR_CHECK(esp_mcp_resource_destroy(res_plain));
    ESP_ERROR_CHECK(esp_mcp_resource_destroy(res_tpl_1));
    ESP_ERROR_CHECK(esp_mcp_resource_destroy(res_tpl_2));
    ESP_ERROR_CHECK(esp_mcp_resource_destroy(res_tpl_3));
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_protocol_2025_resources_templates_list_invalid_cursor", "[mcp][protocol][2025][resources]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_resource_t *res_tpl = esp_mcp_resource_create("device://sensor/{id}", "sensor.by_id", "Sensor by ID",
                                                          "template resource", "application/json", test_resource_read_cb, NULL);
    TEST_ASSERT_NOT_NULL(res_tpl);
    ESP_ERROR_CHECK(esp_mcp_add_resource(mcp, res_tpl));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *invalid_cursor_msg =
        "{\"jsonrpc\":\"2.0\",\"id\":\"res-tpl-invalid\",\"method\":\"resources/templates/list\",\"params\":{\"cursor\":\"missing-template\"}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)invalid_cursor_msg, strlen(invalid_cursor_msg), &outbuf, &outlen));
    cJSON *invalid_cursor_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(invalid_cursor_root);
    assert_error_code(invalid_cursor_root, -32602);
    cJSON_Delete(invalid_cursor_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    const char *invalid_cursor_type_msg =
        "{\"jsonrpc\":\"2.0\",\"id\":\"res-tpl-invalid-type\",\"method\":\"resources/templates/list\",\"params\":{\"cursor\":1}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)invalid_cursor_type_msg, strlen(invalid_cursor_type_msg), &outbuf, &outlen));
    cJSON *invalid_cursor_type_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(invalid_cursor_type_root);
    assert_error_code(invalid_cursor_type_root, -32602);
    cJSON_Delete(invalid_cursor_type_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    const char *invalid_limit_type_msg =
        "{\"jsonrpc\":\"2.0\",\"id\":\"res-tpl-invalid-limit\",\"method\":\"resources/templates/list\",\"params\":{\"limit\":\"bad\"}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)invalid_limit_type_msg, strlen(invalid_limit_type_msg), &outbuf, &outlen));
    cJSON *invalid_limit_type_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(invalid_limit_type_root);
    assert_error_code(invalid_limit_type_root, -32602);
    cJSON_Delete(invalid_limit_type_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    ESP_ERROR_CHECK(esp_mcp_remove_resource(mcp, res_tpl));
    ESP_ERROR_CHECK(esp_mcp_resource_destroy(res_tpl));
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_protocol_2025_prompts_list_pagination_and_invalid_cursor", "[mcp][protocol][2025][prompts]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_prompt_t *prompt_a = esp_mcp_prompt_create("alpha.prompt", "Alpha Prompt", "Prompt A",
                                                       "[{\"role\":\"user\",\"content\":{\"type\":\"text\",\"text\":\"alpha\"}}]",
                                                       NULL, NULL);
    esp_mcp_prompt_t *prompt_b = esp_mcp_prompt_create("beta.prompt", "Beta Prompt", "Prompt B",
                                                       "[{\"role\":\"user\",\"content\":{\"type\":\"text\",\"text\":\"beta\"}}]",
                                                       NULL, NULL);
    TEST_ASSERT_NOT_NULL(prompt_a);
    TEST_ASSERT_NOT_NULL(prompt_b);
    ESP_ERROR_CHECK(esp_mcp_add_prompt(mcp, prompt_a));
    ESP_ERROR_CHECK(esp_mcp_add_prompt(mcp, prompt_b));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *page1_msg = "{\"jsonrpc\":\"2.0\",\"id\":\"prompt-page1\",\"method\":\"prompts/list\",\"params\":{\"limit\":1}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)page1_msg, strlen(page1_msg), &outbuf, &outlen));
    cJSON *page1_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(page1_root);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(page1_root, "error"));
    cJSON *page1_result = cJSON_GetObjectItem(page1_root, "result");
    TEST_ASSERT_NOT_NULL(page1_result);
    cJSON *page1_prompts = cJSON_GetObjectItem(page1_result, "prompts");
    TEST_ASSERT_TRUE(cJSON_IsArray(page1_prompts));
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetArraySize(page1_prompts));
    cJSON *page1_next_cursor = cJSON_GetObjectItem(page1_result, "nextCursor");
    TEST_ASSERT_TRUE(cJSON_IsString(page1_next_cursor));
    char page1_cursor[128] = {0};
    snprintf(page1_cursor, sizeof(page1_cursor), "%s", page1_next_cursor->valuestring);
    cJSON_Delete(page1_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    char page2_msg[256] = {0};
    snprintf(page2_msg, sizeof(page2_msg),
             "{\"jsonrpc\":\"2.0\",\"id\":\"prompt-page2\",\"method\":\"prompts/list\",\"params\":{\"cursor\":\"%s\",\"limit\":1}}",
             page1_cursor);
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)page2_msg, strlen(page2_msg), &outbuf, &outlen));
    cJSON *page2_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(page2_root);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(page2_root, "error"));
    cJSON *page2_result = cJSON_GetObjectItem(page2_root, "result");
    TEST_ASSERT_NOT_NULL(page2_result);
    cJSON *page2_prompts = cJSON_GetObjectItem(page2_result, "prompts");
    TEST_ASSERT_TRUE(cJSON_IsArray(page2_prompts));
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetArraySize(page2_prompts));
    cJSON *page2_prompt = cJSON_GetArrayItem(page2_prompts, 0);
    TEST_ASSERT_NOT_NULL(page2_prompt);
    cJSON *page2_name = cJSON_GetObjectItem(page2_prompt, "name");
    TEST_ASSERT_TRUE(cJSON_IsString(page2_name));
    TEST_ASSERT_EQUAL_STRING(page1_cursor, page2_name->valuestring);
    cJSON_Delete(page2_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    const char *invalid_cursor_msg =
        "{\"jsonrpc\":\"2.0\",\"id\":\"prompt-invalid\",\"method\":\"prompts/list\",\"params\":{\"cursor\":\"missing-prompt\"}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)invalid_cursor_msg, strlen(invalid_cursor_msg), &outbuf, &outlen));
    cJSON *invalid_cursor_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(invalid_cursor_root);
    assert_error_code(invalid_cursor_root, -32602);
    cJSON_Delete(invalid_cursor_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    const char *invalid_cursor_type_msg =
        "{\"jsonrpc\":\"2.0\",\"id\":\"prompt-invalid-type\",\"method\":\"prompts/list\",\"params\":{\"cursor\":1}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)invalid_cursor_type_msg, strlen(invalid_cursor_type_msg), &outbuf, &outlen));
    cJSON *invalid_cursor_type_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(invalid_cursor_type_root);
    assert_error_code(invalid_cursor_type_root, -32602);
    cJSON_Delete(invalid_cursor_type_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    const char *invalid_limit_type_msg =
        "{\"jsonrpc\":\"2.0\",\"id\":\"prompt-invalid-limit\",\"method\":\"prompts/list\",\"params\":{\"limit\":\"bad\"}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)invalid_limit_type_msg, strlen(invalid_limit_type_msg), &outbuf, &outlen));
    cJSON *invalid_limit_type_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(invalid_limit_type_root);
    assert_error_code(invalid_limit_type_root, -32602);
    cJSON_Delete(invalid_limit_type_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    ESP_ERROR_CHECK(esp_mcp_remove_prompt(mcp, prompt_a));
    ESP_ERROR_CHECK(esp_mcp_remove_prompt(mcp, prompt_b));
    ESP_ERROR_CHECK(esp_mcp_prompt_destroy(prompt_a));
    ESP_ERROR_CHECK(esp_mcp_prompt_destroy(prompt_b));
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_protocol_2025_resources_subscribe_unsubscribe", "[mcp][protocol][2025][resources]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    esp_mcp_resource_t *res = esp_mcp_resource_create("device://status", "device.status", "Device Status",
                                                      "status resource", "application/json", test_resource_read_cb, NULL);
    TEST_ASSERT_NOT_NULL(res);
    ESP_ERROR_CHECK(esp_mcp_add_resource(mcp, res));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize_with_capabilities_for_session(handle, "sess-a", NULL);

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *sub_msg = "{\"jsonrpc\":\"2.0\",\"id\":\"res-sub\",\"method\":\"resources/subscribe\",\"params\":{\"uri\":\"device://status\"}}";
    ESP_ERROR_CHECK(esp_mcp_mgr_set_request_context(handle, "sess-a", "test-user", true));
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)sub_msg, strlen(sub_msg), &outbuf, &outlen));
    cJSON *sub_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(sub_root);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(sub_root, "error"));
    cJSON_Delete(sub_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    const char *missing_sess_msg = "{\"jsonrpc\":\"2.0\",\"id\":\"res-sub-missing-session\",\"method\":\"resources/subscribe\",\"params\":{\"uri\":\"device://status\"}}";
    ESP_ERROR_CHECK(esp_mcp_mgr_set_request_context(handle, NULL, "test-user", true));
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)missing_sess_msg, strlen(missing_sess_msg), &outbuf, &outlen));
    cJSON *missing_sess_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(missing_sess_root);
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(missing_sess_root, "error"));
    cJSON_Delete(missing_sess_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    const char *unsub_msg = "{\"jsonrpc\":\"2.0\",\"id\":\"res-unsub\",\"method\":\"resources/unsubscribe\",\"params\":{\"uri\":\"device://status\"}}";
    ESP_ERROR_CHECK(esp_mcp_mgr_set_request_context(handle, "sess-a", "test-user", true));
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)unsub_msg, strlen(unsub_msg), &outbuf, &outlen));
    cJSON *unsub_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(unsub_root);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(unsub_root, "error"));
    cJSON_Delete(unsub_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    ESP_ERROR_CHECK(esp_mcp_mgr_clear_session_state(handle, "sess-a"));

    ESP_ERROR_CHECK(esp_mcp_remove_resource(mcp, res));
    ESP_ERROR_CHECK(esp_mcp_resource_destroy(res));
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_resources_updated_notification_targets_subscribers", "[mcp][protocol][2025][resources][notify]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_resource_t *res_sub = esp_mcp_resource_create("device://subscribed",
                                                          "device.subscribed",
                                                          "Subscribed",
                                                          "subscribed resource",
                                                          "application/json",
                                                          test_resource_read_cb,
                                                          NULL);
    esp_mcp_resource_t *res_other = esp_mcp_resource_create("device://other",
                                                            "device.other",
                                                            "Other",
                                                            "other resource",
                                                            "application/json",
                                                            test_resource_read_cb,
                                                            NULL);
    TEST_ASSERT_NOT_NULL(res_sub);
    TEST_ASSERT_NOT_NULL(res_other);
    ESP_ERROR_CHECK(esp_mcp_add_resource(mcp, res_sub));
    ESP_ERROR_CHECK(esp_mcp_add_resource(mcp, res_other));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize_with_capabilities_for_session(handle, "sess-a", NULL);
    mcp_test_send_initialize_with_capabilities_for_session(handle, "sess-b", NULL);

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *sub_msg =
        "{\"jsonrpc\":\"2.0\",\"id\":\"res-sub\",\"method\":\"resources/subscribe\",\"params\":{\"uri\":\"device://subscribed\"}}";
    ESP_ERROR_CHECK(esp_mcp_mgr_set_request_context(handle, "sess-a", "user-a", true));
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server",
                                               (const uint8_t *)sub_msg,
                                               strlen(sub_msg),
                                               &outbuf, &outlen));
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    mcp_test_reset_emit_capture();
    ESP_ERROR_CHECK(esp_mcp_remove_resource(mcp, res_sub));
    int updated_index = -1;
    int list_changed_index = -1;
    for (int i = 0; i < s_emit_count && i < (int)(sizeof(s_emit_history) / sizeof(s_emit_history[0])); ++i) {
        if (strstr(s_emit_history[i], "\"method\":\"notifications/resources/updated\"")) {
            updated_index = i;
        }
        if (strstr(s_emit_history[i], "\"method\":\"notifications/resources/list_changed\"")) {
            list_changed_index = i;
        }
    }
    TEST_ASSERT_TRUE(list_changed_index >= 0);
    TEST_ASSERT_TRUE(updated_index >= 0);
    TEST_ASSERT_EQUAL_STRING("sess-a", s_emit_session_history[updated_index]);
    TEST_ASSERT_EQUAL_STRING("", s_emit_session_history[list_changed_index]);
    TEST_ASSERT_NULL(strstr(s_emit_history[updated_index], "\"session\":\"sess-b\""));

    mcp_test_reset_emit_capture();
    ESP_ERROR_CHECK(esp_mcp_remove_resource(mcp, res_other));
    for (int i = 0; i < s_emit_count && i < (int)(sizeof(s_emit_history) / sizeof(s_emit_history[0])); ++i) {
        TEST_ASSERT_NULL(strstr(s_emit_history[i], "\"method\":\"notifications/resources/updated\""));
    }

    ESP_ERROR_CHECK(esp_mcp_resource_destroy(res_sub));
    ESP_ERROR_CHECK(esp_mcp_resource_destroy(res_other));
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_protocol_2025_tasks_list", "[mcp][protocol][2025][tasks]")
{
    relax_memory_check = true;
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    esp_mcp_tool_t *tool = esp_mcp_tool_create("task.tool", "Task tool", test_tool_callback_noarg);
    TEST_ASSERT_NOT_NULL(tool);
    ESP_ERROR_CHECK(esp_mcp_add_tool(mcp, tool));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *call_msg = "{\"jsonrpc\":\"2.0\",\"id\":\"task-call\",\"method\":\"tools/call\",\"params\":{\"name\":\"task.tool\",\"arguments\":{},\"task\":{}}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)call_msg, strlen(call_msg), &outbuf, &outlen));
    cJSON *call_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(call_root);
    cJSON_Delete(call_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    const char *list_msg = "{\"jsonrpc\":\"2.0\",\"id\":\"task-list\",\"method\":\"tasks/list\",\"params\":{}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)list_msg, strlen(list_msg), &outbuf, &outlen));
    cJSON *list_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(list_root);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(list_root, "error"));
    cJSON *result = cJSON_GetObjectItem(list_root, "result");
    TEST_ASSERT_NOT_NULL(result);
    cJSON *tasks = cJSON_GetObjectItem(result, "tasks");
    TEST_ASSERT_NOT_NULL(tasks);
    TEST_ASSERT_TRUE(cJSON_IsArray(tasks));
    TEST_ASSERT_GREATER_THAN_INT(0, cJSON_GetArraySize(tasks));
    cJSON *first_task = cJSON_GetArrayItem(tasks, 0);
    TEST_ASSERT_NOT_NULL(first_task);
    cJSON *first_task_id = cJSON_GetObjectItem(first_task, "taskId");
    TEST_ASSERT_NOT_NULL(first_task_id);
    TEST_ASSERT_TRUE(cJSON_IsString(first_task_id));
    TEST_ASSERT_NULL(cJSON_GetObjectItem(result, "_meta"));
    assert_rfc3339_utc_field(first_task, "createdAt");
    assert_rfc3339_utc_field(first_task, "lastUpdatedAt");
    assert_task_ttl_null(first_task);
    cJSON_Delete(list_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_protocol_2025_tasks_list_invalid_cursor", "[mcp][protocol][2025][tasks]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *msg =
        "{\"jsonrpc\":\"2.0\",\"id\":\"task-list-invalid\",\"method\":\"tasks/list\","
        "\"params\":{\"cursor\":\"missing-task\"}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)msg, strlen(msg), &outbuf, &outlen));
    cJSON *root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(root);
    assert_error_code(root, -32602);
    cJSON_Delete(root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_protocol_2025_tasks_result_error_passthrough", "[mcp][protocol][2025][tasks]")
{
    relax_memory_check = true;
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_tool_t *tool = esp_mcp_tool_create("task.error.tool", "Task error tool", test_tool_callback_string);
    TEST_ASSERT_NOT_NULL(tool);
    esp_mcp_property_t *required_prop = esp_mcp_property_create("message", ESP_MCP_PROPERTY_TYPE_STRING);
    TEST_ASSERT_NOT_NULL(required_prop);
    ESP_ERROR_CHECK(esp_mcp_tool_add_property(tool, required_prop));
    ESP_ERROR_CHECK(esp_mcp_add_tool(mcp, tool));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *call_msg =
        "{\"jsonrpc\":\"2.0\",\"id\":\"task-error-call\",\"method\":\"tools/call\","
        "\"params\":{\"name\":\"task.error.tool\",\"arguments\":{},\"task\":{}}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)call_msg, strlen(call_msg), &outbuf, &outlen));
    cJSON *call_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(call_root);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(call_root, "error"));
    cJSON *call_result = cJSON_GetObjectItem(call_root, "result");
    TEST_ASSERT_NOT_NULL(call_result);
    cJSON *task_obj = cJSON_GetObjectItem(call_result, "task");
    TEST_ASSERT_NOT_NULL(task_obj);
    cJSON *task_id = cJSON_GetObjectItem(task_obj, "taskId");
    TEST_ASSERT_NOT_NULL(task_id);
    TEST_ASSERT_TRUE(cJSON_IsString(task_id));
    char task_id_buf[80] = {0};
    snprintf(task_id_buf, sizeof(task_id_buf), "%s", task_id->valuestring);
    cJSON_Delete(call_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    char result_msg[256] = {0};
    snprintf(result_msg, sizeof(result_msg), "{\"jsonrpc\":\"2.0\",\"id\":\"task-error-result\",\"method\":\"tasks/result\",\"params\":{\"taskId\":\"%s\"}}", task_id_buf);
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)result_msg, strlen(result_msg), &outbuf, &outlen));
    cJSON *result_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(result_root);
    assert_error_code(result_root, -32602);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(result_root, "result"));
    cJSON_Delete(result_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_protocol_2025_tasks_status_notification_session_isolation", "[mcp][protocol][2025][tasks]")
{
    relax_memory_check = true;
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_tool_t *tool = esp_mcp_tool_create("task.tool", "Task tool", test_tool_callback_noarg);
    TEST_ASSERT_NOT_NULL(tool);
    ESP_ERROR_CHECK(esp_mcp_add_tool(mcp, tool));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    ESP_ERROR_CHECK(esp_mcp_mgr_set_request_context(handle, "sess-a", "user-a", true));
    mcp_test_reset_emit_capture();
    const char *call_a =
        "{\"jsonrpc\":\"2.0\",\"id\":\"task-sess-a\",\"method\":\"tools/call\","
        "\"params\":{\"name\":\"task.tool\",\"arguments\":{\"__simulateWorkMs\":120},\"task\":{}}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)call_a, strlen(call_a), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;
    vTaskDelay(pdMS_TO_TICKS(220));
    TEST_ASSERT_GREATER_THAN_INT(0, s_emit_count);
    TEST_ASSERT_EQUAL_STRING("sess-a", s_last_emit_session_id);
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"method\":\"notifications/tasks/status\""));
    cJSON *notify_a = cJSON_Parse(s_last_emit_message);
    TEST_ASSERT_NOT_NULL(notify_a);
    cJSON *notify_a_params = cJSON_GetObjectItem(notify_a, "params");
    TEST_ASSERT_NOT_NULL(notify_a_params);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(notify_a_params, "result"));
    TEST_ASSERT_NULL(cJSON_GetObjectItem(notify_a_params, "error"));
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(notify_a_params, "ttl"));
    cJSON_Delete(notify_a);

    ESP_ERROR_CHECK(esp_mcp_mgr_set_request_context(handle, "sess-b", "user-b", true));
    mcp_test_reset_emit_capture();
    const char *call_b =
        "{\"jsonrpc\":\"2.0\",\"id\":\"task-sess-b\",\"method\":\"tools/call\","
        "\"params\":{\"name\":\"task.tool\",\"arguments\":{\"__simulateWorkMs\":120},\"task\":{}}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)call_b, strlen(call_b), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;
    vTaskDelay(pdMS_TO_TICKS(220));
    TEST_ASSERT_GREATER_THAN_INT(0, s_emit_count);
    TEST_ASSERT_EQUAL_STRING("sess-b", s_last_emit_session_id);
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"method\":\"notifications/tasks/status\""));
    cJSON *notify_b = cJSON_Parse(s_last_emit_message);
    TEST_ASSERT_NOT_NULL(notify_b);
    cJSON *notify_b_params = cJSON_GetObjectItem(notify_b, "params");
    TEST_ASSERT_NOT_NULL(notify_b_params);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(notify_b_params, "result"));
    TEST_ASSERT_NULL(cJSON_GetObjectItem(notify_b_params, "error"));
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(notify_b_params, "ttl"));
    cJSON_Delete(notify_b);

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_protocol_2025_tools_call_task_policy_and_capability", "[mcp][protocol][2025][tasks]")
{
    relax_memory_check = true;
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_tool_t *required_tool = esp_mcp_tool_create("required.tool", "Required task tool", test_tool_callback_noarg);
    TEST_ASSERT_NOT_NULL(required_tool);
    ESP_ERROR_CHECK(esp_mcp_tool_set_task_support(required_tool, "required"));
    ESP_ERROR_CHECK(esp_mcp_add_tool(mcp, required_tool));

    esp_mcp_tool_t *forbidden_tool = esp_mcp_tool_create("forbidden.tool", "Forbidden task tool", test_tool_callback_noarg);
    TEST_ASSERT_NOT_NULL(forbidden_tool);
    ESP_ERROR_CHECK(esp_mcp_tool_set_task_support(forbidden_tool, "forbidden"));
    ESP_ERROR_CHECK(esp_mcp_add_tool(mcp, forbidden_tool));

    esp_mcp_tool_t *optional_tool = esp_mcp_tool_create("optional.tool", "Optional task tool", test_tool_callback_noarg);
    TEST_ASSERT_NOT_NULL(optional_tool);
    ESP_ERROR_CHECK(esp_mcp_tool_set_task_support(optional_tool, "optional"));
    ESP_ERROR_CHECK(esp_mcp_add_tool(mcp, optional_tool));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    mcp_test_send_initialize(handle);

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;

    const char *required_without_task =
        "{\"jsonrpc\":\"2.0\",\"id\":\"required-without-task\",\"method\":\"tools/call\","
        "\"params\":{\"name\":\"required.tool\",\"arguments\":{}}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server",
                                               (const uint8_t *)required_without_task,
                                               strlen(required_without_task),
                                               &outbuf, &outlen));
    cJSON *required_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(required_root);
    assert_error_code(required_root, -32601);
    cJSON_Delete(required_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    const char *forbidden_with_task =
        "{\"jsonrpc\":\"2.0\",\"id\":\"forbidden-with-task\",\"method\":\"tools/call\","
        "\"params\":{\"name\":\"forbidden.tool\",\"arguments\":{},\"task\":{}}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server",
                                               (const uint8_t *)forbidden_with_task,
                                               strlen(forbidden_with_task),
                                               &outbuf, &outlen));
    cJSON *forbidden_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(forbidden_root);
    assert_error_code(forbidden_root, -32601);
    cJSON_Delete(forbidden_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    const char *optional_with_task_without_cap =
        "{\"jsonrpc\":\"2.0\",\"id\":\"optional-without-cap\",\"method\":\"tools/call\","
        "\"params\":{\"name\":\"optional.tool\",\"arguments\":{},\"task\":{}}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server",
                                               (const uint8_t *)optional_with_task_without_cap,
                                               strlen(optional_with_task_without_cap),
                                               &outbuf, &outlen));
    cJSON *optional_no_cap_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(optional_no_cap_root);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(optional_no_cap_root, "error"));
    cJSON *optional_no_cap_result = cJSON_GetObjectItem(optional_no_cap_root, "result");
    TEST_ASSERT_NOT_NULL(optional_no_cap_result);
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(optional_no_cap_result, "task"));
    cJSON_Delete(optional_no_cap_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    const char *required_with_task =
        "{\"jsonrpc\":\"2.0\",\"id\":\"required-with-task\",\"method\":\"tools/call\","
        "\"params\":{\"name\":\"required.tool\",\"arguments\":{},\"task\":{}}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server",
                                               (const uint8_t *)required_with_task,
                                               strlen(required_with_task),
                                               &outbuf, &outlen));
    cJSON *required_with_task_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(required_with_task_root);
    cJSON *required_with_task_error = cJSON_GetObjectItem(required_with_task_root, "error");
    TEST_ASSERT_NULL(required_with_task_error);
    cJSON *required_with_task_result = cJSON_GetObjectItem(required_with_task_root, "result");
    TEST_ASSERT_NOT_NULL(required_with_task_result);
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(required_with_task_result, "task"));
    cJSON_Delete(required_with_task_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_server_requests_capability_gate", "[mcp][protocol][2025][capability]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    char request_id[40] = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_mcp_request_roots_list(mcp, "sess-a", NULL, NULL, 1000, request_id, sizeof(request_id)));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_mcp_request_sampling_create(mcp, "sess-a", "{}", NULL, NULL, 1000, request_id, sizeof(request_id)));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_mcp_request_elicitation(mcp, "sess-a", "{}", NULL, NULL, 1000, request_id, sizeof(request_id)));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_mcp_request_tasks_list(mcp, "sess-a", NULL, NULL, NULL, 1000, request_id, sizeof(request_id)));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_mcp_request_tasks_cancel(mcp, "sess-a", "task-1", NULL, NULL, 1000, request_id, sizeof(request_id)));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      esp_mcp_request_sampling_with_tools(mcp, "sess-a", "{\"messages\":[]}", "[]", "required",
                                                          NULL, NULL, 1000, request_id, sizeof(request_id)));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      esp_mcp_request_elicitation_url(mcp, "sess-a", "{}", "https://example.com/consent",
                                                      NULL, NULL, 1000, request_id, sizeof(request_id)));

    mcp_test_send_initialize_with_capabilities_for_session(
        handle,
        "sess-a",
        "{\"roots\":{},\"sampling\":{\"tools\":{}},\"elicitation\":{\"form\":{},\"url\":{}}}");

    mcp_test_reset_emit_capture();
    ESP_ERROR_CHECK(esp_mcp_request_roots_list(mcp, "sess-a", NULL, NULL, 1000, request_id, sizeof(request_id)));
    TEST_ASSERT_NOT_EMPTY(request_id);
    TEST_ASSERT_GREATER_THAN_INT(0, s_emit_count);
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"method\":\"roots/list\""));
    TEST_ASSERT_EQUAL_STRING("sess-a", s_last_emit_session_id);

    mcp_test_reset_emit_capture();
    request_id[0] = '\0';
    ESP_ERROR_CHECK(esp_mcp_request_sampling_create(mcp, "sess-a", "{}", NULL, NULL, 1000, request_id, sizeof(request_id)));
    TEST_ASSERT_NOT_EMPTY(request_id);
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"method\":\"sampling/createMessage\""));

    mcp_test_reset_emit_capture();
    request_id[0] = '\0';
    ESP_ERROR_CHECK(esp_mcp_request_elicitation(mcp, "sess-a", "{}", NULL, NULL, 1000, request_id, sizeof(request_id)));
    TEST_ASSERT_NOT_EMPTY(request_id);
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"method\":\"elicitation/create\""));

    mcp_test_reset_emit_capture();
    request_id[0] = '\0';
    ESP_ERROR_CHECK(esp_mcp_request_elicitation_url(mcp, "sess-a", "{\"title\":\"Open URL\"}",
                                                    "https://example.com/consent",
                                                    NULL, NULL, 1000, request_id, sizeof(request_id)));
    TEST_ASSERT_NOT_EMPTY(request_id);
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"method\":\"elicitation/create\""));
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"mode\":\"url\""));
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"url\":\"https://example.com/consent\""));
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"elicitationId\":\""));
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, request_id));

    mcp_test_reset_emit_capture();
    request_id[0] = '\0';
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      esp_mcp_request_sampling_create(mcp, "sess-a", "{\"messages\":[],\"task\":{}}",
                                                      NULL, NULL, 1000, request_id, sizeof(request_id)));
    TEST_ASSERT_EQUAL_INT(0, s_emit_count);

    mcp_test_reset_emit_capture();
    request_id[0] = '\0';
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      esp_mcp_request_elicitation(mcp, "sess-a", "{\"task\":{}}",
                                                  NULL, NULL, 1000, request_id, sizeof(request_id)));
    TEST_ASSERT_EQUAL_INT(0, s_emit_count);

    mcp_test_reset_emit_capture();
    request_id[0] = '\0';
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      esp_mcp_request_tasks_list(mcp, "sess-a", "cursor-before-task-cap",
                                                 NULL, NULL, 1000, request_id, sizeof(request_id)));
    TEST_ASSERT_EQUAL_INT(0, s_emit_count);

    mcp_test_reset_emit_capture();
    request_id[0] = '\0';
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      esp_mcp_request_tasks_cancel(mcp, "sess-a", "task-before-task-cap",
                                                   NULL, NULL, 1000, request_id, sizeof(request_id)));
    TEST_ASSERT_EQUAL_INT(0, s_emit_count);

    mcp_test_send_initialize_with_capabilities_for_session(
        handle,
        "sess-b",
        "{\"sampling\":{\"tools\":{}},\"elicitation\":{\"form\":{},\"url\":{}},\"tasks\":{\"list\":{},\"cancel\":{},\"requests\":{\"sampling\":{\"createMessage\":{}},\"elicitation\":{\"create\":{}}}}}");

    mcp_test_reset_emit_capture();
    request_id[0] = '\0';
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      esp_mcp_request_roots_list(mcp, "sess-b", NULL, NULL, 1000, request_id, sizeof(request_id)));
    TEST_ASSERT_EQUAL_INT(0, s_emit_count);

    mcp_test_reset_emit_capture();
    request_id[0] = '\0';
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      esp_mcp_request_tasks_list(mcp, "sess-a", "cursor-still-no-task-cap",
                                                 NULL, NULL, 1000, request_id, sizeof(request_id)));
    TEST_ASSERT_EQUAL_INT(0, s_emit_count);

    mcp_test_reset_emit_capture();
    request_id[0] = '\0';
    ESP_ERROR_CHECK(esp_mcp_request_sampling_create(mcp, "sess-b", "{\"messages\":[],\"task\":{\"ttl\":1234}}",
                                                    NULL, NULL, 1000, request_id, sizeof(request_id)));
    TEST_ASSERT_NOT_EMPTY(request_id);
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"method\":\"sampling/createMessage\""));
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"task\":{\"ttl\":1234}"));
    TEST_ASSERT_EQUAL_STRING("sess-b", s_last_emit_session_id);

    mcp_test_reset_emit_capture();
    request_id[0] = '\0';
    ESP_ERROR_CHECK(esp_mcp_request_elicitation(mcp, "sess-b", "{\"task\":{\"ttl\":2345}}",
                                                NULL, NULL, 1000, request_id, sizeof(request_id)));
    TEST_ASSERT_NOT_EMPTY(request_id);
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"method\":\"elicitation/create\""));
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"task\":{\"ttl\":2345}"));
    TEST_ASSERT_EQUAL_STRING("sess-b", s_last_emit_session_id);

    mcp_test_reset_emit_capture();
    request_id[0] = '\0';
    ESP_ERROR_CHECK(esp_mcp_request_tasks_get(mcp, "sess-b", "task-get-1",
                                              NULL, NULL, 1000, request_id, sizeof(request_id)));
    TEST_ASSERT_NOT_EMPTY(request_id);
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"method\":\"tasks/get\""));
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"taskId\":\"task-get-1\""));
    TEST_ASSERT_EQUAL_STRING("sess-b", s_last_emit_session_id);

    mcp_test_reset_emit_capture();
    request_id[0] = '\0';
    ESP_ERROR_CHECK(esp_mcp_request_tasks_result(mcp, "sess-b", "task-result-1",
                                                 NULL, NULL, 1000, request_id, sizeof(request_id)));
    TEST_ASSERT_NOT_EMPTY(request_id);
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"method\":\"tasks/result\""));
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"taskId\":\"task-result-1\""));
    TEST_ASSERT_EQUAL_STRING("sess-b", s_last_emit_session_id);

    mcp_test_reset_emit_capture();
    request_id[0] = '\0';
    ESP_ERROR_CHECK(esp_mcp_request_tasks_list(mcp, "sess-b", "cursor-after-task-cap",
                                               NULL, NULL, 1000, request_id, sizeof(request_id)));
    TEST_ASSERT_NOT_EMPTY(request_id);
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"method\":\"tasks/list\""));
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"cursor\":\"cursor-after-task-cap\""));
    TEST_ASSERT_EQUAL_STRING("sess-b", s_last_emit_session_id);

    mcp_test_reset_emit_capture();
    request_id[0] = '\0';
    ESP_ERROR_CHECK(esp_mcp_request_tasks_cancel(mcp, "sess-b", "task-cancel-1",
                                                 NULL, NULL, 1000, request_id, sizeof(request_id)));
    TEST_ASSERT_NOT_EMPTY(request_id);
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"method\":\"tasks/cancel\""));
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"taskId\":\"task-cancel-1\""));
    TEST_ASSERT_EQUAL_STRING("sess-b", s_last_emit_session_id);

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_server_requests_wait_for_initialized_notification", "[mcp][protocol][2025][gate][capability]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    const char *init_only =
        "{\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2025-11-25\","
        "\"clientInfo\":{\"name\":\"test_client\",\"version\":\"1.0\"},"
        "\"capabilities\":{\"roots\":{}}},\"id\":\"init-only\"}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_set_request_context(handle, "sess-init", NULL, true));
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server",
                                               (const uint8_t *)init_only,
                                               strlen(init_only),
                                               &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    char request_id[40] = {0};
    mcp_test_reset_emit_capture();
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      esp_mcp_request_roots_list(mcp, "sess-init", NULL, NULL, 1000, request_id, sizeof(request_id)));
    TEST_ASSERT_EQUAL_INT(0, s_emit_count);

    const char *initialized =
        "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\",\"params\":{}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server",
                                               (const uint8_t *)initialized,
                                               strlen(initialized),
                                               &outbuf, &outlen));
    TEST_ASSERT_NULL(outbuf);
    TEST_ASSERT_EQUAL_UINT16(0, outlen);

    mcp_test_reset_emit_capture();
    request_id[0] = '\0';
    ESP_ERROR_CHECK(esp_mcp_request_roots_list(mcp, "sess-init", NULL, NULL, 1000, request_id, sizeof(request_id)));
    TEST_ASSERT_NOT_EMPTY(request_id);
    TEST_ASSERT_GREATER_THAN_INT(0, s_emit_count);
    TEST_ASSERT_EQUAL_STRING("sess-init", s_last_emit_session_id);
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"method\":\"roots/list\""));

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_server_sampling_param_capability_gate", "[mcp][protocol][2025][capability][sampling]")
{
    char request_id[40] = {0};

    {
        esp_mcp_t *mcp = NULL;
        ESP_ERROR_CHECK(esp_mcp_create(&mcp));
        int test_config = 12345;
        esp_mcp_mgr_config_t config = {
            .transport = mock_transport,
            .config = &test_config,
            .instance = mcp
        };
        esp_mcp_mgr_handle_t handle = 0;
        ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
        ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
        ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

        mcp_test_send_initialize_with_capabilities_for_session(handle, "sess-sampling", "{\"sampling\":{}}");

        mcp_test_reset_emit_capture();
        ESP_ERROR_CHECK(esp_mcp_request_sampling_create(mcp, "sess-sampling",
                                                        "{\"messages\":[],\"maxTokens\":16,\"includeContext\":\"none\"}",
                                                        NULL, NULL, 1000, request_id, sizeof(request_id)));
        TEST_ASSERT_NOT_EMPTY(request_id);
        TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"includeContext\":\"none\""));

        mcp_test_reset_emit_capture();
        request_id[0] = '\0';
        TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                          esp_mcp_request_sampling_create(mcp, "sess-sampling",
                                                          "{\"messages\":[],\"maxTokens\":16,\"includeContext\":\"thisServer\"}",
                                                          NULL, NULL, 1000, request_id, sizeof(request_id)));
        TEST_ASSERT_EQUAL_INT(0, s_emit_count);

        mcp_test_reset_emit_capture();
        request_id[0] = '\0';
        TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                          esp_mcp_request_sampling_create(mcp, "sess-sampling",
                                                          "{\"messages\":[],\"maxTokens\":16,\"tools\":[],\"toolChoice\":{\"mode\":\"required\"}}",
                                                          NULL, NULL, 1000, request_id, sizeof(request_id)));
        TEST_ASSERT_EQUAL_INT(0, s_emit_count);

        ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
        ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
        ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
    }

    {
        esp_mcp_t *mcp = NULL;
        ESP_ERROR_CHECK(esp_mcp_create(&mcp));
        int test_config = 12345;
        esp_mcp_mgr_config_t config = {
            .transport = mock_transport,
            .config = &test_config,
            .instance = mcp
        };
        esp_mcp_mgr_handle_t handle = 0;
        ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
        ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
        ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

        mcp_test_send_initialize_with_capabilities_for_session(handle, "sess-sampling", "{\"sampling\":{\"context\":{},\"tools\":{}}}");

        mcp_test_reset_emit_capture();
        request_id[0] = '\0';
        ESP_ERROR_CHECK(esp_mcp_request_sampling_create(mcp, "sess-sampling",
                                                        "{\"messages\":[],\"maxTokens\":16,\"includeContext\":\"thisServer\"}",
                                                        NULL, NULL, 1000, request_id, sizeof(request_id)));
        TEST_ASSERT_NOT_EMPTY(request_id);
        TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"includeContext\":\"thisServer\""));

        mcp_test_reset_emit_capture();
        request_id[0] = '\0';
        ESP_ERROR_CHECK(esp_mcp_request_sampling_create(mcp, "sess-sampling",
                                                        "{\"messages\":[],\"maxTokens\":16,\"tools\":[],\"toolChoice\":{\"mode\":\"required\"}}",
                                                        NULL, NULL, 1000, request_id, sizeof(request_id)));
        TEST_ASSERT_NOT_EMPTY(request_id);
        TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"tools\":[]"));
        TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"toolChoice\":{\"mode\":\"required\"}"));

        ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
        ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
        ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
    }
}

TEST_CASE("test_elicitation_complete_notification_dispatch_pending", "[mcp][protocol][2025][elicitation]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *init_with_caps =
        "{\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2025-11-25\","
        "\"clientInfo\":{\"name\":\"test_client\",\"version\":\"1.0\"},"
        "\"capabilities\":{\"elicitation\":{\"url\":{}}}},\"id\":1}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)init_with_caps, strlen(init_with_caps), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    pending_cb_result_t cb_result = {0};
    char request_id[40] = {0};
    mcp_test_reset_emit_capture();
    ESP_ERROR_CHECK(esp_mcp_request_elicitation_url(mcp, NULL, "{}",
                                                    "https://example.com/verify",
                                                    pending_req_cb_capture,
                                                    &cb_result,
                                                    5000,
                                                    request_id,
                                                    sizeof(request_id)));
    TEST_ASSERT_NOT_EMPTY(request_id);
    TEST_ASSERT_EQUAL_INT(0, cb_result.called);
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"elicitationId\":\""));
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, request_id));

    char complete_msg[256] = {0};
    snprintf(complete_msg, sizeof(complete_msg),
             "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/elicitation/complete\","
             "\"params\":{\"elicitationId\":\"%s\",\"status\":\"accepted\"}}",
             request_id);
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server",
                                               (const uint8_t *)complete_msg,
                                               strlen(complete_msg),
                                               &outbuf, &outlen));
    TEST_ASSERT_NULL(outbuf);
    TEST_ASSERT_EQUAL_INT(1, cb_result.called);
    TEST_ASSERT_EQUAL_INT(0, cb_result.error_code);
    TEST_ASSERT_EQUAL_STRING("elicitation/create", cb_result.name);
    TEST_ASSERT_NOT_NULL(strstr(cb_result.payload, "\"elicitationId\""));
    TEST_ASSERT_NOT_NULL(strstr(cb_result.payload, request_id));
    TEST_ASSERT_NOT_NULL(strstr(cb_result.payload, "\"status\":\"accepted\""));

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_callback_resource_read_failure", "[mcp][protocol][error][callback]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    esp_mcp_resource_t *res = esp_mcp_resource_create("device://broken", "device.broken", "Broken Resource",
                                                      "broken resource", "application/json", test_resource_read_fail_cb, NULL);
    TEST_ASSERT_NOT_NULL(res);
    ESP_ERROR_CHECK(esp_mcp_add_resource(mcp, res));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *msg = "{\"jsonrpc\":\"2.0\",\"id\":\"res-read-fail\",\"method\":\"resources/read\",\"params\":{\"uri\":\"device://broken\"}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)msg, strlen(msg), &outbuf, &outlen));
    cJSON *root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(root, "error"));
    cJSON_Delete(root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    ESP_ERROR_CHECK(esp_mcp_remove_resource(mcp, res));
    ESP_ERROR_CHECK(esp_mcp_resource_destroy(res));
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_callback_prompt_render_failure", "[mcp][protocol][error][callback]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    esp_mcp_prompt_t *prompt = esp_mcp_prompt_create("broken.prompt", "Broken Prompt", "Broken prompt",
                                                     NULL, test_prompt_render_fail_cb, NULL);
    TEST_ASSERT_NOT_NULL(prompt);
    ESP_ERROR_CHECK(esp_mcp_add_prompt(mcp, prompt));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *msg = "{\"jsonrpc\":\"2.0\",\"id\":\"prompt-fail\",\"method\":\"prompts/get\",\"params\":{\"name\":\"broken.prompt\"}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)msg, strlen(msg), &outbuf, &outlen));
    cJSON *root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(root, "error"));
    cJSON_Delete(root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    ESP_ERROR_CHECK(esp_mcp_remove_prompt(mcp, prompt));
    ESP_ERROR_CHECK(esp_mcp_prompt_destroy(prompt));
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_callback_completion_invalid_result", "[mcp][protocol][error][callback]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_completion_provider_t *provider = esp_mcp_completion_provider_create(test_completion_invalid_cb, NULL);
    TEST_ASSERT_NOT_NULL(provider);
    ESP_ERROR_CHECK(esp_mcp_set_completion_provider(mcp, provider));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *msg = "{\"jsonrpc\":\"2.0\",\"id\":\"completion-fail\",\"method\":\"completion/complete\",\"params\":{\"ref\":{\"type\":\"ref/prompt\",\"name\":\"status.summary\"},\"argument\":{\"name\":\"device\",\"value\":\"x\"}}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)msg, strlen(msg), &outbuf, &outlen));
    cJSON *root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(root, "error"));
    cJSON_Delete(root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_parser_missing_method_request", "[mcp][protocol][parser]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *msg = "{\"jsonrpc\":\"2.0\",\"id\":42,\"params\":{}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)msg, strlen(msg), &outbuf, &outlen));
    cJSON *root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(root);
    cJSON *error = cJSON_GetObjectItem(root, "error");
    TEST_ASSERT_NOT_NULL(error);
    cJSON *code = cJSON_GetObjectItem(error, "code");
    TEST_ASSERT_NOT_NULL(code);
    TEST_ASSERT_TRUE(cJSON_IsNumber(code));
    TEST_ASSERT_EQUAL_INT(-32600, code->valueint);
    cJSON_Delete(root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_parser_empty_message", "[mcp][protocol][parser]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    esp_err_t ret = esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)"", 0, &outbuf, &outlen);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
    TEST_ASSERT_NULL(outbuf);

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_pending_request_cancel_and_timeout", "[mcp][pending]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));

    pending_cb_result_t cancel_result = {0};
    ESP_ERROR_CHECK(esp_mcp_mgr_track_pending_request(handle, "req-cancel", "sess-a", "roots/list",
                                                      pending_req_cb_capture, &cancel_result, 2000));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      esp_mcp_mgr_track_pending_request(handle, "req-cancel", "sess-a", "roots/list",
                                                        pending_req_cb_capture, &cancel_result, 2000));
    ESP_ERROR_CHECK(esp_mcp_mgr_cancel_pending_request(handle, "req-cancel", "sess-a"));
    TEST_ASSERT_EQUAL(1, cancel_result.called);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, cancel_result.error_code);
    TEST_ASSERT_EQUAL_STRING("roots/list", cancel_result.name);
    TEST_ASSERT_EQUAL_UINT32(ESP_MCP_JSONRPC_ID_UNSPEC, cancel_result.last_jsonrpc_id);

    pending_cb_result_t timeout_result = {0};
    ESP_ERROR_CHECK(esp_mcp_mgr_track_pending_request(handle, "req-timeout", "sess-b", "sampling/createMessage",
                                                      pending_req_cb_capture, &timeout_result, 1));
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_ERROR_CHECK(esp_mcp_mgr_sweep_pending_requests(handle));
    TEST_ASSERT_EQUAL(1, timeout_result.called);
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, timeout_result.error_code);
    TEST_ASSERT_EQUAL_UINT32(ESP_MCP_JSONRPC_ID_UNSPEC, timeout_result.last_jsonrpc_id);

    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, esp_mcp_mgr_cancel_pending_request(handle, "req-timeout", "sess-b"));

    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_mgr_post_apis_dispatch_pending_callbacks", "[mcp][pending][post]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport_with_request,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    pending_cb_result_t init_result = {0};
    esp_mcp_mgr_req_t init_req = {
        .ep_name = "mcp_server",
        .cb = pending_req_cb_capture,
        .user_ctx = &init_result,
    };
    init_req.u.init.protocol_version = "2025-11-25";
    init_req.u.init.name = "esp-test";
    init_req.u.init.version = "1.0.0";
    ESP_ERROR_CHECK(esp_mcp_mgr_post_info_init(handle, &init_req));
    TEST_ASSERT_EQUAL(1, init_result.called);
    TEST_ASSERT_EQUAL(0, init_result.error_code);
    TEST_ASSERT_NOT_EQUAL_HEX32(ESP_MCP_JSONRPC_ID_UNSPEC, init_result.last_jsonrpc_id);
    TEST_ASSERT_NOT_NULL(strstr(init_result.payload, "\"protocolVersion\""));

    pending_cb_result_t list_result = {0};
    esp_mcp_mgr_req_t list_req = {
        .ep_name = "mcp_server",
        .cb = pending_req_cb_capture,
        .user_ctx = &list_result,
    };
    list_req.u.list.cursor = NULL;
    list_req.u.list.limit = -1;
    ESP_ERROR_CHECK(esp_mcp_mgr_post_tools_list(handle, &list_req));
    TEST_ASSERT_EQUAL(1, list_result.called);
    TEST_ASSERT_EQUAL(0, list_result.error_code);
    TEST_ASSERT_NOT_NULL(strstr(list_result.payload, "\"tools\""));

    pending_cb_result_t call_result = {0};
    esp_mcp_mgr_req_t call_req = {
        .ep_name = "mcp_server",
        .cb = pending_req_cb_capture,
        .user_ctx = &call_result,
    };
    call_req.u.call.tool_name = "echo";
    call_req.u.call.args_json = "{}";
    ESP_ERROR_CHECK(esp_mcp_mgr_post_tools_call(handle, &call_req));
    TEST_ASSERT_EQUAL(1, call_result.called);
    TEST_ASSERT_EQUAL(0, call_result.error_code);
    TEST_ASSERT_EQUAL_STRING("echo", call_result.name);
    TEST_ASSERT_NOT_NULL(strstr(call_result.payload, "\"isError\":false"));

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_mgr_post_tools_list_json_includes_optional_limit", "[mcp][mgr][tools]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport_with_request,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    pending_cb_result_t init_result = {0};
    esp_mcp_mgr_req_t init_req = {
        .ep_name = "mcp_server",
        .cb = pending_req_cb_capture,
        .user_ctx = &init_result,
    };
    init_req.u.init.protocol_version = "2025-11-25";
    init_req.u.init.name = "esp-test";
    init_req.u.init.version = "1.0.0";
    ESP_ERROR_CHECK(esp_mcp_mgr_post_info_init(handle, &init_req));
    TEST_ASSERT_EQUAL(1, init_result.called);

    mcp_test_reset_request_capture();
    pending_cb_result_t list_result = {0};
    esp_mcp_mgr_req_t list_req = {
        .ep_name = "mcp_server",
        .cb = pending_req_cb_capture,
        .user_ctx = &list_result,
    };
    list_req.u.list.cursor = NULL;
    list_req.u.list.limit = 3;
    ESP_ERROR_CHECK(esp_mcp_mgr_post_tools_list(handle, &list_req));
    TEST_ASSERT_EQUAL(1, list_result.called);
    TEST_ASSERT_EQUAL(1, s_request_count);
    cJSON *root = cJSON_Parse(s_request_history[0]);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_EQUAL_STRING("tools/list", cJSON_GetObjectItem(root, "method")->valuestring);
    cJSON *params = cJSON_GetObjectItem(root, "params");
    TEST_ASSERT_NOT_NULL(params);
    cJSON *lim = cJSON_GetObjectItem(params, "limit");
    TEST_ASSERT_TRUE(cJSON_IsNumber(lim));
    TEST_ASSERT_EQUAL_INT(3, (int)lim->valuedouble);
    cJSON_Delete(root);

    mcp_test_reset_request_capture();
    list_req.u.list.limit = -1;
    ESP_ERROR_CHECK(esp_mcp_mgr_post_tools_list(handle, &list_req));
    TEST_ASSERT_EQUAL(1, s_request_count);
    TEST_ASSERT_NULL(strstr(s_request_history[0], "\"limit\""));

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_mgr_post_info_init_includes_client_metadata_and_capabilities", "[mcp][client][init]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport_with_request,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    pending_cb_result_t init_result = {0};
    esp_mcp_mgr_req_t init_req = {
        .ep_name = "mcp_server",
        .cb = pending_req_cb_capture,
        .user_ctx = &init_result,
    };
    init_req.u.init.protocol_version = "2025-11-25";
    init_req.u.init.name = "esp-test";
    init_req.u.init.version = "1.2.3";
    init_req.u.init.title = "ESP Test Client";
    init_req.u.init.description = "Client-side MCP validation";
    init_req.u.init.icons_json =
        "[{\"src\":\"https://example.com/icon.png\",\"mimeType\":\"image/png\",\"sizes\":\"128x128\"}]";
    init_req.u.init.website_url = "https://example.com/client";
    init_req.u.init.capabilities_json =
        "{\"roots\":{\"listChanged\":true},\"sampling\":{\"tools\":{}},\"elicitation\":{\"url\":{}},"
        "\"tasks\":{\"list\":{},\"cancel\":{},\"requests\":{\"sampling\":{\"createMessage\":{}},"
        "\"elicitation\":{\"create\":{}}}}}";

    mcp_test_reset_request_capture();
    ESP_ERROR_CHECK(esp_mcp_mgr_post_info_init(handle, &init_req));
    TEST_ASSERT_EQUAL(1, init_result.called);
    TEST_ASSERT_EQUAL(0, init_result.error_code);
    TEST_ASSERT_EQUAL_INT(2, s_request_count);

    cJSON *root = cJSON_Parse(s_request_history[0]);
    TEST_ASSERT_NOT_NULL(root);
    cJSON *params = cJSON_GetObjectItem(root, "params");
    TEST_ASSERT_NOT_NULL(params);
    cJSON *client_info = cJSON_GetObjectItem(params, "clientInfo");
    TEST_ASSERT_NOT_NULL(client_info);
    TEST_ASSERT_EQUAL_STRING("ESP Test Client", cJSON_GetObjectItem(client_info, "title")->valuestring);
    TEST_ASSERT_EQUAL_STRING("Client-side MCP validation", cJSON_GetObjectItem(client_info, "description")->valuestring);
    TEST_ASSERT_EQUAL_STRING("https://example.com/client", cJSON_GetObjectItem(client_info, "websiteUrl")->valuestring);
    cJSON *icons = cJSON_GetObjectItem(client_info, "icons");
    TEST_ASSERT_TRUE(cJSON_IsArray(icons));
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetArraySize(icons));

    cJSON *capabilities = cJSON_GetObjectItem(params, "capabilities");
    TEST_ASSERT_NOT_NULL(capabilities);
    TEST_ASSERT_TRUE(cJSON_IsObject(cJSON_GetObjectItem(capabilities, "roots")));
    TEST_ASSERT_TRUE(cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetObjectItem(capabilities, "roots"), "listChanged")));
    TEST_ASSERT_TRUE(cJSON_IsObject(cJSON_GetObjectItem(cJSON_GetObjectItem(capabilities, "sampling"), "tools")));
    TEST_ASSERT_TRUE(cJSON_IsObject(cJSON_GetObjectItem(cJSON_GetObjectItem(capabilities, "elicitation"), "url")));
    TEST_ASSERT_TRUE(cJSON_IsObject(cJSON_GetObjectItem(cJSON_GetObjectItem(capabilities, "tasks"), "list")));
    cJSON_Delete(root);

    root = cJSON_Parse(s_request_history[1]);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_EQUAL_STRING("notifications/initialized", cJSON_GetObjectItem(root, "method")->valuestring);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(root, "id"));
    TEST_ASSERT_NULL(cJSON_GetObjectItem(root, "params"));
    cJSON_Delete(root);

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_mgr_post_info_init_auto_posts_initialized_before_next_request", "[mcp][client][init][lifecycle]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport_with_request,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    esp_mcp_mgr_req_t init_req = {
        .ep_name = "mcp_server",
        .cb = NULL,
        .user_ctx = NULL,
    };
    init_req.u.init.protocol_version = "2025-11-25";
    init_req.u.init.name = "esp-test";
    init_req.u.init.version = "1.0.0";

    mcp_test_reset_request_capture();
    ESP_ERROR_CHECK(esp_mcp_mgr_post_info_init(handle, &init_req));
    TEST_ASSERT_EQUAL_INT(2, s_request_count);
    TEST_ASSERT_NOT_NULL(strstr(s_request_history[0], "\"method\":\"initialize\""));
    TEST_ASSERT_NOT_NULL(strstr(s_request_history[1], "\"method\":\"notifications/initialized\""));

    esp_mcp_mgr_json_req_t ping_req = {
        .ep_name = "mcp_server",
        .method = "ping",
        .params_json = "{}",
        .cb = NULL,
        .user_ctx = NULL,
    };

    mcp_test_reset_request_capture();
    ESP_ERROR_CHECK(esp_mcp_mgr_post_request_json(handle, &ping_req));
    TEST_ASSERT_EQUAL_INT(1, s_request_count);
    TEST_ASSERT_NOT_NULL(strstr(s_request_history[0], "\"method\":\"ping\""));

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

typedef struct {
    esp_mcp_mgr_handle_t mgr;
} init_manual_initialized_ctx_t;

static esp_err_t init_resp_posts_initialized_cb(int error_code, const char *ep_name, const char *resp_json, void *user_ctx,
                                                uint32_t jsonrpc_request_id)
{
    (void)error_code;
    (void)ep_name;
    (void)resp_json;
    (void)jsonrpc_request_id;
    init_manual_initialized_ctx_t *ctx = (init_manual_initialized_ctx_t *)user_ctx;
    if (!ctx || !ctx->mgr) {
        return ESP_ERR_INVALID_ARG;
    }
    return esp_mcp_mgr_post_initialized(ctx->mgr, "mcp_server");
}

TEST_CASE("test_mgr_initialize_manual_initialized_skips_auto_duplicate", "[mcp][client][init][lifecycle]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport_with_request,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    init_manual_initialized_ctx_t cb_ctx = {
        .mgr = handle,
    };
    esp_mcp_mgr_req_t init_req = {
        .ep_name = "mcp_server",
        .cb = init_resp_posts_initialized_cb,
        .user_ctx = &cb_ctx,
    };
    init_req.u.init.protocol_version = "2025-11-25";
    init_req.u.init.name = "esp-test";
    init_req.u.init.version = "1.0.0";

    mcp_test_reset_request_capture();
    ESP_ERROR_CHECK(esp_mcp_mgr_post_info_init(handle, &init_req));
    TEST_ASSERT_EQUAL_INT(2, s_request_count);

    int initialized_notifications = 0;
    for (int i = 0; i < s_request_count; i++) {
        if (strstr(s_request_history[i], "\"method\":\"notifications/initialized\"")) {
            initialized_notifications++;
        }
    }
    TEST_ASSERT_EQUAL_INT(1, initialized_notifications);

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_mgr_post_json_request_and_notifications", "[mcp][client][jsonrpc]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport_with_request,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    pending_cb_result_t ping_result = {0};
    esp_mcp_mgr_json_req_t ping_req = {
        .ep_name = "mcp_server",
        .method = "ping",
        .params_json = "{}",
        .cb = pending_req_cb_capture,
        .user_ctx = &ping_result,
    };
    mcp_test_reset_request_capture();
    ESP_ERROR_CHECK(esp_mcp_mgr_post_request_json(handle, &ping_req));
    TEST_ASSERT_EQUAL(1, ping_result.called);
    TEST_ASSERT_EQUAL(0, ping_result.error_code);
    TEST_ASSERT_NOT_EQUAL_HEX32(ESP_MCP_JSONRPC_ID_UNSPEC, ping_result.last_jsonrpc_id);
    TEST_ASSERT_EQUAL_STRING("ping", ping_result.name);
    TEST_ASSERT_EQUAL_STRING("{}", ping_result.payload);
    cJSON *root = cJSON_Parse(s_last_request_message);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_EQUAL_STRING("ping", cJSON_GetObjectItem(root, "method")->valuestring);
    TEST_ASSERT_TRUE(cJSON_IsNumber(cJSON_GetObjectItem(root, "id")));
    cJSON_Delete(root);

    esp_mcp_mgr_json_notification_t cancelled = {
        .ep_name = "mcp_server",
        .method = "notifications/cancelled",
        .params_json = "{\"requestId\":\"req-1\",\"reason\":\"user\"}",
    };
    mcp_test_reset_request_capture();
    ESP_ERROR_CHECK(esp_mcp_mgr_post_notification_json(handle, &cancelled));
    TEST_ASSERT_EQUAL_INT(1, s_request_count);
    root = cJSON_Parse(s_last_request_message);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_EQUAL_STRING("notifications/cancelled", cJSON_GetObjectItem(root, "method")->valuestring);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(root, "id"));
    TEST_ASSERT_EQUAL_STRING("req-1", cJSON_GetObjectItem(cJSON_GetObjectItem(root, "params"), "requestId")->valuestring);
    cJSON_Delete(root);

    mcp_test_reset_request_capture();
    ESP_ERROR_CHECK(esp_mcp_mgr_post_initialized(handle, "mcp_server"));
    TEST_ASSERT_EQUAL_INT(1, s_request_count);
    root = cJSON_Parse(s_last_request_message);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_EQUAL_STRING("notifications/initialized", cJSON_GetObjectItem(root, "method")->valuestring);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(root, "id"));
    TEST_ASSERT_NULL(cJSON_GetObjectItem(root, "params"));
    cJSON_Delete(root);

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_mgr_helper_ping_and_tasks_smoke", "[mcp][client][helpers][tasks]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport_with_request,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    pending_cb_result_t result = {0};

    mcp_test_reset_request_capture();
    ESP_ERROR_CHECK(esp_mcp_mgr_post_ping(handle, "mcp_server", pending_req_cb_capture, &result));
    TEST_ASSERT_EQUAL(1, result.called);
    TEST_ASSERT_EQUAL(0, result.error_code);
    TEST_ASSERT_EQUAL_STRING("ping", result.name);
    TEST_ASSERT_NOT_NULL(strstr(s_last_request_message, "\"method\":\"ping\""));

    mcp_test_reset_request_capture();
    ESP_ERROR_CHECK(esp_mcp_mgr_post_tasks_list(handle, "mcp_server", "cursor-1", pending_req_cb_capture, &result));
    TEST_ASSERT_NOT_NULL(strstr(s_last_request_message, "\"method\":\"tasks/list\""));
    TEST_ASSERT_NOT_NULL(strstr(s_last_request_message, "\"cursor\":\"cursor-1\""));

    mcp_test_reset_request_capture();
    ESP_ERROR_CHECK(esp_mcp_mgr_post_tasks_get(handle, "mcp_server", "task-123", pending_req_cb_capture, &result));
    TEST_ASSERT_NOT_NULL(strstr(s_last_request_message, "\"method\":\"tasks/get\""));
    TEST_ASSERT_NOT_NULL(strstr(s_last_request_message, "\"taskId\":\"task-123\""));

    mcp_test_reset_request_capture();
    ESP_ERROR_CHECK(esp_mcp_mgr_post_tasks_result(handle, "mcp_server", "task-123", pending_req_cb_capture, &result));
    TEST_ASSERT_NOT_NULL(strstr(s_last_request_message, "\"method\":\"tasks/result\""));
    TEST_ASSERT_NOT_NULL(strstr(s_last_request_message, "\"taskId\":\"task-123\""));

    mcp_test_reset_request_capture();
    ESP_ERROR_CHECK(esp_mcp_mgr_post_tasks_cancel(handle, "mcp_server", "task-123", pending_req_cb_capture, &result));
    TEST_ASSERT_NOT_NULL(strstr(s_last_request_message, "\"method\":\"tasks/cancel\""));
    TEST_ASSERT_NOT_NULL(strstr(s_last_request_message, "\"taskId\":\"task-123\""));

    memset(&result, 0, sizeof(result));
    mcp_test_reset_request_capture();
    ESP_ERROR_CHECK(esp_mcp_mgr_post_resources_subscribe(handle, "mcp_server", "device://status", pending_req_cb_capture, &result));
    TEST_ASSERT_EQUAL(1, result.called);
    TEST_ASSERT_EQUAL(0, result.error_code);
    TEST_ASSERT_NOT_NULL(strstr(s_last_request_message, "\"method\":\"resources/subscribe\""));
    TEST_ASSERT_NOT_NULL(strstr(s_last_request_message, "\"uri\":\"device://status\""));

    memset(&result, 0, sizeof(result));
    mcp_test_reset_request_capture();
    ESP_ERROR_CHECK(esp_mcp_mgr_post_resources_unsubscribe(handle, "mcp_server", "device://status", pending_req_cb_capture, &result));
    TEST_ASSERT_EQUAL(1, result.called);
    TEST_ASSERT_EQUAL(0, result.error_code);
    TEST_ASSERT_NOT_NULL(strstr(s_last_request_message, "\"method\":\"resources/unsubscribe\""));
    TEST_ASSERT_NOT_NULL(strstr(s_last_request_message, "\"uri\":\"device://status\""));

    memset(&result, 0, sizeof(result));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_mcp_mgr_post_resources_subscribe(handle, "mcp_server", NULL, pending_req_cb_capture, &result));
    TEST_ASSERT_EQUAL(0, result.called);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_mcp_mgr_post_resources_unsubscribe(handle, "mcp_server", "", pending_req_cb_capture, &result));

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_incoming_notification_handler_dispatches_protocol_notifications", "[mcp][protocol][2025][notification]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    incoming_notification_capture_t capture = {0};
    ESP_ERROR_CHECK(esp_mcp_set_incoming_notification_handler(mcp, incoming_notification_capture_cb, &capture));

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *roots_changed =
        "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/roots/list_changed\",\"params\":{}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server",
                                               (const uint8_t *)roots_changed,
                                               strlen(roots_changed),
                                               &outbuf, &outlen));
    TEST_ASSERT_NULL(outbuf);
    TEST_ASSERT_EQUAL(1, capture.called);
    TEST_ASSERT_EQUAL_STRING("notifications/roots/list_changed", capture.method);
    TEST_ASSERT_EQUAL_STRING("{}", capture.params);

    memset(&capture, 0, sizeof(capture));
    const char *progress =
        "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/progress\",\"params\":{\"progressToken\":\"tok-1\",\"progress\":1,\"total\":2}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server",
                                               (const uint8_t *)progress,
                                               strlen(progress),
                                               &outbuf, &outlen));
    TEST_ASSERT_NULL(outbuf);
    TEST_ASSERT_EQUAL(1, capture.called);
    TEST_ASSERT_EQUAL_STRING("notifications/progress", capture.method);
    TEST_ASSERT_NOT_NULL(strstr(capture.params, "\"progressToken\":\"tok-1\""));

    memset(&capture, 0, sizeof(capture));
    const char *task_status =
        "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/tasks/status\",\"params\":{\"taskId\":\"task-123\",\"status\":\"working\",\"pollInterval\":1000}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server",
                                               (const uint8_t *)task_status,
                                               strlen(task_status),
                                               &outbuf, &outlen));
    TEST_ASSERT_NULL(outbuf);
    TEST_ASSERT_EQUAL(1, capture.called);
    TEST_ASSERT_EQUAL_STRING("notifications/tasks/status", capture.method);
    TEST_ASSERT_NOT_NULL(strstr(capture.params, "\"taskId\":\"task-123\""));

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_client_incoming_request_and_notification_dispatch", "[mcp][client][protocol][2025]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport_with_request,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    incoming_request_capture_t req_capture = {
        .return_code = ESP_OK,
    };
    snprintf(req_capture.result_json, sizeof(req_capture.result_json),
             "%s", "{\"roots\":[{\"uri\":\"file:///workspace\",\"name\":\"workspace\"}]}");
    incoming_notification_capture_t notif_capture = {0};
    ESP_ERROR_CHECK(esp_mcp_set_incoming_request_handler(mcp, incoming_request_capture_cb, &req_capture));
    ESP_ERROR_CHECK(esp_mcp_set_incoming_notification_handler(mcp, incoming_notification_capture_cb, &notif_capture));

    pending_cb_result_t init_result = {0};
    esp_mcp_mgr_req_t init_req = {
        .ep_name = "mcp_server",
        .cb = pending_req_cb_capture,
        .user_ctx = &init_result,
    };
    init_req.u.init.protocol_version = "2025-11-25";
    init_req.u.init.name = "esp-test";
    init_req.u.init.version = "1.0.0";
    ESP_ERROR_CHECK(esp_mcp_mgr_post_info_init(handle, &init_req));
    TEST_ASSERT_EQUAL(1, init_result.called);
    TEST_ASSERT_EQUAL(0, init_result.error_code);

    mcp_test_reset_request_capture();
    const char *server_request =
        "{\"jsonrpc\":\"2.0\",\"id\":\"srv-roots\",\"method\":\"roots/list\",\"params\":{}}";
    ESP_ERROR_CHECK(esp_mcp_mgr_perform_handle(handle,
                                               (const uint8_t *)server_request,
                                               strlen(server_request)));
    TEST_ASSERT_EQUAL(1, req_capture.called);
    TEST_ASSERT_EQUAL_STRING("roots/list", req_capture.method);
    TEST_ASSERT_EQUAL_STRING("{}", req_capture.params);
    TEST_ASSERT_EQUAL(1, s_request_count);
    TEST_ASSERT_NOT_NULL(strstr(s_last_request_message, "\"id\":\"srv-roots\""));
    TEST_ASSERT_NOT_NULL(strstr(s_last_request_message, "\"result\":{\"roots\":["));

    mcp_test_reset_request_capture();
    const char *server_notification =
        "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/message\",\"params\":{\"level\":\"info\",\"data\":\"hello\"}}";
    ESP_ERROR_CHECK(esp_mcp_mgr_perform_handle(handle,
                                               (const uint8_t *)server_notification,
                                               strlen(server_notification)));
    TEST_ASSERT_EQUAL(1, notif_capture.called);
    TEST_ASSERT_EQUAL_STRING("notifications/message", notif_capture.method);
    TEST_ASSERT_NOT_NULL(strstr(notif_capture.params, "\"level\":\"info\""));
    TEST_ASSERT_EQUAL_INT(0, s_request_count);

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_client_incoming_request_error_mapping", "[mcp][client][protocol][2025][error]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport_with_request,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    pending_cb_result_t init_result = {0};
    esp_mcp_mgr_req_t init_req = {
        .ep_name = "mcp_server",
        .cb = pending_req_cb_capture,
        .user_ctx = &init_result,
    };
    init_req.u.init.protocol_version = "2025-11-25";
    init_req.u.init.name = "esp-test";
    init_req.u.init.version = "1.0.0";
    ESP_ERROR_CHECK(esp_mcp_mgr_post_info_init(handle, &init_req));
    TEST_ASSERT_EQUAL(1, init_result.called);

    incoming_request_capture_t req_capture = {
        .return_code = ESP_ERR_INVALID_ARG,
    };
    ESP_ERROR_CHECK(esp_mcp_set_incoming_request_handler(mcp, incoming_request_capture_cb, &req_capture));

    mcp_test_reset_request_capture();
    const char *bad_request =
        "{\"jsonrpc\":\"2.0\",\"id\":\"srv-bad\",\"method\":\"sampling/createMessage\",\"params\":{\"messages\":[]}}";
    ESP_ERROR_CHECK(esp_mcp_mgr_perform_handle(handle,
                                               (const uint8_t *)bad_request,
                                               strlen(bad_request)));
    TEST_ASSERT_EQUAL(1, req_capture.called);
    TEST_ASSERT_NOT_NULL(strstr(s_last_request_message, "\"id\":\"srv-bad\""));
    TEST_ASSERT_NOT_NULL(strstr(s_last_request_message, "\"error\":{\"code\":-32602"));

    req_capture.called = 0;
    req_capture.return_code = ESP_ERR_NOT_SUPPORTED;
    req_capture.method[0] = '\0';
    req_capture.params[0] = '\0';
    mcp_test_reset_request_capture();
    ESP_ERROR_CHECK(esp_mcp_mgr_perform_handle(handle,
                                               (const uint8_t *)bad_request,
                                               strlen(bad_request)));
    TEST_ASSERT_EQUAL(1, req_capture.called);
    TEST_ASSERT_NOT_NULL(strstr(s_last_request_message, "\"error\":{\"code\":-32601"));

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_protocol_2025_initialized_notification_gate", "[mcp][protocol][2025][gate]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *init_msg =
        "{\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2025-11-25\","
        "\"clientInfo\":{\"name\":\"test_client\",\"version\":\"1.0\"},\"capabilities\":{}},\"id\":\"init-gate\"}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)init_msg, strlen(init_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    const char *list_before_init_notif =
        "{\"jsonrpc\":\"2.0\",\"id\":\"list-before\",\"method\":\"tools/list\",\"params\":{}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server",
                                               (const uint8_t *)list_before_init_notif,
                                               strlen(list_before_init_notif),
                                               &outbuf, &outlen));
    cJSON *before_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(before_root);
    cJSON *before_err = cJSON_GetObjectItem(before_root, "error");
    TEST_ASSERT_NOT_NULL(before_err);
    cJSON *before_data = cJSON_GetObjectItem(before_err, "data");
    TEST_ASSERT_TRUE(cJSON_IsObject(before_data));
    cJSON *before_reason = cJSON_GetObjectItem(before_data, "reason");
    TEST_ASSERT_TRUE(cJSON_IsString(before_reason));
    TEST_ASSERT_NOT_NULL(strstr(before_reason->valuestring, "initialized"));
    cJSON_Delete(before_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    const char *init_notif = "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\",\"params\":{}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server",
                                               (const uint8_t *)init_notif,
                                               strlen(init_notif),
                                               &outbuf, &outlen));
    TEST_ASSERT_NULL(outbuf);

    const char *list_after_init_notif =
        "{\"jsonrpc\":\"2.0\",\"id\":\"list-after\",\"method\":\"tools/list\",\"params\":{}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server",
                                               (const uint8_t *)list_after_init_notif,
                                               strlen(list_after_init_notif),
                                               &outbuf, &outlen));
    cJSON *after_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(after_root);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(after_root, "error"));
    cJSON_Delete(after_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_protocol_duplicate_request_id_per_session", "[mcp][protocol][session]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *msg = "{\"jsonrpc\":\"2.0\",\"id\":\"dup-req\",\"method\":\"tools/list\",\"params\":{}}";

    ESP_ERROR_CHECK(esp_mcp_mgr_set_request_context(handle, "sess-a", "user-a", true));
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)msg, strlen(msg), &outbuf, &outlen));
    cJSON *root_first = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(root_first);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(root_first, "error"));
    cJSON_Delete(root_first);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)msg, strlen(msg), &outbuf, &outlen));
    cJSON *root_second = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(root_second);
    cJSON *err = cJSON_GetObjectItem(root_second, "error");
    TEST_ASSERT_NOT_NULL(err);
    cJSON *err_data = cJSON_GetObjectItem(err, "data");
    TEST_ASSERT_TRUE(cJSON_IsObject(err_data));
    cJSON *reason_item = cJSON_GetObjectItem(err_data, "reason");
    TEST_ASSERT_TRUE(cJSON_IsString(reason_item));
    TEST_ASSERT_NOT_NULL(strstr(reason_item->valuestring, "duplicate request id"));
    cJSON_Delete(root_second);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    ESP_ERROR_CHECK(esp_mcp_mgr_set_request_context(handle, "sess-b", "user-b", true));
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)msg, strlen(msg), &outbuf, &outlen));
    cJSON *root_third = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(root_third);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(root_third, "error"));
    cJSON_Delete(root_third);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_mgr_emit_message_and_legacy_destroy_api", "[mcp][mgr]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    mcp_test_reset_emit_capture();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mcp_mgr_emit_message(0, "s", "{\"jsonrpc\":\"2.0\"}"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mcp_mgr_emit_message(handle, "s", NULL));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_mgr_emit_message(handle, "sess-x",
                                                       "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/message\",\"params\":{}}"));
    TEST_ASSERT_GREATER_THAN_INT(0, s_emit_count);
    TEST_ASSERT_EQUAL_STRING("sess-x", s_last_emit_session_id);
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"method\":\"notifications/message\""));

    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_mcp_mgr_req_destroy_response_by_id(handle, 1));

    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_engine_notify_log_and_progress", "[mcp][notify]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    mcp_test_reset_emit_capture();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mcp_notify_log_message(NULL, "info", NULL, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_notify_log_message(mcp, "info", "ut", "{\"k\":1}"));
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"method\":\"notifications/message\""));
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"level\":\"info\""));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mcp_notify_log_message(mcp, "verbose", "ut", "{\"k\":1}"));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mcp_notify_progress(mcp, NULL, 0.5, 1.0, "half"));
    ESP_ERROR_CHECK(esp_mcp_mgr_set_request_context(handle, "sess-progress", "user-p", true));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_notify_progress(mcp, "tok", 0.5, 1.0, "half"));
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"method\":\"notifications/progress\""));
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"progressToken\":\"tok\""));
    TEST_ASSERT_EQUAL_STRING("sess-progress", s_last_emit_session_id);
    ESP_ERROR_CHECK(esp_mcp_mgr_set_request_context(handle, NULL, NULL, true));

    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_completion_provider_create_destroy", "[completion]")
{
    esp_mcp_completion_provider_t *provider = esp_mcp_completion_provider_create(test_completion_cb, NULL);
    TEST_ASSERT_NOT_NULL(provider);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_completion_provider_destroy(provider));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mcp_completion_provider_destroy(NULL));
}

TEST_CASE("test_tool_create_ex_and_rich_result", "[tool][protocol][ex]")
{
    mcp_test_reset_ex_cb_flags();
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_tool_t *tool = esp_mcp_tool_create_ex("rich.tool", "Rich Tool", "Extended callback tool", test_tool_callback_ex_rich);
    TEST_ASSERT_NOT_NULL(tool);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_tool_set_title(tool, "Rich Tool V2"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_tool_set_icons_json(tool, "[{\"src\":\"data:image/png;base64,AAAA\"}]"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_tool_set_output_schema_json(tool, "{\"type\":\"object\"}"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_tool_set_annotations_json(tool, "{\"readOnlyHint\":true}"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_tool_set_task_support(tool, "optional"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mcp_tool_set_task_support(tool, "sometimes"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_tool_set_task_support(tool, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mcp_tool_set_title(NULL, "x"));
    TEST_ASSERT_NULL(esp_mcp_tool_create_ex(NULL, "t", "d", test_tool_callback_ex_rich));

    esp_mcp_property_t *arr_prop = esp_mcp_property_create_with_array("arr", "[]");
    esp_mcp_property_t *obj_prop = esp_mcp_property_create_with_object("obj", "{}");
    TEST_ASSERT_NOT_NULL(arr_prop);
    TEST_ASSERT_NOT_NULL(obj_prop);
    ESP_ERROR_CHECK(esp_mcp_tool_add_property(tool, arr_prop));
    ESP_ERROR_CHECK(esp_mcp_tool_add_property(tool, obj_prop));
    ESP_ERROR_CHECK(esp_mcp_add_tool(mcp, tool));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *list_msg = "{\"jsonrpc\":\"2.0\",\"id\":\"rich-list\",\"method\":\"tools/list\",\"params\":{}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)list_msg, strlen(list_msg), &outbuf, &outlen));
    cJSON *list_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(list_root);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(list_root, "error"));
    cJSON *list_result = cJSON_GetObjectItem(list_root, "result");
    TEST_ASSERT_NOT_NULL(list_result);
    cJSON *tools = cJSON_GetObjectItem(list_result, "tools");
    TEST_ASSERT_TRUE(cJSON_IsArray(tools));
    TEST_ASSERT_NOT_NULL(strstr((const char *)outbuf, "\"icons\""));
    TEST_ASSERT_NOT_NULL(strstr((const char *)outbuf, "\"outputSchema\""));
    TEST_ASSERT_NOT_NULL(strstr((const char *)outbuf, "\"annotations\""));
    cJSON_Delete(list_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    const char *call_msg =
        "{\"jsonrpc\":\"2.0\",\"id\":\"rich-call\",\"method\":\"tools/call\",\"params\":{\"name\":\"rich.tool\",\"arguments\":{\"arr\":[1,2],\"obj\":{\"k\":\"v\"}}}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)call_msg, strlen(call_msg), &outbuf, &outlen));
    cJSON *call_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(call_root);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(call_root, "error"));
    cJSON *call_result = cJSON_GetObjectItem(call_root, "result");
    TEST_ASSERT_NOT_NULL(call_result);
    cJSON *content = cJSON_GetObjectItem(call_result, "content");
    TEST_ASSERT_TRUE(cJSON_IsArray(content));
    TEST_ASSERT_TRUE(cJSON_GetArraySize(content) >= 6);
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(call_result, "structuredContent"));
    TEST_ASSERT_TRUE(s_ex_cb_arr_ok);
    TEST_ASSERT_TRUE(s_ex_cb_obj_ok);
    cJSON_Delete(call_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    ESP_ERROR_CHECK(esp_mcp_remove_tool(mcp, tool));
    ESP_ERROR_CHECK(esp_mcp_tool_destroy(tool));
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_property_create_generic_and_tool_result_builder", "[property][tool][coverage]")
{
    esp_mcp_property_t *prop = esp_mcp_property_create("raw.flag", ESP_MCP_PROPERTY_TYPE_BOOLEAN);
    TEST_ASSERT_NOT_NULL(prop);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_property_destroy(prop));
    TEST_ASSERT_NULL(esp_mcp_property_create(NULL, ESP_MCP_PROPERTY_TYPE_BOOLEAN));

    esp_mcp_tool_result_t *result = esp_mcp_tool_result_create();
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_tool_result_add_text(result, "ok"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_tool_result_set_structured_json(result, "{\"ok\":true}"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mcp_tool_result_add_text(NULL, "x"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mcp_tool_result_set_structured_json(result, "[]"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_tool_result_destroy(result));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mcp_tool_result_destroy(NULL));
}

TEST_CASE("test_engine_direct_handle_message_and_context", "[mcp][engine][coverage]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mcp_set_request_context(NULL, "sess-z", "user-z", true));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_set_request_context(mcp, "sess-z", "user-z", true));
    TEST_ASSERT_EQUAL_STRING("sess-z", esp_mcp_get_request_session_id(mcp));

    const char *init_msg = MCP_TEST_INIT_DEFAULT_REQ_JSON;
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_handle_message(mcp, (const uint8_t *)init_msg, strlen(init_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);
    TEST_ASSERT_GREATER_THAN_INT(0, outlen);

    cJSON *root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(root, "result"));
    cJSON_Delete(root);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_free_response(mcp, outbuf));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_free_response(mcp, NULL));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mcp_clear_session_state(mcp, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_clear_session_state(mcp, "sess-z"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_set_request_context(mcp, NULL, NULL, false));
    TEST_ASSERT_NULL(esp_mcp_get_request_session_id(mcp));

    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_initialize_response_custom_server_info_and_instructions", "[mcp][protocol][initialize][coverage]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mcp_set_server_info(NULL, "t", "d", "[]", "https://example.com"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mcp_set_instructions(NULL, "Do this"));
    ESP_ERROR_CHECK(esp_mcp_set_server_info(mcp,
                                            "ESP MCP Server",
                                            "A test server",
                                            "[{\"src\":\"https://example.com/icon.png\"}]",
                                            "https://example.com"));
    ESP_ERROR_CHECK(esp_mcp_set_instructions(mcp, "Use safe mode."));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    const char *init_msg = MCP_TEST_INIT_DEFAULT_REQ_JSON;
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)init_msg, strlen(init_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    cJSON *root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(root, "error"));
    cJSON *result = cJSON_GetObjectItem(root, "result");
    TEST_ASSERT_NOT_NULL(result);
    cJSON *server_info = cJSON_GetObjectItem(result, "serverInfo");
    TEST_ASSERT_NOT_NULL(server_info);

    cJSON *title = cJSON_GetObjectItem(server_info, "title");
    TEST_ASSERT_TRUE(cJSON_IsString(title));
    TEST_ASSERT_EQUAL_STRING("ESP MCP Server", title->valuestring);

    cJSON *description = cJSON_GetObjectItem(server_info, "description");
    TEST_ASSERT_TRUE(cJSON_IsString(description));
    TEST_ASSERT_EQUAL_STRING("A test server", description->valuestring);

    cJSON *website = cJSON_GetObjectItem(server_info, "websiteUrl");
    TEST_ASSERT_TRUE(cJSON_IsString(website));
    TEST_ASSERT_EQUAL_STRING("https://example.com", website->valuestring);

    cJSON *icons = cJSON_GetObjectItem(server_info, "icons");
    TEST_ASSERT_TRUE(cJSON_IsArray(icons));
    TEST_ASSERT_GREATER_THAN_INT(0, cJSON_GetArraySize(icons));

    cJSON *instructions = cJSON_GetObjectItem(result, "instructions");
    TEST_ASSERT_TRUE(cJSON_IsString(instructions));
    TEST_ASSERT_EQUAL_STRING("Use safe mode.", instructions->valuestring);

    cJSON_Delete(root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_server_request_sampling_with_tools_emits_payload", "[mcp][protocol][2025][sampling][coverage]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *init_with_caps =
        "{\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2025-11-25\","
        "\"clientInfo\":{\"name\":\"test_client\",\"version\":\"1.0\"},"
        "\"capabilities\":{\"sampling\":{\"tools\":{}}}},\"id\":1}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server",
                                               (const uint8_t *)init_with_caps,
                                               strlen(init_with_caps),
                                               &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    char request_id[40] = {0};
    mcp_test_reset_emit_capture();
    TEST_ASSERT_EQUAL(ESP_OK,
                      esp_mcp_request_sampling_with_tools(
                          mcp,
                          "sess-sampling",
                          "{\"messages\":[]}",
                          "[{\"name\":\"search\",\"description\":\"Search tool\",\"inputSchema\":{\"type\":\"object\"}}]",
                          "required",
                          NULL,
                          NULL,
                          1000,
                          request_id,
                          sizeof(request_id)));
    TEST_ASSERT_NOT_EMPTY(request_id);
    TEST_ASSERT_GREATER_THAN_INT(0, s_emit_count);
    TEST_ASSERT_EQUAL_STRING("sess-sampling", s_last_emit_session_id);
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"method\":\"sampling/createMessage\""));
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"tools\":["));
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, "\"toolChoice\":{\"mode\":\"required\"}"));
    TEST_ASSERT_NOT_NULL(strstr(s_last_emit_message, request_id));

    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_resources_and_prompts_icons_metadata_in_list", "[mcp][protocol][2025][resources][prompts][coverage]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    esp_mcp_resource_t *res = esp_mcp_resource_create("device://meta", "device.meta", "Device Meta",
                                                      "meta resource", "application/json", test_resource_read_cb, NULL);
    TEST_ASSERT_NOT_NULL(res);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mcp_resource_set_icons(NULL, "[]"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_resource_set_size(res, 4096));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_resource_set_icons(res, "[{\"src\":\"https://example.com/res.png\"}]"));
    ESP_ERROR_CHECK(esp_mcp_add_resource(mcp, res));

    esp_mcp_prompt_t *prompt = esp_mcp_prompt_create("meta.prompt", "Meta Prompt", "Prompt with icon",
                                                     "[{\"role\":\"user\",\"content\":{\"type\":\"text\",\"text\":\"meta\"}}]",
                                                     NULL, NULL);
    TEST_ASSERT_NOT_NULL(prompt);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mcp_prompt_set_icons(NULL, "[]"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_prompt_set_icons(prompt, "[{\"src\":\"https://example.com/prompt.png\"}]"));
    ESP_ERROR_CHECK(esp_mcp_add_prompt(mcp, prompt));

    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    const char *res_list_msg = "{\"jsonrpc\":\"2.0\",\"id\":\"meta-res-list\",\"method\":\"resources/list\",\"params\":{}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server",
                                               (const uint8_t *)res_list_msg,
                                               strlen(res_list_msg),
                                               &outbuf, &outlen));
    cJSON *res_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(res_root);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(res_root, "error"));
    cJSON *res_result = cJSON_GetObjectItem(res_root, "result");
    TEST_ASSERT_NOT_NULL(res_result);
    cJSON *resources = cJSON_GetObjectItem(res_result, "resources");
    TEST_ASSERT_TRUE(cJSON_IsArray(resources));
    cJSON *res_entry = NULL;
    cJSON_ArrayForEach(res_entry, resources) {
        cJSON *uri = cJSON_GetObjectItem(res_entry, "uri");
        if (cJSON_IsString(uri) && uri->valuestring && strcmp(uri->valuestring, "device://meta") == 0) {
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(res_entry);
    cJSON *size_obj = cJSON_GetObjectItem(res_entry, "size");
    TEST_ASSERT_TRUE(cJSON_IsNumber(size_obj));
    TEST_ASSERT_EQUAL_INT(4096, size_obj->valueint);
    cJSON *res_icons = cJSON_GetObjectItem(res_entry, "icons");
    TEST_ASSERT_TRUE(cJSON_IsArray(res_icons));
    TEST_ASSERT_GREATER_THAN_INT(0, cJSON_GetArraySize(res_icons));
    cJSON_Delete(res_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));
    outbuf = NULL;

    const char *prompt_list_msg = "{\"jsonrpc\":\"2.0\",\"id\":\"meta-prompt-list\",\"method\":\"prompts/list\",\"params\":{}}";
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server",
                                               (const uint8_t *)prompt_list_msg,
                                               strlen(prompt_list_msg),
                                               &outbuf, &outlen));
    cJSON *prompt_root = parse_json_response(outbuf, outlen);
    TEST_ASSERT_NOT_NULL(prompt_root);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(prompt_root, "error"));
    cJSON *prompt_result = cJSON_GetObjectItem(prompt_root, "result");
    TEST_ASSERT_NOT_NULL(prompt_result);
    cJSON *prompts = cJSON_GetObjectItem(prompt_result, "prompts");
    TEST_ASSERT_TRUE(cJSON_IsArray(prompts));
    cJSON *prompt_entry = NULL;
    cJSON_ArrayForEach(prompt_entry, prompts) {
        cJSON *name = cJSON_GetObjectItem(prompt_entry, "name");
        if (cJSON_IsString(name) && name->valuestring && strcmp(name->valuestring, "meta.prompt") == 0) {
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(prompt_entry);
    cJSON *prompt_icons = cJSON_GetObjectItem(prompt_entry, "icons");
    TEST_ASSERT_TRUE(cJSON_IsArray(prompt_icons));
    TEST_ASSERT_GREATER_THAN_INT(0, cJSON_GetArraySize(prompt_icons));
    cJSON_Delete(prompt_root);
    ESP_ERROR_CHECK(esp_mcp_mgr_req_destroy_response(handle, outbuf));

    ESP_ERROR_CHECK(esp_mcp_remove_prompt(mcp, prompt));
    ESP_ERROR_CHECK(esp_mcp_prompt_destroy(prompt));
    ESP_ERROR_CHECK(esp_mcp_remove_resource(mcp, res));
    ESP_ERROR_CHECK(esp_mcp_resource_destroy(res));
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

/*
 * MCP spec reference checks (2025-11-25 baseline; OAuth/stdio out of scope).
 * - Ping: https://modelcontextprotocol.io/specification/2025-11-25/basic/utilities/ping.md
 * - Lifecycle: https://modelcontextprotocol.io/specification/2025-11-25/basic/lifecycle.md
 */
TEST_CASE("test_spec_ref_2025_11_25_ping_result_empty_object", "[mcp][spec][2025-11-25][ping]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    const char *ping_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"ping\",\"params\":{},\"id\":501}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)ping_msg, strlen(ping_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_FALSE(is_error);
    cJSON *result = cJSON_GetObjectItem(root, "result");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE(cJSON_IsObject(result));
    TEST_ASSERT_NULL(result->child);

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

TEST_CASE("test_spec_ref_2025_11_25_ping_omitted_params_field", "[mcp][spec][2025-11-25][ping]")
{
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    int test_config = 12345;
    esp_mcp_mgr_config_t config = {
        .transport = mock_transport,
        .config = &test_config,
        .instance = mcp
    };
    esp_mcp_mgr_handle_t handle = 0;
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(handle, "mcp_server", mcp));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(handle));
    mcp_test_send_initialize(handle);

    const char *ping_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"ping\",\"id\":502}";
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    MCP_TEST_REQ_HANDLE(esp_mcp_mgr_req_handle(handle, "mcp_server", (const uint8_t *)ping_msg, strlen(ping_msg), &outbuf, &outlen));
    TEST_ASSERT_NOT_NULL(outbuf);

    bool is_error = false;
    cJSON *root = validate_and_parse_response(outbuf, outlen, &is_error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_FALSE(is_error);
    cJSON *result = cJSON_GetObjectItem(root, "result");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE(cJSON_IsObject(result));
    TEST_ASSERT_NULL(result->child);

    cJSON_Delete(root);
    esp_mcp_mgr_req_destroy_response(handle, outbuf);
    ESP_ERROR_CHECK(esp_mcp_mgr_stop(handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_deinit(handle));
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
}

static size_t before_free_8bit;
static size_t before_free_32bit;

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n", type, before_free, after_free, delta);
    ssize_t threshold = relax_memory_check ? (-90000) : TEST_MEMORY_LEAK_THRESHOLD;
    TEST_ASSERT_MESSAGE(delta >= threshold, "memory leak");
}

void setUp(void)
{
    relax_memory_check = false;
    mcp_test_reset_emit_capture();
    mcp_test_reset_request_capture();
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
    relax_memory_check = false;
}

void app_main(void)
{
    printf("MCP SDK TEST \n");
    unity_run_menu();
}
