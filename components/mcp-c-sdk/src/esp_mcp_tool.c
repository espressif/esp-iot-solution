/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
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

#ifndef CONFIG_MCP_TOOL_RESULT_TEXT_MAX_LEN
#define CONFIG_MCP_TOOL_RESULT_TEXT_MAX_LEN 8192
#endif

#ifndef CONFIG_MCP_TOOL_RESULT_BLOB_MAX_LEN
#define CONFIG_MCP_TOOL_RESULT_BLOB_MAX_LEN 65536
#endif

static bool esp_mcp_tool_str_len_ok(const char *s, size_t max_len)
{
    return s && strnlen(s, max_len + 1) <= max_len;
}

struct esp_mcp_tool_result_s {
    bool is_error;
    cJSON *content;            // array
    cJSON *structured;         // object or NULL
};

static esp_err_t esp_mcp_tool_result_to_json_obj(const esp_mcp_tool_result_t *r, cJSON **out_obj)
{
    ESP_RETURN_ON_FALSE(r && out_obj, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    ESP_RETURN_ON_FALSE(r->content, ESP_ERR_INVALID_STATE, TAG, "Invalid content");

    cJSON *obj = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(obj, ESP_ERR_NO_MEM, TAG, "No mem");

    cJSON *content_dup = cJSON_Duplicate(r->content, 1);
    if (!content_dup || !cJSON_AddItemToObject(obj, "content", content_dup)) {
        cJSON_Delete(content_dup);
        cJSON_Delete(obj);
        return ESP_ERR_NO_MEM;
    }
    if (!cJSON_AddBoolToObject(obj, "isError", r->is_error)) {
        cJSON_Delete(obj);
        return ESP_ERR_NO_MEM;
    }
    if (r->structured) {
        cJSON *struct_dup = cJSON_Duplicate(r->structured, 1);
        if (!struct_dup || !cJSON_AddItemToObject(obj, "structuredContent", struct_dup)) {
            cJSON_Delete(struct_dup);
            cJSON_Delete(obj);
            return ESP_ERR_NO_MEM;
        }
    }

    *out_obj = obj;
    return ESP_OK;
}

esp_mcp_tool_result_t *esp_mcp_tool_result_create(void)
{
    esp_mcp_tool_result_t *r = calloc(1, sizeof(*r));
    if (!r) {
        return NULL;
    }
    r->content = cJSON_CreateArray();
    if (!r->content) {
        free(r);
        return NULL;
    }
    r->is_error = false;
    r->structured = NULL;
    return r;
}

esp_err_t esp_mcp_tool_result_destroy(esp_mcp_tool_result_t *result)
{
    ESP_RETURN_ON_FALSE(result, ESP_ERR_INVALID_ARG, TAG, "Invalid result");
    if (result->content) {
        cJSON_Delete(result->content);
    }
    if (result->structured) {
        cJSON_Delete(result->structured);
    }
    free(result);
    return ESP_OK;
}

esp_err_t esp_mcp_tool_result_set_is_error(esp_mcp_tool_result_t *result, bool is_error)
{
    ESP_RETURN_ON_FALSE(result, ESP_ERR_INVALID_ARG, TAG, "Invalid result");
    result->is_error = is_error;
    return ESP_OK;
}

static esp_err_t esp_mcp_tool_result_add_content_item(esp_mcp_tool_result_t *result, cJSON *item)
{
    ESP_RETURN_ON_FALSE(result && item, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    if (!result->content) {
        cJSON_Delete(item);
        ESP_LOGE(TAG, "Invalid content array");
        return ESP_ERR_INVALID_STATE;
    }
    if (!cJSON_AddItemToArray(result->content, item)) {
        cJSON_Delete(item);
        ESP_LOGE(TAG, "content append failed");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t esp_mcp_tool_result_add_text(esp_mcp_tool_result_t *result, const char *text)
{
    ESP_RETURN_ON_FALSE(result && text, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    ESP_RETURN_ON_FALSE(esp_mcp_tool_str_len_ok(text, CONFIG_MCP_TOOL_RESULT_TEXT_MAX_LEN),
                        ESP_ERR_INVALID_SIZE,
                        TAG,
                        "Text content too large");
    cJSON *t = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(t, ESP_ERR_NO_MEM, TAG, "No mem");
    if (!cJSON_AddStringToObject(t, "type", "text") ||
            !cJSON_AddStringToObject(t, "text", text)) {
        cJSON_Delete(t);
        return ESP_ERR_NO_MEM;
    }
    return esp_mcp_tool_result_add_content_item(result, t);
}

esp_err_t esp_mcp_tool_result_add_image_base64(esp_mcp_tool_result_t *result, const char *mime_type, const char *base64_data)
{
    ESP_RETURN_ON_FALSE(result && mime_type && base64_data, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    ESP_RETURN_ON_FALSE(esp_mcp_tool_str_len_ok(base64_data, CONFIG_MCP_TOOL_RESULT_BLOB_MAX_LEN),
                        ESP_ERR_INVALID_SIZE,
                        TAG,
                        "Image data too large");
    cJSON *t = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(t, ESP_ERR_NO_MEM, TAG, "No mem");
    if (!cJSON_AddStringToObject(t, "type", "image") ||
            !cJSON_AddStringToObject(t, "mimeType", mime_type) ||
            !cJSON_AddStringToObject(t, "data", base64_data)) {
        cJSON_Delete(t);
        return ESP_ERR_NO_MEM;
    }
    return esp_mcp_tool_result_add_content_item(result, t);
}

esp_err_t esp_mcp_tool_result_add_audio_base64(esp_mcp_tool_result_t *result, const char *mime_type, const char *base64_data)
{
    ESP_RETURN_ON_FALSE(result && mime_type && base64_data, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    ESP_RETURN_ON_FALSE(esp_mcp_tool_str_len_ok(base64_data, CONFIG_MCP_TOOL_RESULT_BLOB_MAX_LEN),
                        ESP_ERR_INVALID_SIZE,
                        TAG,
                        "Audio data too large");
    cJSON *t = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(t, ESP_ERR_NO_MEM, TAG, "No mem");
    if (!cJSON_AddStringToObject(t, "type", "audio") ||
            !cJSON_AddStringToObject(t, "mimeType", mime_type) ||
            !cJSON_AddStringToObject(t, "data", base64_data)) {
        cJSON_Delete(t);
        return ESP_ERR_NO_MEM;
    }
    return esp_mcp_tool_result_add_content_item(result, t);
}

esp_err_t esp_mcp_tool_result_add_resource_link(esp_mcp_tool_result_t *result,
                                                const char *uri,
                                                const char *name,
                                                const char *description,
                                                const char *mime_type)
{
    ESP_RETURN_ON_FALSE(result && uri, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    cJSON *t = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(t, ESP_ERR_NO_MEM, TAG, "No mem");
    if (!cJSON_AddStringToObject(t, "type", "resource_link") ||
            !cJSON_AddStringToObject(t, "uri", uri)) {
        cJSON_Delete(t);
        return ESP_ERR_NO_MEM;
    }
    if (name && !cJSON_AddStringToObject(t, "name", name)) {
        cJSON_Delete(t);
        return ESP_ERR_NO_MEM;
    }
    if (description && !cJSON_AddStringToObject(t, "description", description)) {
        cJSON_Delete(t);
        return ESP_ERR_NO_MEM;
    }
    if (mime_type && !cJSON_AddStringToObject(t, "mimeType", mime_type)) {
        cJSON_Delete(t);
        return ESP_ERR_NO_MEM;
    }
    return esp_mcp_tool_result_add_content_item(result, t);
}

static esp_err_t esp_mcp_tool_result_add_embedded_resource(esp_mcp_tool_result_t *result, cJSON *resource_obj)
{
    ESP_RETURN_ON_FALSE(result && resource_obj, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    cJSON *t = cJSON_CreateObject();
    if (!t) {
        cJSON_Delete(resource_obj);
        ESP_LOGE(TAG, "No mem");
        return ESP_ERR_NO_MEM;
    }
    if (!cJSON_AddStringToObject(t, "type", "resource") ||
            !cJSON_AddItemToObject(t, "resource", resource_obj)) {
        cJSON_Delete(resource_obj);
        cJSON_Delete(t);
        ESP_LOGE(TAG, "No mem");
        return ESP_ERR_NO_MEM;
    }
    return esp_mcp_tool_result_add_content_item(result, t);
}

esp_err_t esp_mcp_tool_result_add_embedded_resource_text(esp_mcp_tool_result_t *result,
                                                         const char *uri,
                                                         const char *mime_type,
                                                         const char *text)
{
    ESP_RETURN_ON_FALSE(result && uri && mime_type && text, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    cJSON *r = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(r, ESP_ERR_NO_MEM, TAG, "No mem");
    if (!cJSON_AddStringToObject(r, "uri", uri) ||
            !cJSON_AddStringToObject(r, "mimeType", mime_type) ||
            !cJSON_AddStringToObject(r, "text", text)) {
        cJSON_Delete(r);
        return ESP_ERR_NO_MEM;
    }
    return esp_mcp_tool_result_add_embedded_resource(result, r);
}

esp_err_t esp_mcp_tool_result_add_embedded_resource_blob(esp_mcp_tool_result_t *result,
                                                         const char *uri,
                                                         const char *mime_type,
                                                         const char *base64_blob)
{
    ESP_RETURN_ON_FALSE(result && uri && mime_type && base64_blob, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    cJSON *r = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(r, ESP_ERR_NO_MEM, TAG, "No mem");
    if (!cJSON_AddStringToObject(r, "uri", uri) ||
            !cJSON_AddStringToObject(r, "mimeType", mime_type) ||
            !cJSON_AddStringToObject(r, "blob", base64_blob)) {
        cJSON_Delete(r);
        return ESP_ERR_NO_MEM;
    }
    return esp_mcp_tool_result_add_embedded_resource(result, r);
}

esp_err_t esp_mcp_tool_result_set_structured_json(esp_mcp_tool_result_t *result, const char *structured_json_object)
{
    ESP_RETURN_ON_FALSE(result && structured_json_object, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    cJSON *o = cJSON_Parse(structured_json_object);
    ESP_RETURN_ON_FALSE(o, ESP_ERR_INVALID_ARG, TAG, "Invalid JSON object");
    if (!cJSON_IsObject(o)) {
        cJSON_Delete(o);
        ESP_LOGE(TAG, "structured JSON must be an object");
        return ESP_ERR_INVALID_ARG;
    }
    if (result->structured) {
        cJSON_Delete(result->structured);
    }
    result->structured = o;
    return ESP_OK;
}

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

static esp_mcp_tool_t *esp_mcp_tool_create_base(const char *name, const char *description)
{
    ESP_RETURN_ON_FALSE(name, NULL, TAG, "Invalid name");
    ESP_RETURN_ON_FALSE(description, NULL, TAG, "Invalid description");

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

    tool->callback = NULL;
    tool->properties = esp_mcp_property_list_create();
    if (!tool->properties) {
        ESP_LOGE(TAG, "esp_mcp_tool_create_base: Failed to create property list");
        free(tool->description);
        free(tool->name);
        free(tool);
        return NULL;
    }
    tool->task_support[0] = '\0';

    return tool;
}

esp_mcp_tool_t *esp_mcp_tool_create(const char *name, const char *description, esp_mcp_tool_callback_t callback)
{
    ESP_RETURN_ON_FALSE(callback, NULL, TAG, "Invalid callback");
    esp_mcp_tool_t *tool = esp_mcp_tool_create_base(name, description);
    ESP_RETURN_ON_FALSE(tool, NULL, TAG, "Failed to create tool");
    tool->callback = callback;
    return tool;
}

esp_mcp_tool_t *esp_mcp_tool_create_ex(const char *name, const char *title, const char *description, esp_mcp_tool_callback_ex_t callback)
{
    ESP_RETURN_ON_FALSE(name && description && callback, NULL, TAG, "Invalid args");
    esp_mcp_tool_t *tool = esp_mcp_tool_create_base(name, description);
    ESP_RETURN_ON_FALSE(tool, NULL, TAG, "Failed to create tool");
    tool->callback_ex = (void *)callback;
    if (title) {
        (void)esp_mcp_tool_set_title(tool, title);
    }
    return tool;
}

static esp_err_t esp_mcp_tool_set_dup_str(char **dst, const char *src)
{
    if (*dst) {
        free(*dst);
        *dst = NULL;
    }
    if (!src) {
        return ESP_OK;
    }
    *dst = strdup(src);
    return *dst ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t esp_mcp_tool_set_title(esp_mcp_tool_t *tool, const char *title)
{
    ESP_RETURN_ON_FALSE(tool, ESP_ERR_INVALID_ARG, TAG, "Invalid tool");
    return esp_mcp_tool_set_dup_str(&tool->title, title);
}

esp_err_t esp_mcp_tool_set_icons_json(esp_mcp_tool_t *tool, const char *icons_json)
{
    ESP_RETURN_ON_FALSE(tool, ESP_ERR_INVALID_ARG, TAG, "Invalid tool");
    return esp_mcp_tool_set_dup_str(&tool->icons_json, icons_json);
}

esp_err_t esp_mcp_tool_set_output_schema_json(esp_mcp_tool_t *tool, const char *schema_json)
{
    ESP_RETURN_ON_FALSE(tool, ESP_ERR_INVALID_ARG, TAG, "Invalid tool");
    return esp_mcp_tool_set_dup_str(&tool->output_schema_json, schema_json);
}

esp_err_t esp_mcp_tool_set_annotations_json(esp_mcp_tool_t *tool, const char *annotations_json)
{
    ESP_RETURN_ON_FALSE(tool, ESP_ERR_INVALID_ARG, TAG, "Invalid tool");
    return esp_mcp_tool_set_dup_str(&tool->annotations_json, annotations_json);
}

esp_err_t esp_mcp_tool_set_task_support(esp_mcp_tool_t *tool, const char *task_support)
{
    ESP_RETURN_ON_FALSE(tool, ESP_ERR_INVALID_ARG, TAG, "Invalid tool");
    if (!task_support) {
        tool->task_support[0] = '\0';
        return ESP_OK;
    }
    if (strcmp(task_support, "required") != 0 &&
            strcmp(task_support, "optional") != 0 &&
            strcmp(task_support, "forbidden") != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    strncpy(tool->task_support, task_support, sizeof(tool->task_support) - 1);
    tool->task_support[sizeof(tool->task_support) - 1] = '\0';
    return ESP_OK;
}

char *esp_mcp_tool_to_json(const esp_mcp_tool_t *tool)
{
    ESP_RETURN_ON_FALSE(tool, NULL, TAG, "Invalid tool");

    cJSON *json = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(json, NULL, TAG, "Failed to allocate memory for json");

    cJSON_AddStringToObject(json, "name", tool->name);
    if (tool->title) {
        cJSON_AddStringToObject(json, "title", tool->title);
    }
    cJSON_AddStringToObject(json, "description", tool->description);
    if (tool->icons_json) {
        cJSON *icons = cJSON_Parse(tool->icons_json);
        if (icons && cJSON_IsArray(icons)) {
            cJSON_AddItemToObject(json, "icons", icons);
        } else if (icons) {
            cJSON_Delete(icons);
        }
    }

    cJSON *input_schema = cJSON_CreateObject();
    if (!input_schema) {
        ESP_LOGE(TAG, "Failed to allocate memory for input schema");
        cJSON_Delete(json);
        return NULL;
    }
    cJSON_AddStringToObject(input_schema, "type", "object");
    cJSON_AddBoolToObject(input_schema, "additionalProperties", false);

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

    if (tool->output_schema_json) {
        cJSON *out_schema = cJSON_Parse(tool->output_schema_json);
        if (out_schema && cJSON_IsObject(out_schema)) {
            cJSON_AddItemToObject(json, "outputSchema", out_schema);
        } else if (out_schema) {
            cJSON_Delete(out_schema);
        }
    }
    if (tool->annotations_json) {
        cJSON *ann = cJSON_Parse(tool->annotations_json);
        if (ann && cJSON_IsObject(ann)) {
            cJSON_AddItemToObject(json, "annotations", ann);
        } else if (ann) {
            cJSON_Delete(ann);
        }
    }
    if (tool->task_support[0]) {
        cJSON *exec = cJSON_CreateObject();
        if (exec) {
            cJSON_AddStringToObject(exec, "taskSupport", tool->task_support);
            cJSON_AddItemToObject(json, "execution", exec);
        }
    }

    char *result = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    ESP_RETURN_ON_FALSE(result, NULL, TAG, "Failed to print tool to json");
    return result;
}

char *esp_mcp_tool_call(const esp_mcp_tool_t *tool, const esp_mcp_property_list_t *properties)
{
    ESP_RETURN_ON_FALSE(tool, NULL, TAG, "Invalid tool");
    ESP_RETURN_ON_FALSE(properties, NULL, TAG, "Invalid properties");

    esp_mcp_tool_result_t *r = esp_mcp_tool_result_create();
    ESP_RETURN_ON_FALSE(r, NULL, TAG, "No mem");

    if (tool->callback_ex) {
        esp_mcp_tool_callback_ex_t cb = (esp_mcp_tool_callback_ex_t)tool->callback_ex;
        esp_err_t cb_ret = cb(properties, r);
        if (cb_ret != ESP_OK) {
            (void)esp_mcp_tool_result_set_is_error(r, true);
            if (cJSON_GetArraySize(r->content) == 0) {
                (void)esp_mcp_tool_result_add_text(r, "Tool execution failed");
            }
        }
    } else {
        if (!tool->callback) {
            ESP_LOGE(TAG, "Invalid callback");
            esp_mcp_tool_result_destroy(r);
            return NULL;
        }
        esp_mcp_value_t v = tool->callback(properties);
        char text_str[64] = {0};
        switch (v.type) {
        case ESP_MCP_VALUE_TYPE_BOOLEAN:
            (void)esp_mcp_tool_result_add_text(r, v.data.bool_value ? "true" : "false");
            break;
        case ESP_MCP_VALUE_TYPE_INTEGER:
            snprintf(text_str, sizeof(text_str), "%d", v.data.int_value);
            (void)esp_mcp_tool_result_add_text(r, text_str);
            break;
        case ESP_MCP_VALUE_TYPE_FLOAT:
            snprintf(text_str, sizeof(text_str), "%g", v.data.float_value);
            (void)esp_mcp_tool_result_add_text(r, text_str);
            break;
        case ESP_MCP_VALUE_TYPE_STRING:
            (void)esp_mcp_tool_result_add_text(r, v.data.string_value ? v.data.string_value : "");
            break;
        default:
            (void)esp_mcp_tool_result_add_text(r, "");
            break;
        }
        esp_mcp_value_destroy(&v);
    }

    cJSON *result_obj = NULL;
    if (esp_mcp_tool_result_to_json_obj(r, &result_obj) != ESP_OK) {
        esp_mcp_tool_result_destroy(r);
        return NULL;
    }
    char *result_str = cJSON_PrintUnformatted(result_obj);
    cJSON_Delete(result_obj);
    esp_mcp_tool_result_destroy(r);

    ESP_RETURN_ON_FALSE(result_str, NULL, TAG, "Failed to print tool result to json");
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
    if (tool->title) {
        free(tool->title);
    }
    if (tool->icons_json) {
        free(tool->icons_json);
    }
    if (tool->output_schema_json) {
        free(tool->output_schema_json);
    }
    if (tool->annotations_json) {
        free(tool->annotations_json);
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

    esp_err_t ret = ESP_ERR_NOT_FOUND;
    if (list->mutex) {
        if (xSemaphoreTake(list->mutex, portMAX_DELAY) == pdTRUE) {
            esp_mcp_tool_item_t *info = NULL;
            esp_mcp_tool_item_t *info_next = NULL;
            SLIST_FOREACH_SAFE(info, &list->tools, next, info_next) {
                if (info->tools != tool) {
                    continue;
                }
                SLIST_REMOVE(&list->tools, info, esp_mcp_tool_item_s, next);
                free(info);
                ret = ESP_OK;
                break;
            }
            xSemaphoreGive(list->mutex);
        }
    }

    return ret;
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
