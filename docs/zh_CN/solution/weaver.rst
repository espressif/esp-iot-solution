ESP Weaver
============
:link_to_translation:`en:[English]`

==================  ===========  ===============  ===============  ===============  ===============  ================  =============
支持的芯片            ESP32        ESP32-C2         ESP32-C3         ESP32-C5         ESP32-C6         ESP32-C61         ESP32-S3
==================  ===========  ===============  ===============  ===============  ===============  ================  =============

ESP Weaver 是一个用于构建智能家居设备的设备端 SDK，支持 Home Assistant 本地发现和控制。基于 ESP-IDF 的 esp_local_ctrl 组件和 mDNS 服务发现，实现毫秒级的局域网内通信，无需依赖云服务。

该 SDK 提供了简单的设备/参数模型，支持设备创建、参数管理、写入回调和参数实时推送。支持 Security1 协议（Curve25519 + AES-CTR）并结合拥有证明 (PoP) 实现安全通信。

功能特性
--------------

- 基于 mDNS + esp_local_ctrl 的低延迟本地控制
- 离线可用，无需互联网连接
- 基于拥有证明 (PoP) 的端到端加密
- 零配置设备发现
- 简洁的设备/参数模型，用于智能家居集成
- 兼容 `ESP-Weaver <https://github.com/espressif/esp_weaver>`_ Home Assistant 自定义集成

架构
--------------

.. code-block:: text

    +-------------------+    mDNS Discovery    +-------------------+
    |                   | <------------------- |                   |
    |  Home Assistant   |                      |    ESP Device     |
    |  (ESP-Weaver)     | -------------------> |   (ESP Weaver)    |
    |                   |    Local Control     |                   |
    +-------------------+                      +-------------------+
             |                                          |
             |               Local Network              |
             +------------------------------------------+

1. ESP 设备启动后连接 WiFi 并通过 mDNS 广播服务
2. Home Assistant 中的 ESP-Weaver 集成发现设备
3. 用户输入设备的 PoP 码完成配对
4. 通过 Local Control 协议进行双向通信

示例
--------------

1. LED 灯示例: :example:`weaver/led_light`。RGB LED 灯控制，支持电源、亮度、HSV 颜色和色温。

API 参考
-----------------

.. include-build-file:: inc/esp_weaver.inc

