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

#include "esp_mcp_prompt.h"
#include "esp_mcp_prompt_priv.h"

static const char *TAG = "esp_mcp_prompt";

struct esp_mcp_prompt_s {
    char *name;
    char *title;
    char *description;
    char *messages_json;
    char *annotations_audience_json;
    char *annotations_last_modified;
    bool annotations_has_priority;
    double annotations_priority;
    char *icons_json;
    esp_mcp_prompt_render_cb_t render_cb;
    void *user_ctx;
};

typedef struct esp_mcp_prompt_item_s {
    esp_mcp_prompt_t *prompt;
    SLIST_ENTRY(esp_mcp_prompt_item_s) next;
} esp_mcp_prompt_item_t;

struct esp_mcp_prompt_list_s {
    SLIST_HEAD(prompt_table_t, esp_mcp_prompt_item_s) prompts;
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

static bool is_json_array(const char *json_str)
{
    if (!json_str) {
        return false;
    }
    cJSON *arr = cJSON_Parse(json_str);
    if (!arr) {
        return false;
    }
    bool ok = cJSON_IsArray(arr);
    cJSON_Delete(arr);
    return ok;
}

esp_mcp_prompt_t *esp_mcp_prompt_create(const char *name,
                                        const char *title,
                                        const char *description,
                                        const char *messages_json,
                                        esp_mcp_prompt_render_cb_t render_cb,
                                        void *user_ctx)
{
    ESP_RETURN_ON_FALSE(name, NULL, TAG, "Invalid args");
    ESP_RETURN_ON_FALSE(render_cb || messages_json, NULL, TAG, "Either render_cb or messages_json required");
    if (messages_json) {
        ESP_RETURN_ON_FALSE(is_json_array(messages_json), NULL, TAG, "messages_json must be a JSON array");
    }

    esp_mcp_prompt_t *prompt = calloc(1, sizeof(*prompt));
    ESP_RETURN_ON_FALSE(prompt, NULL, TAG, "No mem");

    if (dup_str(name, &prompt->name) != ESP_OK ||
            dup_str(title, &prompt->title) != ESP_OK ||
            dup_str(description, &prompt->description) != ESP_OK ||
            dup_str(messages_json, &prompt->messages_json) != ESP_OK) {
        (void)esp_mcp_prompt_destroy(prompt);
        return NULL;
    }

    prompt->render_cb = render_cb;
    prompt->user_ctx = user_ctx;
    return prompt;
}

esp_err_t esp_mcp_prompt_destroy(esp_mcp_prompt_t *prompt)
{
    ESP_RETURN_ON_FALSE(prompt, ESP_ERR_INVALID_ARG, TAG, "Invalid prompt");
    free(prompt->name);
    free(prompt->title);
    free(prompt->description);
    free(prompt->messages_json);
    free(prompt->annotations_audience_json);
    free(prompt->annotations_last_modified);
    free(prompt->icons_json);
    free(prompt);
    return ESP_OK;
}

esp_err_t esp_mcp_prompt_set_annotations(esp_mcp_prompt_t *prompt,
                                         const char *audience_json_array,
                                         double priority,
                                         const char *last_modified)
{
    ESP_RETURN_ON_FALSE(prompt, ESP_ERR_INVALID_ARG, TAG, "Invalid prompt");

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

    free(prompt->annotations_audience_json);
    free(prompt->annotations_last_modified);
    prompt->annotations_audience_json = new_audience;
    prompt->annotations_last_modified = new_last_modified;
    prompt->annotations_has_priority = (priority >= 0.0 && priority <= 1.0);
    prompt->annotations_priority = prompt->annotations_has_priority ? priority : 0.0;
    return ESP_OK;
}

esp_err_t esp_mcp_prompt_set_icons(esp_mcp_prompt_t *prompt, const char *icons_json)
{
    ESP_RETURN_ON_FALSE(prompt, ESP_ERR_INVALID_ARG, TAG, "Invalid prompt");
    free(prompt->icons_json);
    prompt->icons_json = NULL;
    if (icons_json) {
        prompt->icons_json = strdup(icons_json);
        ESP_RETURN_ON_FALSE(prompt->icons_json, ESP_ERR_NO_MEM, TAG, "No mem");
    }
    return ESP_OK;
}

cJSON *esp_mcp_prompt_to_json(const esp_mcp_prompt_t *prompt)
{
    ESP_RETURN_ON_FALSE(prompt, NULL, TAG, "Invalid prompt");
    cJSON *obj = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(obj, NULL, TAG, "No mem");
    cJSON_AddStringToObject(obj, "name", prompt->name);
    if (prompt->title) {
        cJSON_AddStringToObject(obj, "title", prompt->title);
    }
    if (prompt->description) {
        cJSON_AddStringToObject(obj, "description", prompt->description);
    }
    if (prompt->annotations_audience_json || prompt->annotations_has_priority || prompt->annotations_last_modified) {
        cJSON *annotations = cJSON_CreateObject();
        if (annotations) {
            if (prompt->annotations_audience_json) {
                cJSON *aud = cJSON_Parse(prompt->annotations_audience_json);
                if (aud && cJSON_IsArray(aud)) {
                    cJSON_AddItemToObject(annotations, "audience", aud);
                } else if (aud) {
                    cJSON_Delete(aud);
                }
            }
            if (prompt->annotations_has_priority) {
                cJSON_AddNumberToObject(annotations, "priority", prompt->annotations_priority);
            }
            if (prompt->annotations_last_modified) {
                cJSON_AddStringToObject(annotations, "lastModified", prompt->annotations_last_modified);
            }
            cJSON_AddItemToObject(obj, "annotations", annotations);
        }
    }
    if (prompt->icons_json) {
        cJSON *icons = cJSON_Parse(prompt->icons_json);
        if (icons) {
            cJSON_AddItemToObject(obj, "icons", icons);
        }
    }
    return obj;
}

esp_err_t esp_mcp_prompt_get_messages(const esp_mcp_prompt_t *prompt,
                                      const char *arguments_json,
                                      char **out_description,
                                      char **out_messages_json)
{
    ESP_RETURN_ON_FALSE(prompt && out_description && out_messages_json, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    *out_description = NULL;
    *out_messages_json = NULL;

    esp_err_t err = ESP_OK;
    bool render_cb_succeeded = false;

    do {
        if (prompt->render_cb) {
            err = prompt->render_cb(arguments_json, out_description, out_messages_json, prompt->user_ctx);
            if (err != ESP_OK) {
                break;
            }
            render_cb_succeeded = true;
            if (!*out_messages_json) {
                *out_messages_json = strdup("[]");
                if (!*out_messages_json) {
                    err = ESP_ERR_NO_MEM;
                    break;
                }
            }
            if (!is_json_array(*out_messages_json)) {
                ESP_LOGE(TAG, "Rendered messages must be JSON array");
                err = ESP_ERR_INVALID_RESPONSE;
                break;
            }
            return ESP_OK;
        }

        if (prompt->description) {
            *out_description = strdup(prompt->description);
            if (!*out_description) {
                err = ESP_ERR_NO_MEM;
                break;
            }
        }
        if (prompt->messages_json) {
            *out_messages_json = strdup(prompt->messages_json);
            if (!*out_messages_json) {
                err = ESP_ERR_NO_MEM;
                break;
            }
        } else {
            *out_messages_json = strdup("[]");
            if (!*out_messages_json) {
                err = ESP_ERR_NO_MEM;
                break;
            }
        }
        return ESP_OK;
    } while (0);

    if (*out_description) {
        free(*out_description);
        *out_description = NULL;
    }

    if (*out_messages_json) {
        free(*out_messages_json);
        *out_messages_json = NULL;
    }

    if (err == ESP_ERR_INVALID_RESPONSE) {
        return err;
    }
    if (prompt->render_cb) {
        if (err == ESP_ERR_NO_MEM && render_cb_succeeded) {
            return ESP_ERR_NO_MEM;
        }
        ESP_RETURN_ON_ERROR(err, TAG, "Render prompt failed");
        return err;
    }
    ESP_RETURN_ON_ERROR(err, TAG, "No mem");
    return err;
}

esp_mcp_prompt_list_t *esp_mcp_prompt_list_create(void)
{
    esp_mcp_prompt_list_t *list = calloc(1, sizeof(*list));
    ESP_RETURN_ON_FALSE(list, NULL, TAG, "No mem");
    SLIST_INIT(&list->prompts);
    list->mutex = xSemaphoreCreateMutex();
    if (!list->mutex) {
        free(list);
        return NULL;
    }
    return list;
}

esp_err_t esp_mcp_prompt_list_add(esp_mcp_prompt_list_t *list, esp_mcp_prompt_t *prompt)
{
    ESP_RETURN_ON_FALSE(list && prompt, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    ESP_RETURN_ON_FALSE(xSemaphoreTake(list->mutex, portMAX_DELAY) == pdTRUE, ESP_ERR_INVALID_STATE, TAG, "Mutex take failed");

    esp_mcp_prompt_item_t *it = NULL;
    SLIST_FOREACH(it, &list->prompts, next) {
        if (it->prompt && it->prompt->name && !strcmp(it->prompt->name, prompt->name)) {
            xSemaphoreGive(list->mutex);
            return ESP_ERR_INVALID_STATE;
        }
    }

    esp_mcp_prompt_item_t *new_item = calloc(1, sizeof(*new_item));
    if (!new_item) {
        xSemaphoreGive(list->mutex);
        return ESP_ERR_NO_MEM;
    }
    new_item->prompt = prompt;
    SLIST_INSERT_HEAD(&list->prompts, new_item, next);
    xSemaphoreGive(list->mutex);
    return ESP_OK;
}

esp_err_t esp_mcp_prompt_list_remove(esp_mcp_prompt_list_t *list, esp_mcp_prompt_t *prompt)
{
    ESP_RETURN_ON_FALSE(list && prompt, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    ESP_RETURN_ON_FALSE(xSemaphoreTake(list->mutex, portMAX_DELAY) == pdTRUE, ESP_ERR_INVALID_STATE, TAG, "Mutex take failed");

    esp_err_t ret = ESP_ERR_NOT_FOUND;
    esp_mcp_prompt_item_t *it = NULL;
    esp_mcp_prompt_item_t *next = NULL;
    SLIST_FOREACH_SAFE(it, &list->prompts, next, next) {
        if (it->prompt == prompt) {
            SLIST_REMOVE(&list->prompts, it, esp_mcp_prompt_item_s, next);
            free(it);
            ret = ESP_OK;
            break;
        }
    }
    xSemaphoreGive(list->mutex);
    return ret;
}

esp_mcp_prompt_t *esp_mcp_prompt_list_find(const esp_mcp_prompt_list_t *list, const char *name)
{
    ESP_RETURN_ON_FALSE(list && name, NULL, TAG, "Invalid args");
    ESP_RETURN_ON_FALSE(xSemaphoreTake(list->mutex, portMAX_DELAY) == pdTRUE, NULL, TAG, "Mutex take failed");

    esp_mcp_prompt_t *found = NULL;
    esp_mcp_prompt_item_t *it = NULL;
    SLIST_FOREACH(it, &list->prompts, next) {
        if (it->prompt && it->prompt->name && !strcmp(it->prompt->name, name)) {
            found = it->prompt;
            break;
        }
    }
    xSemaphoreGive(list->mutex);
    return found;
}

esp_err_t esp_mcp_prompt_list_foreach(const esp_mcp_prompt_list_t *list,
                                      esp_err_t (*callback)(esp_mcp_prompt_t *prompt, void *arg),
                                      void *arg)
{
    ESP_RETURN_ON_FALSE(list && callback, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    ESP_RETURN_ON_FALSE(xSemaphoreTake(list->mutex, portMAX_DELAY) == pdTRUE, ESP_ERR_INVALID_STATE, TAG, "Mutex take failed");

    esp_err_t ret = ESP_OK;
    esp_mcp_prompt_item_t *it = NULL;
    SLIST_FOREACH(it, &list->prompts, next) {
        ret = callback(it->prompt, arg);
        if (ret != ESP_OK) {
            break;
        }
    }
    xSemaphoreGive(list->mutex);
    return ret;
}

esp_err_t esp_mcp_prompt_list_destroy(esp_mcp_prompt_list_t *list)
{
    ESP_RETURN_ON_FALSE(list, ESP_ERR_INVALID_ARG, TAG, "Invalid list");
    if (list->mutex && xSemaphoreTake(list->mutex, portMAX_DELAY) == pdTRUE) {
        esp_mcp_prompt_item_t *it = NULL;
        esp_mcp_prompt_item_t *next = NULL;
        SLIST_FOREACH_SAFE(it, &list->prompts, next, next) {
            SLIST_REMOVE(&list->prompts, it, esp_mcp_prompt_item_s, next);
            if (it->prompt) {
                (void)esp_mcp_prompt_destroy(it->prompt);
            }
            free(it);
        }
        xSemaphoreGive(list->mutex);
        vSemaphoreDelete(list->mutex);
    }
    free(list);
    return ESP_OK;
}
