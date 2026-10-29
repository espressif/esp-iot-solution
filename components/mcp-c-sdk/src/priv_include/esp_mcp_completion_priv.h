/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_mcp_completion.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Execute completion callback and normalize returned result object
 *
 * @param[in] provider Completion provider handle
 * @param[in] ref_type Reference type ("ref/prompt" or "ref/resource")
 * @param[in] name_or_uri Prompt name or resource URI
 * @param[in] argument_name Argument key to complete
 * @param[in] argument_value Partial value to complete
 * @param[in] context_args_json Current arguments JSON object string
 * @param[out] out_result_obj Completion result cJSON object
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_completion_complete(const esp_mcp_completion_provider_t *provider,
                                      const char *ref_type,
                                      const char *name_or_uri,
                                      const char *argument_name,
                                      const char *argument_value,
                                      const char *context_args_json,
                                      struct cJSON **out_result_obj);

#ifdef __cplusplus
}
#endif
