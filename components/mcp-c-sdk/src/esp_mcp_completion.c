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

#include "esp_mcp_completion.h"
#include "esp_mcp_completion_priv.h"

static const char *TAG = "esp_mcp_completion";

struct esp_mcp_completion_provider_s {
    esp_mcp_completion_cb_t cb;
    void *user_ctx;
};

esp_mcp_completion_provider_t *esp_mcp_completion_provider_create(esp_mcp_completion_cb_t cb, void *user_ctx)
{
    ESP_RETURN_ON_FALSE(cb, NULL, TAG, "Invalid callback");
    esp_mcp_completion_provider_t *provider = calloc(1, sizeof(*provider));
    ESP_RETURN_ON_FALSE(provider, NULL, TAG, "No mem");
    provider->cb = cb;
    provider->user_ctx = user_ctx;
    return provider;
}

esp_err_t esp_mcp_completion_provider_destroy(esp_mcp_completion_provider_t *provider)
{
    ESP_RETURN_ON_FALSE(provider, ESP_ERR_INVALID_ARG, TAG, "Invalid provider");
    free(provider);
    return ESP_OK;
}

static cJSON *build_empty_completion_result(void)
{
    cJSON *result = cJSON_CreateObject();
    cJSON *completion = cJSON_CreateObject();
    cJSON *values = cJSON_CreateArray();
    if (!result || !completion || !values) {
        if (result) {
            cJSON_Delete(result);
        }
        if (completion) {
            cJSON_Delete(completion);
        }
        if (values) {
            cJSON_Delete(values);
        }
        return NULL;
    }
    if (!cJSON_AddItemToObject(result, "completion", completion)) {
        cJSON_Delete(completion);
        cJSON_Delete(values);
        cJSON_Delete(result);
        return NULL;
    }
    if (!cJSON_AddItemToObject(completion, "values", values)) {
        cJSON_Delete(values);
        cJSON_Delete(result);
        return NULL;
    }
    if (!cJSON_AddBoolToObject(completion, "hasMore", false)) {
        cJSON_Delete(result);
        return NULL;
    }
    return result;
}

static bool completion_result_well_formed(const cJSON *root)
{
    const cJSON *completion = cJSON_GetObjectItemCaseSensitive((cJSON *)root, "completion");
    if (!completion || !cJSON_IsObject(completion)) {
        return false;
    }
    const cJSON *values = cJSON_GetObjectItemCaseSensitive((cJSON *)completion, "values");
    if (!values || !cJSON_IsArray(values)) {
        return false;
    }
    const cJSON *it = NULL;
    cJSON_ArrayForEach(it, (cJSON *)values) {
        if (!cJSON_IsString(it)) {
            return false;
        }
    }
    const cJSON *has_more = cJSON_GetObjectItemCaseSensitive((cJSON *)completion, "hasMore");
    if (has_more && !cJSON_IsBool(has_more)) {
        return false;
    }
    const cJSON *total = cJSON_GetObjectItemCaseSensitive((cJSON *)completion, "total");
    if (total && !cJSON_IsNumber(total)) {
        return false;
    }
    return true;
}

esp_err_t esp_mcp_completion_complete(const esp_mcp_completion_provider_t *provider,
                                      const char *ref_type,
                                      const char *name_or_uri,
                                      const char *argument_name,
                                      const char *argument_value,
                                      const char *context_args_json,
                                      cJSON **out_result_obj)
{
    ESP_RETURN_ON_FALSE(ref_type && name_or_uri && argument_name && argument_value && out_result_obj, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    *out_result_obj = NULL;

    if (!provider || !provider->cb) {
        *out_result_obj = build_empty_completion_result();
        return *out_result_obj ? ESP_OK : ESP_ERR_NO_MEM;
    }

    cJSON *result = NULL;
    esp_err_t ret = provider->cb(ref_type, name_or_uri, argument_name, argument_value, context_args_json, &result, provider->user_ctx);
    if (ret != ESP_OK) {
        if (result) {
            cJSON_Delete(result);
        }
        ESP_LOGE(TAG, "Completion callback failed");
        return ret;
    }
    if (!result || !cJSON_IsObject(result)) {
        if (result) {
            cJSON_Delete(result);
        }
        ESP_LOGE(TAG, "Completion callback returned invalid result");
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (!completion_result_well_formed(result)) {
        cJSON_Delete(result);
        ESP_LOGE(TAG, "Completion callback result missing completion.values or has wrong types");
        return ESP_ERR_INVALID_RESPONSE;
    }
    *out_result_obj = result;
    return ESP_OK;
}
