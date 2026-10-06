电子货架标签服务
==============================
:link_to_translation:`en:[English]`

**电子货架标签服务** (Electronic Shelf Label Service, ESL) 是蓝牙 SIG 定义的 GATT 服务 (UUID ``0x1857``)，用于 ESL Tag。它提供配置类特性（地址、密钥材料、绝对时间）、可选能力信息（显示 / 图像 / 传感器 / LED），以及通过 ACL 收发命令与响应的 ESL Control Point。

在应用中使用本服务时，建议按以下步骤操作：

#. 在 menuconfig 中启用 ``CONFIG_BLE_ESL`` (可选特性由相关 Kconfig 控制)。
#. 使用 ``esp_ble_conn_init()`` 初始化 BLE 连接管理器。
#. 在 ``esp_ble_conn_start()`` 之前调用 ``esp_ble_esl_init()`` 注册 ESL 服务。
#. 可选用 ``esp_ble_esl_set_display_info()``、``esp_ble_esl_set_image_info()`` 等接口设置本机能力信息。
#. 订阅 ``BLE_ESL_EVENTS`` (事件 ID 为被写入特性的 UUID)，处理地址、密钥、绝对时间、ECP 命令等写入。
#. 通过 ``esp_ble_esl_notify_ecp_response()`` 发送 ECP 响应。

本服务暂无独立示例工程，初始化方式可参考其它 :doc:`BLE 服务 <ble_services>` 示例。

API 参考
-----------------

.. include-build-file:: inc/esp_esl.inc
