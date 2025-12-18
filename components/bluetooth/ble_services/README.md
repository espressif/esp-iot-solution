## BLE Services Component

[![Component Registry](https://components.espressif.com/components/espressif/ble_services/badge.svg)](https://components.espressif.com/components/espressif/ble_services)

- [User Guide](https://docs.espressif.com/projects/espressif-esp-iot-solution/en/latest/bluetooth/ble_services.html)

The ``ble_services`` component provides a simplified API interface for accessing commonly used standard and custom BLE services functionality on a GATT server.

Included services:
- ANS, BAS, DIS, HRS, HTS, TPS, BCS, CTS, UDS, WSS, MIDI

### Adding the component to your project

Please use the component manager command `idf.py add-dependency` to add `ble_services` as a dependency to your project. The component will be downloaded automatically during the CMake step.

```
idf.py add-dependency "espressif/ble_services=*"
```

### Examples

To create a project from the example template, please use the component manager command `idf.py create-project-from-example`.

* BLE Device Information Service
```
idf.py create-project-from-example "espressif/ble_services=*:ble_dis"
```

The example will be downloaded to the current folder. You can navigate into it for building and flashing.

> You can use this command to download other examples. Or you can download examples from esp-iot-solution repository:
1. [ble_ans](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_services/ble_ans)
2. [ble_bas](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_services/ble_bas)
3. [ble_bcs](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_services/ble_bcs)
4. [ble_cts](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_services/ble_cts)
5. [ble_dis](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_services/ble_dis)
6. [ble_hrs](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_services/ble_hrs)
7. [ble_hts](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_services/ble_hts)
8. [ble_tps](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_services/ble_tps)
9. [ble_uds](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_services/ble_uds)
10. [ble_wss](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_services/ble_wss)
11. [ble_midi](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_services/ble_midi)

### Q&A

Q1. I encountered the following problems when using the package manager

```
Executing action: create-project-from-example
CMakeLists.txt not found in project directory /home/username
```

A1. This is because an older version package manager was used, please run `pip install -U idf-component-manager` in ESP-IDF environment to update.
