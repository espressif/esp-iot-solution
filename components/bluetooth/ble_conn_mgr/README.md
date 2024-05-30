## BLE Connection Management Component

[![Component Registry](https://components.espressif.com/components/espressif/ble_conn_mgr/badge.svg)](https://components.espressif.com/components/espressif/ble_conn_mgr)

- [User Guide](https://docs.espressif.com/projects/esp-iot-solution/en/latest/bluetooth/ble_conn_mgr.html)

The ``ble_conn_mgr`` is a BLE connection management component that provides a simplified API interface for accessing commonly used BLE functionalities. It supports various scenarios such as peripheral and central.

Any application layer profile, such as GAP, GATT, GATT characteristics, and services, can be executed on top of this layer.

> Note: only Nimble is supported, the support for Bluedroid will be added in later release version.

### Adding the component to your project

To add the `ble_conn_mgr` to your project's dependencies, please use the command `idf.py add-dependency`. During the `CMake` step, the component will be downloaded automatically.

```
idf.py add-dependency "espressif/ble_conn_mgr=*"
```

### Examples

To create a project using the example template, please use the command `idf.py create-project-from-example`.

* BLE SPP Central
```
idf.py create-project-from-example "espressif/ble_conn_mgr=*:spp_client"
```

* BLE SPP Peripheral
```
idf.py create-project-from-example "espressif/ble_conn_mgr=*:spp_server"
```

The example will be downloaded into the current folder. You can navigate to it for building and flashing.

> You can use this command to download other examples as well. Alternatively, you can download examples from the esp-iot-solution repository:
1. [ble_periodic_adv](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_conn_mgr/ble_periodic_adv)
2. [ble_periodic_sync](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_conn_mgr/ble_periodic_sync)
3. [ble_spp](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_conn_mgr/ble_spp)

### Q&A

Q1. I encountered the following problems when using the package manager

```
Executing action: create-project-from-example
CMakeLists.txt not found in project directory /home/username
```

A1. This is because an older version package manager was used, please run `pip install -U idf-component-manager` in ESP-IDF environment to update.
