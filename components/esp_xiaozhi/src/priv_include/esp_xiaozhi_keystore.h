/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include <nvs_flash.h>

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  NVS operations callbacks for keystore.
 *         When set via esp_xiaozhi_keystore_register_nvs_ops(), all NVS access goes through these
 *         callbacks (e.g. to run NVS on an SRAM task and avoid PSRAM stack issues). NULL means use
 *         default direct nvs_* calls.
 */
typedef struct esp_xiaozhi_keystore_nvs_ops_s {
    esp_err_t (*nvs_open)(const char *name_space, bool read_write, nvs_handle_t *out_handle);
    void (*nvs_close)(nvs_handle_t handle);
    esp_err_t (*nvs_commit)(nvs_handle_t handle);
    esp_err_t (*nvs_get_str)(nvs_handle_t handle, const char *key, char *out_value, size_t *length);
    esp_err_t (*nvs_set_str)(nvs_handle_t handle, const char *key, const char *value);
    esp_err_t (*nvs_get_i32)(nvs_handle_t handle, const char *key, int32_t *out_value);
    esp_err_t (*nvs_set_i32)(nvs_handle_t handle, const char *key, int32_t value);
    esp_err_t (*nvs_erase_key)(nvs_handle_t handle, const char *key);
    esp_err_t (*nvs_erase_all)(nvs_handle_t handle);
} esp_xiaozhi_keystore_nvs_ops_t;

/**
 * @brief  Register NVS operations (e.g. to delegate to NVS service on SRAM task). Pass NULL to use default direct NVS.
 *
 * @param[in] ops  Pointer to ops struct, or NULL for default
 */
void esp_xiaozhi_keystore_register_nvs_ops(const esp_xiaozhi_keystore_nvs_ops_t *ops);

/**
 * @brief  Keystore handle structure
 */
typedef struct esp_xiaozhi_chat_keystore_s {
    char *name_space;         /*!< NVS namespace name */
    bool read_write;          /*!< Whether the handle is opened for read/write (true) or read-only (false) */
    bool dirty;               /*!< Flag indicating if there are uncommitted changes */
    nvs_handle_t nvs_handle;  /*!< NVS handle for the namespace */
} esp_xiaozhi_chat_keystore_t;

/**
 * @brief  Initialize keystore handle
 *
 * @param[in]  xiaozhi_chat_keystore  Pointer to keystore handle
 * @param[in]  name_space             NVS namespace name
 * @param[in]  read_write             Whether to open for read/write (true) or read-only (false)
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid xiaozhi_chat_keystore or name_space
 *       - Other                 Error from NVS open
 */
esp_err_t esp_xiaozhi_chat_keystore_init(esp_xiaozhi_chat_keystore_t *xiaozhi_chat_keystore, const char *name_space, bool read_write);

/**
 * @brief  Deinitialize keystore handle
 *
 * @param[in] xiaozhi_chat_keystore  Pointer to keystore handle
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid xiaozhi_chat_keystore
 *       - Other                 Error from NVS commit/close
 */
esp_err_t esp_xiaozhi_chat_keystore_deinit(esp_xiaozhi_chat_keystore_t *xiaozhi_chat_keystore);

/**
 * @brief  Get string value from NVS
 *
 * @param[in]  xiaozhi_chat_keystore  Keystore handle
 * @param[in]  key                    Key name
 * @param[in]  default_value         Default value if key not found
 * @param[out] out_value             Buffer to store the value
 * @param[in]  max_len               Maximum buffer length
 *
 * @return
 *       - ESP_OK   On success
 *       - Other    Error from NVS get (e.g. key not found returns success with default_value copied)
 */
esp_err_t esp_xiaozhi_chat_keystore_get_string(esp_xiaozhi_chat_keystore_t *xiaozhi_chat_keystore, const char *key,
                                               const char *default_value, char *out_value, uint16_t max_len);

/**
 * @brief  Set string value in NVS
 *
 * @param[in] xiaozhi_chat_keystore  Keystore handle
 * @param[in] key                    Key name
 * @param[in] value                 Value to set
 *
 * @return
 *       - ESP_OK   On success
 *       - Other    Error from NVS set
 */
esp_err_t esp_xiaozhi_chat_keystore_set_string(esp_xiaozhi_chat_keystore_t *xiaozhi_chat_keystore, const char *key, const char *value);

/**
 * @brief  Get integer value from NVS
 *
 * @param[in]  xiaozhi_chat_keystore  Keystore handle
 * @param[in]  key                    Key name
 * @param[in]  default_value         Default value if key not found
 * @param[out] out_value             Pointer to store the value
 *
 * @return
 *       - ESP_OK   On success
 *       - Other    Error from NVS get
 */
esp_err_t esp_xiaozhi_chat_keystore_get_int(esp_xiaozhi_chat_keystore_t *xiaozhi_chat_keystore, const char *key,
                                            int32_t default_value, int32_t *out_value);

/**
 * @brief  Set integer value in NVS
 *
 * @param[in] xiaozhi_chat_keystore  Keystore handle
 * @param[in] key                   Key name
 * @param[in] value                 Value to set
 *
 * @return
 *       - ESP_OK   On success
 *       - Other    Error from NVS set
 */
esp_err_t esp_xiaozhi_chat_keystore_set_int(esp_xiaozhi_chat_keystore_t *xiaozhi_chat_keystore, const char *key, int32_t value);

/**
 * @brief  Erase a key from NVS
 *
 * @param[in] xiaozhi_chat_keystore  Keystore handle
 * @param[in] key                    Key name to erase
 *
 * @return
 *       - ESP_OK   On success
 *       - Other    Error from NVS erase_key
 */
esp_err_t esp_xiaozhi_chat_keystore_erase_key(esp_xiaozhi_chat_keystore_t *xiaozhi_chat_keystore, const char *key);

/**
 * @brief  Erase all keys in the namespace
 *
 * @param[in] xiaozhi_chat_keystore  Keystore handle
 *
 * @return
 *       - ESP_OK   On success
 *       - Other    Error from NVS erase_all
 */
esp_err_t esp_xiaozhi_chat_keystore_erase_all(esp_xiaozhi_chat_keystore_t *xiaozhi_chat_keystore);

/**
 * @brief  Commit changes to NVS
 *
 * @param[in] xiaozhi_chat_keystore  Keystore handle
 *
 * @return
 *       - ESP_OK   On success
 *       - Other    Error from NVS commit
 */
esp_err_t esp_xiaozhi_chat_keystore_commit(esp_xiaozhi_chat_keystore_t *xiaozhi_chat_keystore);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
