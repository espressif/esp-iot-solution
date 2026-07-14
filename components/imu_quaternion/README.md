# imu_quaternion

## Overview

`imu_quaternion` is a reusable ESP-IDF component that maintains a body-to-world quaternion from IMU accel, gyro, and timestamp input samples. The public sample structure can also carry an optional calibrated body-frame magnetic sample. The solver keeps the same gravity-plus-heading initialization semantics as before, then applies a small yaw-only magnetic correction during continuous updates when a valid magnetic reference is available. Application-layer behaviors such as cursor mapping, screen projection, HID reports, and gesture policies stay outside the component.

## Features

- Initialize orientation through a unified gravity-plus-heading alignment path.
- Update quaternion state from streaming IMU samples.
- Apply optional yaw-only magnetic drift correction during continuous updates.
- Learn gyro bias during detected rest periods.
- Reinitialize after guarded gyro over-limit and recovery conditions.
- Rotate vectors between body frame and world frame with the current quaternion state.

## Requirements

- ESP-IDF `>=5.0`
- One caller-managed IMU sampling path that provides `accel[3]`, `gyro[3]`, and `timestamp_us`
- Optional calibrated body-frame magnetic samples may also be supplied through `mag_valid` and `mag[3]`

## Directory Structure

- `include/imu_quaternion.h`: public API and public configuration/data types
- `private_include/`: internal runtime state and solver helper declarations
- `src/`: solver core, math helpers, initialization, bias learning, and gyro guard implementation
- `examples/`: offline runnable replay example for recorded-data validation

## Public APIs

- `imu_quat_create()`: create one solver instance
- `imu_quat_delete()`: destroy one solver instance
- `imu_quat_set_config()`: replace the stored runtime configuration
- `imu_quat_reset_state()`: clear runtime state while keeping the handle and configuration alive
- `imu_quat_set_gyro_bias_dps()`: inject a caller-provided initial gyro bias estimate in deg/s
- `imu_quat_get_gyro_bias_dps()`: read back the current gyro bias estimate in deg/s
- `imu_quat_reinitialize_from_sample()`: explicitly rebuild the world frame from one IMU sample
- `imu_quat_update()`: process one IMU sample and return the latest solver output
- `imu_quat_rotate_body_to_world()`: rotate one body-frame vector into world frame
- `imu_quat_rotate_world_to_body()`: rotate one world-frame vector into body frame

## Configuration Model

`imu_quat_config_t` keeps only the configuration items that affect component behavior at the public boundary:

- `accel_target_axis`
- `heading_ref_body[3]`
- `heading_target_axis`
- `mag_input_enabled`
- `gyro_bias_enabled`
- `gyro_guard_enabled`
- `gyro_guard_limit_dps`

The detailed warmup gain, rest-detection windows, bias-learning gains, and gyro-guard recovery windows are internal implementation parameters and are not part of the public configuration surface.

`accel_target_axis` selects which world axis the measured accel direction should align to during initialization. `heading_ref_body[3]` selects the body-frame heading direction that always defines the startup zero pose. `heading_target_axis` selects which world axis the projected heading reference should align to. `accel_target_axis` and `heading_target_axis` must be orthogonal. Call `imu_quat_reinitialize_from_sample()` to explicitly establish world frame before the first normal `imu_quat_update()`.

`gyro_bias_enabled` controls only runtime learning. When it is `true`, the solver keeps updating gyro bias during detected rest periods. When it is `false`, the solver keeps whatever bias it currently has, which may be a caller-injected startup bias or the default zero bias.

## How To Use

```c
#include "imu_quaternion.h"

imu_quat_config_t config = {
    .accel_target_axis = IMU_QUAT_AXIS_POS_Z,
    .heading_ref_body = {-1.0f, 0.0f, 0.0f},
    .heading_target_axis = IMU_QUAT_AXIS_NEG_X,
    .mag_input_enabled = false,
    .gyro_bias_enabled = true,
    .gyro_guard_enabled = true,
    .gyro_guard_limit_dps = 150.0f,
};

imu_quat_handle_t handle = NULL;
imu_quat_output_t out = {0};

ESP_ERROR_CHECK(imu_quat_create(&config, &handle));
ESP_ERROR_CHECK(imu_quat_set_gyro_bias_dps(
    handle,
    (float[3]){0.02f, -0.01f, 0.00f}));

imu_quat_sample_t first = {
    .accel = {0.0f, 0.0f, 1.0f},
    .gyro = {0.0f, 0.0f, 0.0f},
    .mag_valid = false,
    .timestamp_us = 1000,
};

imu_quat_sample_t second = first;
second.timestamp_us = 11000;

ESP_ERROR_CHECK(imu_quat_reinitialize_from_sample(handle, &first));
ESP_ERROR_CHECK(imu_quat_update(handle, &second, &out));
ESP_ERROR_CHECK(imu_quat_delete(handle));
```

## Notes

- `updated` means the current call completed a normal continuous update step.
- `reinitialized` means the current call rebuilt solver state through guarded runtime reinitialization.
- `rest_detected` reflects the internal bias learner state, not a separate application event.
- The current default alignment remains compatible with the AirMouse quaternion pose path: measured accel aligns to world `+Z`, and the default startup heading reference aligns to world `-X`.
- Initialization always uses `heading_ref_body[3]` as the heading source, so the device's configured forward direction defines the startup zero pose.
- When `mag_input_enabled` is true and the current sample carries a valid calibrated magnetic field, the solver records an initial horizontal magnetic reference in world frame for later use.
- Continuous updates still use the same gravity-plus-heading initialization semantics. Magnetic input does not redefine the startup zero pose.
- During normal updates, magnetic correction is optional, yaw-only, and guarded by an internal rejection path separate from accel rejection. The current implementation feeds the yaw-only magnetic error back directly with a relatively aggressive gain so motion-time heading recovery can be evaluated clearly. If magnetic input is unavailable or rejected, the solver falls back to the accel/gyro path for that frame.
- `imu_quat_set_gyro_bias_dps()` is intended for startup/restored bias injection. Runtime rest-based bias learning can stay enabled and continue refining the estimate afterward.
- `imu_quat_reset_state()` only clears solver runtime state. It does not by itself redefine the world frame.
- `imu_quat_update()` is now a pure continuous-update API. It requires a prior successful `imu_quat_reinitialize_from_sample()` after create/reset before normal updates can proceed.

## Examples

The component now keeps short offline demos under `examples/` instead of the
previous component-local Unity behavior test app.

- `examples/replay/`
  Recorded six-axis replay demo using copied CSV data plus a local leak check.
