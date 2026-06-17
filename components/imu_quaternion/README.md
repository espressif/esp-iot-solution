# imu_quaternion

## Overview

`imu_quaternion` is a reusable ESP-IDF component that maintains a body-to-world quaternion from IMU accel, gyro, and timestamp input samples. The component only owns orientation solving, runtime reinitialization, gyro bias learning, and gyro over-limit protection. Application-layer behaviors such as cursor mapping, screen projection, HID reports, and gesture policies stay outside the component.

## Features

- Initialize orientation either from the startup pose directly or from accel plus heading alignment constraints.
- Update quaternion state from streaming IMU samples.
- Learn gyro bias during detected rest periods.
- Reinitialize after guarded gyro over-limit and recovery conditions.
- Rotate vectors between body frame and world frame with the current quaternion state.

## Requirements

- ESP-IDF `>=5.0`
- One caller-managed IMU sampling path that provides `accel[3]`, `gyro[3]`, and `timestamp_us`

## Directory Structure

- `include/imu_quaternion.h`: public API and public configuration/data types
- `private_include/`: internal runtime state and solver helper declarations
- `src/`: solver core, math helpers, initialization, bias learning, and gyro guard implementation
- `test/`: Unity tests for default behavior, initialization constraints, bias-disable behavior, and gyro-guard behavior

## Public APIs

- `imu_quat_create()`: create one solver instance
- `imu_quat_delete()`: destroy one solver instance
- `imu_quat_set_config()`: replace the stored runtime configuration
- `imu_quat_reset_state()`: clear runtime state while keeping the handle and configuration alive
- `imu_quat_update()`: process one IMU sample and return the latest solver output
- `imu_quat_rotate_body_to_world()`: rotate one body-frame vector into world frame
- `imu_quat_rotate_world_to_body()`: rotate one world-frame vector into body frame

## Configuration Model

`imu_quat_config_t` keeps only the configuration items that affect component behavior at the public boundary:

- `init_strategy`
- `accel_target_axis`
- `heading_ref_body[3]`
- `heading_target_axis`
- `gyro_bias_enabled`
- `gyro_guard_enabled`
- `gyro_guard_limit_dps`

The detailed warmup gain, rest-detection windows, bias-learning gains, and gyro-guard recovery windows are internal implementation parameters and are not part of the public configuration surface.

`accel_target_axis` selects which world axis the measured accel direction should align to during initialization. `heading_ref_body[3]` selects the body-frame reference direction used to define startup heading. `heading_target_axis` selects which world axis the projected heading reference should align to. `accel_target_axis` and `heading_target_axis` must be orthogonal. If accel-heading initialization cannot build a valid pose frame, `imu_quat_update()` returns `ESP_ERR_INVALID_STATE`.

`IMU_QUAT_INIT_CURRENT_POSE` keeps the startup body frame as the initial world reference. `IMU_QUAT_INIT_ACCEL_HEADING_ALIGNMENT` instead constructs a body-to-world frame from the measured accel direction plus the configured startup heading constraint.

## How To Use

```c
#include "imu_quaternion.h"

imu_quat_config_t config = {
    .init_strategy = IMU_QUAT_INIT_ACCEL_HEADING_ALIGNMENT,
    .accel_target_axis = IMU_QUAT_AXIS_POS_Z,
    .heading_ref_body = {-1.0f, 0.0f, 0.0f},
    .heading_target_axis = IMU_QUAT_AXIS_NEG_X,
    .gyro_bias_enabled = true,
    .gyro_guard_enabled = true,
    .gyro_guard_limit_dps = 150.0f,
};

imu_quat_handle_t handle = NULL;
imu_quat_output_t out = {0};

ESP_ERROR_CHECK(imu_quat_create(&config, &handle));

imu_quat_sample_t sample = {
    .accel = {0.0f, 0.0f, 1.0f},
    .gyro = {0.0f, 0.0f, 0.0f},
    .timestamp_us = 1000,
};

ESP_ERROR_CHECK(imu_quat_update(handle, &sample, &out));
ESP_ERROR_CHECK(imu_quat_delete(handle));
```

## Notes

- `updated` means the current call completed a normal continuous update step.
- `reinitialized` means the current call rebuilt solver state through guarded runtime reinitialization.
- `rest_detected` reflects the internal bias learner state, not a separate application event.
- The current default alignment remains compatible with the AirMouse quaternion pose path: measured accel aligns to world `+Z`, and the default startup heading reference aligns to world `-X`.
