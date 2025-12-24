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
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <cJSON.h>

#include "esp_mcp_item.h"
#include "esp_mcp_error.h"
#include "esp_mcp_property_priv.h"
#include "esp_mcp_tool_priv.h"
#include "esp_mcp_priv.h"

static const char *TAG = "esp_mcp_engine";

static esp_err_t esp_mcp_add_mbuf(esp_mcp_t *mcp, uint16_t id, uint8_t *outbuf, uint16_t outlen)
{
    if (mcp->mbuf.outbuf) {
        cJSON_free(mcp->mbuf.outbuf);
        mcp->mbuf.outbuf = NULL;
        mcp->mbuf.outlen = 0;
        mcp->mbuf.id = 0;
    }

    mcp->mbuf.id = id;
    mcp->mbuf.outbuf = outbuf;
    mcp->mbuf.outlen = outlen;

    return ESP_OK;
}

static esp_err_t esp_mcp_reply_result(esp_mcp_t *mcp, int id, const char *result)
{
    ESP_RETURN_ON_FALSE(mcp && result, ESP_ERR_INVALID_ARG, TAG, "Invalid server or result");

    cJSON *payload = cJSON_CreateObject();
    if (!payload) {
        ESP_LOGE(TAG, "Failed to allocate memory for payload");
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(payload, "jsonrpc", "2.0");
    cJSON_AddNumberToObject(payload, "id", id);

    cJSON *result_obj = cJSON_Parse(result);
    if (result_obj) {
        cJSON_AddItemToObject(payload, "result", result_obj);
    } else {
        cJSON_AddStringToObject(payload, "result", result);
    }

    char *payload_str = cJSON_PrintUnformatted(payload);
    if (payload_str) {
        esp_mcp_add_mbuf(mcp, id, (uint8_t*)payload_str, strlen(payload_str));
    }

    cJSON_Delete(payload);

    return ESP_OK;
}

static esp_err_t esp_mcp_reply_error(esp_mcp_t *mcp, int error_code, const char *message, int request_id)
{
    ESP_RETURN_ON_FALSE(mcp && message, ESP_ERR_INVALID_ARG, TAG, "Invalid server or message");

    cJSON *payload = cJSON_CreateObject();
    if (!payload) {
        ESP_LOGE(TAG, "Failed to allocate memory for payload");
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(payload, "jsonrpc", "2.0");

    if (error_code == MCP_ERROR_CODE_PARSE_ERROR || error_code == MCP_ERROR_CODE_INVALID_REQUEST) {
        cJSON_AddNullToObject(payload, "id");
    } else if (request_id != 0) {
        cJSON_AddNumberToObject(payload, "id", request_id);
    } else {
        cJSON_AddNullToObject(payload, "id");
    }

    cJSON *error = cJSON_Parse(message);
    if (!error) {
        error = cJSON_CreateObject();
        if (!error) {
            ESP_LOGE(TAG, "Failed to allocate memory for error");
            cJSON_Delete(payload);
            return ESP_ERR_NO_MEM;
        }
        cJSON_AddNumberToObject(error, "code", error_code);
        cJSON_AddStringToObject(error, "message", message);
    }
    cJSON_AddItemToObject(payload, "error", error);

    char *payload_str = cJSON_PrintUnformatted(payload);
    if (payload_str) {
        esp_mcp_add_mbuf(mcp, request_id, (uint8_t*)payload_str, strlen(payload_str));
    }
    cJSON_Delete(payload);

    return ESP_OK;
}

static esp_err_t esp_mcp_parse_capabilities(esp_mcp_t *mcp, const cJSON *capabilities)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid server");
    ESP_RETURN_ON_FALSE(capabilities, ESP_ERR_INVALID_ARG, TAG, "Invalid capabilities");

    cJSON *vision = cJSON_GetObjectItem(capabilities, "vision");
    if (cJSON_IsObject(vision)) {
        cJSON *url = cJSON_GetObjectItem(vision, "url");
        cJSON *token = cJSON_GetObjectItem(vision, "token");
        if (cJSON_IsString(url)) {
            ESP_LOGI(TAG, "Vision URL: %s", url->valuestring);
        }

        if (cJSON_IsString(token)) {
            const char *token_str = token->valuestring;
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

static esp_err_t esp_mcp_handle_initialize(esp_mcp_t *mcp, const cJSON *params, int id)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid server");

    cJSON *capabilities_param = cJSON_GetObjectItem(params, "capabilities");
    if (capabilities_param && cJSON_IsObject(capabilities_param)) {
        esp_mcp_parse_capabilities(mcp, capabilities_param);
    }

    const esp_app_desc_t *app_desc = esp_app_get_description();
    cJSON *message = cJSON_CreateObject();
    if (!message) {
        ESP_LOGE(TAG, "Failed to allocate memory for message");
        return ESP_ERR_NO_MEM;
    }

    cJSON *tools = cJSON_CreateObject();
    cJSON *experimental = cJSON_CreateObject();
    cJSON *server_info = cJSON_CreateObject();
    cJSON *capabilities = cJSON_CreateObject();

    if (!tools || !experimental || !server_info || !capabilities) {
        ESP_LOGE(TAG, "Failed to allocate memory for message components");
        if (tools) {
            cJSON_Delete(tools);
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
    cJSON_AddItemToObject(capabilities, "experimental", experimental);
    cJSON_AddItemToObject(capabilities, "tools", tools);

    cJSON_AddStringToObject(server_info, "name", app_desc->project_name);
    cJSON_AddStringToObject(server_info, "version", app_desc->version);

    cJSON_AddStringToObject(message, "protocolVersion", "2024-11-05");
    cJSON_AddItemToObject(message, "capabilities", capabilities);
    cJSON_AddItemToObject(message, "serverInfo", server_info);

    char *message_str = cJSON_PrintUnformatted(message);
    if (message_str) {
        esp_mcp_reply_result(mcp, id, message_str);
        cJSON_free(message_str);
    }
    cJSON_Delete(message);

    return ESP_OK;
}

static esp_err_t esp_mcp_tools_foreach_cb(esp_mcp_tool_t *tool, void *arg)
{
    esp_mcp_tool_foreach_ctx_t *ctx = (esp_mcp_tool_foreach_ctx_t *)arg;

    if (!(*ctx->found_cursor)) {
        if (!strcmp(tool->name, ctx->cursor_str)) {
            *ctx->found_cursor = true;
        } else {
            return ESP_OK;
        }
    }

    char *tool_json = esp_mcp_tool_to_json(tool);
    if (tool_json) {
        char *tmp_payload = cJSON_PrintUnformatted(ctx->tools);
        if (!tmp_payload) {
            cJSON_free(tool_json);
            return ESP_OK;
        }

        if (strlen(tmp_payload) > CONFIG_MCP_TOOLLIST_MAX_SIZE) {
            strncpy(ctx->next_cursor, tool->name, sizeof(ctx->next_cursor) - 1);
            ctx->next_cursor[sizeof(ctx->next_cursor) - 1] = '\0';
            cJSON_free(tool_json);
            cJSON_free(tmp_payload);
            ctx->should_break = true;
            return ESP_FAIL;
        }

        cJSON *tool_obj = cJSON_Parse(tool_json);
        if (tool_obj) {
            cJSON_AddItemToArray(ctx->tools_array, tool_obj);
        } else {
            ESP_LOGE(TAG, "Failed to parse tool JSON: %s", tool_json);
        }
        cJSON_free(tool_json);
        cJSON_free(tmp_payload);
    }
    return ESP_OK;
}

static esp_err_t esp_mcp_handle_tools_list(esp_mcp_t *mcp, const cJSON *params, int id)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid server");

    char cursor_str[256] = {0};
    char next_cursor[256] = {0};
    bool found_cursor = true;
    cJSON *cursor = cJSON_GetObjectItem(params, "cursor");
    if (cursor && cJSON_IsString(cursor)) {
        strncpy(cursor_str, cursor->valuestring, sizeof(cursor_str) - 1);
        cursor_str[sizeof(cursor_str) - 1] = '\0';
        found_cursor = false;
    }

    esp_err_t ret = ESP_OK;
    char *payload_str = NULL;

    cJSON *tools = cJSON_CreateObject();
    if (!tools) {
        ESP_LOGE(TAG, "Failed to allocate memory for tools");
        return ESP_ERR_NO_MEM;
    }

    cJSON *tools_array = cJSON_CreateArray();
    if (!tools_array) {
        ESP_LOGE(TAG, "Failed to allocate memory for tools array");
        cJSON_Delete(tools);
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddItemToObject(tools, "tools", tools_array);

    esp_mcp_tool_foreach_ctx_t ctx = {
        .tools = (void *)tools,
        .tools_array = (void *)tools_array,
        .cursor_str = cursor_str,
        .next_cursor = next_cursor,
        .found_cursor = &found_cursor,
        .should_break = false
    };

    esp_mcp_tool_list_foreach(mcp->tools, esp_mcp_tools_foreach_cb, &ctx);

    if (strlen(next_cursor)) {
        cJSON_AddStringToObject(tools, "nextCursor", next_cursor);
    }

    payload_str = cJSON_PrintUnformatted(tools);
    if (payload_str) {
        bool list_not_empty = !esp_mcp_tool_list_is_empty(mcp->tools);
        if (strlen(payload_str) == 12 && list_not_empty) {
            ESP_LOGE(TAG, "tools/list: Failed to add tool %s because of payload size limit", next_cursor);
            char error_buf[MCP_ERROR_BUF_MAX_SIZE] = {0};
            snprintf(error_buf, sizeof(error_buf), "%s: ", MCP_ERROR_MESSAGE_FAILED_TO_ADD_TOOL);
            char *error_msg = calloc(strlen(error_buf) + strlen(next_cursor) + 1, sizeof(char));
            if (error_msg) {
                strcpy(error_msg, error_buf);
                strcat(error_msg, next_cursor);
                esp_mcp_reply_error(mcp, MCP_ERROR_CODE_FAILED_TO_ADD_TOOL, error_msg, id);
                free(error_msg);
            } else {
                esp_mcp_reply_error(mcp, MCP_ERROR_CODE_FAILED_TO_ADD_TOOL, MCP_ERROR_MESSAGE_FAILED_TO_ADD_TOOL, id);
            }
            ret = ESP_ERR_NO_MEM;
        } else {
            esp_mcp_reply_result(mcp, id, payload_str);
        }
        cJSON_free(payload_str);
    }
    cJSON_Delete(tools);

    return ret;
}

static esp_err_t esp_mcp_tool_args_cb(const esp_mcp_property_t *property, void *arg)
{
    esp_mcp_property_foreach_ctx_t *ctx = (esp_mcp_property_foreach_ctx_t *)arg;
    const char *name = property->name;
    cJSON *value = cJSON_GetObjectItem(ctx->arg, name);
    if (value) {
        if (property->type == ESP_MCP_PROPERTY_TYPE_BOOLEAN) {
            esp_mcp_property_list_add_property(ctx->list, esp_mcp_property_create_with_bool(name, value->valueint == 1));
        } else if (property->type == ESP_MCP_PROPERTY_TYPE_INTEGER) {
            int int_value = value->valueint;
            if (property->has_range) {
                if (int_value < property->min_value || int_value > property->max_value) {
                    ESP_LOGE(TAG, "tools/call: Integer argument %s (%d) is out of range [%d, %d]",
                             name, int_value, property->min_value, property->max_value);
                    ctx->has_error = true;
                    return ESP_ERR_INVALID_ARG;
                }
            }
            esp_mcp_property_list_add_property(ctx->list, esp_mcp_property_create_with_int(name, int_value));
        } else if (property->type == ESP_MCP_PROPERTY_TYPE_STRING) {
            esp_mcp_property_list_add_property(ctx->list, esp_mcp_property_create_with_string(name, value->valuestring));
        } else if (property->type == ESP_MCP_PROPERTY_TYPE_ARRAY) {
            char *value_str = cJSON_PrintUnformatted(value);
            if (value_str) {
                esp_mcp_property_list_add_property(ctx->list, esp_mcp_property_create_with_array(name, value_str));
                cJSON_free(value_str);
            } else {
                ctx->has_error = true;
                ESP_LOGE(TAG, "tools/call: Failed to print array argument: %s", name);
                return ESP_ERR_INVALID_ARG;
            }
        } else if (property->type == ESP_MCP_PROPERTY_TYPE_OBJECT) {
            char *value_str = cJSON_PrintUnformatted(value);
            if (value_str) {
                esp_mcp_property_list_add_property(ctx->list, esp_mcp_property_create_with_object(name, value_str));
                cJSON_free(value_str);
            } else {
                ctx->has_error = true;
                ESP_LOGE(TAG, "tools/call: Failed to print object argument: %s", name);
                return ESP_ERR_INVALID_ARG;
            }
        }
    } else {
        ctx->has_error = true;
        ESP_LOGE(TAG, "tools/call: Missing argument: %s", name);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static esp_err_t esp_mcp_do_tool_call(esp_mcp_t *mcp, int id, const char *tool_name, const cJSON *tool_arguments, int stack_size)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid server");
    ESP_RETURN_ON_FALSE(tool_name, ESP_ERR_INVALID_ARG, TAG, "Invalid tool name");
    ESP_RETURN_ON_FALSE(tool_arguments, ESP_ERR_INVALID_ARG, TAG, "Invalid tool arguments");
    ESP_RETURN_ON_FALSE(stack_size > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid stack size");

    esp_mcp_tool_t *tool = esp_mcp_tool_list_find_tool(mcp->tools, tool_name);
    if (!tool) {
        ESP_LOGE(TAG, "tools/call: Unknown tool: %s", tool_name);
        char *error_msg = NULL;
        char error_buf[MCP_ERROR_BUF_MAX_SIZE] = {0};
        snprintf(error_buf, sizeof(error_buf), "%s: ", MCP_ERROR_MESSAGE_INVALID_PARAMS);
        error_msg = calloc(strlen(error_buf) + strlen(tool_name) + 1, sizeof(char));
        if (error_msg) {
            strcpy(error_msg, error_buf);
            strcat(error_msg, tool_name);
            esp_mcp_reply_error(mcp, MCP_ERROR_CODE_INVALID_PARAMS, error_msg, id);
            free(error_msg);
        } else {
            esp_mcp_reply_error(mcp, MCP_ERROR_CODE_INVALID_PARAMS, MCP_ERROR_MESSAGE_INVALID_PARAMS, id);
        }
        return ESP_ERR_INVALID_ARG;
    }

    esp_mcp_property_list_t *arguments = esp_mcp_property_list_create();
    if (arguments) {
        if (tool_arguments && cJSON_IsObject(tool_arguments)) {
            esp_mcp_property_foreach_ctx_t args_ctx = {
                .list = arguments,
                .callback = esp_mcp_tool_args_cb,
                .arg = (void *)tool_arguments,
                .has_error = false
            };

            esp_err_t foreach_ret = esp_mcp_property_list_foreach(tool->properties, &args_ctx);
            if (foreach_ret != ESP_OK || args_ctx.has_error) {
                ESP_LOGE(TAG, "tools/call: Failed to process tool arguments");
                esp_mcp_property_list_destroy(arguments);
                esp_mcp_reply_error(mcp, MCP_ERROR_CODE_INVALID_PARAMS, MCP_ERROR_MESSAGE_INVALID_PARAMS, id);
                return ESP_ERR_INVALID_ARG;
            }
        }

        char *result = esp_mcp_tool_call(tool, arguments);
        if (result) {
            esp_mcp_reply_result(mcp, id, result);
            cJSON_free(result);
        } else {
            esp_mcp_reply_error(mcp, MCP_ERROR_CODE_TOOL_CALL_FAILED, MCP_ERROR_MESSAGE_TOOL_CALL_FAILED, id);
        }

        esp_mcp_property_list_destroy(arguments);
    } else {
        esp_mcp_reply_error(mcp, MCP_ERROR_CODE_FAILED_TO_CREATE_ARGUMENTS, MCP_ERROR_MESSAGE_FAILED_TO_CREATE_ARGUMENTS, id);
    }

    return ESP_OK;
}

static esp_err_t esp_mcp_handle_tools_call(esp_mcp_t *mcp, const cJSON *params, int id)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid server");
    ESP_RETURN_ON_FALSE(params, ESP_ERR_INVALID_ARG, TAG, "Invalid params");

    if (!cJSON_IsObject(params)) {
        ESP_LOGE(TAG, "tools/call: Missing params");
        esp_mcp_reply_error(mcp, MCP_ERROR_CODE_INVALID_REQUEST, MCP_ERROR_MESSAGE_INVALID_REQUEST, id);
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *tool_name = cJSON_GetObjectItem(params, "name");
    if (!cJSON_IsString(tool_name)) {
        ESP_LOGE(TAG, "tools/call: Missing name");
        esp_mcp_reply_error(mcp, MCP_ERROR_CODE_INVALID_REQUEST, MCP_ERROR_MESSAGE_INVALID_REQUEST, id);
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *tool_arguments = cJSON_GetObjectItem(params, "arguments");
    if (tool_arguments != NULL && !cJSON_IsObject(tool_arguments)) {
        ESP_LOGE(TAG, "tools/call: Invalid arguments");
        esp_mcp_reply_error(mcp, MCP_ERROR_CODE_INVALID_PARAMS, MCP_ERROR_MESSAGE_INVALID_PARAMS, id);
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *stack_size = cJSON_GetObjectItem(params, "stackSize");
    if (stack_size != NULL && !cJSON_IsNumber(stack_size)) {
        ESP_LOGE(TAG, "tools/call: Invalid stackSize");
        esp_mcp_reply_error(mcp, MCP_ERROR_CODE_INVALID_PARAMS, MCP_ERROR_MESSAGE_INVALID_PARAMS, id);
        return ESP_ERR_INVALID_ARG;
    }

    return esp_mcp_do_tool_call(mcp, id, tool_name->valuestring, tool_arguments,
                                stack_size ? stack_size->valueint : CONFIG_MCP_TOOLCALL_STACK_SIZE);
}

static esp_err_t esp_mcp_handle_ping(esp_mcp_t *mcp, const cJSON *params, int id)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid server");

    return esp_mcp_reply_result(mcp, id, "pong");
}

esp_err_t esp_mcp_get_mbuf(esp_mcp_t *mcp, uint8_t **outbuf, uint16_t *outlen)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid server");
    ESP_RETURN_ON_FALSE(outbuf, ESP_ERR_INVALID_ARG, TAG, "Invalid outbuf");
    ESP_RETURN_ON_FALSE(outlen, ESP_ERR_INVALID_ARG, TAG, "Invalid outlen");

    *outbuf = mcp->mbuf.outbuf;
    *outlen = mcp->mbuf.outlen;

    return ESP_OK;
}

esp_err_t esp_mcp_remove_mbuf(esp_mcp_t *mcp, const uint8_t *outbuf)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid server");

    esp_err_t ret = ESP_ERR_NOT_FOUND;
    if (mcp->mbuf.outbuf == outbuf) {
        cJSON_free(mcp->mbuf.outbuf);
        mcp->mbuf.outbuf = NULL;
        mcp->mbuf.outlen = 0;
        mcp->mbuf.id = 0;
        ret = ESP_OK;
    }

    return ret;
}

esp_err_t esp_mcp_create(esp_mcp_t **mcp)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP engine");

    esp_mcp_t *temp_mcp = (esp_mcp_t *)calloc(1, sizeof(esp_mcp_t));
    ESP_RETURN_ON_FALSE(temp_mcp, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for MCP engine");

    temp_mcp->tools = esp_mcp_tool_list_create();
    if (!temp_mcp->tools) {
        ESP_LOGE(TAG, "Failed to allocate memory for MCP tools");
        free(temp_mcp);
        return ESP_ERR_NO_MEM;
    }

    *mcp = temp_mcp;

    return ESP_OK;
}

esp_err_t esp_mcp_destroy(esp_mcp_t *mcp)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP engine");

    esp_err_t ret = esp_mcp_tool_list_destroy(mcp->tools);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error destroying tool list: %s", esp_err_to_name(ret));
    }

    if (mcp->mbuf.outbuf) {
        cJSON_free(mcp->mbuf.outbuf);
        mcp->mbuf.outbuf = NULL;
        mcp->mbuf.outlen = 0;
        mcp->mbuf.id = 0;
    }

    free(mcp);

    return ESP_OK;
}

esp_err_t esp_mcp_add_tool(esp_mcp_t *mcp, esp_mcp_tool_t *tool)
{
    ESP_RETURN_ON_FALSE(mcp && tool, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP engine or tool");

    esp_mcp_tool_t *existing_tool = esp_mcp_tool_list_find_tool(mcp->tools, tool->name);
    if (existing_tool) {
        ESP_LOGW(TAG, "Tool %s already added", tool->name);
        return ESP_ERR_INVALID_STATE;
    }

    return esp_mcp_tool_list_add_tool(mcp->tools, tool);
}

esp_err_t esp_mcp_remove_tool(esp_mcp_t *mcp, esp_mcp_tool_t *tool)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP engine");
    ESP_RETURN_ON_FALSE(tool, ESP_ERR_INVALID_ARG, TAG, "Invalid tool");

    return esp_mcp_tool_list_remove_tool(mcp->tools, tool);
}

static esp_err_t esp_mcp_parse_json_message(esp_mcp_t *mcp, const cJSON *json)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid server");
    ESP_RETURN_ON_FALSE(json, ESP_ERR_INVALID_ARG, TAG, "Invalid JSON");

    cJSON *version = cJSON_GetObjectItem(json, "jsonrpc");
    if (version == NULL || !cJSON_IsString(version) || strcmp(version->valuestring, "2.0") != 0) {
        ESP_LOGE(TAG, "Invalid JSONRPC version: %s", version ? version->valuestring : "null");
        esp_mcp_reply_error(mcp, MCP_ERROR_CODE_INVALID_REQUEST, MCP_ERROR_MESSAGE_INVALID_REQUEST, 0);
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *method = cJSON_GetObjectItem(json, "method");
    if (method == NULL || !cJSON_IsString(method)) {
        ESP_LOGE(TAG, "Missing method");
        esp_mcp_reply_error(mcp, MCP_ERROR_CODE_INVALID_REQUEST, MCP_ERROR_MESSAGE_INVALID_REQUEST, 0);
        return ESP_ERR_INVALID_ARG;
    }

    const char *method_str = method->valuestring;
    if (!strncmp(method_str, "notifications", strlen("notifications"))) {
        char *notification_json = cJSON_PrintUnformatted(json);
        if (notification_json) {
            ESP_LOGI(TAG, "Received notification: %s", notification_json);
            cJSON_free(notification_json);
        } else {
            ESP_LOGI(TAG, "Received notification (method: %s)", method_str);
        }
        return ESP_OK;
    }

    cJSON *params = cJSON_GetObjectItem(json, "params");
    if (params != NULL && !cJSON_IsObject(params)) {
        ESP_LOGE(TAG, "Invalid params for method: %s", method_str);
        esp_mcp_reply_error(mcp, MCP_ERROR_CODE_INVALID_REQUEST, MCP_ERROR_MESSAGE_INVALID_REQUEST, 0);
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *id = cJSON_GetObjectItem(json, "id");
    if (id == NULL || !cJSON_IsNumber(id)) {
        ESP_LOGE(TAG, "Invalid id for method: %s", method_str);
        esp_mcp_reply_error(mcp, MCP_ERROR_CODE_INVALID_REQUEST, MCP_ERROR_MESSAGE_INVALID_REQUEST, 0);
        return ESP_ERR_INVALID_ARG;
    }
    int id_int = id->valueint;

    if (!strcmp(method_str, "initialize")) {
        return esp_mcp_handle_initialize(mcp, params, id_int);
    } else if (!strcmp(method_str, "tools/list")) {
        return esp_mcp_handle_tools_list(mcp, params, id_int);
    } else if (!strcmp(method_str, "tools/call")) {
        return esp_mcp_handle_tools_call(mcp, params, id_int);
    } else if (!strcmp(method_str, "ping")) {
        return esp_mcp_handle_ping(mcp, params, id_int);
    } else {
        ESP_LOGE(TAG, "Method not implemented: %s", method_str);
        char error_buf[MCP_ERROR_BUF_MAX_SIZE] = {0};
        snprintf(error_buf, sizeof(error_buf), "%s: ", MCP_ERROR_MESSAGE_METHOD_NOT_FOUND);
        char *error_msg = calloc(strlen(error_buf) + strlen(method_str) + 1, sizeof(char));
        if (error_msg) {
            strcpy(error_msg, error_buf);
            strcat(error_msg, method_str);
            esp_mcp_reply_error(mcp, MCP_ERROR_CODE_METHOD_NOT_FOUND, error_msg, id_int);
            free(error_msg);
        } else {
            esp_mcp_reply_error(mcp, MCP_ERROR_CODE_METHOD_NOT_FOUND, MCP_ERROR_MESSAGE_METHOD_NOT_FOUND, id_int);
        }
    }

    return ESP_OK;
}

esp_err_t esp_mcp_parse_message(esp_mcp_t *mcp, const char *message)
{
    ESP_RETURN_ON_FALSE(mcp, ESP_ERR_INVALID_ARG, TAG, "Invalid server");
    ESP_RETURN_ON_FALSE(message, ESP_ERR_INVALID_ARG, TAG, "Invalid message");

    cJSON *json = cJSON_Parse(message);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse MCP message: %s", message);
        esp_mcp_reply_error(mcp, MCP_ERROR_CODE_PARSE_ERROR, MCP_ERROR_MESSAGE_PARSE_ERROR, 0);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = esp_mcp_parse_json_message(mcp, json);
    cJSON_Delete(json);

    return ret;
}
