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
    esp_partition_subtype_t subtype;  /*! Partition subtype. if ESP_PARTITION_SUBTYPE_ANY will use the next_update_partition by default. */
    const char *label;                /*! Partition label. Set this value if looking for partition with a specific name. if subtype==ESP_PARTITION_SUBTYPE_ANY, label default to NULL.*/
    bool if_restart;                  /*! if restart system to new app partition after UF2 flashing done */
    update_complete_cb_t complete_cb; /*! user callback called after uf2 update complete */
} tinyuf2_ota_config_t;

/**
 * @brief tinyuf2 nvs configurations
 * 
 */
typedef struct {
    const char *part_name;            /*! Partition name. */
    const char *namespace_name;       /*! Namespace name. */
    nvs_modified_cb_t modified_cb;    /*! user callback called after uf2 update complete */
} tinyuf2_nvs_config_t;

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

#ifdef __cplusplus
}
#endif

#endif /* _ESP_TINYUF2_H_ */