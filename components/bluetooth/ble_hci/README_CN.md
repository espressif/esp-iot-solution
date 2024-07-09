# BLE HCI 组件

[![Component Registry](https://components.espressif.com/components/espressif/ble_hci/badge.svg)](https://components.espressif.com/components/espressif/ble_hci)

- [User Guide](https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/bluetooth/ble_hci.html)

BLE HCI 组件用于通过 VHCI 接口直接操作 BLE Controller 实现广播,扫描等功能。
相比于通过 Nimble 或 Bluedroid 协议栈发起广播和扫描，使用该组件有如下优点：
- 更少的内存占用
- 更小的固件尺寸
- 更快的初始化流程

## 支持指令列表
- 发送广播包
- 扫描广播包
- 白名单
- 设置本地地址
