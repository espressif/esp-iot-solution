/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MCP property type enumeration
 *
 * Supported data types for MCP properties. Properties support boolean, integer,
 * float, string, JSON array, and JSON object types.
 */
typedef enum esp_mcp_property_type_e {
    ESP_MCP_PROPERTY_TYPE_BOOLEAN,                  /*!< Boolean property type */
    ESP_MCP_PROPERTY_TYPE_INTEGER,                  /*!< Integer property type */
    ESP_MCP_PROPERTY_TYPE_FLOAT,                    /*!< Float property type */
    ESP_MCP_PROPERTY_TYPE_STRING,                   /*!< String property type */
    ESP_MCP_PROPERTY_TYPE_ARRAY,                    /*!< JSON array property type */
    ESP_MCP_PROPERTY_TYPE_OBJECT,                   /*!< JSON object property type */
    ESP_MCP_PROPERTY_TYPE_MAX,                      /*!< Maximum property type (reserved) */
} esp_mcp_property_type_t;

/**
 * @brief MCP property structure
 *
 * Opaque structure representing a single MCP property with its name, type,
 * and optional default value.
 */
typedef struct esp_mcp_property_s esp_mcp_property_t;

/**
 * @brief MCP property list structure
 *
 * Opaque structure representing a collection of MCP properties.
 */
typedef struct esp_mcp_property_list_s esp_mcp_property_list_t;

/**
 * @brief Create a MCP property
 *
 * Creates a new MCP property with the specified name and type.
 *
 * @param[in] name Property name (must not be NULL)
 * @param[in] type Property type
 * @return Pointer to the created property, or NULL on failure
 */
esp_mcp_property_t *esp_mcp_property_create(const char *name, esp_mcp_property_type_t type);

/**
 * @brief Create a MCP property with a default boolean value
 *
 * @param[in] name Property name (must not be NULL)
 * @param[in] default_value Default boolean value
 * @return Pointer to the created property, or NULL on failure
 */
esp_mcp_property_t *esp_mcp_property_create_with_bool(const char *name, bool default_value);

/**
 * @brief Create a MCP property with a default integer value
 *
 * @param[in] name Property name (must not be NULL)
 * @param[in] default_value Default integer value
 * @return Pointer to the created property, or NULL on failure
 */
esp_mcp_property_t *esp_mcp_property_create_with_int(const char *name, int default_value);

/**
 * @brief Create a MCP property with a default float value
 *
 * @param[in] name Property name (must not be NULL)
 * @param[in] default_value Default float value
 * @return Pointer to the created property, or NULL on failure
 */
esp_mcp_property_t *esp_mcp_property_create_with_float(const char *name, float default_value);

/**
 * @brief Create a MCP property with a default string value
 *
 * @param[in] name Property name (must not be NULL)
 * @param[in] default_value Default string value (must not be NULL)
 * @return Pointer to the created property, or NULL on failure
 */
esp_mcp_property_t *esp_mcp_property_create_with_string(const char *name, const char *default_value);

/**
 * @brief Create a MCP property with a default JSON array value
 *
 * @param[in] name Property name (must not be NULL)
 * @param[in] default_value Default JSON array string (must be valid JSON, must not be NULL)
 * @return Pointer to the created property, or NULL on failure
 */
esp_mcp_property_t *esp_mcp_property_create_with_array(const char *name, const char *default_value);

/**
 * @brief Create a MCP property with a default JSON object value
 *
 * @param[in] name Property name (must not be NULL)
 * @param[in] default_value Default JSON object string (must be valid JSON, must not be NULL)
 * @return Pointer to the created property, or NULL on failure
 */
esp_mcp_property_t *esp_mcp_property_create_with_object(const char *name, const char *default_value);

/**
 * @brief Create a MCP property with a range constraint
 *
 * Creates an integer property with minimum and maximum value constraints.
 *
 * @param[in] name Property name (must not be NULL)
 * @param[in] min_value Minimum allowed value
 * @param[in] max_value Maximum allowed value (must be >= min_value)
 * @return Pointer to the created property, or NULL on failure
 */
esp_mcp_property_t *esp_mcp_property_create_with_range(const char *name, int min_value, int max_value);

/**
 * @brief Create a MCP property with a default value and range constraint
 *
 * Creates an integer property with a default value and minimum/maximum constraints.
 *
 * @param[in] name Property name (must not be NULL)
 * @param[in] default_value Default integer value (must be within [min_value, max_value])
 * @param[in] min_value Minimum allowed value
 * @param[in] max_value Maximum allowed value (must be >= min_value)
 * @return Pointer to the created property, or NULL on failure
 */
esp_mcp_property_t *esp_mcp_property_create_with_int_and_range(const char *name, int default_value, int min_value, int max_value);

/**
 * @brief Destroy a MCP property
 *
 * Frees all resources associated with the property.
 *
 * @param[in] property Pointer to the property to destroy (must not be NULL)
 * @return
 *      - ESP_OK: Property destroyed successfully
 *      - ESP_ERR_INVALID_ARG: Invalid parameter
 */
esp_err_t esp_mcp_property_destroy(esp_mcp_property_t *property);

/**
 * @brief Get a boolean property value from a property list
 *
 * @param[in] list Pointer to the property list (must not be NULL)
 * @param[in] name Property name (must not be NULL)
 * @return Boolean value, or false if property not found or type mismatch
 */
bool esp_mcp_property_list_get_property_bool(const esp_mcp_property_list_t *list, const char *name);

/**
 * @brief Get an integer property value from a property list
 *
 * @param[in] list Pointer to the property list (must not be NULL)
 * @param[in] name Property name (must not be NULL)
 * @return Integer value, or 0 if property not found or type mismatch
 */
int esp_mcp_property_list_get_property_int(const esp_mcp_property_list_t *list, const char *name);

/**
 * @brief Get a float property value from a property list
 *
 * @param[in] list Pointer to the property list (must not be NULL)
 * @param[in] name Property name (must not be NULL)
 * @return Float value, or 0.0 if property not found or type mismatch
 */
float esp_mcp_property_list_get_property_float(const esp_mcp_property_list_t *list, const char *name);

/**
 * @brief Get a JSON array property value from a property list
 *
 * @param[in] list Pointer to the property list (must not be NULL)
 * @param[in] name Property name (must not be NULL)
 * @return Pointer to the JSON array string (internal pointer, do not free), or NULL if not found or type mismatch
 */
const char *esp_mcp_property_list_get_property_array(const esp_mcp_property_list_t *list, const char *name);

/**
 * @brief Get a JSON object property value from a property list
 *
 * @param[in] list Pointer to the property list (must not be NULL)
 * @param[in] name Property name (must not be NULL)
 * @return Pointer to the JSON object string (internal pointer, do not free), or NULL if not found or type mismatch
 */
const char *esp_mcp_property_list_get_property_object(const esp_mcp_property_list_t *list, const char *name);

/**
 * @brief Get a string property value from a property list
 *
 * @param[in] list Pointer to the property list (must not be NULL)
 * @param[in] name Property name (must not be NULL)
 * @return Pointer to the string value (internal pointer, do not free), or NULL if not found or type mismatch
 */
const char *esp_mcp_property_list_get_property_string(const esp_mcp_property_list_t *list, const char *name);

#ifdef __cplusplus
}
#endif
