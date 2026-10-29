# ESP Xiaozhi Component Test Application

This directory contains test applications for the ESP Xiaozhi component.

## Test Structure

- `main/test_esp_xiaozhi.c` - Main test file containing unit tests for ESP Xiaozhi functionality
- `sdkconfig.defaults` - Default configuration for the test application

## Test Cases

The test application includes the following test cases:

1. **esp_xiaozhi_init_deinit** - Tests basic component initialization and deinitialization
2. **esp_xiaozhi_init_null_config** - Tests error handling with NULL config
3. **esp_xiaozhi_init_null_handle** - Tests error handling with NULL handle
4. **esp_xiaozhi_deinit_null_handle** - Tests error handling with NULL handle in deinit

## Building and Running

To build the test application:

```bash
cd components/esp_xiaozhi/test_apps
idf.py build
```

To run the tests:

```bash
idf.py -p <PORT> flash monitor
```

Then press **ENTER** to open the test menu, and type a number or `*` to run tests.

If you see **"Writing to serial is timing out"**: ensure the monitor is connected to the same port used as the application console. For ESP32-S2/S3/C2/C6 boards with a single USB cable, `sdkconfig.defaults` sets the console to **USB Serial/JTAG** so the same port works for both flashing and the interactive menu. After changing `sdkconfig.defaults`, run `idf.py fullclean && idf.py build` so the new console setting takes effect.

## CI Integration

The test application can be integrated into the CI pipeline and will run automatically when:
- ESP Xiaozhi component code is modified
- The test application itself is modified
- Manual CI trigger

## Notes

- All tests use Unity test framework
- Tests are designed to be run on actual hardware or QEMU
- Memory leak detection is enabled for all tests
- Tests verify proper resource cleanup

