/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MCP prompt definition object
 */
typedef struct esp_mcp_prompt_s esp_mcp_prompt_t;

/**
 * @brief Prompt render callback
 *
 * The callback should allocate output strings with malloc/calloc/strdup.
 * - out_description can be NULL
 * - out_messages_json must be a JSON array string when present
 *
 * @param[in] arguments_json Prompt arguments as JSON object string (nullable)
 * @param[out] out_description Optional rendered description (allocated by callback)
 * @param[out] out_messages_json Rendered messages JSON array string (allocated by callback)
 * @param[in] user_ctx User context provided at prompt creation
 * @return ESP_OK on success
 */
typedef esp_err_t (*esp_mcp_prompt_render_cb_t)(const char *arguments_json,
                                                char **out_description,
                                                char **out_messages_json,
                                                void *user_ctx);

/**
 * @brief Create an MCP prompt definition
 *
 * @param[in] name Prompt name
 * @param[in] title Optional prompt title (nullable)
 * @param[in] description Optional prompt description (nullable)
 * @param[in] messages_json Static messages JSON array string (nullable)
 * @param[in] render_cb Optional render callback for dynamic prompt output
 * @param[in] user_ctx User context passed to render callback
 * @return Prompt instance on success, NULL on failure
 */
esp_mcp_prompt_t *esp_mcp_prompt_create(const char *name,
                                        const char *title,
                                        const char *description,
                                        const char *messages_json,
                                        esp_mcp_prompt_render_cb_t render_cb,
                                        void *user_ctx);

/**
 * @brief Set optional MCP prompt annotations
 *
 * @param prompt Prompt instance
 * @param audience_json_array JSON array string, e.g. ["assistant","user"]
 * @param priority Priority [0,1], set <0 to keep unset
 * @param last_modified ISO datetime string
 */
esp_err_t esp_mcp_prompt_set_annotations(esp_mcp_prompt_t *prompt,
                                         const char *audience_json_array,
                                         double priority,
                                         const char *last_modified);

/**
 * @brief Set prompt icons
 *
 * @param prompt Prompt instance
 * @param icons_json JSON object string for icons (nullable, set NULL to clear)
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_prompt_set_icons(esp_mcp_prompt_t *prompt, const char *icons_json);

/**
 * @brief Destroy an MCP prompt definition
 *
 * @param[in] prompt Prompt instance
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_prompt_destroy(esp_mcp_prompt_t *prompt);

#ifdef __cplusplus
}
#endif
