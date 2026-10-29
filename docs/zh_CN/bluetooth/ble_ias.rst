立即警报服务
==============================
:link_to_translation:`en:[English]`

**立即警报服务**（Immediate Alert Service, IAS）是蓝牙 SIG 定义的 GATT 服务，通过 **Alert Level** 特性使已连接的对端可请求本机立即发出提示（例如查找外设时的声音或振动）。

Alert Level 为单字节，取值对应 **无警报**、**弱警报**、**强警报**，即 ``ESP_BLE_IAS_ALERT_LEVEL_NO_ALERT``、``ESP_BLE_IAS_ALERT_LEVEL_MILD_ALERT``、``ESP_BLE_IAS_ALERT_LEVEL_HIGH_ALERT``（分别为 ``0x00``、``0x01``、``0x02``）。

在应用中使用本服务时，建议按以下步骤操作：

#. 在 menuconfig 中启用 ``CONFIG_BLE_IAS``。
#. 使用 ``esp_ble_conn_init()`` 初始化 BLE 连接管理器（若工程需要，请先完成默认事件循环等相关准备）。
#. 在连接管理器完成初始化之后，调用 ``esp_ble_ias_init()`` 将 IAS 注册到 GATT 服务端；在常见示例流程中，请在 ``esp_ble_conn_start()`` 之前执行该注册。
#. 在 ``esp_ble_ias_init()`` **之后** 再调用 ``esp_ble_ias_register_alert_cb()``，以便先完成服务注册，再安装在对端写入合法 Alert Level 时触发的处理函数。

本服务暂无独立示例工程，初始化方式可参考其它 :doc:`BLE 服务 <ble_services>` 示例。

API 参考
-----------------

.. include-build-file:: inc/esp_ias.inc
