/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
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
 * @brief Signed axis selector used by quaternion configuration.
 */
typedef enum {
    IMU_QUAT_AXIS_POS_X = 0, /*!< Positive X axis. */
    IMU_QUAT_AXIS_NEG_X,     /*!< Negative X axis. */
    IMU_QUAT_AXIS_POS_Y,     /*!< Positive Y axis. */
    IMU_QUAT_AXIS_NEG_Y,     /*!< Negative Y axis. */
    IMU_QUAT_AXIS_POS_Z,     /*!< Positive Z axis. */
    IMU_QUAT_AXIS_NEG_Z,     /*!< Negative Z axis. */
} imu_quat_axis_t;

/**
 * @brief One IMU sample consumed by the quaternion solver.
 */
typedef struct {
    float accel[3];        /*!< Acceleration sample in g, body-frame XYZ order. */
    float gyro[3];         /*!< Angular velocity sample in deg/s, body-frame XYZ order. */
    bool mag_valid;        /*!< True when mag[] contains a calibrated body-frame magnetic sample. */
    float mag[3];          /*!< Calibrated magnetic field sample in body-frame XYZ order, usually in uT. */
    int64_t timestamp_us;  /*!< Sample timestamp in microseconds. */
} imu_quat_sample_t;

/**
 * @brief Runtime configuration of one quaternion solver instance.
 *
 * The configuration describes how the solver initializes its body-to-world
 * frame and whether optional bias learning and gyro-guard logic are enabled.
 */
typedef struct {
    imu_quat_axis_t accel_target_axis;   /*!< World axis that measured accel should align to at initialization. */
    float heading_ref_body[3];           /*!< Body-frame heading reference vector used during alignment init. */
    imu_quat_axis_t heading_target_axis; /*!< World axis that the projected heading reference should align to. */
    bool mag_input_enabled;              /*!< Enable optional magnetic input for recording an initial world-frame magnetic
                                              reference and future yaw correction. The initialization heading still uses
                                              heading_ref_body. */
    bool gyro_bias_enabled;              /*!< Enable runtime rest-based gyro bias learning. When false, the solver keeps
                                              the current bias fixed after create/reset or caller-provided
                                              initialization. */
    bool gyro_guard_enabled;             /*!< Enable gyro over-limit guard and guarded reinitialization. */
    float gyro_guard_limit_dps;          /*!< Gyro over-limit threshold in deg/s. */
} imu_quat_config_t;

/**
 * @brief Output state returned by one quaternion update.
 */
typedef struct {
    bool updated;        /*!< True if this call completed a normal continuous update step. */
    bool reinitialized;  /*!< True if this call rebuilt the solver state through guarded reinitialization. */
    bool rest_detected;  /*!< True if the internal bias learner currently considers the device at rest. */
    float q_w;           /*!< Current quaternion scalar part. */
    float q_x;           /*!< Current quaternion X component. */
    float q_y;           /*!< Current quaternion Y component. */
    float q_z;           /*!< Current quaternion Z component. */
} imu_quat_output_t;

/** @cond */
typedef struct imu_quat_t *imu_quat_handle_t;
/** @endcond */

/**
 * @brief Create one quaternion solver instance.
 *
 * When @p config is NULL, the solver uses the component default configuration.
 *
 * @param config Runtime configuration, or NULL to use defaults.
 * @param quat_handle Returned solver handle.
 *
 * @return
 *      - ESP_OK: The solver was created successfully.
 *      - ESP_ERR_INVALID_ARG: The input arguments are invalid.
 *      - ESP_ERR_NO_MEM: Memory allocation failed.
 */
esp_err_t imu_quat_create(const imu_quat_config_t *config, imu_quat_handle_t *quat_handle);

/**
 * @brief Delete one quaternion solver instance.
 *
 * @param handle Solver handle.
 *
 * @return
 *      - ESP_OK: The solver was deleted successfully.
 */
esp_err_t imu_quat_delete(imu_quat_handle_t handle);

/**
 * @brief Replace the runtime configuration of an existing solver instance.
 *
 * This API only updates the stored configuration. Call
 * `imu_quat_reset_state()` if the application wants the new configuration to
 * take effect from a clean runtime state immediately.
 *
 * @param handle Solver handle.
 * @param config New runtime configuration.
 *
 * @return
 *      - ESP_OK: The configuration was updated successfully.
 *      - ESP_ERR_INVALID_ARG: The input arguments are invalid.
 */
esp_err_t imu_quat_set_config(imu_quat_handle_t handle, const imu_quat_config_t *config);

/**
 * @brief Reset the runtime state of an existing solver instance.
 *
 * This resets pose, bias-learning state, gyro-guard state, and warmup state
 * while keeping the solver handle and current configuration alive.
 *
 * @param handle Solver handle.
 *
 * @return
 *      - ESP_OK: The runtime state was reset successfully.
 *      - ESP_ERR_INVALID_ARG: The input arguments are invalid.
 */
esp_err_t imu_quat_reset_state(imu_quat_handle_t handle);

/**
 * @brief Replace the current gyro bias estimate with a caller-provided value.
 *
 * This API is intended for startup/restored bias injection. The provided
 * vector uses deg/s units for convenience, while the internal solver stores
 * gyro bias in rad/s. Runtime rest-based bias learning can remain enabled and
 * continue refining the estimate after this initial value is applied.
 *
 * @param handle Solver handle.
 * @param gyro_bias_dps Gyro bias in deg/s, body-frame XYZ order.
 *
 * @return
 *      - ESP_OK: The gyro bias was updated successfully.
 *      - ESP_ERR_INVALID_ARG: The input arguments are invalid.
 */
esp_err_t imu_quat_set_gyro_bias_dps(imu_quat_handle_t handle, const float gyro_bias_dps[3]);

/**
 * @brief Read back the current gyro bias estimate in deg/s.
 *
 * @param handle Solver handle.
 * @param gyro_bias_dps Returned gyro bias in deg/s, body-frame XYZ order.
 *
 * @return
 *      - ESP_OK: The gyro bias was returned successfully.
 *      - ESP_ERR_INVALID_ARG: The input arguments are invalid.
 */
esp_err_t imu_quat_get_gyro_bias_dps(imu_quat_handle_t handle, float gyro_bias_dps[3]);

/**
 * @brief Explicitly reinitialize the solver from one IMU sample.
 *
 * This rebuilds the world frame from the current sample using the stored
 * configuration. The initialization heading always comes from
 * `config.heading_ref_body`. When magnetic input is enabled and the sample
 * carries a valid calibrated magnetic field, the solver records an initial
 * world-frame magnetic reference after initialization.
 *
 * @param handle Solver handle.
 * @param sample Input IMU sample used to rebuild the world frame.
 *
 * @return
 *      - ESP_OK: The solver was reinitialized successfully.
 *      - ESP_ERR_INVALID_ARG: The input arguments are invalid.
 *      - ESP_ERR_INVALID_STATE: The sample cannot build a valid pose frame.
 */
esp_err_t imu_quat_reinitialize_from_sample(imu_quat_handle_t handle, const imu_quat_sample_t *sample);

/**
 * @brief Consume one IMU sample and update solver output.
 *
 * This API only performs the normal continuous-update path. Call
 * `imu_quat_reinitialize_from_sample()` first after create/reset to establish
 * the world frame explicitly before feeding normal update samples.
 *
 * @param handle Solver handle.
 * @param sample Input IMU sample.
 * @param out Returned solver output for this update.
 *
 * @return
 *      - ESP_OK: The sample was processed successfully.
 *      - ESP_ERR_INVALID_ARG: The input arguments are invalid.
 *      - ESP_ERR_INVALID_STATE: The solver has not been explicitly initialized
 *        yet, or the update state/timestamp is invalid.
 *      - ESP_FAIL: The update failed.
 */
esp_err_t imu_quat_update(imu_quat_handle_t handle, const imu_quat_sample_t *sample, imu_quat_output_t *out);

/**
 * @brief Rotate one body-frame vector into world frame using the current quaternion.
 *
 * @param handle Solver handle.
 * @param v_body Input body-frame vector.
 * @param v_world Returned world-frame vector.
 *
 * @return
 *      - ESP_OK: The vector was rotated successfully.
 *      - ESP_ERR_INVALID_ARG: The input arguments are invalid.
 */
esp_err_t imu_quat_rotate_body_to_world(imu_quat_handle_t handle, const float v_body[3], float v_world[3]);

/**
 * @brief Rotate one world-frame vector into body frame using the current quaternion.
 *
 * @param handle Solver handle.
 * @param v_world Input world-frame vector.
 * @param v_body Returned body-frame vector.
 *
 * @return
 *      - ESP_OK: The vector was rotated successfully.
 *      - ESP_ERR_INVALID_ARG: The input arguments are invalid.
 */
esp_err_t imu_quat_rotate_world_to_body(imu_quat_handle_t handle, const float v_world[3], float v_body[3]);

#ifdef __cplusplus
}
#endif
