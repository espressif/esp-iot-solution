## BLE Services Component

[![Component Registry](https://components.espressif.com/components/espressif/ble_services/badge.svg)](https://components.espressif.com/components/espressif/ble_services)

- [User Guide](https://docs.espressif.com/projects/espressif-esp-iot-solution/en/latest/bluetooth/ble_services.html)

The ``ble_services`` component provides a simplified API interface for accessing commonly used standard and custom BLE services functionality on a GATT server.

Included services:
- ANS, BAS, BCS, CTS, DIS, ESL, HRS, HTS, IAS, MIDI ,OTA ,OTS, TPS, UDS, WSS

### Adding the component to your project

Please use the component manager command `idf.py add-dependency` to add `ble_services` as a dependency to your project. The component will be downloaded automatically during the CMake step.

```
idf.py add-dependency "espressif/ble_services=*"
```

_Note:_ This component requires the [``ble_conn_mgr``](../ble_conn_mgr) component to function.

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
8. [ble_ias](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_services/ble_ias)
9. [ble_midi_peripheral](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_profiles/ble_midi/ble_midi_peripheral), [ble_midi_central](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_profiles/ble_midi/ble_midi_central) (BLE‑MIDI profile examples; `ble_midi` component)
10. [ble_ota](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_services/ble_ota)
11. [ble_ots](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_services/ble_ots)
12. [ble_tps](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_services/ble_tps)
13. [ble_uds](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_services/ble_uds)
14. [ble_wss](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_services/ble_wss)

### Q&A

Q1. I encountered the following problems when using the package manager

```
Executing action: create-project-from-example
CMakeLists.txt not found in project directory /home/username
```

A1. This is because an older version package manager was used, please run `pip install -U idf-component-manager` in ESP-IDF environment to update.
