BLE MIDI 配置文件
==============================
:link_to_translation:`en:[English]`

BLE MIDI 配置文件允许设备通过蓝牙低功耗（Bluetooth Low Energy）进行
MIDI（乐器数字接口）消息的传输。该配置文件适用于电子乐器、MIDI 控制器、
音频设备以及移动应用等场景，能够在低功耗条件下实现低时延的数据通信。

BLE MIDI 支持标准 MIDI 消息的传输，包括音符事件、控制变化以及系统消息等，
并通过蓝牙低功耗技术，在保证通信实时性的同时，
有效降低了设备的功耗。

该配置文件遵循 MIDI 制造商协会（MIDI Manufacturers Association，MMA）
以及蓝牙技术联盟（Bluetooth SIG）定义的 MIDI over BLE 规范。

本仓库在 ``examples/bluetooth/ble_profiles/ble_midi/`` 目录下提供两个示例，分别覆盖两种链路角色：
``ble_midi_peripheral`` (GATT 服务端，经 Notification 向外发送 MIDI) 与
``ble_midi_central`` (GATT 客户端，订阅对端 MIDI I/O 特征实现 MIDI In)。
可在两块开发板上分别烧录以做对测。

示例
--------------

:example:`bluetooth/ble_profiles/ble_midi/ble_midi_peripheral`.
:example:`bluetooth/ble_profiles/ble_midi/ble_midi_central`.

API 参考
-----------------

.. include-build-file:: inc/esp_ble_midi.inc
