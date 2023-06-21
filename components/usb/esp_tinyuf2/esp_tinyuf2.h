#ifndef _ESP_TINYUF2_H_
#define _ESP_TINYUF2_H_
#include "esp_partition.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_TINYUF2_CONFIG()          \
{                                         \
    .subtype = ESP_PARTITION_SUBTYPE_ANY, \
    .label = NULL,                        \
    .if_restart = true                   \
}

/**
 * @brief user callback called after uf2 update complete
 * 
 */
typedef void (*update_complete_cb_t)(void);

/**
 * @brief tinyuf2 configurations
 * 
 */
typedef struct {
    esp_partition_subtype_t subtype;  /*! Partition subtype. if ESP_PARTITION_SUBTYPE_ANY will use the next_update_partition by default. */
    const char *label;                /*! Partition label. Set this value if looking for partition with a specific name. if subtype==ESP_PARTITION_SUBTYPE_ANY, label default to NULL.*/
    bool if_restart;                  /*! if restart system to new app partition after UF2 flashing done */
    update_complete_cb_t complete_cb; /*! user callback called after uf2 update complete */
}tinyuf2_config_t;

/**
 * @brief Flashing app to specified partition through USB UF2 (Virtual USB Disk)
 * 
 * @param config tinyuf2 configs described in tinyuf2_config_t
 * @return
 *      - ESP_ERR_INVALID_ARG invalid parameter, please check partitions
 *      - ESP_OK  Success
 */
esp_err_t tinyuf2_updater_install(tinyuf2_config_t *config);

/**
 * @brief Delete tinyuf2 updater driver, not implement
 * 
 * void tinyuf2_updater_delete();
 */

#ifdef __cplusplus
}
#endif

#endif /* _ESP_TINYUF2_H_ */