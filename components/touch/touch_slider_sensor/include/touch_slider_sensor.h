/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
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
 * @brief Touch slider event.
 */
typedef enum {
    TOUCH_SLIDER_EVENT_NONE,        /*!< Touch slider is stationary. */
    TOUCH_SLIDER_EVENT_RIGHT_SWIPE, /*!< Slider was swiped toward position_range. */
    TOUCH_SLIDER_EVENT_LEFT_SWIPE,  /*!< Slider was swiped toward position zero. */
    TOUCH_SLIDER_EVENT_RELEASE,     /*!< All slider channels were released. */
    TOUCH_SLIDER_EVENT_POSITION,    /*!< A new slider position was calculated. */
} touch_slider_event_t;

/**
 * @brief Configuration for a touch slider sensor.
 *
 * `thresholds` are absolute normalized fused-count values in the FSM decision
 * domain. They are not physical raw counts and are not ratios relative to a
 * presumed baseline of 5000. The array values are copied by create().
 */
typedef struct {
    uint32_t channel_num;          /*!< Number of slider channels; must be at least two. */
    uint32_t *channel_list;        /*!< Channel IDs, with channel_num entries. */
    uint32_t *thresholds;          /*!< Per-channel normalized press thresholds. */
    uint32_t sample_period_ms;     /*!< Low-level sampling period; zero selects its default. */
    uint32_t sample_cfg_num;       /*!< Frequency samples per frame; zero selects its default. */
    uint32_t debounce_active;      /*!< Consecutive active frames; zero selects the FSM default. */
    uint32_t debounce_inactive;    /*!< Consecutive inactive frames; zero selects the FSM default. */
    uint32_t filter_reset_times;   /*!< Inactive event cycles before resetting the position filter. */
    uint32_t position_range;       /*!< Maximum position; output is in [0, position_range]. */
    uint8_t calculate_window;      /*!< Adjacent channels used for position; zero selects a default. */
    float swipe_threshold;         /*!< Speed threshold for identifying a swipe. */
    float swipe_hysterisis;        /*!< Hysteresis around the swipe speed threshold. */
    float swipe_alpha;             /*!< IIR coefficient for speed estimation, in [0, 1]. */
    bool skip_lowlevel_init;       /*!< Reuse an existing low-level instance when true. */
} touch_slider_config_t;

typedef struct touch_slider_sensor_t *touch_slider_handle_t;

/**
 * @brief Touch slider event callback.
 *
 * @param[in] handle Touch slider sensor handle.
 * @param[in] event Event type.
 * @param[in] data Position for POSITION, or signed displacement for RELEASE.
 * @param[in] cb_arg User data supplied at create().
 */
typedef void (*touch_slider_event_cb_t)(touch_slider_handle_t handle,
                                        touch_slider_event_t event,
                                        int32_t data, void *cb_arg);

/**
 * @brief Create a touch slider sensor instance.
 *
 * The low-level driver is started before callbacks are registered, so startup
 * calibration samples cannot enter the FSM. One FSM receives complete frames
 * and performs all configured frequency fusion.
 *
 * @param[in]  config Slider configuration.
 * @param[out] handle Receives the created sensor handle.
 * @param[in]  cb Event callback, or NULL.
 * @param[in]  cb_arg User data passed to the callback.
 * @return ESP_OK on success, or an error from allocation, low-level, or FSM setup.
 */
esp_err_t touch_slider_sensor_create(const touch_slider_config_t *config,
                                     touch_slider_handle_t *handle,
                                     touch_slider_event_cb_t cb, void *cb_arg);

/**
 * @brief Delete a touch slider sensor instance.
 *
 * @param[in] handle Handle returned by touch_slider_sensor_create(), or NULL.
 * @return ESP_OK on success, or the first stop/delete error.
 */
esp_err_t touch_slider_sensor_delete(touch_slider_handle_t handle);

/**
 * @brief Get the normalized fused filtered amplitude for one channel.
 *
 * The returned value is the positive part of `fused_filtered`; frequency
 * fusion and smoothing are performed by the FSM.
 *
 * @param[in]  handle Slider sensor handle.
 * @param[in]  channel Configured channel number.
 * @param[out] data Receives the nonnegative normalized amplitude.
 * @return ESP_OK, ESP_ERR_INVALID_ARG, ESP_ERR_INVALID_STATE, or ESP_ERR_NOT_FOUND.
 */
esp_err_t touch_slider_sensor_get_data(touch_slider_handle_t handle,
                                       uint32_t channel, uint32_t *data);

/**
 * @brief Get the current slider state.
 *
 * @param[in]  handle Slider sensor handle.
 * @param[out] pressed True when any channel is active.
 * @param[out] pos Current filtered position.
 * @param[out] speed Current filtered position speed.
 */
void touch_slider_sensor_get_state(touch_slider_handle_t handle, bool *pressed,
                                   uint32_t *pos, float *speed);

/**
 * @brief Process pending touch slider frames and emit events.
 *
 * @param[in] handle Slider sensor handle.
 * @return ESP_OK on success, or an FSM processing error.
 */
esp_err_t touch_slider_sensor_handle_events(touch_slider_handle_t handle);

#ifdef __cplusplus
}
#endif
