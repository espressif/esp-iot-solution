# IMU Gesture Inference Example

This example verifies the `imu_gesture` inference detector with offline test input.

It reuses the exported `components/inference/` input/model files copied from
`framework/examples/imu_inference_test/` and adds a detector memory leak check
before running realtime and one-shot inference demos.

## Build

```bash
cd common_components/imu_gesture/examples/inference
idf.py build
```

## Expected Result

The serial log first runs a memory leak check, then prints realtime and
single-shot inference results from the copied test tensor.
