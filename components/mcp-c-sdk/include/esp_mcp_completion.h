/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

typedef struct cJSON cJSON;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Completion provider object
 */
typedef struct esp_mcp_completion_provider_s esp_mcp_completion_provider_t;

/**
 * @brief Completion callback for completion/complete requests
 *
 * @param[in] ref_type Reference type ("ref/prompt" or "ref/resource")
 * @param[in] name_or_uri Prompt name or resource URI
 * @param[in] argument_name Argument name being completed
 * @param[in] argument_value Partial argument value from client
 * @param[in] context_args_json JSON object string containing current arguments context (nullable)
 * @param[out] out_result_obj Output cJSON object with completion result
 * @param[in] user_ctx User context provided when provider was created
 * @return ESP_OK on success
 */
typedef esp_err_t (*esp_mcp_completion_cb_t)(const char *ref_type,
                                             const char *name_or_uri,
                                             const char *argument_name,
                                             const char *argument_value,
                                             const char *context_args_json,
                                             cJSON **out_result_obj,
                                             void *user_ctx);

/**
 * @brief Create completion provider
 *
 * @param[in] cb Completion callback
 * @param[in] user_ctx User context for callback
 * @return Completion provider instance on success, NULL on failure
 */
esp_mcp_completion_provider_t *esp_mcp_completion_provider_create(esp_mcp_completion_cb_t cb, void *user_ctx);

/**
 * @brief Destroy completion provider
 *
 * @param[in] provider Completion provider instance
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_completion_provider_destroy(esp_mcp_completion_provider_t *provider);

#ifdef __cplusplus
}
#endif
