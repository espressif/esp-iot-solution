# IMU Gesture Space Switch Example

This example verifies the `imu_gesture` space-switch detector with offline replay data.

It replays copied dataset samples and checks two things:

- repeated create/delete memory leak behavior
- each labeled action segment produces at least one matching direction event

The replay data comes from:

- `framework/tools/imu_data_collection/test_imu_gesture_data/dataset_raw/session_20260714_153559.csv`

The copied test data uses the first six channels as `ax, ay, az, gx, gy, gz`.
`label 1/2/3/4` correspond to `up/down/left/right` action segments.

## Build

```bash
cd common_components/imu_gesture/examples/space_switch
idf.py build
```

## Expected Result

The serial log first runs a memory leak check, then replays the copied dataset and reports whether the four labeled action segments were each hit at least once.
