# IMU Quaternion Replay Example

This example replays offline six-axis IMU data through `imu_quaternion`.

The sample values come from:

- `framework/tools/imu_data_collection/test_new_collect/dataset_raw/session_20260710_153900.csv`

Only the first six channels are used:

- `ax`
- `ay`
- `az`
- `gx`
- `gy`
- `gz`

The `label` column is ignored.

## Build

```bash
cd components/imu_quaternion/test_apps/replay
idf.py build
```

## Expected Result

The serial log first runs a memory leak check, then replays the copied dataset with a fixed synthetic timestamp step, prints intermediate quaternion pose updates at a fixed interval, and finally prints the last replay pose state.
