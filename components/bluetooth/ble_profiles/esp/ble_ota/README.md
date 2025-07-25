# ESP BLE OTA Component description

``esp ble ota`` is a firmware upgrade component for data sending and receiving based on customized BLE Services. The firmware to be upgraded will be subcontracted by the client and transmitted sequentially. After receiving data from the client, packet sequence and CRC check will be checked and ACK will be returned.

## Example

- [examples/bluetooth/ble_ota](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_ota): This example is based on the ble_ota component, upgrade firmware via BLE.

You can create a project from this example by the following command:

```
idf.py create-project-from-example "espressif/ble_ota^0.1.8:ble_ota"
```

> ![TIP]
>
> When using NimBLE, it may be necessary to increase `CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE`。
