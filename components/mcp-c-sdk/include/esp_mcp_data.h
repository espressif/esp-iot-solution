/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief MCP value type enumeration
 *
 * Supported data types for MCP values. Each type represents a different
 * data representation that can be stored and transmitted.
 */
typedef enum {
    ESP_MCP_VALUE_TYPE_INVALID = -1, /*!< Invalid value (error state) */
    ESP_MCP_VALUE_TYPE_BOOLEAN,  /*!< Boolean value */
    ESP_MCP_VALUE_TYPE_INTEGER,  /*!< Integer value (signed 32-bit) */
    ESP_MCP_VALUE_TYPE_FLOAT,    /*!< Floating-point value (32-bit) */
    ESP_MCP_VALUE_TYPE_STRING    /*!< String value (null-terminated) */
} esp_mcp_value_type_t;

/**
 * @brief MCP value data union
 *
 * Union containing the actual data for MCP values. The active member is determined
 * by the type field in the parent esp_mcp_value_t structure.
 *
 * @note Only access the member corresponding to the value type.
 */
typedef union {
    bool bool_value;            /*!< Boolean data value */
    int int_value;              /*!< Integer data value (signed 32-bit) */
    float float_value;          /*!< Floating-point data value (32-bit) */
    char *string_value;         /*!< String data value (pointer to null-terminated C string) */
} esp_mcp_value_data_t;

/**
 * @brief MCP value structure
 *
 * Complete MCP value with its type and associated data. Provides a unified
 * way to handle different data types in the MCP protocol.
 *
 * @note The type field determines which member of the data union is valid.
 * @note For STRING type, string_value points to dynamically allocated memory
 *       that will be freed by esp_mcp_value_destroy().
 */
typedef struct {
    esp_mcp_value_type_t type;  /*!< Type of the value (boolean, integer, float, or string) */
    esp_mcp_value_data_t data;  /*!< Union containing the actual value data */
} esp_mcp_value_t;

/**
 * @brief Create a MCP value with a boolean value
 *
 * @param[in] value Boolean value to store
 * @return MCP value structure with type ESP_MCP_VALUE_TYPE_BOOLEAN
 */
esp_mcp_value_t esp_mcp_value_create_bool(bool value);

/**
 * @brief Create a MCP value with an integer value
 *
 * @param[in] value Integer value to store (signed 32-bit)
 * @return MCP value structure with type ESP_MCP_VALUE_TYPE_INTEGER
 */
esp_mcp_value_t esp_mcp_value_create_int(int value);

/**
 * @brief Create a MCP value with a float value
 *
 * @param[in] value Floating-point value to store (32-bit)
 * @return MCP value structure with type ESP_MCP_VALUE_TYPE_FLOAT
 */
esp_mcp_value_t esp_mcp_value_create_float(float value);

/**
 * @brief Create a MCP value with a string value
 *
 * Creates a copy of the input string. The string will be automatically
 * freed when esp_mcp_value_destroy() is called.
 *
 * @param[in] value Pointer to a null-terminated C string (must not be NULL)
 * @return MCP value structure:
 *      - type ESP_MCP_VALUE_TYPE_STRING: Success
 *      - type ESP_MCP_VALUE_TYPE_INVALID: value is NULL or memory allocation failed
 *
 * @note The input string is duplicated internally. The caller can safely free
 *       or modify the original string after this call.
 * @note Always check if the returned type is ESP_MCP_VALUE_TYPE_INVALID before using.
 */
esp_mcp_value_t esp_mcp_value_create_string(const char *value);

/**
 * @brief Destroy a MCP value
 *
 * Releases all resources associated with the MCP value, including dynamically
 * allocated string memory for STRING type values.
 *
 * @param[in,out] value Pointer to the MCP value structure to destroy (must not be NULL)
 * @return
 *      - ESP_OK: Value destroyed successfully
 *      - ESP_ERR_INVALID_ARG: Invalid parameter
 *
 * @note After calling this function, the value structure should not be used again.
 */
esp_err_t esp_mcp_value_destroy(esp_mcp_value_t *value);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
