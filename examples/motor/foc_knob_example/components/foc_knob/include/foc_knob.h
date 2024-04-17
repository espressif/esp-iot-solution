/*
 * SPDX-FileCopyrightText: 2016-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifndef PI
#define PI 3.14159265358979f
#endif

typedef void (* foc_knob_cb_t)(void *foc_knob_handle, void *user_data);
typedef void *foc_knob_handle_t; /*!< Knob operation handle */

typedef struct {
    float angle_to_detent_center;          /*!< The angle from the current position. */
    float position_width_radians;          /*!< Width of each position in radians */
    int32_t num_positions;                 /*!< Number of positions */
    int32_t position;                      /*!< Current position */
    const char *descriptor;                /*!< Description */
} foc_knob_state_t;

/**
 * @brief Knob parameters
 *
 */
typedef struct {
    int32_t    num_positions;               /*!< Number of positions */
    int32_t    position;                    /*!< Current position */
    float      position_width_radians;      /*!< Width of each position in radians */
    float      detent_strength_unit;        /*!< Strength of detent during normal rotation */
    float      endstop_strength_unit;       /*!< Strength of detent when reaching the end */
    float      snap_point;                  /*!< Snap point for each position */
    const char *descriptor;                 /*!< Description */
} foc_knob_param_t;

/**
 * @brief Knob events
 *
 */
typedef enum {
    FOC_KNOB_INC = 0,                      /*!< EVENT: Position increase */
    FOC_KNOB_DEC,                          /*!< EVENT: Position decrease */
    FOC_KNOB_H_LIM,                        /*!< EVENT: Count reaches maximum limit */
    FOC_KNOB_L_LIM,                        /*!< EVENT: Count reaches the minimum limit */
    FOC_KNOB_ANGLE_CHANGE,                 /*!< EVENT: Angle change */
    FOC_KNOB_EVENT_MAX,                    /*!< EVENT: Number of events */
} foc_knob_event_t;

typedef float (*foc_knob_pid_cb_t)(float P, float D, float limit, float error);

/**
 * @brief Knob specified configurations, used when creating a Knob
 */
typedef struct {
    foc_knob_param_t const **param_lists;   /*!< foc mode lists, if not set use default_foc_knob_param_lst */
    uint16_t param_list_num;                /*!< foc mode number */
    float max_torque_out_limit;             /*!< max torque out limit */
    float max_torque;                       /*!< max torque in limit */
    foc_knob_pid_cb_t pid_cb;               /*!< PID callback function */
} foc_knob_config_t;

/**
 * @brief Create a knob
 *
 * @param config Pointer to knob configuration
 *
 * @return A handle to the created knob
 */
foc_knob_handle_t foc_knob_create(const foc_knob_config_t *config);

/**
 * @brief Change the mode of the knob.
 *
 * @param handle A handle to the knob.
 * @param mode The mode to be set for the knob.
 * @return
 *     - ESP_OK if the successful.
 *     - ESP_ERR_INVALID_ARG if the provided arguments are invalid.
 */
esp_err_t foc_knob_change_mode(foc_knob_handle_t handle, uint16_t mode);

/**
 * @brief Start knob operation.
 *
 * This function controls the operation of a knob based on the specified mode, shaft velocity,
 * and shaft angle.
 *
 * @param handle A handle to the knob.
 * @param shaft_velocity The velocity of the knob's shaft.
 * @param shaft_angle The angle of the knob's shaft.
 *
 * @return The torque applied during the knob operation.
 */
float foc_knob_run(foc_knob_handle_t handle, float shaft_velocity, float shaft_angle);

/**
 * @brief Delete a knob and free associated memory.
 *
 * This function is used to delete a knob created using the knob_create function.
 * It frees the dynamically allocated memory for the knob structure.
 *
 * @param handle A handle to the knob to be deleted.
* @return ESP_OK on success
*/
esp_err_t foc_knob_delete(foc_knob_handle_t handle);

/**
 * @brief Registers a callback function for a Field-Oriented Control (FOC) knob event.
 *
 * @param handle The handle to the FOC knob.
 * @param event The event type to register the callback for.
 * @param cb The callback function to be invoked when the specified event occurs.
 * @param usr_data User-defined data that will be passed to the callback function when invoked.
 *
 * @return
 *     - ESP_OK if the callback is successfully registered.
 *     - ESP_ERR_INVALID_ARG if the provided arguments are invalid.
 */
esp_err_t foc_knob_register_cb(foc_knob_handle_t handle, foc_knob_event_t event, foc_knob_cb_t cb, void *usr_data);

/**
 * @brief Unregisters a callback function for a Field-Oriented Control (FOC) knob event.
 *
 * @param handle The handle to the FOC knob.
 * @param event The event type for which the callback needs to be unregistered.
 *
 * @return
 *     - ESP_OK if the callback is successfully unregistered.
 *     - ESP_ERR_INVALID_ARG if the provided arguments are invalid.
 */
esp_err_t foc_knob_unregister_cb(foc_knob_handle_t handle, foc_knob_event_t event);

/**
 * @brief Get the current state of the knob.
 *
 * @param handle The handle to the FOC knob.
 * @param state The pointer to the state structure to be filled.
 * @return
 *     - ESP_OK if the successful.
 *     - ESP_ERR_INVALID_ARG if the provided arguments are invalid.
 */
esp_err_t foc_knob_get_state(foc_knob_handle_t handle, foc_knob_state_t *state);

/**
 * @brief Get the current event of the knob.
 *
 * @param handle The handle to the FOC knob.
 * @param event The pointer to the event structure to be filled.
 * @return
 *     - ESP_OK if the successful.
 *     - ESP_ERR_INVALID_ARG if the provided arguments are invalid.
 */
esp_err_t foc_knob_get_event(foc_knob_handle_t handle, foc_knob_event_t *event);

/**
 * @brief Get the current mode of the knob.
 *
 * @param handle The handle to the FOC knob.
 * @param position The pointer to the position structure to be filled.
 * @return
 *     - ESP_OK if the successful.
 *     - ESP_ERR_INVALID_ARG if the provided arguments are invalid.
 */
esp_err_t foc_knob_set_currect_mode_position(foc_knob_handle_t handle, int32_t position);

/**
 * @brief Get the current mode's position of the knob.
 *
 * @param handle The handle to the FOC knob.
 * @param position The pointer to the position structure to be filled.
 * @return
 *     - ESP_OK if the successful.
 *     - ESP_ERR_INVALID_ARG if the provided arguments are invalid.
 */
esp_err_t foc_knob_get_current_mode_position(foc_knob_handle_t handle, int32_t *position);

/**
 * @brief Set the position of the knob.
 *
 * @param handle The handle to the FOC knob.
 * @param mode The mode of setting the position
 * @param position The desired position value to be set for the FOC knob.
 * @return
 *     - ESP_OK if the successful.
 *     - ESP_ERR_INVALID_ARG if the provided arguments are invalid.
 */
esp_err_t foc_knob_set_position(foc_knob_handle_t handle, uint16_t mode, int32_t position);

/**
 * @brief Get the position of the knob.
 *
 * @param handle The handle to the FOC knob.
 * @param mode The mode of getting the position
 * @param position The pointer to the position structure to be filled.
 * @return
 *     - ESP_OK if the successful.
 *     - ESP_ERR_INVALID_ARG if the provided arguments are invalid.
 */
esp_err_t foc_knob_get_position(foc_knob_handle_t handle, uint16_t mode, int32_t *position);

#ifdef __cplusplus
}
#endif
