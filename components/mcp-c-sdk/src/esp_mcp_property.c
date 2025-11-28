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
#include "esp_mcp_property_priv.h"

static const char *TAG = "esp_mcp_property";

esp_mcp_property_t *esp_mcp_property_create(const char *name, esp_mcp_property_type_t type)
{
    ESP_RETURN_ON_FALSE(name, NULL, TAG, "Invalid name");

    esp_mcp_property_t *property = (esp_mcp_property_t *)calloc(1, sizeof(esp_mcp_property_t));
    ESP_RETURN_ON_FALSE(property, NULL, TAG, "Failed to allocate memory for property");

    property->name = strdup(name);
    if (!property->name) {
        ESP_LOGE(TAG, "property_create: Failed to allocate memory for property name");
        free(property);
        return NULL;
    }

    property->type = type;
    property->has_default_value = false;
    property->has_range = false;
    property->min_value = 0;
    property->max_value = 0;

    return property;
}

esp_mcp_property_t *esp_mcp_property_create_with_bool(const char *name, bool default_value)
{
    ESP_RETURN_ON_FALSE(name, NULL, TAG, "Invalid name");

    esp_mcp_property_t *property = esp_mcp_property_create(name, ESP_MCP_PROPERTY_TYPE_BOOLEAN);
    ESP_RETURN_ON_FALSE(property, NULL, TAG, "Failed to allocate memory for property");

    property->has_default_value = true;
    property->default_value.bool_value = default_value;

    return property;
}

esp_mcp_property_t *esp_mcp_property_create_with_int(const char *name, int default_value)
{
    ESP_RETURN_ON_FALSE(name, NULL, TAG, "Invalid name");

    esp_mcp_property_t *property = esp_mcp_property_create(name, ESP_MCP_PROPERTY_TYPE_INTEGER);
    ESP_RETURN_ON_FALSE(property, NULL, TAG, "Failed to allocate memory for property");

    property->has_default_value = true;
    property->default_value.int_value = default_value;

    return property;
}

esp_mcp_property_t *esp_mcp_property_create_with_float(const char *name, float default_value)
{
    ESP_RETURN_ON_FALSE(name, NULL, TAG, "Invalid name");

    esp_mcp_property_t *property = esp_mcp_property_create(name, ESP_MCP_PROPERTY_TYPE_FLOAT);
    ESP_RETURN_ON_FALSE(property, NULL, TAG, "Failed to allocate memory for property");

    property->has_default_value = true;
    property->default_value.float_value = default_value;

    return property;
}

esp_mcp_property_t *esp_mcp_property_create_with_array(const char *name, const char *default_value)
{
    ESP_RETURN_ON_FALSE(name, NULL, TAG, "Invalid name");
    ESP_RETURN_ON_FALSE(default_value, NULL, TAG, "Invalid default value");

    cJSON *json_arr = cJSON_Parse(default_value);
    ESP_RETURN_ON_FALSE(json_arr, NULL, TAG, "Invalid JSON format");

    if (!cJSON_IsArray(json_arr)) {
        ESP_LOGE(TAG, "Default value is not a JSON array");
        cJSON_Delete(json_arr);
        return NULL;
    }
    cJSON_Delete(json_arr);

    esp_mcp_property_t *property = esp_mcp_property_create(name, ESP_MCP_PROPERTY_TYPE_ARRAY);
    ESP_RETURN_ON_FALSE(property, NULL, TAG, "Failed to allocate memory for property");

    property->has_default_value = true;
    property->default_value.string_value = strdup(default_value);
    if (!property->default_value.string_value) {
        ESP_LOGE(TAG, "property_create_with_default_array: Failed to allocate memory for property default value");
        esp_mcp_property_destroy(property);
        return NULL;
    }

    return property;
}

esp_mcp_property_t *esp_mcp_property_create_with_object(const char *name, const char *default_value)
{
    ESP_RETURN_ON_FALSE(name, NULL, TAG, "Invalid name");
    ESP_RETURN_ON_FALSE(default_value, NULL, TAG, "Invalid default value");

    cJSON *json_obj = cJSON_Parse(default_value);
    ESP_RETURN_ON_FALSE(json_obj, NULL, TAG, "Invalid JSON format");

    if (!cJSON_IsObject(json_obj)) {
        ESP_LOGE(TAG, "Default value is not a JSON object");
        cJSON_Delete(json_obj);
        return NULL;
    }
    cJSON_Delete(json_obj);

    esp_mcp_property_t *property = esp_mcp_property_create(name, ESP_MCP_PROPERTY_TYPE_OBJECT);
    ESP_RETURN_ON_FALSE(property, NULL, TAG, "Failed to allocate memory for property");

    property->has_default_value = true;
    property->default_value.string_value = strdup(default_value);
    if (!property->default_value.string_value) {
        ESP_LOGE(TAG, "property_create_with_default_object: Failed to allocate memory for property default value");
        esp_mcp_property_destroy(property);
        return NULL;
    }

    return property;
}

esp_mcp_property_t *esp_mcp_property_create_with_string(const char *name, const char *default_value)
{
    ESP_RETURN_ON_FALSE(name, NULL, TAG, "Invalid name");
    ESP_RETURN_ON_FALSE(default_value, NULL, TAG, "Invalid default value");

    esp_mcp_property_t *property = esp_mcp_property_create(name, ESP_MCP_PROPERTY_TYPE_STRING);
    ESP_RETURN_ON_FALSE(property, NULL, TAG, "Failed to allocate memory for property");

    property->has_default_value = true;
    property->default_value.string_value = strdup(default_value);
    if (!property->default_value.string_value) {
        ESP_LOGE(TAG, "property_create_with_default_string: Failed to allocate memory for property default value");
        esp_mcp_property_destroy(property);
        return NULL;
    }

    return property;
}

esp_mcp_property_t *esp_mcp_property_create_with_range(const char *name, int min_value, int max_value)
{
    ESP_RETURN_ON_FALSE(name, NULL, TAG, "Invalid name");

    esp_mcp_property_t *property = esp_mcp_property_create(name, ESP_MCP_PROPERTY_TYPE_INTEGER);
    ESP_RETURN_ON_FALSE(property, NULL, TAG, "Failed to allocate memory for property");

    property->has_range = true;
    property->min_value = min_value;
    property->max_value = max_value;

    return property;
}

esp_mcp_property_t *esp_mcp_property_create_with_int_and_range(const char *name, int default_value, int min_value, int max_value)
{
    ESP_RETURN_ON_FALSE(name, NULL, TAG, "Invalid name");

    esp_mcp_property_t *property = esp_mcp_property_create_with_int(name, default_value);
    ESP_RETURN_ON_FALSE(property, NULL, TAG, "Failed to allocate memory for property");

    property->has_range = true;
    property->min_value = min_value;
    property->max_value = max_value;

    return property;
}

esp_err_t esp_mcp_property_destroy(esp_mcp_property_t *property)
{
    ESP_RETURN_ON_FALSE(property, ESP_ERR_INVALID_ARG, TAG, "Invalid property");

    free(property->name);

    if (property->has_default_value) {
        switch (property->type) {
        case ESP_MCP_PROPERTY_TYPE_STRING:
        case ESP_MCP_PROPERTY_TYPE_ARRAY:
        case ESP_MCP_PROPERTY_TYPE_OBJECT:
            if (property->default_value.string_value) {
                free(property->default_value.string_value);
            }
            break;
        default:
            break;
        }
    }

    free(property);

    return ESP_OK;
}

char *esp_mcp_property_to_json(const esp_mcp_property_t *property)
{
    ESP_RETURN_ON_FALSE(property, NULL, TAG, "Invalid property");

    cJSON *json = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(json, NULL, TAG, "Failed to allocate memory for json");

    switch (property->type) {
    case ESP_MCP_PROPERTY_TYPE_BOOLEAN:
        cJSON_AddStringToObject(json, "type", "boolean");
        if (property->has_default_value) {
            cJSON_AddBoolToObject(json, "default", property->default_value.bool_value);
        }
        break;

    case ESP_MCP_PROPERTY_TYPE_INTEGER:
        cJSON_AddStringToObject(json, "type", "integer");
        if (property->has_default_value) {
            cJSON_AddNumberToObject(json, "default", property->default_value.int_value);
        }

        if (property->has_range) {
            cJSON_AddNumberToObject(json, "minimum", property->min_value);
            cJSON_AddNumberToObject(json, "maximum", property->max_value);
        }
        break;

    case ESP_MCP_PROPERTY_TYPE_FLOAT:
        cJSON_AddStringToObject(json, "type", "number");
        if (property->has_default_value) {
            cJSON_AddNumberToObject(json, "default", property->default_value.float_value);
        }
        break;

    case ESP_MCP_PROPERTY_TYPE_STRING:
        cJSON_AddStringToObject(json, "type", "string");
        if (property->has_default_value) {
            cJSON_AddStringToObject(json, "default", property->default_value.string_value);
        }
        break;

    case ESP_MCP_PROPERTY_TYPE_ARRAY:
        cJSON_AddStringToObject(json, "type", "array");
        if (property->has_default_value) {
            cJSON *default_obj = cJSON_Parse(property->default_value.string_value);
            if (default_obj) {
                cJSON_AddItemToObject(json, "default", default_obj);
            } else {
                cJSON_AddStringToObject(json, "default", property->default_value.string_value);
            }
        }
        break;

    case ESP_MCP_PROPERTY_TYPE_OBJECT:
        cJSON_AddStringToObject(json, "type", "object");
        if (property->has_default_value) {
            cJSON *default_obj = cJSON_Parse(property->default_value.string_value);
            if (default_obj) {
                cJSON_AddItemToObject(json, "default", default_obj);
            } else {
                cJSON_AddStringToObject(json, "default", property->default_value.string_value);
            }
        }
        break;
    default:
        ESP_LOGE(TAG, "property_to_json: Unknown property type");
        break;
    }

    char *result = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    ESP_RETURN_ON_FALSE(result, NULL, TAG, "Failed to print property to json");
    return result;
}

esp_mcp_property_list_t *esp_mcp_property_list_create(void)
{
    esp_mcp_property_list_t *list = (esp_mcp_property_list_t *)calloc(1, sizeof(esp_mcp_property_list_t));
    ESP_RETURN_ON_FALSE(list, NULL, TAG, "Failed to allocate memory for list");

    SLIST_INIT(&list->properties);

    list->mutex = xSemaphoreCreateMutex();
    if (!list->mutex) {
        ESP_LOGE(TAG, "Failed to create mutex for property list");
        free(list);
        return NULL;
    }

    return list;
}

esp_err_t esp_mcp_property_list_add_property(esp_mcp_property_list_t *list, esp_mcp_property_t *property)
{
    ESP_RETURN_ON_FALSE(list, ESP_ERR_INVALID_ARG, TAG, "Invalid list");
    ESP_RETURN_ON_FALSE(property, ESP_ERR_INVALID_ARG, TAG, "Invalid property");

    if (xSemaphoreTake(list->mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take property list mutex");
        return ESP_ERR_INVALID_STATE;
    }

    esp_mcp_property_item_t *info = (esp_mcp_property_item_t*)calloc(1, sizeof(esp_mcp_property_item_t));
    if (!info) {
        xSemaphoreGive(list->mutex);
        ESP_LOGE(TAG, "Failed to allocate memory for info");
        return ESP_ERR_NO_MEM;
    }

    info->property = property;
    SLIST_INSERT_HEAD(&list->properties, info, next);

    xSemaphoreGive(list->mutex);

    return ESP_OK;
}

esp_err_t esp_mcp_property_list_remove_property(esp_mcp_property_list_t *list, esp_mcp_property_t *property)
{
    ESP_RETURN_ON_FALSE(list, ESP_ERR_INVALID_ARG, TAG, "Invalid list");
    ESP_RETURN_ON_FALSE(property, ESP_ERR_INVALID_ARG, TAG, "Invalid property");

    if (xSemaphoreTake(list->mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take property list mutex");
        return ESP_ERR_INVALID_STATE;
    }

    esp_mcp_property_item_t *info = NULL;
    esp_mcp_property_item_t *info_next = NULL;
    SLIST_FOREACH_SAFE(info, &list->properties, next, info_next) {
        if (info->property != property) {
            continue;
        }
        SLIST_REMOVE(&list->properties, info, esp_mcp_property_item_s, next);
        esp_mcp_property_destroy(info->property);
        free(info);
        break;
    }
    xSemaphoreGive(list->mutex);

    return ESP_OK;
}

static esp_mcp_property_t *esp_mcp_property_list_get_property(const esp_mcp_property_list_t *list, const char *name)
{
    ESP_RETURN_ON_FALSE(list, NULL, TAG, "Invalid list");
    ESP_RETURN_ON_FALSE(name, NULL, TAG, "Invalid name");

    if (xSemaphoreTake(list->mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take property list mutex");
        return NULL;
    }

    esp_mcp_property_t *result = NULL;
    esp_mcp_property_item_t *info = NULL;
    SLIST_FOREACH(info, &list->properties, next) {
        if (!strcmp(info->property->name, name)) {
            result = info->property;
            break;
        }
    }

    xSemaphoreGive(list->mutex);
    return result;
}

bool esp_mcp_property_list_get_property_bool(const esp_mcp_property_list_t *list, const char *name)
{
    ESP_RETURN_ON_FALSE(list, false, TAG, "Invalid list");
    ESP_RETURN_ON_FALSE(name, false, TAG, "Invalid name");

    esp_mcp_property_t *property = esp_mcp_property_list_get_property(list, name);
    ESP_RETURN_ON_FALSE(property, false, TAG, "Failed to get property");
    if (property->type != ESP_MCP_PROPERTY_TYPE_BOOLEAN) {
        ESP_LOGE(TAG, "Property is not a boolean type");
        return false;
    }

    return property->default_value.bool_value;
}

int esp_mcp_property_list_get_property_int(const esp_mcp_property_list_t *list, const char *name)
{
    ESP_RETURN_ON_FALSE(list, 0, TAG, "Invalid list");
    ESP_RETURN_ON_FALSE(name, 0, TAG, "Invalid name");

    esp_mcp_property_t *property = esp_mcp_property_list_get_property(list, name);
    ESP_RETURN_ON_FALSE(property, 0, TAG, "Failed to get property");
    if (property->type != ESP_MCP_PROPERTY_TYPE_INTEGER) {
        ESP_LOGE(TAG, "Property is not an integer type");
        return 0;
    }

    return property->default_value.int_value;
}

float esp_mcp_property_list_get_property_float(const esp_mcp_property_list_t *list, const char *name)
{
    ESP_RETURN_ON_FALSE(list, 0.0f, TAG, "Invalid list");
    ESP_RETURN_ON_FALSE(name, 0.0f, TAG, "Invalid name");

    esp_mcp_property_t *property = esp_mcp_property_list_get_property(list, name);
    ESP_RETURN_ON_FALSE(property, 0.0f, TAG, "Failed to get property");
    if (property->type != ESP_MCP_PROPERTY_TYPE_FLOAT) {
        ESP_LOGE(TAG, "Property is not a float type");
        return 0.0f;
    }

    return property->default_value.float_value;
}

const char *esp_mcp_property_list_get_property_array(const esp_mcp_property_list_t *list, const char *name)
{
    ESP_RETURN_ON_FALSE(list, NULL, TAG, "Invalid list");
    ESP_RETURN_ON_FALSE(name, NULL, TAG, "Invalid name");

    esp_mcp_property_t *property = esp_mcp_property_list_get_property(list, name);
    ESP_RETURN_ON_FALSE(property, NULL, TAG, "Failed to get property");
    ESP_RETURN_ON_FALSE(property->type == ESP_MCP_PROPERTY_TYPE_ARRAY, NULL, TAG, "Property is not an array type");

    char *default_value = property->default_value.string_value;
    cJSON *json_arr = cJSON_Parse(default_value);
    ESP_RETURN_ON_FALSE(json_arr, NULL, TAG, "Invalid JSON format");

    if (!cJSON_IsArray(json_arr)) {
        ESP_LOGE(TAG, "Property value is not a JSON array");
        cJSON_Delete(json_arr);
        return NULL;
    }
    cJSON_Delete(json_arr);

    return default_value;
}

const char *esp_mcp_property_list_get_property_object(const esp_mcp_property_list_t *list, const char *name)
{
    ESP_RETURN_ON_FALSE(list, NULL, TAG, "Invalid list");
    ESP_RETURN_ON_FALSE(name, NULL, TAG, "Invalid name");

    esp_mcp_property_t *property = esp_mcp_property_list_get_property(list, name);
    ESP_RETURN_ON_FALSE(property, NULL, TAG, "Failed to get property");
    ESP_RETURN_ON_FALSE(property->type == ESP_MCP_PROPERTY_TYPE_OBJECT, NULL, TAG, "Property is not an object type");

    char *default_value = property->default_value.string_value;
    cJSON *json_obj = cJSON_Parse(default_value);
    ESP_RETURN_ON_FALSE(json_obj, NULL, TAG, "Invalid JSON format");

    if (!cJSON_IsObject(json_obj)) {
        ESP_LOGE(TAG, "Property value is not a JSON object");
        cJSON_Delete(json_obj);
        return NULL;
    }
    cJSON_Delete(json_obj);

    return default_value;
}

const char *esp_mcp_property_list_get_property_string(const esp_mcp_property_list_t *list, const char *name)
{
    ESP_RETURN_ON_FALSE(list, NULL, TAG, "Invalid list");
    ESP_RETURN_ON_FALSE(name, NULL, TAG, "Invalid name");

    esp_mcp_property_t *property = esp_mcp_property_list_get_property(list, name);
    ESP_RETURN_ON_FALSE(property, NULL, TAG, "Failed to get property");

    return property->default_value.string_value;
}

char *esp_mcp_property_list_to_json(const esp_mcp_property_list_t *list)
{
    ESP_RETURN_ON_FALSE(list, NULL, TAG, "Invalid list");

    if (xSemaphoreTake(list->mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take property list mutex");
        return NULL;
    }

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        xSemaphoreGive(list->mutex);
        ESP_LOGE(TAG, "Failed to allocate memory for json");
        return NULL;
    }

    esp_mcp_property_item_t *info = NULL;
    SLIST_FOREACH(info, &list->properties, next) {
        char *prop_json = esp_mcp_property_to_json(info->property);
        if (prop_json) {
            cJSON *prop_obj = cJSON_Parse(prop_json);
            if (prop_obj) {
                cJSON_AddItemToObject(json, info->property->name, prop_obj);
            }
            cJSON_free(prop_json);
        }
    }

    xSemaphoreGive(list->mutex);

    char *result = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    ESP_RETURN_ON_FALSE(result, NULL, TAG, "Failed to print property list to json");
    return result;
}

esp_err_t esp_mcp_property_list_foreach(const esp_mcp_property_list_t *list, esp_mcp_property_foreach_ctx_t *ctx)
{
    ESP_RETURN_ON_FALSE(list, ESP_ERR_INVALID_ARG, TAG, "Invalid list");
    ESP_RETURN_ON_FALSE(ctx, ESP_ERR_INVALID_ARG, TAG, "Invalid context");

    if (xSemaphoreTake(list->mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take property list mutex");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_OK;
    esp_mcp_property_item_t *info = NULL;
    SLIST_FOREACH(info, &list->properties, next) {
        ret = ctx->callback(info->property, ctx);
        if (ret != ESP_OK) {
            break;
        }
    }

    xSemaphoreGive(list->mutex);
    return ret;
}

esp_err_t esp_mcp_property_list_destroy(esp_mcp_property_list_t *list)
{
    ESP_RETURN_ON_FALSE(list, ESP_ERR_INVALID_ARG, TAG, "Invalid list");

    if (list->mutex) {
        if (xSemaphoreTake(list->mutex, portMAX_DELAY) == pdTRUE) {
            esp_mcp_property_item_t *info = NULL;
            esp_mcp_property_item_t *info_next = NULL;
            SLIST_FOREACH_SAFE(info, &list->properties, next, info_next) {
                SLIST_REMOVE(&list->properties, info, esp_mcp_property_item_s, next);
                esp_mcp_property_destroy(info->property);
                free(info);
            }
            xSemaphoreGive(list->mutex);
            vSemaphoreDelete(list->mutex);
        }
    }

    free(list);

    return ESP_OK;
}
