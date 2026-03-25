/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <esp_log.h>
#include <esp_check.h>
#include <string.h>
#include <stdlib.h>

#include "esp_xiaozhi_keystore.h"

static const char *TAG = "ESP_XIAOZHI_KEYSTORE";

static esp_err_t default_nvs_open(const char *name_space, bool read_write, nvs_handle_t *out_handle)
{
    return nvs_open(name_space, read_write ? NVS_READWRITE : NVS_READONLY, out_handle);
}

static void default_nvs_close(nvs_handle_t handle)
{
    nvs_close(handle);
}

static esp_err_t default_nvs_commit(nvs_handle_t handle)
{
    return nvs_commit(handle);
}

static esp_err_t default_nvs_get_str(nvs_handle_t handle, const char *key, char *out_value, size_t *length)
{
    return nvs_get_str(handle, key, out_value, length);
}

static esp_err_t default_nvs_set_str(nvs_handle_t handle, const char *key, const char *value)
{
    return nvs_set_str(handle, key, value);
}

static esp_err_t default_nvs_get_i32(nvs_handle_t handle, const char *key, int32_t *out_value)
{
    return nvs_get_i32(handle, key, out_value);
}

static esp_err_t default_nvs_set_i32(nvs_handle_t handle, const char *key, int32_t value)
{
    return nvs_set_i32(handle, key, value);
}

static esp_err_t default_nvs_erase_key(nvs_handle_t handle, const char *key)
{
    return nvs_erase_key(handle, key);
}

static esp_err_t default_nvs_erase_all(nvs_handle_t handle)
{
    return nvs_erase_all(handle);
}

static const esp_xiaozhi_keystore_nvs_ops_t s_default_nvs_ops = {
    .nvs_open      = default_nvs_open,
    .nvs_close     = default_nvs_close,
    .nvs_commit    = default_nvs_commit,
    .nvs_get_str   = default_nvs_get_str,
    .nvs_set_str   = default_nvs_set_str,
    .nvs_get_i32   = default_nvs_get_i32,
    .nvs_set_i32   = default_nvs_set_i32,
    .nvs_erase_key = default_nvs_erase_key,
    .nvs_erase_all = default_nvs_erase_all,
};

static const esp_xiaozhi_keystore_nvs_ops_t *s_nvs_ops = NULL;

static const esp_xiaozhi_keystore_nvs_ops_t *get_nvs_ops(void)
{
    return (s_nvs_ops != NULL) ? s_nvs_ops : &s_default_nvs_ops;
}

static bool keystore_is_initialized(const esp_xiaozhi_chat_keystore_t *xiaozhi_chat_keystore)
{
    return xiaozhi_chat_keystore != NULL && xiaozhi_chat_keystore->nvs_handle != 0;
}

static esp_err_t validate_nvs_ops(const esp_xiaozhi_keystore_nvs_ops_t *ops)
{
    ESP_RETURN_ON_FALSE(ops != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid NVS ops");
    ESP_RETURN_ON_FALSE(ops->nvs_open != NULL, ESP_ERR_INVALID_ARG, TAG, "Missing nvs_open");
    ESP_RETURN_ON_FALSE(ops->nvs_close != NULL, ESP_ERR_INVALID_ARG, TAG, "Missing nvs_close");
    ESP_RETURN_ON_FALSE(ops->nvs_commit != NULL, ESP_ERR_INVALID_ARG, TAG, "Missing nvs_commit");
    ESP_RETURN_ON_FALSE(ops->nvs_get_str != NULL, ESP_ERR_INVALID_ARG, TAG, "Missing nvs_get_str");
    ESP_RETURN_ON_FALSE(ops->nvs_set_str != NULL, ESP_ERR_INVALID_ARG, TAG, "Missing nvs_set_str");
    ESP_RETURN_ON_FALSE(ops->nvs_get_i32 != NULL, ESP_ERR_INVALID_ARG, TAG, "Missing nvs_get_i32");
    ESP_RETURN_ON_FALSE(ops->nvs_set_i32 != NULL, ESP_ERR_INVALID_ARG, TAG, "Missing nvs_set_i32");
    ESP_RETURN_ON_FALSE(ops->nvs_erase_key != NULL, ESP_ERR_INVALID_ARG, TAG, "Missing nvs_erase_key");
    ESP_RETURN_ON_FALSE(ops->nvs_erase_all != NULL, ESP_ERR_INVALID_ARG, TAG, "Missing nvs_erase_all");
    return ESP_OK;
}

void esp_xiaozhi_keystore_register_nvs_ops(const esp_xiaozhi_keystore_nvs_ops_t *ops)
{
    s_nvs_ops = ops;
}

esp_err_t esp_xiaozhi_chat_keystore_init(esp_xiaozhi_chat_keystore_t *xiaozhi_chat_keystore, const char *name_space, bool read_write)
{
    ESP_RETURN_ON_FALSE(xiaozhi_chat_keystore, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(name_space, ESP_ERR_INVALID_ARG, TAG, "Invalid name space");

    // Initialize function pointers
    const esp_xiaozhi_keystore_nvs_ops_t *ops = get_nvs_ops();
    ESP_RETURN_ON_ERROR(validate_nvs_ops(ops), TAG, "Invalid NVS ops");

    // Initialize handle
    memset(xiaozhi_chat_keystore, 0, sizeof(esp_xiaozhi_chat_keystore_t));

    // Copy namespace name
    size_t ns_len = strlen(name_space) + 1;
    xiaozhi_chat_keystore->name_space = (char *)calloc(1, ns_len);
    ESP_RETURN_ON_FALSE(xiaozhi_chat_keystore->name_space != NULL, ESP_ERR_NO_MEM, TAG,
                        "Failed to allocate memory for name space");
    strncpy(xiaozhi_chat_keystore->name_space, name_space, ns_len - 1);
    xiaozhi_chat_keystore->name_space[ns_len - 1] = '\0';

    xiaozhi_chat_keystore->read_write = read_write;
    xiaozhi_chat_keystore->dirty = false;

    esp_err_t ret = ops->nvs_open(name_space, read_write, &xiaozhi_chat_keystore->nvs_handle);
    if (ret) {
        esp_xiaozhi_chat_keystore_deinit(xiaozhi_chat_keystore);
        ESP_RETURN_ON_ERROR(ret, TAG, "Failed to open NVS handle: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t esp_xiaozhi_chat_keystore_deinit(esp_xiaozhi_chat_keystore_t *xiaozhi_chat_keystore)
{
    ESP_RETURN_ON_FALSE(xiaozhi_chat_keystore, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_err_t ret = ESP_OK;
    if (xiaozhi_chat_keystore->nvs_handle != 0) {
        if (xiaozhi_chat_keystore->read_write && xiaozhi_chat_keystore->dirty) {
            ret = get_nvs_ops()->nvs_commit(xiaozhi_chat_keystore->nvs_handle);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to commit NVS changes: %s", esp_err_to_name(ret));
            }
        }
        get_nvs_ops()->nvs_close(xiaozhi_chat_keystore->nvs_handle);
        xiaozhi_chat_keystore->nvs_handle = 0;
    }

    if (xiaozhi_chat_keystore->name_space != NULL) {
        free(xiaozhi_chat_keystore->name_space);
        xiaozhi_chat_keystore->name_space = NULL;
    }

    xiaozhi_chat_keystore->read_write = false;
    xiaozhi_chat_keystore->dirty = false;

    return ret;
}

esp_err_t esp_xiaozhi_chat_keystore_get_string(esp_xiaozhi_chat_keystore_t *xiaozhi_chat_keystore, const char *key,
                                               const char *default_value, char *out_value, uint16_t max_len)
{
    ESP_RETURN_ON_FALSE(xiaozhi_chat_keystore, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(key, ESP_ERR_INVALID_ARG, TAG, "Invalid key");
    ESP_RETURN_ON_FALSE(out_value, ESP_ERR_INVALID_ARG, TAG, "Invalid out value");
    ESP_RETURN_ON_FALSE(max_len > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid max length");
    ESP_RETURN_ON_FALSE(keystore_is_initialized(xiaozhi_chat_keystore), ESP_ERR_INVALID_STATE, TAG, "Keystore not initialized");

    size_t length = max_len;
    esp_err_t ret = get_nvs_ops()->nvs_get_str(xiaozhi_chat_keystore->nvs_handle, key, out_value, &length);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        if (default_value != NULL) {
            strncpy(out_value, default_value, max_len - 1);
            out_value[max_len - 1] = '\0';
        } else {
            out_value[0] = '\0';
        }
        return ESP_OK;
    }

    return ret;
}

esp_err_t esp_xiaozhi_chat_keystore_set_string(esp_xiaozhi_chat_keystore_t *xiaozhi_chat_keystore, const char *key, const char *value)
{
    ESP_RETURN_ON_FALSE(xiaozhi_chat_keystore, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(key, ESP_ERR_INVALID_ARG, TAG, "Invalid key");
    ESP_RETURN_ON_FALSE(value, ESP_ERR_INVALID_ARG, TAG, "Invalid value");
    ESP_RETURN_ON_FALSE(keystore_is_initialized(xiaozhi_chat_keystore), ESP_ERR_INVALID_STATE, TAG, "Keystore not initialized");
    ESP_RETURN_ON_FALSE(xiaozhi_chat_keystore->read_write, ESP_ERR_NOT_ALLOWED, TAG, "Namespace is not open for writing");

    esp_err_t ret = get_nvs_ops()->nvs_set_str(xiaozhi_chat_keystore->nvs_handle, key, value);
    if (!ret) {
        xiaozhi_chat_keystore->dirty = true;
    }

    return ret;
}

esp_err_t esp_xiaozhi_chat_keystore_get_int(esp_xiaozhi_chat_keystore_t *xiaozhi_chat_keystore, const char *key,
                                            int32_t default_value, int32_t *out_value)
{
    ESP_RETURN_ON_FALSE(xiaozhi_chat_keystore, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(key, ESP_ERR_INVALID_ARG, TAG, "Invalid key");
    ESP_RETURN_ON_FALSE(out_value, ESP_ERR_INVALID_ARG, TAG, "Invalid out value");
    ESP_RETURN_ON_FALSE(keystore_is_initialized(xiaozhi_chat_keystore), ESP_ERR_INVALID_STATE, TAG, "Keystore not initialized");

    esp_err_t ret = get_nvs_ops()->nvs_get_i32(xiaozhi_chat_keystore->nvs_handle, key, out_value);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        *out_value = default_value;
        return ESP_OK;
    }

    return ret;
}

esp_err_t esp_xiaozhi_chat_keystore_set_int(esp_xiaozhi_chat_keystore_t *xiaozhi_chat_keystore, const char *key, int32_t value)
{
    ESP_RETURN_ON_FALSE(xiaozhi_chat_keystore, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(key, ESP_ERR_INVALID_ARG, TAG, "Invalid key");
    ESP_RETURN_ON_FALSE(keystore_is_initialized(xiaozhi_chat_keystore), ESP_ERR_INVALID_STATE, TAG, "Keystore not initialized");
    ESP_RETURN_ON_FALSE(xiaozhi_chat_keystore->read_write, ESP_ERR_NOT_ALLOWED, TAG, "Namespace is not open for writing");

    esp_err_t ret = get_nvs_ops()->nvs_set_i32(xiaozhi_chat_keystore->nvs_handle, key, value);
    if (!ret) {
        xiaozhi_chat_keystore->dirty = true;
    }

    return ret;
}

esp_err_t esp_xiaozhi_chat_keystore_erase_key(esp_xiaozhi_chat_keystore_t *xiaozhi_chat_keystore, const char *key)
{
    ESP_RETURN_ON_FALSE(xiaozhi_chat_keystore, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(key, ESP_ERR_INVALID_ARG, TAG, "Invalid key");
    ESP_RETURN_ON_FALSE(keystore_is_initialized(xiaozhi_chat_keystore), ESP_ERR_INVALID_STATE, TAG, "Keystore not initialized");
    ESP_RETURN_ON_FALSE(xiaozhi_chat_keystore->read_write, ESP_ERR_NOT_ALLOWED, TAG, "Namespace is not open for writing");

    esp_err_t ret = get_nvs_ops()->nvs_erase_key(xiaozhi_chat_keystore->nvs_handle, key);
    if (!ret) {
        xiaozhi_chat_keystore->dirty = true;
    }

    return ret;
}

esp_err_t esp_xiaozhi_chat_keystore_erase_all(esp_xiaozhi_chat_keystore_t *xiaozhi_chat_keystore)
{
    ESP_RETURN_ON_FALSE(xiaozhi_chat_keystore, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(keystore_is_initialized(xiaozhi_chat_keystore), ESP_ERR_INVALID_STATE, TAG, "Keystore not initialized");
    ESP_RETURN_ON_FALSE(xiaozhi_chat_keystore->read_write, ESP_ERR_NOT_ALLOWED, TAG, "Namespace is not open for writing");

    esp_err_t ret = get_nvs_ops()->nvs_erase_all(xiaozhi_chat_keystore->nvs_handle);
    if (!ret) {
        xiaozhi_chat_keystore->dirty = true;
    }

    return ret;
}

esp_err_t esp_xiaozhi_chat_keystore_commit(esp_xiaozhi_chat_keystore_t *xiaozhi_chat_keystore)
{
    ESP_RETURN_ON_FALSE(xiaozhi_chat_keystore, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(keystore_is_initialized(xiaozhi_chat_keystore), ESP_ERR_INVALID_STATE, TAG, "Keystore not initialized");
    ESP_RETURN_ON_FALSE(xiaozhi_chat_keystore->read_write, ESP_ERR_NOT_ALLOWED, TAG, "Namespace is not open for writing");

    esp_err_t ret = get_nvs_ops()->nvs_commit(xiaozhi_chat_keystore->nvs_handle);
    if (!ret) {
        xiaozhi_chat_keystore->dirty = false;
    }

    return ret;
}
