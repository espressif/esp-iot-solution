/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_mcp_resource.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Internal resource registry/list container
 */
typedef struct esp_mcp_resource_list_s esp_mcp_resource_list_t;

/**
 * @brief Create resource list
 *
 * @return Resource list handle, or NULL on failure
 */
esp_mcp_resource_list_t *esp_mcp_resource_list_create(void);

/**
 * @brief Destroy resource list and contained resources
 *
 * @param[in] list Resource list handle
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_resource_list_destroy(esp_mcp_resource_list_t *list);

/**
 * @brief Add resource to list
 *
 * @param[in] list Resource list handle
 * @param[in] resource Resource handle
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_resource_list_add(esp_mcp_resource_list_t *list, esp_mcp_resource_t *resource);

/**
 * @brief Remove resource from list
 *
 * @param[in] list Resource list handle
 * @param[in] resource Resource handle
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_resource_list_remove(esp_mcp_resource_list_t *list, esp_mcp_resource_t *resource);

/**
 * @brief Find resource by URI
 *
 * @param[in] list Resource list handle
 * @param[in] uri Resource URI
 * @return Resource handle if found, otherwise NULL
 */
esp_mcp_resource_t *esp_mcp_resource_list_find(const esp_mcp_resource_list_t *list, const char *uri);
esp_err_t esp_mcp_resource_list_find_ex(const esp_mcp_resource_list_t *list, const char *uri, esp_mcp_resource_t **out_resource);

/**
 * @brief Iterate all resources in list
 *
 * @param[in] list Resource list handle
 * @param[in] callback Callback invoked for each resource
 * @param[in] arg User context passed to callback
 * @return ESP_OK on success, or callback error
 */
esp_err_t esp_mcp_resource_list_foreach(const esp_mcp_resource_list_t *list,
                                        esp_err_t (*callback)(esp_mcp_resource_t *resource, void *arg),
                                        void *arg);

/**
 * @brief Convert resource definition to JSON object
 *
 * @param[in] resource Resource handle
 * @return cJSON object on success, NULL on failure
 */
struct cJSON *esp_mcp_resource_to_json(const esp_mcp_resource_t *resource);

/**
 * @brief Get resource URI
 *
 * @param[in] resource Resource handle
 * @return Resource URI string
 */
const char *esp_mcp_resource_get_uri(const esp_mcp_resource_t *resource);

/**
 * @brief Read resource content
 *
 * @param[in] resource Resource handle
 * @param[out] out_mime_type MIME type output
 * @param[out] out_text Text output (nullable)
 * @param[out] out_blob_base64 Base64 blob output (nullable)
 * @return ESP_OK on success
 */
esp_err_t esp_mcp_resource_read(const esp_mcp_resource_t *resource,
                                char **out_mime_type,
                                char **out_text,
                                char **out_blob_base64);

#ifdef __cplusplus
}
#endif
