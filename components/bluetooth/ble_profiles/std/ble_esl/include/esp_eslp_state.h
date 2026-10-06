/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_timer.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief   ESL Profile states
 */
typedef enum {
    BLE_ESLP_STATE_UNASSOCIATED = 0,                                    /*!< Not provisioned / bond cleared */
    BLE_ESLP_STATE_CONFIGURING,                                         /*!< Connected, being configured */
    BLE_ESLP_STATE_UPDATING,                                            /*!< Connected while associated */
    BLE_ESLP_STATE_SYNCHRONIZED,                                        /*!< Synchronized to AP PAwR train */
    BLE_ESLP_STATE_UNSYNCHRONIZED,                                      /*!< Associated, not connected, not synced */
} esp_ble_eslp_state_t;

/**
 * @brief   Lifecycle timeout kinds
 */
typedef enum {
    BLE_ESLP_TIMEOUT_SYNC = 0,                                          /*!< No valid sync activity for timeout -> Unsynchronized */
    BLE_ESLP_TIMEOUT_UNSYNC,                                            /*!< Stayed Unsynchronized for timeout -> Unassociated */
} esp_ble_eslp_timeout_kind_t;

/**
 * @brief   State change notification
 */
typedef struct {
    esp_ble_eslp_state_t prev_state;                                    /*!< Previous state */
    esp_ble_eslp_state_t new_state;                                     /*!< New state */
} esp_ble_eslp_state_changed_t;

typedef void (*esp_ble_eslp_sm_changed_cb_t)(const esp_ble_eslp_state_changed_t *changed, void *ctx);
typedef void (*esp_ble_eslp_sm_timeout_cb_t)(esp_ble_eslp_timeout_kind_t kind, void *ctx);

/**
 * @brief   ESL lifecycle state machine
 */
typedef struct {
    esp_ble_eslp_state_t state;                                         /*!< Current state */
    esp_timer_handle_t sync_timer;                                      /*!< Synchronized activity timeout timer */
    esp_timer_handle_t unsync_timer;                                    /*!< Unsynchronized timeout timer */
    esp_ble_eslp_sm_changed_cb_t on_changed;                            /*!< Optional state-change callback */
    esp_ble_eslp_sm_timeout_cb_t on_timeout;                            /*!< Optional timeout callback */
    void *ctx;                                                          /*!< User context for callbacks */
    bool inited;                                                        /*!< Initialization flag */
} esp_ble_eslp_sm_t;

/**
 * @brief Return a short string for an ESLP state
 *
 * @param[in]  state The ESLP state
 *
 * @return
 *  - Constant C string
 */
const char *esp_ble_eslp_state_str(esp_ble_eslp_state_t state);

/**
 * @brief True if the state implies association with an AP
 *
 * @param[in]  state The ESLP state
 *
 * @return
 *  - true if associated
 *  - false otherwise
 */
bool esp_ble_eslp_state_is_associated(esp_ble_eslp_state_t state);

/**
 * @brief Check whether a state transition is allowed
 *
 * @param[in]  from The current state
 * @param[in]  to   The target state
 *
 * @return
 *  - true if the transition is allowed
 *  - false otherwise
 */
bool esp_ble_eslp_sm_transition_valid(esp_ble_eslp_state_t from, esp_ble_eslp_state_t to);

/**
 * @brief Initialize a state machine instance
 *
 * @param[in,out] sm         The pointer to store the state machine
 * @param[in]     on_changed Optional state-change callback
 * @param[in]     on_timeout Optional timeout callback
 * @param[in]     ctx        User context for callbacks
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_sm_init(esp_ble_eslp_sm_t *sm,
                               esp_ble_eslp_sm_changed_cb_t on_changed,
                               esp_ble_eslp_sm_timeout_cb_t on_timeout,
                               void *ctx);

/**
 * @brief Deinitialize a state machine instance and delete timers
 *
 * @param[in,out] sm The pointer to store the state machine
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_sm_deinit(esp_ble_eslp_sm_t *sm);

/**
 * @brief Get the current state
 *
 * @param[in]  sm The pointer to store the state machine
 *
 * @return
 *  - Current ESLP state, or BLE_ESLP_STATE_UNASSOCIATED on invalid argument
 */
esp_ble_eslp_state_t esp_ble_eslp_sm_get_state(const esp_ble_eslp_sm_t *sm);

/**
 * @brief Request a state transition
 *
 * @param[in,out] sm        The pointer to store the state machine
 * @param[in]     new_state The target state
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_sm_set_state(esp_ble_eslp_sm_t *sm, esp_ble_eslp_state_t new_state);

/**
 * @brief Refresh the Synchronized-state activity timer
 *
 * @param[in,out] sm The pointer to store the state machine
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 */
esp_err_t esp_ble_eslp_sm_note_sync_activity(esp_ble_eslp_sm_t *sm);

#ifdef __cplusplus
}
#endif
