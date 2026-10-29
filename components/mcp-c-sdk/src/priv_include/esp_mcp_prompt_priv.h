/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_mcp_prompt.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Internal prompt registry/list container
 */
typedef struct esp_mcp_prompt_list_s esp_mcp_prompt_list_t;

/**
 * @brief Create prompt list
 *
 * @return Prompt list handle, or NULL on failure
 */
esp_mcp_prompt_list_t *esp_mcp_prompt_list_create(void);

/**
 * @brief Destroy prompt list and contained prompt items
 *
 * @param[in] list Prompt list handle
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_prompt_list_destroy(esp_mcp_prompt_list_t *list);

/**
 * @brief Add prompt to list
 *
 * @param[in] list Prompt list handle
 * @param[in] prompt Prompt handle
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_prompt_list_add(esp_mcp_prompt_list_t *list, esp_mcp_prompt_t *prompt);

/**
 * @brief Remove prompt from list
 *
 * @param[in] list Prompt list handle
 * @param[in] prompt Prompt handle
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_prompt_list_remove(esp_mcp_prompt_list_t *list, esp_mcp_prompt_t *prompt);

/**
 * @brief Find prompt by name
 *
 * @param[in] list Prompt list handle
 * @param[in] name Prompt name
 * @return Prompt handle if found, otherwise NULL
 */
esp_mcp_prompt_t *esp_mcp_prompt_list_find(const esp_mcp_prompt_list_t *list, const char *name);

/**
 * @brief Iterate all prompts in list
 *
 * @param[in] list Prompt list handle
 * @param[in] callback Callback invoked for each prompt
 * @param[in] arg User context passed to callback
 * @return ESP_OK on success, or callback error
 */
esp_err_t esp_mcp_prompt_list_foreach(const esp_mcp_prompt_list_t *list,
                                      esp_err_t (*callback)(esp_mcp_prompt_t *prompt, void *arg),
                                      void *arg);

/**
 * @brief Convert prompt definition to JSON object
 *
 * @param[in] prompt Prompt handle
 * @return cJSON object on success, NULL on failure
 */
struct cJSON *esp_mcp_prompt_to_json(const esp_mcp_prompt_t *prompt);

/**
 * @brief Resolve prompt messages using static payload or render callback
 *
 * @param[in] prompt Prompt handle
 * @param[in] arguments_json Prompt arguments JSON object string
 * @param[out] out_description Rendered description (nullable)
 * @param[out] out_messages_json Rendered messages JSON array string
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_prompt_get_messages(const esp_mcp_prompt_t *prompt,
                                      const char *arguments_json,
                                      char **out_description,
                                      char **out_messages_json);

#ifdef __cplusplus
}
#endif
