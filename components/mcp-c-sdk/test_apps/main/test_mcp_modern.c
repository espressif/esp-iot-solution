/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "unity.h"
#include "esp_mcp_engine.h"

#define MODERN_META "\"_meta\":{\"io.modelcontextprotocol/protocolVersion\":\"2026-07-28\",\"io.modelcontextprotocol/clientCapabilities\":{}}"

static cJSON *modern_request(esp_mcp_t *mcp, const char *request)
{
    uint8_t *response = NULL;
    uint16_t length = 0;
    (void)esp_mcp_handle_message(mcp, (const uint8_t *)request, strlen(request), &response, &length);
    if (!response) {
        return NULL;
    }
    cJSON *json = cJSON_ParseWithLength((const char *)response, length);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_free_response(mcp, response));
    TEST_ASSERT_NOT_NULL(json);
    return json;
}

static esp_mcp_t *modern_engine(void)
{
    esp_mcp_t *mcp = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_create(&mcp));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_set_xiaozhi_compatibility(mcp, true));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_set_request_context(mcp, "modern-test", "test-peer", true));
    return mcp;
}

static void assert_error(cJSON *response, int code)
{
    TEST_ASSERT_NOT_NULL(response);
    cJSON *error = cJSON_GetObjectItem(response, "error");
    TEST_ASSERT_NOT_NULL(error);
    TEST_ASSERT_EQUAL(code, cJSON_GetObjectItem(error, "code")->valueint);
    cJSON_Delete(response);
}

static void assert_error_message(cJSON *response, int code, const char *message)
{
    TEST_ASSERT_NOT_NULL(response);
    cJSON *error = cJSON_GetObjectItem(response, "error");
    TEST_ASSERT_TRUE(cJSON_IsObject(error));
    TEST_ASSERT_EQUAL(code, cJSON_GetObjectItem(error, "code")->valueint);
    TEST_ASSERT_TRUE(cJSON_IsString(cJSON_GetObjectItem(error, "message")));
    TEST_ASSERT_EQUAL_STRING(message, cJSON_GetObjectItem(error, "message")->valuestring);
    cJSON_Delete(response);
}

static esp_err_t context_tool(const esp_mcp_property_list_t *properties,
                              const esp_mcp_request_context_t *context,
                              esp_mcp_tool_result_t *result, void *user_ctx)
{
    int *calls = user_ctx;
    (*calls)++;
    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_EQUAL_STRING("2026-07-28", context->protocol_version);
    TEST_ASSERT_EQUAL_STRING("modern-test", context->session_id);
    TEST_ASSERT_EQUAL_STRING("test-peer", context->auth_subject);
    TEST_ASSERT_TRUE(context->is_authenticated);
    TEST_ASSERT_NOT_NULL(context->meta_json);
    TEST_ASSERT_EQUAL(7, esp_mcp_property_list_get_property_int(properties, "count"));
    TEST_ASSERT_TRUE(esp_mcp_property_list_get_property_bool(properties, "enabled"));
    return esp_mcp_tool_result_add_text(result, "done");
}

TEST_CASE("modern discovery works before initialize", "[mcp][modern]")
{
    esp_mcp_t *mcp = modern_engine();
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_set_extensions_json(mcp, "{\"test/extension\":{}}"));
    cJSON *response = modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":\"discover\",\"method\":\"server/discover\",\"params\":{" MODERN_META "}}");
    cJSON *result = cJSON_GetObjectItem(response, "result");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("complete", cJSON_GetObjectItem(result, "resultType")->valuestring);
    TEST_ASSERT_EQUAL(60000, cJSON_GetObjectItem(result, "ttlMs")->valueint);
    TEST_ASSERT_EQUAL_STRING("private", cJSON_GetObjectItem(result, "cacheScope")->valuestring);
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(result, "supportedVersions"));
    cJSON *meta = cJSON_GetObjectItem(result, "_meta");
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(meta, "io.modelcontextprotocol/serverInfo"));
    TEST_ASSERT_NULL(cJSON_GetObjectItem(meta, "io.modelcontextprotocol/clientCapabilities"));
    cJSON_Delete(response);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_destroy(mcp));
}

TEST_CASE("modern metadata validation and legacy lifecycle remain independent", "[mcp][modern]")
{
    esp_mcp_t *mcp = modern_engine();
    cJSON *legacy = modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}");
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(legacy, "result"));
    cJSON_Delete(legacy);
    assert_error(modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"server/discover\",\"params\":{}}"), -32602);
    assert_error(modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\",\"params\":{\"_meta\":{\"io.modelcontextprotocol/protocolVersion\":\"2099-01-01\",\"io.modelcontextprotocol/clientCapabilities\":{}}}}"), -32022);
    cJSON *response = modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\",\"params\":{" MODERN_META "}}");
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(response, "result"));
    cJSON_Delete(response);
    /* Both Xiaozhi revisions are stateless and may reuse request IDs. */
    response = modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}");
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(response, "result"));
    cJSON_Delete(response);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_destroy(mcp));
}

TEST_CASE("modern tool arguments use defaults and reject coercion", "[mcp][modern]")
{
    esp_mcp_t *mcp = modern_engine();
    int calls = 0;
    esp_mcp_tool_t *tool = esp_mcp_tool_create_with_context("test", "test", context_tool, &calls);
    TEST_ASSERT_NOT_NULL(tool);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_tool_add_property(tool, esp_mcp_property_create_with_int("count", 7)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_tool_add_property(tool, esp_mcp_property_create("enabled", ESP_MCP_PROPERTY_TYPE_BOOLEAN)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_add_tool(mcp, tool));
    const char *invalid[] = {
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"test\"," MODERN_META "}}",
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"test\",\"arguments\":{\"enabled\":1}," MODERN_META "}}",
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"test\",\"arguments\":{\"enabled\":true,\"count\":1.5}," MODERN_META "}}",
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        assert_error(modern_request(mcp, invalid[i]), -32602);
    }
    TEST_ASSERT_EQUAL(0, calls);
    cJSON *response = modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"test\",\"arguments\":{\"enabled\":true}," MODERN_META "}}");
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(response, "result"));
    TEST_ASSERT_EQUAL(1, calls);
    cJSON_Delete(response);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_destroy(mcp));
}

static esp_mcp_value_t unused_tool(const esp_mcp_property_list_t *properties)
{
    (void)properties;
    return esp_mcp_value_create_bool(true);
}

TEST_CASE("modern tool visibility capabilities and string bounds", "[mcp][modern]")
{
    esp_mcp_t *mcp = modern_engine();
    esp_mcp_tool_t *tool = esp_mcp_tool_create("private", "test", unused_tool);
    TEST_ASSERT_NOT_NULL(tool);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_tool_set_user_only(tool, true));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_tool_set_required_capabilities_json(tool, "{\"extensions\":{\"test/capability\":{}}}"));
    esp_mcp_property_t *text = esp_mcp_property_create("text", ESP_MCP_PROPERTY_TYPE_STRING);
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_property_set_max_length(text, 3));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_tool_add_property(tool, text));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_add_tool(mcp, tool));
    cJSON *response = modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\",\"params\":{" MODERN_META "}}");
    TEST_ASSERT_EQUAL(0, cJSON_GetArraySize(cJSON_GetObjectItem(cJSON_GetObjectItem(response, "result"), "tools")));
    cJSON_Delete(response);
    response = modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\",\"params\":{\"withUserTools\":true," MODERN_META "}}");
    cJSON *listed = cJSON_GetArrayItem(cJSON_GetObjectItem(cJSON_GetObjectItem(response, "result"), "tools"), 0);
    TEST_ASSERT_NOT_NULL(listed);
    cJSON *schema = cJSON_GetObjectItem(cJSON_GetObjectItem(listed, "inputSchema"), "properties");
    TEST_ASSERT_EQUAL(3, cJSON_GetObjectItem(cJSON_GetObjectItem(schema, "text"), "maxLength")->valueint);
    cJSON_Delete(response);
    assert_error(modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"private\",\"arguments\":{\"text\":\"ok\"}," MODERN_META "}}"), -32021);
    assert_error(modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"private\",\"arguments\":{\"text\":\"long\"},\"_meta\":{\"io.modelcontextprotocol/protocolVersion\":\"2026-07-28\",\"io.modelcontextprotocol/clientCapabilities\":{\"extensions\":{\"test/capability\":{}}}}}}"), -32602);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_destroy(mcp));
}

TEST_CASE("modern ids round trip and notifications do not reply", "[mcp][modern]")
{
    esp_mcp_t *mcp = modern_engine();
    cJSON *response = modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":9007199254740991,\"method\":\"tools/list\",\"params\":{" MODERN_META "}}");
    TEST_ASSERT_EQUAL_DOUBLE(9007199254740991.0, cJSON_GetObjectItem(response, "id")->valuedouble);
    cJSON_Delete(response);
    assert_error(modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1.5,\"method\":\"tools/list\",\"params\":{" MODERN_META "}}"), -32600);
    response = modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1.5,\"method\":\"tools/call\",\"params\":{\"_meta\":false}}");
    TEST_ASSERT_TRUE(cJSON_IsNull(cJSON_GetObjectItem(response, "id")));
    assert_error(response, -32600);
    response = modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":42}");
    TEST_ASSERT_EQUAL(42, cJSON_GetObjectItem(response, "id")->valueint);
    assert_error(response, -32600);
    assert_error(modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"server/discover\",\"params\":false}"), -32602);
    TEST_ASSERT_NULL(modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"absent\"," MODERN_META "}}"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_destroy(mcp));
}

static esp_err_t reject_metadata(const esp_mcp_request_context_t *context, void *user_ctx)
{
    (void)user_ctx;
    return context->protocol_version ? ESP_ERR_INVALID_ARG : ESP_OK;
}

TEST_CASE("modern validators reject bad extension values and preserve exact error ids", "[mcp][modern]")
{
    esp_mcp_t *mcp = modern_engine();
    esp_mcp_tool_t *tool = esp_mcp_tool_create("extension", "test", unused_tool);
    TEST_ASSERT_NOT_NULL(tool);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_tool_set_required_capabilities_json(tool, "{\"extensions\":{\"test/extension\":{}}}"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_tool_set_request_validator(tool, reject_metadata, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_add_tool(mcp, tool));
    cJSON *response = modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":9007199254740991,\"method\":\"tools/call\",\"params\":{\"name\":\"extension\",\"_meta\":{\"io.modelcontextprotocol/protocolVersion\":\"2026-07-28\",\"io.modelcontextprotocol/clientCapabilities\":{\"extensions\":{\"test/extension\":false}}}}}");
    TEST_ASSERT_EQUAL_DOUBLE(9007199254740991.0, cJSON_GetObjectItem(response, "id")->valuedouble);
    assert_error(response, -32602);
    response = modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"server/discover\",\"params\":{" MODERN_META "}}");
    cJSON *caps = cJSON_GetObjectItem(cJSON_GetObjectItem(response, "result"), "capabilities");
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(cJSON_GetObjectItem(caps, "extensions"), "test/extension"));
    cJSON_Delete(response);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_destroy(mcp));
}

TEST_CASE("modern pagination includes envelope budget and skips hidden tails", "[mcp][modern]")
{
    esp_mcp_t *mcp = modern_engine();
    char *description = malloc(5001);
    TEST_ASSERT_NOT_NULL(description);
    memset(description, 'd', 5000);
    description[5000] = '\0';
    for (unsigned i = 0; i < 3; i++) {
        char name[16];
        snprintf(name, sizeof(name), "tool-%u", i);
        esp_mcp_tool_t *tool = esp_mcp_tool_create(name, description, unused_tool);
        TEST_ASSERT_NOT_NULL(tool);
        if (i == 2) {
            TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_tool_set_user_only(tool, true));
        }
        TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_add_tool(mcp, tool));
    }
    free(description);
    char request[1024];
    snprintf(request, sizeof(request), "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\",\"params\":{\"limit\":1," MODERN_META "}}");
    cJSON *response = modern_request(mcp, request);
    cJSON *result = cJSON_GetObjectItem(response, "result");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL(1, cJSON_GetArraySize(cJSON_GetObjectItem(result, "tools")));
    char cursor[128];
    snprintf(cursor, sizeof(cursor), "%s", cJSON_GetObjectItem(result, "nextCursor")->valuestring);
    cJSON_Delete(response);
    snprintf(request, sizeof(request), "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\",\"params\":{\"limit\":1,\"cursor\":\"%s\"," MODERN_META "}}", cursor);
    response = modern_request(mcp, request);
    result = cJSON_GetObjectItem(response, "result");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL(1, cJSON_GetArraySize(cJSON_GetObjectItem(result, "tools")));
    TEST_ASSERT_NULL(cJSON_GetObjectItem(result, "nextCursor"));
    cJSON_Delete(response);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_destroy(mcp));
}

TEST_CASE("modern pagination preserves long cursors and full response size", "[mcp][modern]")
{
    esp_mcp_t *mcp = modern_engine();
    char name[340];
    char *description = malloc(3701);
    TEST_ASSERT_NOT_NULL(description);
    memset(description, 'd', 3700);
    description[3700] = '\0';
    for (unsigned i = 0; i < 3; ++i) {
        memset(name, 'a' + i, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        esp_mcp_tool_t *tool = esp_mcp_tool_create(name, description, unused_tool);
        TEST_ASSERT_NOT_NULL(tool);
        TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_add_tool(mcp, tool));
    }
    free(description);
    unsigned total = 0;
    char cursor[sizeof(name)] = {0};
    for (unsigned page = 0; page < 4; ++page) {
        char request[768];
        snprintf(request, sizeof(request), "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\",\"params\":{\"cursor\":\"%s\"," MODERN_META "}}", cursor);
        cJSON *response = modern_request(mcp, request);
        cJSON *result = cJSON_GetObjectItem(response, "result");
        TEST_ASSERT_NOT_NULL(result);
        char *encoded = cJSON_PrintUnformatted(response);
        TEST_ASSERT_NOT_NULL(encoded);
        TEST_ASSERT_TRUE(strlen(encoded) <= 8000 && strlen(encoded) <= CONFIG_MCP_TOOLLIST_MAX_SIZE);
        cJSON_free(encoded);
        int count = cJSON_GetArraySize(cJSON_GetObjectItem(result, "tools"));
        TEST_ASSERT_GREATER_THAN(0, count);
        total += count;
        cJSON *next = cJSON_GetObjectItem(result, "nextCursor");
        if (next) {
            TEST_ASSERT_TRUE(cJSON_IsString(next));
            TEST_ASSERT_EQUAL(sizeof(name) - 1, strlen(next->valuestring));
            snprintf(cursor, sizeof(cursor), "%s", next->valuestring);
        }
        bool has_next = next != NULL;
        cJSON_Delete(response);
        if (!has_next) {
            break;
        }
    }
    TEST_ASSERT_EQUAL(3, total);
    cJSON *ignored_limit = modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\",\"params\":{\"limit\":1.5," MODERN_META "}}");
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(ignored_limit, "result"));
    cJSON_Delete(ignored_limit);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_destroy(mcp));
}

static const char *s_failed_text = "tool failure";

static esp_err_t failed_tool(const esp_mcp_property_list_t *properties, esp_mcp_tool_result_t *result)
{
    (void)properties;
    esp_mcp_tool_result_set_is_error(result, true);
    return esp_mcp_tool_result_add_text(result, s_failed_text);
}

TEST_CASE("Xiaozhi legacy errors stay JSON-RPC errors and modern failures stay tool results", "[mcp][modern]")
{
    esp_mcp_t *mcp = modern_engine();
    esp_mcp_tool_t *tool = esp_mcp_tool_create_ex("failed", NULL, "test", failed_tool);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_add_tool(mcp, tool));
    cJSON *response = modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":0,\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{},\"clientInfo\":{\"name\":\"test\",\"version\":\"1\"}}}");
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(response, "result"));
    cJSON_Delete(response);
    s_failed_text = "{}";
    assert_error_message(modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"failed\"}}"), -32603, "{}");
    response = modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"failed\"," MODERN_META "}}");
    TEST_ASSERT_TRUE(cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetObjectItem(response, "result"), "isError")));
    cJSON_Delete(response);
    s_failed_text = "false";
    assert_error_message(modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"failed\"}}"), -32603, "false");
    s_failed_text = "tool failure";
    assert_error_message(modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\",\"params\":{\"name\":\"failed\"}}"), -32603, "tool failure");
    response = modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"failed\"," MODERN_META "}}");
    cJSON *result = cJSON_GetObjectItem(response, "result");
    TEST_ASSERT_TRUE(cJSON_IsTrue(cJSON_GetObjectItem(result, "isError")));
    cJSON_Delete(response);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_destroy(mcp));
}

#define GENERIC_INITIALIZE "{\"jsonrpc\":\"2.0\",\"id\":\"init\",\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{},\"clientInfo\":{\"name\":\"test\",\"version\":\"1\"}}}"

static esp_mcp_t *generic_engine(void)
{
    esp_mcp_t *mcp = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_create(&mcp));
    TEST_ASSERT_FALSE(esp_mcp_is_xiaozhi_compatibility_enabled(mcp));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_set_request_context(mcp, "generic-test", "test-peer", true));
    return mcp;
}

TEST_CASE("Xiaozhi strict JSON parsing does not change generic parsing", "[mcp][compat]")
{
    esp_mcp_t *generic = generic_engine();
    esp_mcp_t *xiaozhi = modern_engine();
    cJSON *response = modern_request(generic, GENERIC_INITIALIZE);
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(response, "result"));
    cJSON_Delete(response);
    const char *request = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"} trailing";
    response = modern_request(generic, request);
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(response, "result"));
    cJSON_Delete(response);
    assert_error(modern_request(xiaozhi, request), -32700);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_destroy(xiaozhi));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_destroy(generic));
}

TEST_CASE("Xiaozhi legacy tool errors do not change generic tool results", "[mcp][compat]")
{
    s_failed_text = "tool failure";
    esp_mcp_t *generic = generic_engine();
    esp_mcp_t *xiaozhi = modern_engine();
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_add_tool(generic, esp_mcp_tool_create_ex("failed", NULL, "test", failed_tool)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_add_tool(xiaozhi, esp_mcp_tool_create_ex("failed", NULL, "test", failed_tool)));
    cJSON *response = modern_request(generic, GENERIC_INITIALIZE);
    cJSON_Delete(response);
    const char *request = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"failed\"}}";
    response = modern_request(generic, request);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(response, "error"));
    TEST_ASSERT_TRUE(cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetObjectItem(response, "result"), "isError")));
    cJSON_Delete(response);
    assert_error(modern_request(xiaozhi, request), -32603);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_destroy(xiaozhi));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_destroy(generic));
}

static esp_mcp_value_t count_tool(const esp_mcp_property_list_t *properties)
{
    return esp_mcp_value_create_int(esp_mcp_property_list_get_property_int(properties, "count"));
}

static void add_count_tool(esp_mcp_t *mcp)
{
    esp_mcp_tool_t *tool = esp_mcp_tool_create("count", "test", count_tool);
    TEST_ASSERT_NOT_NULL(tool);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_tool_add_property(tool, esp_mcp_property_create_with_int("count", 7)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_add_tool(mcp, tool));
}

static void assert_text(cJSON *response, const char *expected)
{
    TEST_ASSERT_NOT_NULL(response);
    cJSON *result = cJSON_GetObjectItem(response, "result");
    cJSON *content = cJSON_GetArrayItem(cJSON_GetObjectItem(result, "content"), 0);
    TEST_ASSERT_NOT_NULL(content);
    TEST_ASSERT_EQUAL_STRING(expected, cJSON_GetObjectItem(content, "text")->valuestring);
    cJSON_Delete(response);
}

TEST_CASE("Xiaozhi compatibility is opt in per engine and generic metadata stays generic", "[mcp][compat]")
{
    esp_mcp_t *generic = generic_engine();
    esp_mcp_t *xiaozhi = modern_engine();
    TEST_ASSERT_TRUE(esp_mcp_is_xiaozhi_compatibility_enabled(xiaozhi));
    assert_error(modern_request(generic, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}"), -32600);
    cJSON *response = modern_request(xiaozhi, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}");
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(response, "result"));
    cJSON_Delete(response);
    response = modern_request(generic, GENERIC_INITIALIZE);
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(response, "result"));
    cJSON_Delete(response);
    response = modern_request(generic, GENERIC_INITIALIZE);
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(response, "result"));
    cJSON_Delete(response);
    response = modern_request(generic, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"ping\",\"params\":{" MODERN_META "}}");
    cJSON *result = cJSON_GetObjectItem(response, "result");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(result, "resultType"));
    const cJSON *meta = cJSON_GetObjectItem(result, "_meta");
    TEST_ASSERT_EQUAL_STRING("2026-07-28", cJSON_GetObjectItem(meta, "io.modelcontextprotocol/protocolVersion")->valuestring);
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(meta, "io.modelcontextprotocol/clientCapabilities"));
    TEST_ASSERT_NULL(cJSON_GetObjectItem(meta, "io.modelcontextprotocol/serverInfo"));
    cJSON_Delete(response);
    response = modern_request(generic, "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"resources/list\"}");
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(cJSON_GetObjectItem(response, "result"), "resources"));
    cJSON_Delete(response);
    assert_error(modern_request(generic, "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"resources/list\"}"), -32600);
    assert_error(modern_request(xiaozhi, "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"resources/list\"}"), -32601);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_set_xiaozhi_compatibility(xiaozhi, false));
    assert_error(modern_request(xiaozhi, "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"tools/list\"}"), -32600);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_destroy(xiaozhi));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_destroy(generic));
}

TEST_CASE("generic tool arguments retain previous conversion and missing default behavior", "[mcp][compat]")
{
    esp_mcp_t *mcp = generic_engine();
    add_count_tool(mcp);
    cJSON *response = modern_request(mcp, GENERIC_INITIALIZE);
    cJSON_Delete(response);
    assert_error(modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"count\",\"arguments\":{}}}"), -32602);
    assert_text(modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"count\",\"arguments\":{\"count\":3.9}}}"), "3");
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_destroy(mcp));
}

TEST_CASE("Xiaozhi legacy calls are stateless use defaults and retain legacy integer conversion", "[mcp][compat]")
{
    esp_mcp_t *mcp = modern_engine();
    add_count_tool(mcp);
    const char *call = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"count\"}}";
    assert_text(modern_request(mcp, call), "7");
    assert_text(modern_request(mcp, call), "7");
    assert_text(modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"count\",\"arguments\":{\"count\":3.9}}}"), "3");
    assert_error(modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"count\",\"arguments\":{\"count\":3.9}," MODERN_META "}}"), -32602);
    assert_error(modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\"}"), -32602);
    assert_error(modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"missing\"}}"), -32602);
    for (unsigned i = 0; i < 2; i++) {
        cJSON *response = modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\"}");
        cJSON *result = cJSON_GetObjectItem(response, "result");
        TEST_ASSERT_EQUAL_STRING("2024-11-05", cJSON_GetObjectItem(result, "protocolVersion")->valuestring);
        TEST_ASSERT_NULL(cJSON_GetObjectItem(result, "resultType"));
        cJSON_Delete(response);
    }
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_destroy(mcp));
}

TEST_CASE("Xiaozhi version advertisements and string IDs match the upstream contract", "[mcp][compat]")
{
    esp_mcp_t *mcp = modern_engine();
    char board_name[] = "test-board";
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_set_xiaozhi_server_identity(mcp, board_name, "2.6.0"));
    board_name[0] = 'X';
    cJSON *response = modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"server/discover\",\"params\":{" MODERN_META "}}");
    cJSON *server = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(response, "result"), "_meta"), "io.modelcontextprotocol/serverInfo");
    TEST_ASSERT_EQUAL_STRING("test-board", cJSON_GetObjectItem(server, "name")->valuestring);
    TEST_ASSERT_EQUAL_STRING("2.6.0", cJSON_GetObjectItem(server, "version")->valuestring);
    cJSON *versions = cJSON_GetObjectItem(cJSON_GetObjectItem(response, "result"), "supportedVersions");
    TEST_ASSERT_EQUAL(2, cJSON_GetArraySize(versions));
    TEST_ASSERT_EQUAL_STRING("2026-07-28", cJSON_GetArrayItem(versions, 0)->valuestring);
    TEST_ASSERT_EQUAL_STRING("2024-11-05", cJSON_GetArrayItem(versions, 1)->valuestring);
    cJSON_Delete(response);
    response = modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\",\"params\":{\"_meta\":{\"io.modelcontextprotocol/protocolVersion\":\"2024-11-05\",\"io.modelcontextprotocol/clientCapabilities\":{}}}}");
    versions = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(response, "error"), "data"), "supported");
    TEST_ASSERT_EQUAL(2, cJSON_GetArraySize(versions));
    assert_error(response, -32022);
    char id[300];
    memset(id, 'i', sizeof(id) - 1);
    id[sizeof(id) - 1] = '\0';
    char request[768];
    snprintf(request, sizeof(request), "{\"jsonrpc\":\"2.0\",\"id\":\"%s\",\"method\":\"tools/list\",\"params\":{" MODERN_META "}}", id);
    response = modern_request(mcp, request);
    TEST_ASSERT_EQUAL_STRING(id, cJSON_GetObjectItem(response, "id")->valuestring);
    cJSON_Delete(response);
    assert_error(modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1.5,\"method\":\"tools/list\"}"), -32600);
    response = modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\"}");
    server = cJSON_GetObjectItem(cJSON_GetObjectItem(response, "result"), "serverInfo");
    TEST_ASSERT_EQUAL_STRING("test-board", cJSON_GetObjectItem(server, "name")->valuestring);
    TEST_ASSERT_EQUAL_STRING("2.6.0", cJSON_GetObjectItem(server, "version")->valuestring);
    cJSON_Delete(response);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_set_xiaozhi_server_identity(mcp, NULL, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mcp_set_xiaozhi_server_identity(mcp, "", "2.6.0"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_destroy(mcp));
}

TEST_CASE("Xiaozhi listing keeps registration order and ignores generic-only extensions", "[mcp][compat]")
{
    esp_mcp_t *generic = generic_engine();
    esp_mcp_t *xiaozhi = modern_engine();
    esp_mcp_t *engines[] = {generic, xiaozhi};
    for (unsigned i = 0; i < 2; i++) {
        for (unsigned j = 0; j < 2; j++) {
            esp_mcp_tool_t *tool = esp_mcp_tool_create(j ? "second" : "first", "test", unused_tool);
            TEST_ASSERT_NOT_NULL(tool);
            TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_tool_set_task_support(tool, "required"));
            TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_add_tool(engines[i], tool));
        }
    }
    cJSON *response = modern_request(generic, GENERIC_INITIALIZE);
    cJSON_Delete(response);
    response = modern_request(generic, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\",\"params\":{\"limit\":1}}");
    cJSON *tools = cJSON_GetObjectItem(cJSON_GetObjectItem(response, "result"), "tools");
    TEST_ASSERT_EQUAL(1, cJSON_GetArraySize(tools));
    cJSON *tool = cJSON_GetArrayItem(tools, 0);
    TEST_ASSERT_EQUAL_STRING("second", cJSON_GetObjectItem(tool, "name")->valuestring);
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(tool, "execution"));
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(cJSON_GetObjectItem(tool, "inputSchema"), "additionalProperties"));
    cJSON_Delete(response);
    response = modern_request(xiaozhi, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\",\"params\":{\"limit\":\"ignored\"," MODERN_META "}}");
    tools = cJSON_GetObjectItem(cJSON_GetObjectItem(response, "result"), "tools");
    TEST_ASSERT_EQUAL(2, cJSON_GetArraySize(tools));
    tool = cJSON_GetArrayItem(tools, 0);
    TEST_ASSERT_EQUAL_STRING("first", cJSON_GetObjectItem(tool, "name")->valuestring);
    TEST_ASSERT_EQUAL_STRING("second", cJSON_GetObjectItem(cJSON_GetArrayItem(tools, 1), "name")->valuestring);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(tool, "execution"));
    TEST_ASSERT_NULL(cJSON_GetObjectItem(cJSON_GetObjectItem(tool, "inputSchema"), "additionalProperties"));
    cJSON_Delete(response);
    assert_text(modern_request(xiaozhi, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"first\",\"task\":\"ignored\",\"stackSize\":\"ignored\"}}"), "true");
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_destroy(xiaozhi));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_destroy(generic));
}

static esp_err_t image_tool(const esp_mcp_property_list_t *properties, esp_mcp_tool_result_t *result)
{
    (void)properties;
    return esp_mcp_tool_result_add_image_base64(result, "image/jpeg", "AA==");
}

static esp_err_t notification_tool(const esp_mcp_property_list_t *properties,
                                   const esp_mcp_request_context_t *request,
                                   esp_mcp_tool_result_t *result, void *user_ctx)
{
    (void)properties;
    (void)request;
    (*(unsigned *)user_ctx)++;
    return esp_mcp_tool_result_add_text(result, "done");
}

TEST_CASE("Xiaozhi rejects batches and never executes notification tool calls", "[mcp][compat]")
{
    esp_mcp_t *mcp = modern_engine();
    unsigned calls = 0;
    esp_mcp_tool_t *tool = esp_mcp_tool_create_with_context("notify", "test", notification_tool, &calls);
    TEST_ASSERT_NOT_NULL(tool);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_add_tool(mcp, tool));
    TEST_ASSERT_NULL(modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"notify\"}}"));
    TEST_ASSERT_NULL(modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"notify\"," MODERN_META "}}"));
    TEST_ASSERT_NULL(modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"notifications/initialized\",\"params\":{\"_meta\":false}}"));
    assert_error(modern_request(mcp, "[{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"notify\"}}]"), -32600);
    assert_error(modern_request(mcp, "{\"JSONRPC\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"notify\"}}"), -32600);
    TEST_ASSERT_EQUAL(0, calls);
    cJSON *response = modern_request(mcp, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"notify\",\"_meta\":{\"test/ignored\":true}}}");
    cJSON *result = cJSON_GetObjectItem(response, "result");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(result, "_meta"));
    cJSON_Delete(response);
    TEST_ASSERT_EQUAL(1, calls);
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_destroy(mcp));
}

TEST_CASE("image results wrap only for the Xiaozhi legacy dialect", "[mcp][compat]")
{
    esp_mcp_t *generic = generic_engine();
    esp_mcp_t *xiaozhi = modern_engine();
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_add_tool(generic, esp_mcp_tool_create_ex("image", NULL, "test", image_tool)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_add_tool(xiaozhi, esp_mcp_tool_create_ex("image", NULL, "test", image_tool)));
    cJSON *response = modern_request(generic, GENERIC_INITIALIZE);
    cJSON_Delete(response);
    const char *legacy = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"image\"}}";
    const char *modern = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"image\"," MODERN_META "}}";
    esp_mcp_t *engines[] = {generic, xiaozhi, xiaozhi};
    for (unsigned i = 0; i < 3; i++) {
        response = modern_request(engines[i], i == 2 ? modern : legacy);
        cJSON *image = cJSON_GetArrayItem(cJSON_GetObjectItem(cJSON_GetObjectItem(response, "result"), "content"), 0);
        TEST_ASSERT_NOT_NULL(image);
        if (i == 1) {
            cJSON *wrapped = cJSON_GetObjectItem(image, "image");
            TEST_ASSERT_TRUE(cJSON_IsString(wrapped));
            cJSON *decoded = cJSON_Parse(wrapped->valuestring);
            TEST_ASSERT_EQUAL_STRING("image/jpeg", cJSON_GetObjectItem(decoded, "mimeType")->valuestring);
            TEST_ASSERT_EQUAL_STRING("AA==", cJSON_GetObjectItem(decoded, "data")->valuestring);
            cJSON_Delete(decoded);
            TEST_ASSERT_NULL(cJSON_GetObjectItem(image, "mimeType"));
        } else {
            TEST_ASSERT_EQUAL_STRING("image/jpeg", cJSON_GetObjectItem(image, "mimeType")->valuestring);
            TEST_ASSERT_EQUAL_STRING("AA==", cJSON_GetObjectItem(image, "data")->valuestring);
            TEST_ASSERT_NULL(cJSON_GetObjectItem(image, "image"));
        }
        cJSON_Delete(response);
    }
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_destroy(xiaozhi));
    TEST_ASSERT_EQUAL(ESP_OK, esp_mcp_destroy(generic));
}
