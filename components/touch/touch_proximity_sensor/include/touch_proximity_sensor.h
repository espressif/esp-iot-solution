/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Touch proximity sensor state.
 */
typedef enum {
    PROXI_STATE_INACTIVE = 0, /*!< No proximity event is active. */
    PROXI_STATE_ACTIVE,       /*!< The configured proximity threshold is active. */
} proxi_state_t;

/**
 * @brief Configuration for a touch proximity sensor.
 *
 * Proximity channels use the low-level proximity data path and retain its
 * accumulated-count unit. They are not ordinary touch raw counts and are not
 * aligned to a 5000-count touch baseline. `thresholds` are normalized fused
 * decision counts produced by the FSM after the independent proximity
 * baseline is established; tune them from the target's measured idle signal.
 * All arrays are copied by create().
 */
typedef struct {
    uint32_t channel_num;       /*!< Number of proximity channels. */
    uint32_t *channel_list;     /*!< Channel IDs, with channel_num entries. */
    uint32_t *thresholds;       /*!< Per-channel normalized proximity thresholds. */
    uint32_t sample_period_ms;  /*!< Requested scan period; zero selects the low-level default. */
    uint32_t sample_cfg_num;    /*!< Samples per frame; zero selects the low-level default. */
    uint32_t proximity_count;   /*!< Measurements accumulated per frame; zero selects the Kconfig default. */
    uint32_t debounce_active;   /*!< Consecutive active frames; zero selects the FSM default. */
    uint32_t debounce_inactive; /*!< Consecutive inactive frames; zero selects the FSM default. */
    bool skip_lowlevel_init;    /*!< Reuse an existing proximity low-level instance when true. */
} touch_proxi_config_t;

typedef struct touch_proximity_sensor_t *touch_proximity_handle_t;

/**
 * @brief Proximity sensor state-change callback.
 *
 * @param[in] channel Channel that changed state.
 * @param[in] event New stable proximity state.
 * @param[in] cb_arg User data supplied at create().
 */
typedef void (*proxi_cb_t)(uint32_t channel, proxi_state_t event, void *cb_arg);

/**
 * @brief Create a touch proximity sensor instance.
 *
 * A pure proximity low-level configuration is created when requested. Its
 * startup path does not use ordinary touch 5000-count alignment. One FSM
 * consumes complete proximity frames and performs any configured frequency
 * fusion without using the ordinary touch frequency estimator.
 *
 * @param[in]  config Proximity sensor configuration.
 * @param[out] sensor_handle Receives the created sensor handle.
 * @param[in]  cb State-change callback, or NULL.
 * @param[in]  cb_arg User data passed to the callback.
 * @return ESP_OK on success, or an allocation, low-level, or FSM error.
 */
esp_err_t touch_proximity_sensor_create(const touch_proxi_config_t *config,
                                        touch_proximity_handle_t *sensor_handle,
                                        proxi_cb_t cb, void *cb_arg);

/**
 * @brief Delete a touch proximity sensor instance.
 *
 * @param[in] proxi_sensor Handle returned by create(), or NULL.
 * @return ESP_OK on success, or the first stop/delete error.
 */
esp_err_t touch_proximity_sensor_delete(touch_proximity_handle_t proxi_sensor);

/**
 * @brief Get the normalized positive fused proximity amplitude.
 *
 * @param[in]  handle Proximity sensor handle.
 * @param[in]  channel Configured channel ID.
 * @param[out] data Nonnegative normalized fused filtered amplitude.
 * @return ESP_OK, ESP_ERR_INVALID_ARG, ESP_ERR_INVALID_STATE, or ESP_ERR_NOT_FOUND.
 */
esp_err_t touch_proximity_sensor_get_data(touch_proximity_handle_t handle,
                                          uint32_t channel, uint32_t *data);

/**
 * @brief Get the current state of one proximity channel.
 *
 * @param[in]  handle Proximity sensor handle.
 * @param[in]  channel Configured channel ID.
 * @param[out] state Receives the stable state.
 * @return ESP_OK, ESP_ERR_INVALID_ARG, ESP_ERR_INVALID_STATE, or ESP_ERR_NOT_FOUND.
 */
esp_err_t touch_proximity_sensor_get_state(touch_proximity_handle_t handle,
                                           uint32_t channel, proxi_state_t *state);

/**
 * @brief Process pending proximity frames and invoke callbacks.
 *
 * @param[in] handle Proximity sensor handle.
 * @return ESP_OK on success, or an FSM processing error.
 */
esp_err_t touch_proximity_sensor_handle_events(touch_proximity_handle_t handle);

#ifdef __cplusplus
}
#endif
