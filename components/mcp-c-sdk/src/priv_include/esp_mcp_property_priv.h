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
esp_err_t esp_mcp_property_destroy(esp_mcp_property_t* property);

/**
 * @brief Convert an MCP property to a JSON string
 *
 * @note This is an internal API used by the MCP server implementation.
 *
 * @param[in] property Pointer to the property (must not be NULL)
 * @return Pointer to the JSON string (caller must free using free()), or NULL on failure
 */
char* esp_mcp_property_to_json(const esp_mcp_property_t* property);

/**
 * @brief Convert a property list to a JSON string
 *
 * @note This is an internal API used by the MCP server implementation.
 *
 * @param[in] list Pointer to the property list (must not be NULL)
 * @return Pointer to the JSON string (caller must free using free()), or NULL on failure
 */
char* esp_mcp_property_list_to_json(const esp_mcp_property_list_t* list);

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
esp_err_t esp_mcp_property_list_destroy(esp_mcp_property_list_t* list);

#ifdef __cplusplus
}
#endif

