BLE OTA 配置文件
==============================
:link_to_translation:`en:[English]`

BLE OTA 配置文件基于 ``esp_ble_conn_mgr`` 提供分扇区固件传输能力。
该配置文件使用 OTA 服务 UUID ``0x8018``，并处理主机发送的 START/STOP 命令与固件数据包。

该配置文件会校验：

- 命令包 CRC16
- 扇区序号与分包序号连续性
- 扇区 CRC16（通过后才将数据回调给应用）

示例
--------------

:example:`bluetooth/ble_profiles/ble_ota`.

API 参考
-----------------

.. include-build-file:: inc/esp_ble_ota_raw.inc
