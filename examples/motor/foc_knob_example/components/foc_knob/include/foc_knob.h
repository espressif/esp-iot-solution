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

#define IDLE_VELOCITY_EWMA_ALPHA  0.001
#define IDLE_VELOCITY_RAD_PER_SEC  0.05
#define IDLE_CORRECTION_DELAY_MILLIS  500
#define IDLE_CORRECTION_MAX_ANGLE_RAD  (5 * PI / 180)
#define IDLE_CORRECTION_RATE_ALPHA  0.0005
#define DEAD_ZONE_DETENT_PERCENT  0.2
#define DEAD_ZONE_RAD  (1 * PI / 180)

typedef void *knob_handle_t; /*!< Knob operation handle */

/**
 * @brief Knob parameters
 *
 */
typedef struct {
    int32_t num_positions;        // Number of positions
    int32_t position;             // Current position
    float position_width_radians; // Width of each position in radians
    float detent_strength_unit;   // Strength of detent during normal rotation
    float endstop_strength_unit;  // Strength of detent when reaching the end
    float snap_point;             // Snap point for each position
    const char *descriptor;       // Description
} knob_param_t;

/**
 * @brief Knob specified configurations, used when creating a Knob
 */
typedef struct {
    knob_param_t const **param_lists;
    uint16_t param_list_num;
} knob_config_t;

/**
 * @brief Create a knob
 *
 * @param config Pointer to knob configuration
 *
 * @return A handle to the created knob
 */
knob_handle_t knob_create(const knob_config_t *config);

/**
 * @brief Get knob parameters for a specific mode.
 *
 * This function retrieves the knob parameters for a specific mode and returns a pointer
 * to a 'knob_param_t' structure.
 *
 * @param handle A handle to the knob.
 * @param mode The mode for which parameters are requested.
 *
 * @return A pointer to the 'knob_param_t' structure if successful, or NULL if the mode
 * is out of range or the handle is invalid.
 */
knob_param_t *knob_get_param(knob_handle_t handle, int mode);

/**
 * @brief Start knob operation.
 *
 * This function controls the operation of a knob based on the specified mode, shaft velocity,
 * and shaft angle.
 *
 * @param handle A handle to the knob.
 * @param mode The mode of the knob operation.
 * @param shaft_velocity The velocity of the knob's shaft.
 * @param shaft_angle The angle of the knob's shaft.
 *
 * @return The torque applied during the knob operation.
 */
float knob_start(knob_handle_t handle, int mode, float shaft_velocity, float shaft_angle);

/**
 * @brief Delete a knob and free associated memory.
 *
 * This function is used to delete a knob created using the knob_create function.
 * It frees the dynamically allocated memory for the knob structure.
 *
 * @param handle A handle to the knob to be deleted.
* @return ESP_OK on success
*/
esp_err_t knob_delete(knob_handle_t handle);

#ifdef __cplusplus
}
#endif
