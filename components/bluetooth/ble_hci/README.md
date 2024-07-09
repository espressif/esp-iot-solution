# BLE HCI Component
[中文版](./README_CN.md)

[![Component Registry](https://components.espressif.com/components/espressif/ble_hci/badge.svg)](https://components.espressif.com/components/espressif/ble_hci)

- [User Guide](https://docs.espressif.com/projects/esp-iot-solution/en/latest/bluetooth/ble_hci.html)

The ``ble_hci`` is used to operate the BLE Controller directly through the VHCI interface to realize broadcasting, scanning and other functions.

Compared to initiating broadcasts and scans through the Nimble or Bluedroid stacks, using this component has the following advantages:
- Smaller memory footprint
- Smaller firmware size
- Faster initialization process

## List of supported commands

- Send broadcast packets
- Scan broadcast packets
- Add/Remove whitelist
- Set local address

## Adding the component to your project

To add the `ble_hci` to your project's dependencies, please use the command `idf.py add-dependency`. During the `CMake` step, the component will be downloaded automatically.

```
idf.py add-dependency "espressif/ble_hci=*"
```
