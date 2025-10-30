# BTHome Component Test Application

This directory contains test applications for the BTHome component.

## Test Structure

- `main/bthome_test.c` - Main test file containing unit tests for BTHome functionality
- `pytest_bthome.py` - Pytest configuration for automated testing
- `sdkconfig.defaults` - Default configuration for the test application

## Test Cases

The test application includes the following test cases:

1. **bthome_create_delete** - Tests basic object creation and deletion
2. **bthome_encryption_config** - Tests encryption key and MAC address configuration
3. **bthome_payload_creation** - Tests sensor data payload creation
4. **bthome_binary_sensor_data** - Tests binary sensor data handling
5. **bthome_event_data** - Tests event data handling
6. **bthome_adv_data_creation** - Tests advertisement data creation
7. **bthome_adv_data_parsing** - Tests advertisement data parsing
8. **bthome_memory_management** - Tests memory management and leak detection
9. **bthome_error_handling** - Tests error handling and edge cases

## Building and Running

To build the test application:

```bash
cd components/bluetooth/ble_adv/bthome/test_apps
idf.py build
```

To run the tests:

```bash
idf.py monitor
```

## CI Integration

The test application is integrated into the CI pipeline and will run automatically when:
- BTHome component code is modified
- The test application itself is modified
- Manual CI trigger

The tests run on multiple ESP32 variants (ESP32, ESP32-S3, ESP32-C3) and IDF versions (4.4, 5.0, 5.1, 5.2).
