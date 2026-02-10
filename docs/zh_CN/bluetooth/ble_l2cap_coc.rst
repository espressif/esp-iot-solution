BLE L2CAP 面向连接的通道 (CoC)
=============================================
:link_to_translation:`en:[English]`

==================  ===========  ===============  =============== =============
支持的芯片            ESP32        ESP32-C2          ESP32-C3        ESP32-S3
==================  ===========  ===============  =============== =============

L2CAP 面向连接的通道（CoC）是一种 BLE 功能，可在 BLE 设备之间实现可靠的、
面向流的数据传输。与基于属性协议的 GATT 不同，L2CAP CoC 提供了类似 TCP
的面向连接的通道模型，支持更大数据量的传输和流控制。

L2CAP CoC 的主要特性：

- **面向连接**：在设备之间建立专用通道
- **流控制**：通过控制传输速率防止数据丢失
- **更大的 MTU**：支持最大 512 字节的 MTU（可配置）
- **双向通信**：在已建立的通道上全双工通信
- **多通道**：支持多个并发的 CoC 通道

L2CAP CoC 特别适用于以下应用场景：

- 大文件传输
- 音频/视频流传输
- 批量数据同步
- 任何需要可靠的、面向流的通信的应用

应用示例
---------------------------

PSM（协议/服务复用器）值可通过 Kconfig 进行配置。在示例项目中，可在 ``Example Configuration → L2CAP CoC PSM`` 菜单中设置。
有效范围：0x0001-0x00FF (1-255)。

- **0x0001-0x007F**：固定 Bluetooth SIG 定义的服务
- **0x0080-0x00FF**：动态自定义服务

中央设备和外围设备必须使用相同的 PSM 值才能建立连接。

 .. code-block:: c

    // PSM 值通过 Kconfig 配置 (CONFIG_EXAMPLE_L2CAP_COC_PSM)
    // 默认值：0x00EF (239)
    #define L2CAP_COC_PSM CONFIG_EXAMPLE_L2CAP_COC_PSM
    #define L2CAP_COC_MTU CONFIG_BLE_CONN_MGR_L2CAP_COC_MTU  // 最大传输单元

    // 初始化 L2CAP CoC 内存池
    ESP_ERROR_CHECK(esp_ble_conn_l2cap_coc_mem_init());

    // 创建 L2CAP CoC 服务端
    // app_ble_conn_l2cap_coc_event_handler 是你的回调函数
    esp_err_t rc = esp_ble_conn_l2cap_coc_create_server(
        L2CAP_COC_PSM, L2CAP_COC_MTU,
        app_ble_conn_l2cap_coc_event_handler, NULL);

示例
--------------

1. BLE L2CAP CoC 中心设备示例: :example:`bluetooth/ble_l2cap_coc/l2cap_coc_central`.
2. BLE L2CAP CoC 外围设备示例: :example:`bluetooth/ble_l2cap_coc/l2cap_coc_peripheral`.

API 参考
-----------------

请参考 :doc:`BLE 连接管理 <ble_conn_mgr>` 中的 L2CAP CoC API 参考。
