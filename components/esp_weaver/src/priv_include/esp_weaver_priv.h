/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <esp_err.h>
#include "esp_weaver.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Node config protocol version */
#define ESP_WEAVER_CONFIG_VERSION   "2026-01-01"

/**
 * @brief Weaver state enumeration
 */
typedef enum {
    ESP_WEAVER_STATE_DEINIT = 0,          /*!< Not initialized */
    ESP_WEAVER_STATE_INIT_DONE,           /*!< Initialized, local control not started */
    ESP_WEAVER_STATE_LOCAL_CTRL_STARTING, /*!< Local control starting */
    ESP_WEAVER_STATE_LOCAL_CTRL_STARTED,  /*!< Local control started */
} esp_weaver_state_t;

/**
 * @brief Get current weaver state
 *
 * @return Current state
 */
esp_weaver_state_t esp_weaver_get_state(void);

/**
 * @brief Set weaver state (internal use only)
 *
 * @param[in] state New state
 */
void esp_weaver_set_state(esp_weaver_state_t state);

/**
 * @brief Get node configuration as JSON string
 *
 * Caller must free the returned string.
 *
 * @return Allocated JSON string on success, or NULL on failure
 */
char *esp_weaver_get_node_config(void);

/**
 * @brief Get all parameters as JSON string
 *
 * Caller must free the returned string.
 *
 * @return Allocated JSON string on success, or NULL on failure
 */
char *esp_weaver_get_node_params(void);

/**
 * @brief Get changed parameters as JSON string
 *
 * Only includes parameters with VALUE_CHANGE flag set.
 * Clears the flags after generating JSON.
 * Caller must free the returned string.
 *
 * @return Allocated JSON string on success, or NULL on failure
 */
char *esp_weaver_get_changed_node_params(void);

/**
 * @brief Handle incoming parameter set request
 *
 * @param[in] data JSON data string (must not be NULL)
 * @param[in] data_len Length of data
 * @param[in] src Request source identifier
 * @return
 *      - ESP_OK: Parameters set successfully
 *      - ESP_ERR_INVALID_STATE: Node not initialized
 *      - ESP_FAIL: JSON parse failed
 *      - ESP_ERR_NO_MEM: Memory allocation failed
 */
esp_err_t esp_weaver_handle_set_params(const char *data, size_t data_len, esp_weaver_req_src_t src);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
