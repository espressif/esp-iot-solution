/* SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ESP_TINYUF2_H_
#define _ESP_TINYUF2_H_
#include "esp_partition.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_TINYUF2_OTA_CONFIG()      \
{                                         \
    .subtype = ESP_PARTITION_SUBTYPE_ANY, \
    .label = NULL,                        \
    .if_restart = true                    \
}

#define DEFAULT_TINYUF2_NVS_CONFIG() \
{                                         \
    .part_name = "nvs",                   \
    .namespace_name = "tuf2",             \
}

#define UF2_RESET_REASON_VALUE (CONFIG_UF2_OTA_RESET_REASON_VALUE)

/**
 * @brief user callback called after uf2 update complete
 *
 */
typedef void (*update_complete_cb_t)(void);

/**
 * @brief user callback called after nvs modified
 *
 */
typedef void (*nvs_modified_cb_t)(void);

/**
 * @brief tinyuf2 configurations
 *
 */
typedef struct {
    esp_partition_subtype_t subtype;  /*!< Partition subtype. if ESP_PARTITION_SUBTYPE_ANY will use the next_update_partition by default. */
    const char *label;                /*!< Partition label. Set this value if looking for partition with a specific name. if subtype==ESP_PARTITION_SUBTYPE_ANY, label default to NULL.*/
    bool if_restart;                  /*!< if restart system to new app partition after UF2 flashing done */
    update_complete_cb_t complete_cb; /*!< user callback called after uf2 update complete */
} tinyuf2_ota_config_t;

/**
 * @brief tinyuf2 nvs configurations
 *
 */
typedef struct {
    const char *part_name;            /*!< Partition name. */
    const char *namespace_name;       /*!< Namespace name. */
    nvs_modified_cb_t modified_cb;    /*!< user callback called after uf2 update complete */
} tinyuf2_nvs_config_t;

/**
 * @brief tinyuf2 current state
 */
typedef enum {
    TINYUF2_STATE_NOT_INSTALLED = 0,  /*!< tinyuf2 driver not installed */
    TINYUF2_STATE_INSTALLED,          /*!< tinyuf2 driver installed */
    TINYUF2_STATE_MOUNTED,            /*!< USB mounted */
} tinyuf2_state_t;

/**
 * @brief Flashing app to specified partition through USB UF2 (Virtual USB Disk)，
 * and support operate NVS partition through USB UF2 CONFIG.ini file.
 *
 * @param ota_config tinyuf2 configs described in tinyuf2_ota_config_t
 * @param nvs_config tinyuf2 nvs configs described in tinyuf2_nvs_config_t
 * @return
 *      - ESP_ERR_INVALID_ARG invalid parameter, please check partitions
 *      - ESP_ERR_INVALID_STATE tinyuf2 already installed
 *      - ESP_OK  Success
 */
esp_err_t esp_tinyuf2_install(tinyuf2_ota_config_t *ota_config, tinyuf2_nvs_config_t *nvs_config);

/**
 * @brief Uninstall tinyuf2, only reset USB to default state
 * @note not release memory due to tinyusb not support teardown
 *
 * @return esp_err_t
 *      - ESP_ERR_INVALID_STATE tinyuf2 not installed
 *      - ESP_OK  Success
 */
esp_err_t esp_tinyuf2_uninstall(void);

/**
 * @brief Get tinyuf2 current state
 *
 * @return tinyuf2_state_t
 */
tinyuf2_state_t esp_tinyuf2_current_state(void);

/**
 * @brief Restart system and set reset reason to UF2_RESET_REASON_VALUE
 *
 */
void esp_restart_from_tinyuf2(void);

#ifdef CONFIG_UF2_INI_NVS_VALUE_HIDDEN
/**
 * @brief Set the state of the "all keys hidden" flag.
 *
 * This function updates the global flag indicating whether all keys
 * should be treated as hidden.
 *
 * @param[in] if_hidden Boolean flag to indicate the hidden state:
 *                      - true: Set all keys as hidden.
 *                      - false: Set all keys as visible.
 *
 * @return
 *      - ESP_OK: Operation was successful.
 */
esp_err_t esp_tinyuf2_set_all_key_hidden(bool if_hidden)

/**
 * @brief Add a key to the hidden keys list.
 *
 * This function dynamically allocates memory to store a copy of the
 * provided key and adds it to the list of hidden keys. It ensures
 * that the maximum limit of hidden keys is not exceeded (CONFIG_UF2_INI_NVS_HIDDEN_MAX_NUM).
 *
 * @param[in] key A pointer to the null-terminated string representing the key to hide.
 *
 * @return
 *      - ESP_OK: Key added successfully.
 *      - ESP_ERR_INVALID_ARG: Provided key is NULL.
 *      - ESP_ERR_NO_MEM: Memory allocation failed or maximum number of hidden keys exceeded.
 */
esp_err_t esp_tinyuf2_add_key_hidden(const char *key)
#endif

#ifdef __cplusplus
}
#endif

#endif /* _ESP_TINYUF2_H_ */
