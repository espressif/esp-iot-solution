/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <esp_check.h>
#include <esp_log.h>

#include "esp_mcp_data.h"

static const char *TAG = "esp_mcp_data";

esp_mcp_value_t esp_mcp_value_create_bool(bool value)
{
    esp_mcp_value_t mcp_value = {
        .type = ESP_MCP_VALUE_TYPE_BOOLEAN,
        .data.bool_value = value
    };

    return mcp_value;
}

esp_mcp_value_t esp_mcp_value_create_int(int value)
{
    esp_mcp_value_t mcp_value = {
        .type = ESP_MCP_VALUE_TYPE_INTEGER,
        .data.int_value = value
    };

    return mcp_value;
}

esp_mcp_value_t esp_mcp_value_create_float(float value)
{
    esp_mcp_value_t mcp_value = {
        .type = ESP_MCP_VALUE_TYPE_FLOAT,
        .data.float_value = value
    };

    return mcp_value;
}

esp_mcp_value_t esp_mcp_value_create_string(const char *value)
{
    esp_mcp_value_t mcp_value = {
        .type = ESP_MCP_VALUE_TYPE_INVALID,
        .data.string_value = NULL
    };

    if (!value) {
        ESP_LOGE(TAG, "Failed to create string value: input string is NULL");
        return mcp_value;
    }

    mcp_value.data.string_value = strdup(value);
    if (mcp_value.data.string_value) {
        mcp_value.type = ESP_MCP_VALUE_TYPE_STRING;
    }

    return mcp_value;
}

esp_err_t esp_mcp_value_destroy(esp_mcp_value_t *value)
{
    ESP_RETURN_ON_FALSE(value, ESP_ERR_INVALID_ARG, TAG, "Invalid value");

    if (value->type == ESP_MCP_VALUE_TYPE_STRING && value->data.string_value) {
        free(value->data.string_value);
        value->data.string_value = NULL;
    }

    value->type = ESP_MCP_VALUE_TYPE_INVALID;

    return ESP_OK;
}
