/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <esp_log.h>
#include <esp_app_desc.h>
#include <esp_check.h>
#include <cJSON.h>

#include "esp_mcp_item.h"
#include "esp_mcp_error.h"
#include "esp_mcp_tool_priv.h"
#include "esp_mcp_server_priv.h"

static const char* TAG = "esp_mcp_server";

static const esp_mcp_error_t s_mcp_error_messages[] = {
    {.code = MCP_ERROR_CODE_PARSE_ERROR,                .message = MCP_ERROR_MESSAGE_PARSE_ERROR},
    {.code = MCP_ERROR_CODE_INVALID_REQUEST,            .message = MCP_ERROR_MESSAGE_INVALID_REQUEST},
    {.code = MCP_ERROR_CODE_METHOD_NOT_FOUND,           .message = MCP_ERROR_MESSAGE_METHOD_NOT_FOUND},
    {.code = MCP_ERROR_CODE_INVALID_PARAMS,             .message = MCP_ERROR_MESSAGE_INVALID_PARAMS},
    {.code = MCP_ERROR_CODE_INTERNAL_ERROR,             .message = MCP_ERROR_MESSAGE_INTERNAL_ERROR},
    {.code = MCP_ERROR_CODE_SERVER_ERROR,               .message = MCP_ERROR_MESSAGE_SERVER_ERROR},
    {.code = MCP_ERROR_CODE_FAILED_TO_ADD_TOOL,         .message = MCP_ERROR_MESSAGE_FAILED_TO_ADD_TOOL},
    {.code = MCP_ERROR_CODE_TOOL_CALL_FAILED,           .message = MCP_ERROR_MESSAGE_TOOL_CALL_FAILED},
    {.code = MCP_ERROR_CODE_FAILED_TO_CREATE_ARGUMENTS, .message = MCP_ERROR_MESSAGE_FAILED_TO_CREATE_ARGUMENTS},
};

static int esp_mcp_server_find_error_code(const char* message)
{
    for (size_t i = 0; i < sizeof(s_mcp_error_messages) / sizeof(s_mcp_error_messages[0]); i++) {
        if (!strcmp(s_mcp_error_messages[i].message, message)) {
            return s_mcp_error_messages[i].code;
        }
    }

    return MCP_ERROR_CODE_SERVER_ERROR;
}

static esp_mcp_mbuf_t *esp_mcp_server_find_mbuf_with_id(esp_mcp_server_t* server, uint16_t id)
{
    esp_mcp_mbuf_t *mbuf = NULL;
    if (!server) {
        ESP_LOGE(TAG, "esp_mcp_server_find_mbuf_with_id: Invalid server");
        return NULL;
    }

    SLIST_FOREACH(mbuf, &server->mbuf, next) {
        if (!mbuf) {
            continue;
        }
        if (!mbuf) {
            continue;
        }

        return mbuf;
    }

    return NULL;
}

static esp_err_t esp_mcp_server_add_mbuf(esp_mcp_server_t* server, uint16_t id, uint16_t outlen, uint8_t *outbuf)
{
    ESP_RETURN_ON_FALSE(server, ESP_ERR_INVALID_ARG, TAG, "Invalid server");

    esp_mcp_mbuf_t *mbuf = esp_mcp_server_find_mbuf_with_id(server, id);

    if (!mbuf) {
        mbuf = (esp_mcp_mbuf_t*)calloc(1, sizeof(esp_mcp_mbuf_t));
        if (!mbuf) {
            ESP_LOGE(TAG, "Failed to allocate memory for storing outbuf and outlen");
            return ESP_ERR_NO_MEM;
        }
        SLIST_INSERT_HEAD(&server->mbuf, mbuf, next);
        mbuf->id = id;
    } else {
        if (mbuf->outbuf) {
            cJSON_free(mbuf->outbuf);
        }
    }

    mbuf->outbuf = outbuf;
    mbuf->outlen = outlen;

    return ESP_OK;
}

static esp_err_t esp_mcp_server_reply_result(esp_mcp_server_t* server, int id, const char* result)
{
    ESP_RETURN_ON_FALSE(server && result, ESP_ERR_INVALID_ARG, TAG, "Invalid server or result");

    cJSON* payload = cJSON_CreateObject();
    if (!payload) {
        ESP_LOGE(TAG, "Failed to allocate memory for payload");
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(payload, "jsonrpc", "2.0");
    cJSON_AddNumberToObject(payload, "id", id);

    cJSON* result_obj = cJSON_Parse(result);
    if (result_obj) {
        cJSON_AddItemToObject(payload, "result", result_obj);
    } else {
        cJSON_AddStringToObject(payload, "result", result);
    }

    char* payload_str = cJSON_PrintUnformatted(payload);
    if (payload_str) {
        esp_mcp_server_add_mbuf(server, id, strlen(payload_str), (uint8_t*)payload_str);
    }

    cJSON_Delete(payload);

    return ESP_OK;
}

static esp_err_t esp_mcp_server_reply_error(esp_mcp_server_t* server, int id, const char* message)
{
    ESP_RETURN_ON_FALSE(server && message, ESP_ERR_INVALID_ARG, TAG, "Invalid server or message");

    cJSON* payload = cJSON_CreateObject();
    if (!payload) {
        ESP_LOGE(TAG, "Failed to allocate memory for payload");
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(payload, "jsonrpc", "2.0");

    switch (id) {
        case MCP_ERROR_CODE_PARSE_ERROR:
        case MCP_ERROR_CODE_INVALID_REQUEST:
            cJSON_AddNullToObject(payload, "id");
            break;
        default:
            cJSON_AddNumberToObject(payload, "id", id);
            break;
    }

    cJSON* error = cJSON_Parse(message);
    if (!error) {
        error = cJSON_CreateObject();
        if (!error) {
            ESP_LOGE(TAG, "Failed to allocate memory for error");
            cJSON_Delete(payload);
            return ESP_ERR_NO_MEM;
        }
        cJSON_AddNumberToObject(error, "code", esp_mcp_server_find_error_code(message));
        cJSON_AddStringToObject(error, "message", message);
    }
    cJSON_AddItemToObject(payload, "error", error);

    char* payload_str = cJSON_PrintUnformatted(payload);
    if (payload_str) {
        esp_mcp_server_add_mbuf(server, id, strlen(payload_str), (uint8_t*)payload_str);
    }
    cJSON_Delete(payload);

    return ESP_OK;
}

static esp_err_t esp_mcp_server_parse_capabilities(esp_mcp_server_t* server, const cJSON* capabilities)
{
    ESP_RETURN_ON_FALSE(server, ESP_ERR_INVALID_ARG, TAG, "Invalid server");
    ESP_RETURN_ON_FALSE(capabilities, ESP_ERR_INVALID_ARG, TAG, "Invalid capabilities");

    cJSON* vision = cJSON_GetObjectItem(capabilities, "vision");
    if (cJSON_IsObject(vision)) {
        cJSON* url = cJSON_GetObjectItem(vision, "url");
        cJSON* token = cJSON_GetObjectItem(vision, "token");
        if (cJSON_IsString(url)) {
            ESP_LOGI(TAG, "Vision URL: %s", url->valuestring);
        }

        if (cJSON_IsString(token)) {
            const char* token_str = token->valuestring;
            int token_len = strlen(token_str);
            if (token_len > 8) {
                char masked_token[16] = {0};
                strncpy(masked_token, token_str, 4);
                strcat(masked_token, "***");
                strcat(masked_token, token_str + token_len - 4);
                ESP_LOGI(TAG, "Vision Token: %s", masked_token);
            } else {
                ESP_LOGI(TAG, "Vision Token: ***");
            }
        }
    }

    return ESP_OK;
}

static esp_err_t esp_mcp_server_handle_initialize(esp_mcp_server_t* server, const cJSON* params, int id)
{
    ESP_RETURN_ON_FALSE(server, ESP_ERR_INVALID_ARG, TAG, "Invalid server");

    cJSON* capabilities_param = cJSON_GetObjectItem(params, "capabilities");
    if (capabilities_param && cJSON_IsObject(capabilities_param)) {
        esp_mcp_server_parse_capabilities(server, capabilities_param);
    }

    const esp_app_desc_t* app_desc = esp_app_get_description();
    cJSON* message = cJSON_CreateObject();
    if (!message) {
        ESP_LOGE(TAG, "Failed to allocate memory for message");
        return ESP_ERR_NO_MEM;
    }

    cJSON* tools = cJSON_CreateObject();
    cJSON* prompts = cJSON_CreateObject();
    cJSON* experimental = cJSON_CreateObject();
    cJSON* server_info = cJSON_CreateObject();
    cJSON* capabilities = cJSON_CreateObject();

    if (!tools || !prompts || !experimental || !server_info || !capabilities) {
        ESP_LOGE(TAG, "Failed to allocate memory for message components");
        if (tools) {
            cJSON_Delete(tools);
        }

        if (prompts) {
            cJSON_Delete(prompts);
        }

        if (experimental) {
            cJSON_Delete(experimental);
        }

        if (server_info) {
            cJSON_Delete(server_info);
        }

        if (capabilities) {
            cJSON_Delete(capabilities);
        }

        cJSON_Delete(message);
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddBoolToObject(tools, "listChanged", false);
    cJSON_AddBoolToObject(prompts, "listChanged", false);
    cJSON_AddItemToObject(capabilities, "experimental", experimental);
    cJSON_AddItemToObject(capabilities, "prompts", prompts);
    cJSON_AddItemToObject(capabilities, "tools", tools);

    cJSON_AddStringToObject(server_info, "name", app_desc->project_name);
    cJSON_AddStringToObject(server_info, "version", app_desc->version);

    cJSON_AddStringToObject(message, "protocolVersion", "2024-11-05");
    cJSON_AddItemToObject(message, "capabilities", capabilities);
    cJSON_AddItemToObject(message, "serverInfo", server_info);

    char* message_str = cJSON_PrintUnformatted(message);
    if (message_str) {
        esp_mcp_server_reply_result(server, id, message_str);
        cJSON_free(message_str);
    }
    cJSON_Delete(message);

    return ESP_OK;
}

static esp_err_t esp_mcp_server_handle_tools_list(esp_mcp_server_t* server, const cJSON* params, int id)
{
    ESP_RETURN_ON_FALSE(server, ESP_ERR_INVALID_ARG, TAG, "Invalid server");

    char cursor_str[256] = {0};
    char next_cursor[256] = {0};
    bool found_cursor = true;
    cJSON* cursor = cJSON_GetObjectItem(params, "cursor");
    if (cursor && cJSON_IsString(cursor)) {
        strncpy(cursor_str, cursor->valuestring, sizeof(cursor_str) - 1);
        cursor_str[sizeof(cursor_str) - 1] = '\0';
        found_cursor = false;
    }

    esp_err_t ret = ESP_OK;
    char *payload_str = NULL;

    cJSON* tools = cJSON_CreateObject();
    if (!tools) {
        ESP_LOGE(TAG, "Failed to allocate memory for tools");
        return ESP_ERR_NO_MEM;
    }

    cJSON* tools_array = cJSON_CreateArray();
    if (!tools_array) {
        ESP_LOGE(TAG, "Failed to allocate memory for tools array");
        cJSON_Delete(tools);
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddItemToObject(tools, "tools", tools_array);

    esp_mcp_tool_item_t *info = NULL;
    SLIST_FOREACH(info, server->tools, next) {
        if (!found_cursor) {
            if (!strcmp(info->tools->name, cursor_str)) {
                found_cursor = true;
            } else {
                continue;
            }
        }

        char* tool_json = esp_mcp_tool_to_json(info->tools);
        if (tool_json) {
            payload_str = cJSON_PrintUnformatted(tools);
            if (!payload_str) {
                cJSON_free(tool_json);
                continue;
            }

            if (strlen(payload_str) > CONFIG_MCP_TOOLLIST_MAX_SIZE) {
                strncpy(next_cursor, info->tools->name, sizeof(next_cursor) - 1);
                next_cursor[sizeof(next_cursor) - 1] = '\0';
                cJSON_free(tool_json);
                cJSON_free(payload_str);
                break;
            }

            cJSON* tool_obj = cJSON_Parse(tool_json);
            if (tool_obj) {
                cJSON_AddItemToArray(tools_array, tool_obj);
            } else {
                ESP_LOGE(TAG, "Failed to parse tool JSON: %s", tool_json);
            }
            cJSON_free(tool_json);
            cJSON_free(payload_str);
        }
    }

    if (strlen(next_cursor)) {
        cJSON_AddStringToObject(tools, "nextCursor", next_cursor);
    }

    payload_str = cJSON_PrintUnformatted(tools);
    if (payload_str) {
        if (strlen(payload_str) == 12 && SLIST_FIRST(server->tools) != NULL) {
            ESP_LOGE(TAG, "tools/list: Failed to add tool %s because of payload size limit", next_cursor);
            char error_buf[MCP_ERROR_BUF_MAX_SIZE] = {0};
            snprintf(error_buf, sizeof(error_buf), "%s: ", MCP_ERROR_MESSAGE_FAILED_TO_ADD_TOOL);
            char* error_msg = calloc(strlen(error_buf) + strlen(next_cursor) + 1, sizeof(char));
            if (error_msg) {
                strcpy(error_msg, error_buf);
                strcat(error_msg, next_cursor);
                esp_mcp_server_reply_error(server, id, error_msg);
                free(error_msg);
            } else {
                esp_mcp_server_reply_error(server, id, MCP_ERROR_MESSAGE_FAILED_TO_ADD_TOOL);
            }
            ret = ESP_ERR_NO_MEM;
        } else {
            esp_mcp_server_reply_result(server, id, payload_str);
        }
        cJSON_free(payload_str);
    }
    cJSON_Delete(tools);

    return ret;
}

static esp_err_t esp_mcp_server_do_tool_call(esp_mcp_server_t* server, int id, const char* tool_name, const cJSON* tool_arguments, int stack_size)
{
    ESP_RETURN_ON_FALSE(server, ESP_ERR_INVALID_ARG, TAG, "Invalid server");
    ESP_RETURN_ON_FALSE(tool_name, ESP_ERR_INVALID_ARG, TAG, "Invalid tool name");
    ESP_RETURN_ON_FALSE(tool_arguments, ESP_ERR_INVALID_ARG, TAG, "Invalid tool arguments");
    ESP_RETURN_ON_FALSE(stack_size > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid stack size");

    esp_mcp_tool_t* tool = esp_mcp_tool_list_find_tool(server->tools, tool_name);
    if (!tool) {
        ESP_LOGE(TAG, "tools/call: Unknown tool: %s", tool_name);
        char *error_msg = NULL;
        char error_buf[MCP_ERROR_BUF_MAX_SIZE] = {0};
        snprintf(error_buf, sizeof(error_buf), "%s: ", MCP_ERROR_MESSAGE_INVALID_PARAMS);
        error_msg = calloc(strlen(error_buf) + strlen(tool_name) + 1, sizeof(char));
        if (error_msg) {
            strcpy(error_msg, error_buf);
            strcat(error_msg, tool_name);
            esp_mcp_server_reply_error(server, id, error_msg);
            free(error_msg);
        } else {
            esp_mcp_server_reply_error(server, id, MCP_ERROR_MESSAGE_INVALID_PARAMS);
        }
        return ESP_ERR_INVALID_ARG;
    }

    esp_mcp_property_list_t* arguments = esp_mcp_property_list_create();
    if (arguments) {
        if (tool_arguments && cJSON_IsObject(tool_arguments)) {
            esp_mcp_property_item_t *info = NULL;
            SLIST_FOREACH(info, tool->properties, next) {
                const char* name = info->property->name;
                cJSON* value = cJSON_GetObjectItem(tool_arguments, name);
                if (value) {
                    if (info->property->type == ESP_MCP_PROPERTY_TYPE_BOOLEAN) {
                        esp_mcp_property_list_add_property(arguments, esp_mcp_property_create_with_bool(name, value->valueint == 1));
                    } else if (info->property->type == ESP_MCP_PROPERTY_TYPE_INTEGER) {
                        esp_mcp_property_list_add_property(arguments, esp_mcp_property_create_with_int(name, value->valueint));
                    } else if (info->property->type == ESP_MCP_PROPERTY_TYPE_STRING) {
                        esp_mcp_property_list_add_property(arguments, esp_mcp_property_create_with_string(name, value->valuestring));
                    } else if (info->property->type == ESP_MCP_PROPERTY_TYPE_ARRAY) {
                        char* value_str = cJSON_PrintUnformatted(value);
                        if (value_str) {
                            esp_mcp_property_list_add_property(arguments, esp_mcp_property_create_with_array(name, value_str));
                            cJSON_free(value_str);
                        } else {
                            ESP_LOGE(TAG, "tools/call: Failed to print array argument: %s", name);
                        }   
                    } else if (info->property->type == ESP_MCP_PROPERTY_TYPE_OBJECT) {
                        char* value_str = cJSON_PrintUnformatted(value);
                        if (value_str) {
                            esp_mcp_property_list_add_property(arguments, esp_mcp_property_create_with_object(name, value_str));
                            cJSON_free(value_str);
                        } else {
                            ESP_LOGE(TAG, "tools/call: Failed to print object argument: %s", name);
                        }
                    }
                } else {
                    ESP_LOGE(TAG, "tools/call: Missing argument: %s", name);
                }
            }
        }

        char* result = esp_mcp_tool_call(tool, arguments);
        if (result) {
            esp_mcp_server_reply_result(server, id, result);
            cJSON_free(result);
        } else {
            esp_mcp_server_reply_error(server, id, MCP_ERROR_MESSAGE_TOOL_CALL_FAILED);
        }

        esp_mcp_property_list_destroy(arguments);
    } else {
        esp_mcp_server_reply_error(server, id, MCP_ERROR_MESSAGE_FAILED_TO_CREATE_ARGUMENTS);
    }

    return ESP_OK;
}

static esp_err_t esp_mcp_server_handle_tools_call(esp_mcp_server_t* server, const cJSON* params, int id)
{
    ESP_RETURN_ON_FALSE(server, ESP_ERR_INVALID_ARG, TAG, "Invalid server");
    ESP_RETURN_ON_FALSE(params, ESP_ERR_INVALID_ARG, TAG, "Invalid params");

    if (!cJSON_IsObject(params)) {
        ESP_LOGE(TAG, "tools/call: Missing params");
        esp_mcp_server_reply_error(server, id, MCP_ERROR_MESSAGE_INVALID_REQUEST);
        return ESP_ERR_INVALID_ARG;
    }

    cJSON* tool_name = cJSON_GetObjectItem(params, "name");
    if (!cJSON_IsString(tool_name)) {
        ESP_LOGE(TAG, "tools/call: Missing name");
        esp_mcp_server_reply_error(server, id, MCP_ERROR_MESSAGE_INVALID_REQUEST);
        return ESP_ERR_INVALID_ARG;
    }

    cJSON* tool_arguments = cJSON_GetObjectItem(params, "arguments");
    if (tool_arguments != NULL && !cJSON_IsObject(tool_arguments)) {
        ESP_LOGE(TAG, "tools/call: Invalid arguments");
        esp_mcp_server_reply_error(server, id, MCP_ERROR_MESSAGE_INVALID_PARAMS);
        return ESP_ERR_INVALID_ARG;
    }

    cJSON* stack_size = cJSON_GetObjectItem(params, "stackSize");
    if (stack_size != NULL && !cJSON_IsNumber(stack_size)) {
        ESP_LOGE(TAG, "tools/call: Invalid stackSize");
        esp_mcp_server_reply_error(server, id, MCP_ERROR_MESSAGE_INVALID_PARAMS);
        return ESP_ERR_INVALID_ARG;
    }

    return esp_mcp_server_do_tool_call(server, id, tool_name->valuestring, tool_arguments,
                                       stack_size ? stack_size->valueint : CONFIG_MCP_TOOLCALL_STACK_SIZE);
}

static esp_err_t esp_mcp_server_handle_ping(esp_mcp_server_t* server, const cJSON* params, int id)
{
    ESP_RETURN_ON_FALSE(server, ESP_ERR_INVALID_ARG, TAG, "Invalid server");

    return esp_mcp_server_reply_result(server, id, "pong");
}

esp_err_t esp_mcp_server_get_mbuf(esp_mcp_server_t* server, uint16_t id, uint16_t *outlen, uint8_t **outbuf)
{
    ESP_RETURN_ON_FALSE(server, ESP_ERR_INVALID_ARG, TAG, "Invalid server");
    ESP_RETURN_ON_FALSE(outlen, ESP_ERR_INVALID_ARG, TAG, "Invalid outlen");
    ESP_RETURN_ON_FALSE(outbuf, ESP_ERR_INVALID_ARG, TAG, "Invalid outbuf");

    esp_mcp_mbuf_t *mbuf = esp_mcp_server_find_mbuf_with_id(server, id);
    ESP_RETURN_ON_FALSE(mbuf, ESP_ERR_NOT_FOUND, TAG, "Outbuf with id %d not found", id);

    *outbuf = mbuf->outbuf;
    *outlen = mbuf->outlen;

    return ESP_OK;
}

esp_err_t esp_mcp_server_create(esp_mcp_server_t** server)
{
    ESP_RETURN_ON_FALSE(server, ESP_ERR_INVALID_ARG, TAG, "Invalid server");

    esp_mcp_server_t* temp_server = (esp_mcp_server_t*)calloc(1, sizeof(esp_mcp_server_t));
    ESP_RETURN_ON_FALSE(temp_server, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for MCP server");

    temp_server->tools = esp_mcp_tool_list_create();
    if (!temp_server->tools) {
        ESP_LOGE(TAG, "Failed to allocate memory for MCP tools");
        free(temp_server);
        return ESP_ERR_NO_MEM;
    }

    SLIST_INIT(&temp_server->mbuf);
    *server = temp_server;

    return ESP_OK;
}

esp_err_t esp_mcp_server_destroy(esp_mcp_server_t* server)
{
    ESP_RETURN_ON_FALSE(server, ESP_ERR_INVALID_ARG, TAG, "Invalid server");

    esp_err_t ret = esp_mcp_tool_list_destroy(server->tools);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error destroying tool list: %s", esp_err_to_name(ret));
    }

    esp_mcp_mbuf_t *mbuf = NULL;
    while (!SLIST_EMPTY(&server->mbuf)) {
        mbuf = SLIST_FIRST(&server->mbuf);
        SLIST_REMOVE_HEAD(&server->mbuf, next);
        if (mbuf->outbuf) {
            cJSON_free(mbuf->outbuf);
        }
        free(mbuf);
    }

    free(server);

    return ESP_OK;
}

esp_err_t esp_mcp_server_add_tool(esp_mcp_server_t* server, esp_mcp_tool_t* tool)
{
    ESP_RETURN_ON_FALSE(server && tool, ESP_ERR_INVALID_ARG, TAG, "Invalid server or tool");

    esp_mcp_tool_t* existing_tool = esp_mcp_tool_list_find_tool(server->tools, tool->name);
    if (existing_tool) {
        ESP_LOGW(TAG, "Tool %s already added", tool->name);
        return ESP_OK;
    }

    esp_mcp_tool_list_add_tool(server->tools, tool);

    return ESP_OK;
}

esp_err_t esp_mcp_server_add_tool_with_callback(esp_mcp_server_t* server, const char* name, const char* description, const esp_mcp_property_list_t* properties, esp_mcp_tool_callback_t callback)
{
    ESP_RETURN_ON_FALSE(server, ESP_ERR_INVALID_ARG, TAG, "Invalid server");
    ESP_RETURN_ON_FALSE(name, ESP_ERR_INVALID_ARG, TAG, "Invalid name");
    ESP_RETURN_ON_FALSE(description, ESP_ERR_INVALID_ARG, TAG, "Invalid description");
    ESP_RETURN_ON_FALSE(properties, ESP_ERR_INVALID_ARG, TAG, "Invalid properties");
    ESP_RETURN_ON_FALSE(callback, ESP_ERR_INVALID_ARG, TAG, "Invalid callback");

    esp_mcp_tool_t* tool = esp_mcp_tool_create(name, description, properties, callback);
    ESP_RETURN_ON_FALSE(tool, ESP_ERR_NO_MEM, TAG, "Failed to create tool");

    esp_mcp_server_add_tool(server, tool);

    return ESP_OK;
}

esp_err_t esp_mcp_server_parse_json_message(esp_mcp_server_t* server, const cJSON* json)
{
    ESP_RETURN_ON_FALSE(server, ESP_ERR_INVALID_ARG, TAG, "Invalid server");
    ESP_RETURN_ON_FALSE(json, ESP_ERR_INVALID_ARG, TAG, "Invalid JSON");

    cJSON* version = cJSON_GetObjectItem(json, "jsonrpc");
    if (version == NULL || !cJSON_IsString(version) || strcmp(version->valuestring, "2.0") != 0) {
        ESP_LOGE(TAG, "Invalid JSONRPC version: %s", version ? version->valuestring : "null");
        esp_mcp_server_reply_error(server, MCP_ERROR_CODE_INVALID_REQUEST, MCP_ERROR_MESSAGE_INVALID_REQUEST);
        return ESP_ERR_INVALID_ARG;
    }

    cJSON* method = cJSON_GetObjectItem(json, "method");
    if (method == NULL || !cJSON_IsString(method)) {
        ESP_LOGE(TAG, "Missing method");
        esp_mcp_server_reply_error(server, MCP_ERROR_CODE_INVALID_REQUEST, MCP_ERROR_MESSAGE_INVALID_REQUEST);
        return ESP_ERR_INVALID_ARG;
    }

    const char* method_str = method->valuestring;
    if (!strncmp(method_str, "notifications", strlen("notifications"))) {
        return ESP_OK;
    }

    cJSON* params = cJSON_GetObjectItem(json, "params");
    if (params != NULL && !cJSON_IsObject(params)) {
        ESP_LOGE(TAG, "Invalid params for method: %s", method_str);
        esp_mcp_server_reply_error(server, MCP_ERROR_CODE_INVALID_REQUEST, MCP_ERROR_MESSAGE_INVALID_REQUEST);
        return ESP_ERR_INVALID_ARG;
    }

    cJSON* id = cJSON_GetObjectItem(json, "id");
    if (id == NULL || !cJSON_IsNumber(id)) {
        ESP_LOGE(TAG, "Invalid id for method: %s", method_str);
        esp_mcp_server_reply_error(server, MCP_ERROR_CODE_INVALID_REQUEST, MCP_ERROR_MESSAGE_INVALID_REQUEST);
        return ESP_ERR_INVALID_ARG;
    }
    int id_int = id->valueint;

    if (!strcmp(method_str, "initialize")) {
        return esp_mcp_server_handle_initialize(server, params, id_int);
    } else if (!strcmp(method_str, "tools/list")) {
        return esp_mcp_server_handle_tools_list(server, params, id_int);
    } else if (!strcmp(method_str, "tools/call")) {
        return esp_mcp_server_handle_tools_call(server, params, id_int);
    } else if (!strcmp(method_str, "ping")) {
        return esp_mcp_server_handle_ping(server, params, id_int);
    } else {
        ESP_LOGE(TAG, "Method not implemented: %s", method_str);
        char error_buf[MCP_ERROR_BUF_MAX_SIZE] = {0};
        snprintf(error_buf, sizeof(error_buf), "%s: ", MCP_ERROR_MESSAGE_METHOD_NOT_FOUND);
        char* error_msg = calloc(strlen(error_buf) + strlen(method_str) + 1, sizeof(char));
        if (error_msg) {
            strcpy(error_msg, error_buf);
            strcat(error_msg, method_str);
            esp_mcp_server_reply_error(server, id_int, error_msg);
            free(error_msg);
        } else {
            esp_mcp_server_reply_error(server, id_int, MCP_ERROR_MESSAGE_METHOD_NOT_FOUND);
        }
    }

    return ESP_OK;
}

esp_err_t esp_mcp_server_parse_message(esp_mcp_server_t* server, const char* message)
{
    ESP_RETURN_ON_FALSE(server, ESP_ERR_INVALID_ARG, TAG, "Invalid server");
    ESP_RETURN_ON_FALSE(message, ESP_ERR_INVALID_ARG, TAG, "Invalid message");

    cJSON* json = cJSON_Parse(message);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse MCP message: %s", message);
        esp_mcp_server_reply_error(server, MCP_ERROR_CODE_PARSE_ERROR, MCP_ERROR_MESSAGE_PARSE_ERROR);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = esp_mcp_server_parse_json_message(server, json);
    cJSON_Delete(json);

    return ret;
}
