/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "unity.h"
#include "esp_mcp_engine.h"
#include "esp_mcp_mgr.h"
#include "esp_mcp_tool.h"
#include "esp_mcp_property.h"
#include "esp_mcp_data.h"

static const char *TAG = "mcp_test";

#define TEST_MEMORY_LEAK_THRESHOLD (-3000)

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

typedef struct {
    esp_mcp_t *mcp;
    int thread_id;
} thread_test_params_t;

static void thread_test_task(void *pvParameters)
{
    thread_test_params_t *params = (thread_test_params_t *)pvParameters;
    esp_mcp_t *mcp = params->server;
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

        // Remove tool (if it was added)
        if (ret == ESP_OK) {
            ret = esp_mcp_remove_tool(mcp, tool);
            TEST_ASSERT_EQUAL(ESP_OK, ret);
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
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    thread_test_mutex = xSemaphoreCreateMutex();
    TEST_ASSERT_NOT_NULL(thread_test_mutex);
    thread_test_counter = 0;

    // Create multiple threads that add/remove tools concurrently
    TaskHandle_t tasks[THREAD_TEST_THREADS];
    thread_test_params_t params[THREAD_TEST_THREADS];

    for (int i = 0; i < THREAD_TEST_THREADS; i++) {
        params[i].mcp = mcp;
        params[i].thread_id = i;
        xTaskCreate(thread_test_task, "thread_test", 4096, &params[i], 5, &tasks[i]);
    }

    // Wait for all threads to complete
    for (int i = 0; i < THREAD_TEST_THREADS; i++) {
        vTaskDelay(5000 / portTICK_PERIOD_MS); // Wait up to 5 seconds
    }

    // Verify all threads completed
    xSemaphoreTake(thread_test_mutex, portMAX_DELAY);
    int final_counter = thread_test_counter;
    xSemaphoreGive(thread_test_mutex);

    ESP_LOGI(TAG, "Thread test completed: %d iterations", final_counter);
    TEST_ASSERT_EQUAL(THREAD_TEST_ITERATIONS * THREAD_TEST_THREADS, final_counter);

    vSemaphoreDelete(thread_test_mutex);
    ESP_ERROR_CHECK(esp_mcp_destroy(mcp));
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

static const esp_mcp_transport_t mock_transport = {
    .init = mock_transport_init,
    .deinit = mock_transport_deinit,
    .start = mock_transport_start,
    .stop = mock_transport_stop,
    .create_config = mock_transport_create_config,
    .delete_config = mock_transport_delete_config,
    .register_endpoint = mock_transport_register_endpoint,
    .unregister_endpoint = mock_transport_unregister_endpoint,
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
    const char *init_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"params\":{},\"id\":1}";
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

    const char *msg = "{\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"params\":{},\"id\":1}";
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

    const char *msg = "{\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"params\":{},\"id\":1}";
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

    const char *init_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"params\":{},\"id\":1}";
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
