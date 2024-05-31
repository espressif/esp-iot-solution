## Alert Notification Profile Component

[![Component Registry](https://components.espressif.com/components/espressif/ble_anp/badge.svg)](https://components.espressif.com/components/espressif/ble_anp)

- [User Guide](https://docs.espressif.com/projects/espressif-esp-iot-solution/en/latest/bluetooth/ble_profiles.html)

``ble_anp`` is a component which provide a simplified API interface for accessing the commonly used BLE alert notification profile functionality on a GATT Client.

### Add component to your project

Please use the component manager command `add-dependency` to add the `ble_anp` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/ble_anp=*"
```

### Examples

Please use the component manager command `create-project-from-example` to create the project from example template

```
idf.py create-project-from-example "espressif/ble_anp=*:ble_anp"
```

Then the example will be downloaded in current folder, you can check into it for build and flash.

> You can use this command to download other examples. Or you can download examples from esp-iot-solution repository:
1. [ble_anp](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_profiles/ble_anp)

### Q&A

Q1. I encountered the following problems when using the package manager

```
Executing action: create-project-from-example
CMakeLists.txt not found in project directory /home/username
```

A1. This is because an older version package manager was used, please run `pip install -U idf-component-manager` in ESP-IDF environment to update.
