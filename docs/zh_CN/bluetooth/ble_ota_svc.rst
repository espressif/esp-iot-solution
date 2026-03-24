BLE OTA 服务
==============================
:link_to_translation:`en:[English]`

BLE OTA 服务定义了 GATT 服务 ``0x8018`` 及以下特征值：

- ``0x8020`` (RECV_FW)：固件数据通道与固件 ACK 通知
- ``0x8021`` (PROGRESS_BAR)：升级进度（可读、可通知）
- ``0x8022`` (COMMAND)：OTA 命令通道与命令 ACK 通知
- ``0x8023`` (CUSTOMER)：厂商自定义数据通道

该服务属于传输层能力，可被不同 OTA 配置文件复用。
与 ``ble_ota_raw`` 搭配时，命令和固件数据解析由该配置文件负责。

示例
--------------

:example:`bluetooth/ble_profiles/ble_ota`.

API 参考
-----------------

.. include-build-file:: inc/esp_ble_ota_svc.inc
