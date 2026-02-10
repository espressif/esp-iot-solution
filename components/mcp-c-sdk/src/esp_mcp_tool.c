/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <esp_check.h>
#include <esp_log.h>
#include <cJSON.h>

#include "esp_mcp_item.h"
#include "esp_mcp_tool_priv.h"
#include "esp_mcp_property_priv.h"

static const char *TAG = "esp_mcp_tool";

static esp_err_t esp_mcp_tool_required_props_cb(const esp_mcp_property_t *property, void *arg)
{
    esp_mcp_property_foreach_ctx_t *ctx = (esp_mcp_property_foreach_ctx_t *)arg;
    if (!property->has_default_value) {
        cJSON *name_item = cJSON_CreateString(property->name);
        if (!name_item) {
            ESP_LOGE(TAG, "Failed to create required property name: %s", property->name);
            ctx->has_error = true;
            return ESP_FAIL;
        }
        cJSON_AddItemToArray(ctx->arg, name_item);
    }
    return ESP_OK;
}

esp_mcp_tool_t *esp_mcp_tool_create(const char *name, const char *description, esp_mcp_tool_callback_t callback)
{
    ESP_RETURN_ON_FALSE(name, NULL, TAG, "Invalid name");
    ESP_RETURN_ON_FALSE(description, NULL, TAG, "Invalid description");
    ESP_RETURN_ON_FALSE(callback, NULL, TAG, "Invalid callback");

    esp_mcp_tool_t *tool = (esp_mcp_tool_t *)calloc(1, sizeof(esp_mcp_tool_t));
    ESP_RETURN_ON_FALSE(tool, NULL, TAG, "Failed to allocate memory for tool");

    tool->name = strdup(name);
    if (!tool->name) {
        ESP_LOGE(TAG, "esp_mcp_tool_create: Failed to allocate memory for tool name");
        free(tool);
        return NULL;
    }

    tool->description = strdup(description);
    if (!tool->description) {
        ESP_LOGE(TAG, "esp_mcp_tool_create: Failed to allocate memory for tool description");
        free(tool->name);
        free(tool);
        return NULL;
    }

    tool->callback = callback;
    tool->properties = esp_mcp_property_list_create();

    return tool;
}

char *esp_mcp_tool_to_json(const esp_mcp_tool_t *tool)
{
    ESP_RETURN_ON_FALSE(tool, NULL, TAG, "Invalid tool");

    cJSON *json = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(json, NULL, TAG, "Failed to allocate memory for json");

    cJSON_AddStringToObject(json, "name", tool->name);
    cJSON_AddStringToObject(json, "description", tool->description);

    cJSON *input_schema = cJSON_CreateObject();
    if (!input_schema) {
        ESP_LOGE(TAG, "Failed to allocate memory for input schema");
        cJSON_Delete(json);
        return NULL;
    }
    cJSON_AddStringToObject(input_schema, "type", "object");

    char *properties_json = esp_mcp_property_list_to_json(tool->properties);
    if (properties_json) {
        cJSON *properties_obj = cJSON_Parse(properties_json);
        if (properties_obj) {
            cJSON_AddItemToObject(input_schema, "properties", properties_obj);
        }
        cJSON_free(properties_json);
    }

    cJSON *required_array = cJSON_CreateArray();
    if (!required_array) {
        ESP_LOGE(TAG, "Failed to allocate memory for required array");
        cJSON_Delete(input_schema);
        cJSON_Delete(json);
        return NULL;
    }

    esp_mcp_property_foreach_ctx_t ctx = {
        .list = tool->properties,
        .callback = esp_mcp_tool_required_props_cb,
        .arg = required_array,
        .has_error = false
    };

    esp_err_t foreach_ret = esp_mcp_property_list_foreach(tool->properties, &ctx);
    if (foreach_ret != ESP_OK || ctx.has_error) {
        cJSON_Delete(required_array);
        cJSON_Delete(input_schema);
        cJSON_Delete(json);
        return NULL;
    }

    if (cJSON_GetArraySize(required_array) > 0) {
        cJSON_AddItemToObject(input_schema, "required", required_array);
    } else {
        cJSON_Delete(required_array);
    }

    cJSON_AddItemToObject(json, "inputSchema", input_schema);

    char *result = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    ESP_RETURN_ON_FALSE(result, NULL, TAG, "Failed to print tool to json");
    return result;
}

char *esp_mcp_tool_call(const esp_mcp_tool_t *tool, const esp_mcp_property_list_t *properties)
{
    ESP_RETURN_ON_FALSE(tool, NULL, TAG, "Invalid tool");
    ESP_RETURN_ON_FALSE(tool->callback, NULL, TAG, "Invalid callback");
    ESP_RETURN_ON_FALSE(properties, NULL, TAG, "Invalid properties");

    esp_mcp_value_t result = tool->callback(properties);

    cJSON *result_json = cJSON_CreateObject();
    if (!result_json) {
        ESP_LOGE(TAG, "Failed to allocate memory for result");
        esp_mcp_value_destroy(&result);
        return NULL;
    }

    cJSON *content = cJSON_CreateArray();
    if (!content) {
        ESP_LOGE(TAG, "Failed to allocate memory for content");
        cJSON_Delete(result_json);
        esp_mcp_value_destroy(&result);
        return NULL;
    }

    cJSON *text = cJSON_CreateObject();
    if (!text) {
        ESP_LOGE(TAG, "Failed to allocate memory for text");
        cJSON_Delete(content);
        cJSON_Delete(result_json);
        esp_mcp_value_destroy(&result);
        return NULL;
    }

    cJSON_AddStringToObject(text, "type", "text");
    switch (result.type) {
    case ESP_MCP_VALUE_TYPE_BOOLEAN:
        cJSON_AddStringToObject(text, "text", result.data.bool_value ? "true" : "false");
        break;
    case ESP_MCP_VALUE_TYPE_INTEGER: {
        char text_str[22] = {0};
        snprintf(text_str, sizeof(text_str), "%d", result.data.int_value);
        cJSON_AddStringToObject(text, "text", text_str);
    }
    break;
    case ESP_MCP_VALUE_TYPE_FLOAT: {
        char text_str[32] = {0};
        snprintf(text_str, sizeof(text_str), "%g", result.data.float_value);
        cJSON_AddStringToObject(text, "text", text_str);
    }
    break;
    case ESP_MCP_VALUE_TYPE_STRING:
        cJSON_AddStringToObject(text, "text", result.data.string_value);
        break;
    default:
        ESP_LOGE(TAG, "mcp_tool_call: Unknown return type");
        break;
    }

    cJSON_AddItemToArray(content, text);
    cJSON_AddItemToObject(result_json, "content", content);
    cJSON_AddBoolToObject(result_json, "isError", false);

    char *result_str = cJSON_PrintUnformatted(result_json);
    cJSON_Delete(result_json);

    esp_mcp_value_destroy(&result);

    ESP_RETURN_ON_FALSE(result_str, NULL, TAG, "Failed to print tool call result to json");
    return result_str;
}

esp_err_t esp_mcp_tool_add_property(esp_mcp_tool_t *tool, esp_mcp_property_t *property)
{
    ESP_RETURN_ON_FALSE(tool, ESP_ERR_INVALID_ARG, TAG, "Invalid tool");
    ESP_RETURN_ON_FALSE(property, ESP_ERR_INVALID_ARG, TAG, "Invalid property");

    return esp_mcp_property_list_add_property(tool->properties, property);
}

esp_err_t esp_mcp_tool_remove_property(esp_mcp_tool_t *tool, esp_mcp_property_t *property)
{
    ESP_RETURN_ON_FALSE(tool, ESP_ERR_INVALID_ARG, TAG, "Invalid tool");
    ESP_RETURN_ON_FALSE(property, ESP_ERR_INVALID_ARG, TAG, "Invalid property");

    return esp_mcp_property_list_remove_property(tool->properties, property);
}

esp_err_t esp_mcp_tool_destroy(esp_mcp_tool_t *tool)
{
    ESP_RETURN_ON_FALSE(tool, ESP_ERR_INVALID_ARG, TAG, "Invalid tool");

    if (tool->name) {
        free(tool->name);
    }

    if (tool->description) {
        free(tool->description);
    }

    esp_mcp_property_list_destroy(tool->properties);

    free(tool);

    return ESP_OK;
}

esp_mcp_tool_list_t *esp_mcp_tool_list_create(void)
{
    esp_mcp_tool_list_t *list = (esp_mcp_tool_list_t *)calloc(1, sizeof(esp_mcp_tool_list_t));
    ESP_RETURN_ON_FALSE(list, NULL, TAG, "Failed to allocate memory for tool list");

    SLIST_INIT(&list->tools);

    list->mutex = xSemaphoreCreateMutex();
    if (!list->mutex) {
        ESP_LOGE(TAG, "Failed to create tool list mutex");
        free(list);
        return NULL;
    }

    return list;
}

esp_err_t esp_mcp_tool_list_add_tool(esp_mcp_tool_list_t *list, esp_mcp_tool_t *tool)
{
    ESP_RETURN_ON_FALSE(list, ESP_ERR_INVALID_ARG, TAG, "Invalid list");
    ESP_RETURN_ON_FALSE(tool, ESP_ERR_INVALID_ARG, TAG, "Invalid tool");

    if (xSemaphoreTake(list->mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take tool list mutex");
        return ESP_ERR_INVALID_STATE;
    }

    esp_mcp_tool_item_t *info = (esp_mcp_tool_item_t *)calloc(1, sizeof(esp_mcp_tool_item_t));
    if (!info) {
        xSemaphoreGive(list->mutex);
        ESP_LOGE(TAG, "Failed to allocate memory for info");
        return ESP_ERR_NO_MEM;
    }

    info->tools = tool;
    SLIST_INSERT_HEAD(&list->tools, info, next);

    xSemaphoreGive(list->mutex);

    return ESP_OK;
}

esp_mcp_tool_t *esp_mcp_tool_list_find_tool(const esp_mcp_tool_list_t *list, const char *name)
{
    ESP_RETURN_ON_FALSE(list, NULL, TAG, "Invalid list");
    ESP_RETURN_ON_FALSE(name, NULL, TAG, "Invalid name");

    if (xSemaphoreTake(list->mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take tool list mutex");
        return NULL;
    }

    esp_mcp_tool_t *result = NULL;
    esp_mcp_tool_item_t *info = NULL;
    SLIST_FOREACH(info, &list->tools, next) {
        if (!strcmp(info->tools->name, name)) {
            result = info->tools;
            break;
        }
    }

    xSemaphoreGive(list->mutex);
    return result;
}

esp_err_t esp_mcp_tool_list_foreach(const esp_mcp_tool_list_t *list, esp_err_t (*callback)(esp_mcp_tool_t *tool, void *arg), void *arg)
{
    ESP_RETURN_ON_FALSE(list, ESP_ERR_INVALID_ARG, TAG, "Invalid list");
    ESP_RETURN_ON_FALSE(callback, ESP_ERR_INVALID_ARG, TAG, "Invalid callback");

    if (xSemaphoreTake(list->mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take tool list mutex");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_OK;
    esp_mcp_tool_item_t *info = NULL;
    SLIST_FOREACH(info, &list->tools, next) {
        ret = callback(info->tools, arg);
        if (ret != ESP_OK) {
            break;
        }
    }

    xSemaphoreGive(list->mutex);
    return ret;
}

bool esp_mcp_tool_list_is_empty(const esp_mcp_tool_list_t *list)
{
    if (!list) {
        return true;
    }

    if (xSemaphoreTake(list->mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take tool list mutex");
        return true;
    }

    bool empty = SLIST_EMPTY(&list->tools);
    xSemaphoreGive(list->mutex);
    return empty;
}

esp_err_t esp_mcp_tool_list_remove_tool(esp_mcp_tool_list_t *list, esp_mcp_tool_t *tool)
{
    ESP_RETURN_ON_FALSE(list, ESP_ERR_INVALID_ARG, TAG, "Invalid list");
    ESP_RETURN_ON_FALSE(tool, ESP_ERR_INVALID_ARG, TAG, "Invalid tool");

    if (list->mutex) {
        if (xSemaphoreTake(list->mutex, portMAX_DELAY) == pdTRUE) {
            esp_mcp_tool_item_t *info = NULL;
            esp_mcp_tool_item_t *info_next = NULL;
            SLIST_FOREACH_SAFE(info, &list->tools, next, info_next) {
                if (info->tools != tool) {
                    continue;
                }
                SLIST_REMOVE(&list->tools, info, esp_mcp_tool_item_s, next);
                esp_mcp_tool_destroy(info->tools);
                free(info);
                break;
            }
            xSemaphoreGive(list->mutex);
        }
    }

    return ESP_OK;
}

esp_err_t esp_mcp_tool_list_destroy(esp_mcp_tool_list_t *list)
{
    ESP_RETURN_ON_FALSE(list, ESP_ERR_INVALID_ARG, TAG, "Invalid list");

    if (list->mutex) {
        if (xSemaphoreTake(list->mutex, portMAX_DELAY) == pdTRUE) {
            esp_mcp_tool_item_t *info = NULL;
            esp_mcp_tool_item_t *info_next = NULL;
            SLIST_FOREACH_SAFE(info, &list->tools, next, info_next) {
                SLIST_REMOVE(&list->tools, info, esp_mcp_tool_item_s, next);
                esp_mcp_tool_destroy(info->tools);
                free(info);
            }
            xSemaphoreGive(list->mutex);
            vSemaphoreDelete(list->mutex);
        }
    }

    free(list);

    return ESP_OK;
}
