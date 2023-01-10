# ESP BLE OTA 组件说明

``esp ble ota`` 是基于自定义 ``BLE Services`` 进行数据收发的固件升级组件，待升级固件会被主机分包之后依次传输，从机接收到数据之后进行包序列以及 CRC 校验并回复 ACK。

## 从机示例代码

[ESP BLE OTA](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_ota)
