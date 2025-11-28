/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_mcp_property.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Context for property list foreach callback
 *
 */
typedef struct esp_mcp_property_foreach_ctx_s {
    esp_mcp_property_list_t *list;                                        /*!< Pointer to the property list (must not be NULL) */
    void *arg;                                                            /*!< Arguments passed to the callback (must not be NULL) */
    esp_err_t (*callback)(const esp_mcp_property_t *property, void *arg); /*!< Callback function called for each property (must not be NULL) */
    bool has_error;                                                       /*!< Whether an error occurred (must not be NULL) */
} esp_mcp_property_foreach_ctx_t;

/**
 * @brief Destroy an MCP property
 *
 * Frees all resources associated with the property.
 *
 * @note This is an internal API. Properties are automatically destroyed when
 *       the property list is destroyed via esp_mcp_property_list_destroy().
 *
 * @param[in] property Pointer to the property to destroy (must not be NULL)
 * @return
 *      - ESP_OK: Property destroyed successfully
 *      - ESP_ERR_INVALID_ARG: Invalid parameter
 */
esp_err_t esp_mcp_property_destroy(esp_mcp_property_t *property);

/**
 * @brief Convert an MCP property to a JSON string
 *
 * @note This is an internal API used by the MCP server implementation.
 *
 * @param[in] property Pointer to the property (must not be NULL)
 * @return Pointer to the JSON string (caller must free using free()), or NULL on failure
 */
char *esp_mcp_property_to_json(const esp_mcp_property_t *property);

/**
 * @brief Convert a property list to a JSON string
 *
 * @note This is an internal API used by the MCP server implementation.
 *
 * @param[in] list Pointer to the property list (must not be NULL)
 * @return Pointer to the JSON string (caller must free using free()), or NULL on failure
 */
char *esp_mcp_property_list_to_json(const esp_mcp_property_list_t *list);

/**
 * @brief Create a MCP property list
 *
 * Creates a new empty property list.
 *
 * @return Pointer to the created property list, or NULL on failure
 */
esp_mcp_property_list_t *esp_mcp_property_list_create(void);

/**
 * @brief Add a property to a property list
 *
 * Adds a property to the list. The list takes ownership of the property.
 *
 * @param[in] list Pointer to the property list (must not be NULL)
 * @param[in] property Pointer to the property to add (must not be NULL)
 * @return
 *      - ESP_OK: Property added successfully
 *      - ESP_ERR_INVALID_ARG: Invalid parameter
 */
esp_err_t esp_mcp_property_list_add_property(esp_mcp_property_list_t *list, esp_mcp_property_t *property);

/**
 * @brief Remove a property from a property list
 *
 * Removes a property from the list.
 *
 * @param[in] list Pointer to the property list (must not be NULL)
 * @param[in] property Pointer to the property to remove (must not be NULL)
 * @return ESP_OK if successful, otherwise an error code
 */
esp_err_t esp_mcp_property_list_remove_property(esp_mcp_property_list_t *list, esp_mcp_property_t *property);

/**
 * @brief Iterate over all properties in the list
 *
 * Calls the callback function for each property in the list. The iteration
 * is protected by mutex, so the list cannot be modified during iteration.
 *
 * @note This is an internal API used by the MCP server implementation.
 *
 * @param[in] list Pointer to the property list (must not be NULL)
 * @param[in] callback Callback function called for each property (must not be NULL)
 * @param[in] arg User data passed to the callback
 * @return
 *      - ESP_OK: All properties processed successfully
 *      - Other: Error code returned by callback
 */
esp_err_t esp_mcp_property_list_foreach(const esp_mcp_property_list_t *list, esp_mcp_property_foreach_ctx_t *ctx);

/**
 * @brief Destroy a property list
 *
 * Frees all resources, including all properties in the list.
 *
 * @note This is an internal API. Property lists are typically managed by the MCP server
 *       and destroyed automatically when tools are destroyed.
 *
 * @param[in] list Pointer to the property list to destroy (must not be NULL)
 * @return
 *      - ESP_OK: Property list destroyed successfully
 *      - ESP_ERR_INVALID_ARG: Invalid parameter
 */
esp_err_t esp_mcp_property_list_destroy(esp_mcp_property_list_t *list);

#ifdef __cplusplus
}
#endif
