/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <esp_check.h>
#include <esp_log.h>
#include <cJSON.h>
#include <sys/queue.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "esp_mcp_resource.h"
#include "esp_mcp_resource_priv.h"

static const char *TAG = "esp_mcp_resource";

struct esp_mcp_resource_s {
    char *uri;
    char *name;
    char *title;
    char *description;
    char *mime_type;
    char *annotations_audience_json;
    char *annotations_last_modified;
    bool annotations_has_priority;
    double annotations_priority;
    bool has_size;
    int64_t size;
    char *icons_json;
    esp_mcp_resource_read_cb_t read_cb;
    void *user_ctx;
};

typedef struct esp_mcp_resource_item_s {
    esp_mcp_resource_t *resource;
    SLIST_ENTRY(esp_mcp_resource_item_s) next;
} esp_mcp_resource_item_t;

struct esp_mcp_resource_list_s {
    SLIST_HEAD(resource_table_t, esp_mcp_resource_item_s) resources;
    SemaphoreHandle_t mutex;
};

static esp_err_t dup_str(const char *src, char **dst)
{
    if (!src) {
        *dst = NULL;
        return ESP_OK;
    }
    *dst = strdup(src);
    return *dst ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_mcp_resource_t *esp_mcp_resource_create(const char *uri,
                                            const char *name,
                                            const char *title,
                                            const char *description,
                                            const char *mime_type,
                                            esp_mcp_resource_read_cb_t read_cb,
                                            void *user_ctx)
{
    ESP_RETURN_ON_FALSE(uri && name, NULL, TAG, "Invalid args");

    esp_mcp_resource_t *res = calloc(1, sizeof(*res));
    ESP_RETURN_ON_FALSE(res, NULL, TAG, "No mem");

    if (dup_str(uri, &res->uri) != ESP_OK ||
            dup_str(name, &res->name) != ESP_OK ||
            dup_str(title, &res->title) != ESP_OK ||
            dup_str(description, &res->description) != ESP_OK ||
            dup_str(mime_type, &res->mime_type) != ESP_OK) {
        (void)esp_mcp_resource_destroy(res);
        return NULL;
    }

    res->read_cb = read_cb;
    res->user_ctx = user_ctx;
    return res;
}

esp_err_t esp_mcp_resource_destroy(esp_mcp_resource_t *resource)
{
    ESP_RETURN_ON_FALSE(resource, ESP_ERR_INVALID_ARG, TAG, "Invalid resource");
    free(resource->uri);
    free(resource->name);
    free(resource->title);
    free(resource->description);
    free(resource->mime_type);
    free(resource->annotations_audience_json);
    free(resource->annotations_last_modified);
    free(resource->icons_json);
    free(resource);
    return ESP_OK;
}

esp_err_t esp_mcp_resource_set_annotations(esp_mcp_resource_t *resource,
                                           const char *audience_json_array,
                                           double priority,
                                           const char *last_modified)
{
    ESP_RETURN_ON_FALSE(resource, ESP_ERR_INVALID_ARG, TAG, "Invalid resource");

    char *new_audience = NULL;
    char *new_last_modified = NULL;
    if (audience_json_array) {
        new_audience = strdup(audience_json_array);
        ESP_RETURN_ON_FALSE(new_audience, ESP_ERR_NO_MEM, TAG, "No mem");
    }
    if (last_modified) {
        new_last_modified = strdup(last_modified);
        if (!new_last_modified) {
            free(new_audience);
            return ESP_ERR_NO_MEM;
        }
    }

    free(resource->annotations_audience_json);
    free(resource->annotations_last_modified);
    resource->annotations_audience_json = new_audience;
    resource->annotations_last_modified = new_last_modified;
    resource->annotations_has_priority = (priority >= 0.0 && priority <= 1.0);
    resource->annotations_priority = resource->annotations_has_priority ? priority : 0.0;
    return ESP_OK;
}

esp_err_t esp_mcp_resource_set_size(esp_mcp_resource_t *resource, int64_t size)
{
    ESP_RETURN_ON_FALSE(resource, ESP_ERR_INVALID_ARG, TAG, "Invalid resource");
    if (size >= 0) {
        resource->has_size = true;
        resource->size = size;
    } else {
        resource->has_size = false;
        resource->size = 0;
    }
    return ESP_OK;
}

esp_err_t esp_mcp_resource_set_icons(esp_mcp_resource_t *resource, const char *icons_json)
{
    ESP_RETURN_ON_FALSE(resource, ESP_ERR_INVALID_ARG, TAG, "Invalid resource");
    free(resource->icons_json);
    resource->icons_json = NULL;
    if (icons_json) {
        resource->icons_json = strdup(icons_json);
        ESP_RETURN_ON_FALSE(resource->icons_json, ESP_ERR_NO_MEM, TAG, "No mem");
    }
    return ESP_OK;
}

esp_err_t esp_mcp_resource_read(const esp_mcp_resource_t *resource,
                                char **out_mime_type,
                                char **out_text,
                                char **out_blob_base64)
{
    ESP_RETURN_ON_FALSE(resource && out_mime_type && out_text && out_blob_base64, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    *out_mime_type = NULL;
    *out_text = NULL;
    *out_blob_base64 = NULL;

    if (resource->read_cb) {
        esp_err_t ret = resource->read_cb(resource->uri, out_mime_type, out_text, out_blob_base64, resource->user_ctx);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "resource read callback failed: %s", esp_err_to_name(ret));
            free(*out_mime_type);
            free(*out_text);
            free(*out_blob_base64);
            *out_mime_type = NULL;
            *out_text = NULL;
            *out_blob_base64 = NULL;
            return ret;
        }
        if (!*out_mime_type && resource->mime_type) {
            *out_mime_type = strdup(resource->mime_type);
            if (!*out_mime_type) {
                free(*out_text);
                free(*out_blob_base64);
                *out_text = NULL;
                *out_blob_base64 = NULL;
                return ESP_ERR_NO_MEM;
            }
        }
        return ESP_OK;
    }

    if (resource->mime_type) {
        *out_mime_type = strdup(resource->mime_type);
        ESP_RETURN_ON_FALSE(*out_mime_type, ESP_ERR_NO_MEM, TAG, "No mem");
    }
    *out_text = strdup("");
    if (!*out_text) {
        free(*out_mime_type);
        *out_mime_type = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

cJSON *esp_mcp_resource_to_json(const esp_mcp_resource_t *resource)
{
    ESP_RETURN_ON_FALSE(resource, NULL, TAG, "Invalid resource");
    cJSON *obj = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(obj, NULL, TAG, "No mem");

    if (!cJSON_AddStringToObject(obj, "uri", resource->uri) ||
            !cJSON_AddStringToObject(obj, "name", resource->name)) {
        cJSON_Delete(obj);
        return NULL;
    }
    if (resource->title) {
        cJSON_AddStringToObject(obj, "title", resource->title);
    }
    if (resource->description) {
        cJSON_AddStringToObject(obj, "description", resource->description);
    }
    if (resource->mime_type) {
        cJSON_AddStringToObject(obj, "mimeType", resource->mime_type);
    }
    if (resource->annotations_audience_json || resource->annotations_has_priority || resource->annotations_last_modified) {
        cJSON *annotations = cJSON_CreateObject();
        if (annotations) {
            if (resource->annotations_audience_json) {
                cJSON *aud = cJSON_Parse(resource->annotations_audience_json);
                if (aud && cJSON_IsArray(aud)) {
                    cJSON_AddItemToObject(annotations, "audience", aud);
                } else if (aud) {
                    cJSON_Delete(aud);
                }
            }
            if (resource->annotations_has_priority) {
                cJSON_AddNumberToObject(annotations, "priority", resource->annotations_priority);
            }
            if (resource->annotations_last_modified) {
                cJSON_AddStringToObject(annotations, "lastModified", resource->annotations_last_modified);
            }
            cJSON_AddItemToObject(obj, "annotations", annotations);
        }
    }
    if (resource->has_size) {
        cJSON_AddNumberToObject(obj, "size", (double)resource->size);
    }
    if (resource->icons_json) {
        cJSON *icons = cJSON_Parse(resource->icons_json);
        if (icons) {
            cJSON_AddItemToObject(obj, "icons", icons);
        }
    }
    return obj;
}

const char *esp_mcp_resource_get_uri(const esp_mcp_resource_t *resource)
{
    return resource ? resource->uri : NULL;
}

esp_mcp_resource_list_t *esp_mcp_resource_list_create(void)
{
    esp_mcp_resource_list_t *list = calloc(1, sizeof(*list));
    ESP_RETURN_ON_FALSE(list, NULL, TAG, "No mem");
    SLIST_INIT(&list->resources);
    list->mutex = xSemaphoreCreateMutex();
    if (!list->mutex) {
        free(list);
        return NULL;
    }
    return list;
}

esp_err_t esp_mcp_resource_list_add(esp_mcp_resource_list_t *list, esp_mcp_resource_t *resource)
{
    ESP_RETURN_ON_FALSE(list && resource, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    ESP_RETURN_ON_FALSE(xSemaphoreTake(list->mutex, portMAX_DELAY) == pdTRUE, ESP_ERR_INVALID_STATE, TAG, "Mutex take failed");

    esp_mcp_resource_item_t *it = NULL;
    SLIST_FOREACH(it, &list->resources, next) {
        if (it->resource && it->resource->uri && !strcmp(it->resource->uri, resource->uri)) {
            xSemaphoreGive(list->mutex);
            return ESP_ERR_INVALID_STATE;
        }
    }

    esp_mcp_resource_item_t *new_item = calloc(1, sizeof(*new_item));
    if (!new_item) {
        xSemaphoreGive(list->mutex);
        return ESP_ERR_NO_MEM;
    }
    new_item->resource = resource;
    SLIST_INSERT_HEAD(&list->resources, new_item, next);
    xSemaphoreGive(list->mutex);
    return ESP_OK;
}

esp_err_t esp_mcp_resource_list_remove(esp_mcp_resource_list_t *list, esp_mcp_resource_t *resource)
{
    ESP_RETURN_ON_FALSE(list && resource, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    ESP_RETURN_ON_FALSE(xSemaphoreTake(list->mutex, portMAX_DELAY) == pdTRUE, ESP_ERR_INVALID_STATE, TAG, "Mutex take failed");

    esp_err_t ret = ESP_ERR_NOT_FOUND;
    esp_mcp_resource_item_t *it = NULL;
    esp_mcp_resource_item_t *next = NULL;
    SLIST_FOREACH_SAFE(it, &list->resources, next, next) {
        if (it->resource == resource) {
            SLIST_REMOVE(&list->resources, it, esp_mcp_resource_item_s, next);
            free(it);
            ret = ESP_OK;
            break;
        }
    }
    xSemaphoreGive(list->mutex);
    return ret;
}

esp_err_t esp_mcp_resource_list_find_ex(const esp_mcp_resource_list_t *list,
                                        const char *uri,
                                        esp_mcp_resource_t **out_resource)
{
    ESP_RETURN_ON_FALSE(list && uri && out_resource, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    *out_resource = NULL;
    ESP_RETURN_ON_FALSE(xSemaphoreTake(list->mutex, portMAX_DELAY) == pdTRUE, ESP_ERR_INVALID_STATE, TAG, "Mutex take failed");

    esp_mcp_resource_item_t *it = NULL;
    SLIST_FOREACH(it, &list->resources, next) {
        if (it->resource && it->resource->uri && !strcmp(it->resource->uri, uri)) {
            *out_resource = it->resource;
            break;
        }
    }
    xSemaphoreGive(list->mutex);
    return *out_resource ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_mcp_resource_t *esp_mcp_resource_list_find(const esp_mcp_resource_list_t *list, const char *uri)
{
    esp_mcp_resource_t *resource = NULL;
    esp_err_t ret = esp_mcp_resource_list_find_ex(list, uri, &resource);
    if (ret != ESP_OK && ret != ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "resource_list_find failed: %s", esp_err_to_name(ret));
    }
    return resource;
}

esp_err_t esp_mcp_resource_list_foreach(const esp_mcp_resource_list_t *list,
                                        esp_err_t (*callback)(esp_mcp_resource_t *resource, void *arg),
                                        void *arg)
{
    ESP_RETURN_ON_FALSE(list && callback, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    ESP_RETURN_ON_FALSE(xSemaphoreTake(list->mutex, portMAX_DELAY) == pdTRUE, ESP_ERR_INVALID_STATE, TAG, "Mutex take failed");

    esp_err_t ret = ESP_OK;
    esp_mcp_resource_item_t *it = NULL;
    SLIST_FOREACH(it, &list->resources, next) {
        ret = callback(it->resource, arg);
        if (ret != ESP_OK) {
            break;
        }
    }
    xSemaphoreGive(list->mutex);
    return ret;
}

esp_err_t esp_mcp_resource_list_destroy(esp_mcp_resource_list_t *list)
{
    ESP_RETURN_ON_FALSE(list, ESP_ERR_INVALID_ARG, TAG, "Invalid list");
    if (list->mutex && xSemaphoreTake(list->mutex, portMAX_DELAY) == pdTRUE) {
        esp_mcp_resource_item_t *it = NULL;
        esp_mcp_resource_item_t *next = NULL;
        SLIST_FOREACH_SAFE(it, &list->resources, next, next) {
            SLIST_REMOVE(&list->resources, it, esp_mcp_resource_item_s, next);
            if (it->resource) {
                (void)esp_mcp_resource_destroy(it->resource);
            }
            free(it);
        }
        xSemaphoreGive(list->mutex);
        vSemaphoreDelete(list->mutex);
    }
    free(list);
    return ESP_OK;
}
