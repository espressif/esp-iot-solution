# IMU Gesture Knob Example

This example verifies the `imu_gesture` knob detector with offline recorded data replay.

It keeps the runtime path short and board-independent:

- one knob detector
- one callback
- one copied replay dataset
- one repeated create/delete memory leak check

The replay data comes from `framework/tools/imu_data_collection/test_imu_gesture_data/test_imu_knob_data/dataset_raw/session_20260714_154839.csv`. Labels `1` and `2` both mark knob-action intervals, and this example only checks that each labeled interval produces at least one knob event without assuming direction.

## Build

```bash
cd common_components/imu_gesture/examples/knob
idf.py build
```

## Expected Result

The serial log first runs a memory leak check, then replays the recorded knob sequence and aborts if any labeled action interval fails to produce a knob event.
