/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MCP resource definition object
 */
typedef struct esp_mcp_resource_s esp_mcp_resource_t;

/**
 * @brief Resource read callback
 *
 * The callback should allocate output strings with malloc/calloc/strdup.
 * The framework will free them with free().
 *
 * @param[in] uri Requested resource URI
 * @param[out] out_mime_type Output MIME type string (allocated by callback)
 * @param[out] out_text Output text payload (allocated by callback, nullable)
 * @param[out] out_blob_base64 Output base64 payload (allocated by callback, nullable)
 * @param[in] user_ctx User context provided at resource creation
 * @return ESP_OK on success
 */
typedef esp_err_t (*esp_mcp_resource_read_cb_t)(const char *uri,
                                                char **out_mime_type,
                                                char **out_text,
                                                char **out_blob_base64,
                                                void *user_ctx);

/**
 * @brief Create an MCP resource definition
 *
 * @param[in] uri Resource URI
 * @param[in] name Resource name
 * @param[in] title Optional resource title (nullable)
 * @param[in] description Optional resource description (nullable)
 * @param[in] mime_type Default MIME type (nullable)
 * @param[in] read_cb Optional callback to read resource content dynamically
 * @param[in] user_ctx User context passed to read callback
 * @return Resource instance on success, NULL on failure
 */
esp_mcp_resource_t *esp_mcp_resource_create(const char *uri,
                                            const char *name,
                                            const char *title,
                                            const char *description,
                                            const char *mime_type,
                                            esp_mcp_resource_read_cb_t read_cb,
                                            void *user_ctx);

/**
 * @brief Set optional MCP resource annotations
 *
 * @param resource Resource instance
 * @param audience_json_array JSON array string, e.g. ["assistant","user"]
 * @param priority Priority [0,1], set <0 to keep unset
 * @param last_modified ISO datetime string
 */
esp_err_t esp_mcp_resource_set_annotations(esp_mcp_resource_t *resource,
                                           const char *audience_json_array,
                                           double priority,
                                           const char *last_modified);

/**
 * @brief Set resource size in bytes
 *
 * @param resource Resource instance
 * @param size Size in bytes (set negative to unset)
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_resource_set_size(esp_mcp_resource_t *resource, int64_t size);

/**
 * @brief Set resource icons
 *
 * @param resource Resource instance
 * @param icons_json JSON object string for icons (nullable, set NULL to clear)
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_resource_set_icons(esp_mcp_resource_t *resource, const char *icons_json);

/**
 * @brief Destroy an MCP resource definition
 *
 * @param[in] resource Resource instance
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_resource_destroy(esp_mcp_resource_t *resource);

#ifdef __cplusplus
}
#endif
