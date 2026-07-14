# imu_gesture

## Overview

`imu_gesture` is a reusable ESP-IDF component for runtime IMU gesture detection.

The component consumes timestamped IMU samples and emits normalized gesture events through a unified detector interface.

The current component includes three detector types:

- knob detector
- space-switch detector
- inference detector

## Features

- Unified detector lifecycle and callback interface
- Internal sample queue per detector
- Realtime queue-driven processing for knob, space-switch, and inference detectors
- Application-owned one-shot inference capture flow
- Detector-specific default macros in public headers instead of component `Kconfig`

## Component Scope

The component boundary is intentionally narrow.

- Input: one `imu_gesture_sample_t` at a time
- Output: `imu_gesture_event_t` callback events
- Application-owned responsibilities: task scheduling, detector priority, business mapping, realtime divider policy, one-shot capture lifecycle, and inference confirm/reset logic

## Requirements

- ESP-IDF `>= 5.0`

## Directory Structure

- `include/`: public headers
- `private_include/`: internal private headers
- `src/`: detector implementations and shared detector runtime
- `examples/`: short runnable detector examples for knob, space-switch, and inference, each using offline input and local leak checks

## Public Headers

- `imu_gesture_types.h`: shared detector handle, sample, event, axis, and callback types
- `imu_gesture.h`: common detector queue, processing, reset, delete, and callback APIs
- `imu_gesture_knob.h`: knob detector config, defaults, and create API
- `imu_gesture_space_switch.h`: space-switch detector config, defaults, and create API
- `imu_gesture_inference.h`: inference model/config/result types and inference detector APIs

## Public APIs

Detector creation:

- `imu_gesture_knob_detector_create()`
- `imu_gesture_space_switch_detector_create()`
- `imu_gesture_inference_detector_create()`

Common detector operations:

- `imu_gesture_detector_push_sample()`
- `imu_gesture_detector_process_pending()`
- `imu_gesture_detector_reset()`
- `imu_gesture_detector_del()`
- `imu_gesture_detector_register_cb()`
- `imu_gesture_detector_unregister_cb()`

Inference-only helpers:

- `imu_gesture_inference_detector_get_last_result()`
- `imu_gesture_inference_detector_process_single_shot_buffer()`

## Processing Model

Each detector owns its own internal sample queue.

`imu_gesture_detector_push_sample()` only enqueues one sample.

`imu_gesture_detector_process_pending()` drains queued samples for that detector and runs the detector logic.

If the queue is full, the oldest sample is dropped and the newest sample is kept.

Knob and space-switch detectors are fully driven through `push_sample() + process_pending()`.

Inference detectors use two paths:

- realtime inference: `push_sample() + process_pending()`
- one-shot inference: `imu_gesture_inference_detector_process_single_shot_buffer()`

## Inference Model

The inference detector separates compile-time model facts from runtime policy.

`imu_gesture_inference_model_t` describes the model shape and callbacks:

- `input_length`
- `input_channels`
- `output_count`
- `model_init`
- `model_predict`

`imu_gesture_inference_config_t` adds runtime detector policy:

- `model`
- `window_step`
- `sample_queue_len`

Applications still feed one `imu_gesture_sample_t` at a time in time order.

The detector owns internal window assembly and passes the time-ordered sample
window directly to `model_predict()` flattened in `[L, C]` order.

The detector caches the latest successful inference result, and applications can read it through `imu_gesture_inference_detector_get_last_result()`.

## Defaults

The component does not provide a detector `Kconfig` menu.

Built-in detector defaults live in the detector-specific public headers as `IMU_GESTURE_*_DEFAULT_*` macros.

Applications may use those macros directly or fill config structures explicitly.

## How To Use

Create detectors:

```c
#include "imu_gesture.h"
#include "imu_gesture_knob.h"
#include "imu_gesture_space_switch.h"

imu_gesture_detector_handle_t knob = NULL;
imu_gesture_detector_handle_t space_switch = NULL;

imu_gesture_knob_config_t knob_config = IMU_GESTURE_KNOB_DEFAULT_CONFIG();
imu_gesture_space_switch_config_t space_switch_config =
    IMU_GESTURE_SPACE_SWITCH_DEFAULT_CONFIG();

ESP_ERROR_CHECK(imu_gesture_knob_detector_create(&knob_config, &knob));
ESP_ERROR_CHECK(
    imu_gesture_space_switch_detector_create(&space_switch_config, &space_switch));
```

Push one sample and process pending data:

```c
imu_gesture_sample_t sample = {
    .accel = {0},
    .gyro = {0},
    .timestamp_us = timestamp_us,
};

ESP_ERROR_CHECK(imu_gesture_detector_push_sample(knob, &sample));

size_t processed = 0;
ESP_ERROR_CHECK(imu_gesture_detector_process_pending(knob, &processed));
```

Register one callback on a detector:

```c
static void my_cb(imu_gesture_detector_handle_t detector,
                  imu_gesture_event_t event,
                  void *user_data)
{
    // Handle detector event here.
}

ESP_ERROR_CHECK(imu_gesture_detector_register_cb(knob, my_cb, NULL));
```

Run one-shot inference from an application-owned capture buffer:

```c
ESP_ERROR_CHECK(imu_gesture_detector_reset(detector));

if (capture_count == INPUT_LENGTH) {
    ESP_ERROR_CHECK(
        imu_gesture_inference_detector_process_single_shot_buffer(
            detector,
            capture_buffer,
            capture_count));
}
```

## Notes

Knob direction is determined from the configured primary gyro axis and `cw_sign`.

Knob posture gating uses the absolute value of the computed plane angle, and the `atan2(accel[first], accel[second])` axis pair can be configured explicitly.

For one-shot inference, the application owns capture start/stop policy and buffer lifetime. The detector only evaluates the provided full sample window.

## Examples

The component now keeps its detector-behavior demos under `examples/` instead
of a component-local behavior test app.

- `examples/knob/`
  Offline recorded knob replay example with a leak check.
- `examples/space_switch/`
  Offline recorded space-switch replay example with a leak check.
- `examples/inference/`
  Minimal inference detector example with copied offline model/test input and a leak check.
