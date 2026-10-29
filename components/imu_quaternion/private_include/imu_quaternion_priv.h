/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "imu_quaternion.h"

#define IMU_QUAT_DEG2RAD 0.01745329252f
#define IMU_QUAT_RAD2DEG 57.2957795131f
#define IMU_QUAT_INTERNAL_FEEDBACK_GAIN 0.5f
#define IMU_QUAT_INTERNAL_WARMUP_GAIN 10.0f
#define IMU_QUAT_INTERNAL_WARMUP_PERIOD_SEC 3.0f
#define IMU_QUAT_INTERNAL_REST_FILTER_TAU_SEC 0.5f
#define IMU_QUAT_INTERNAL_REST_MIN_TIME_SEC 1.2f
#define IMU_QUAT_INTERNAL_REST_GYRO_THRESHOLD_DPS 5.0f
#define IMU_QUAT_INTERNAL_REST_ACC_THRESHOLD_G 0.12f
#define IMU_QUAT_INTERNAL_BIAS_FILTER_TAU_SEC 0.5f
#define IMU_QUAT_INTERNAL_BIAS_REST_GAIN 0.1f
#define IMU_QUAT_INTERNAL_BIAS_CLIP_DPS 2.0f
#define IMU_QUAT_INTERNAL_GYRO_RECOVER_DPS 30.0f
#define IMU_QUAT_INTERNAL_GYRO_REINIT_WINDOW_MS 200U
#define IMU_QUAT_INTERNAL_GYRO_RECOVER_WINDOW_MS 400U
#define IMU_QUAT_INTERNAL_ACCEL_REJECTION_DEG 20.0f
#define IMU_QUAT_INTERNAL_MAG_REJECTION_DEG 45.0f
#define IMU_QUAT_INTERNAL_REJECTION_RECOVERY_COUNT 50U
#define IMU_QUAT_INTERNAL_MAG_FEEDBACK_GAIN 0.8f
#define IMU_QUAT_INTERNAL_MAG_FILTER_TAU_SEC 0.05f
#define IMU_QUAT_INTERNAL_MAG_FIELD_MIN 5.0f

/**
 * @brief Internal quaternion pose state.
 */
typedef struct {
    float q_w;                 /*!< Quaternion scalar part. */
    float q_x;                 /*!< Quaternion X component. */
    float q_y;                 /*!< Quaternion Y component. */
    float q_z;                 /*!< Quaternion Z component. */
    float up_ref[3];           /*!< Current world-frame up reference vector. */
    bool mag_ref_valid;        /*!< True when the initial world-frame magnetic reference has been recorded. */
    float mag_ref[3];          /*!< Initial horizontal magnetic reference in world frame. */
    int64_t last_timestamp_us;  /*!< Timestamp of the last processed sample. */
} imu_quat_pose_state_t;

/**
 * @brief Internal runtime gain state.
 */
typedef struct {
    bool warming_up;         /*!< True while warmup gain decay is active. */
    float steady_gain;       /*!< Steady-state correction gain. */
    float live_gain;         /*!< Current correction gain. */
    float gain_drop_per_sec; /*!< Warmup gain decay rate. */
} imu_quat_runtime_gain_t;

/**
 * @brief Internal gyro bias learner state.
 */
typedef struct {
    bool rest_detected;    /*!< True when the device is currently detected at rest. */
    bool bias_lp_valid;    /*!< True when the bias LP state has been initialized. */
    float rest_time_sec;   /*!< Accumulated rest duration. */
    float gyro_bias[3];    /*!< Learned gyro bias in rad/s. */
    float rest_gyro_lp[3]; /*!< Low-pass filtered gyro signal for rest detection. */
    float rest_acc_lp[3];  /*!< Low-pass filtered accel signal for rest detection. */
    float bias_gyro_lp[3]; /*!< Low-pass filtered gyro signal for bias learning. */
} imu_quat_bias_state_t;

/**
 * @brief Internal gyro guard state.
 */
typedef struct {
    float gyro_over_limit_time_sec; /*!< Time accumulated above the guard limit. */
    float gyro_recover_time_sec;    /*!< Time accumulated below the guard recovery limit. */
    bool gyro_reinit_armed;         /*!< True when the next valid recovery should reinitialize the solver. */
} imu_quat_gyro_guard_state_t;

/**
 * @brief Internal accel/mag rejection state.
 */
typedef struct {
    uint16_t accel_reject_count; /*!< Consecutive accel rejection count. */
    uint16_t mag_reject_count;   /*!< Consecutive mag rejection count. */
    bool accel_ignored;          /*!< True when accel correction is ignored for the current frame. */
    bool mag_ignored;            /*!< True when mag correction is ignored for the current frame. */
    bool mag_error_lp_valid;     /*!< True when the magnetic yaw feedback LP state has been initialized. */
    float mag_error_lp[3];       /*!< Low-pass filtered magnetic yaw feedback vector. */
} imu_quat_rejection_state_t;

struct imu_quat_t {
    imu_quat_config_t config;
    imu_quat_pose_state_t pose;
    imu_quat_runtime_gain_t runtime_gain;
    imu_quat_bias_state_t bias;
    imu_quat_gyro_guard_state_t gyro_guard;
    imu_quat_rejection_state_t rejection;
};

void imu_quat_fill_output(imu_quat_handle_t handle, imu_quat_output_t *out);

esp_err_t imu_quat_complementary_update(imu_quat_handle_t handle, const imu_quat_sample_t *sample, float dt_sec);
